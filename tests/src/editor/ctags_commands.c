/*
   src/editor-plugins/ctags - tests of the commands, their results and the tags they own

   Copyright (C) 2026
   Ilia Maslakov il.smind@gmail.com

   This file is part of M-Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see https://www.gnu.org/licenses/.
 */

#define TEST_SUITE_NAME "/src/editor-plugins/ctags"

#include "tests/mctest.h"

#include "src/editor-plugins/ctags/ctags-plugin.c"

/* --------------------------------------------------------------------------------------------- */

#define TAGS_TEXT                                                                                  \
    "!_TAG_FILE_SORTED\t1\t//\n"                                                                   \
    "Foo\ta.h\t/^struct Foo {$/;\"\ts\tline:1\n"                                                   \
    "field\ta.h\t/^    struct { int inner; } field;$/;\"\tm\tline:2\tstruct:Foo"                   \
    "\ttyperef:struct:__anon1\n"                                                                   \
    "helper\ta.c\t/^int helper (void)$/;\"\tf\tline:1\n"                                           \
    "helper\tb.c\t/^int helper (int x)$/;\"\tf\tline:1\n"                                          \
    "helper_two\ta.c\t/^int helper_two (void)$/;\"\tf\tline:5\n"                                   \
    "inner\ta.h\t/^    struct { int inner; } field;$/;\"\tm\tline:2\tstruct:__anon1\n"             \
    "member\ta.h\t/^    int member;$/;\"\tm\tline:3\tstruct:Foo\n"
#define TAGS_COUNT    7

#define NEW_TAGS_TEXT "helper\ta.c\t/^int helper (void)$/;\"\tf\tline:1\n"

static char *tmp_dir;
static char *a_c;
static ctags_data_t data;
static mc_editor_host_t host;
static ctags_repo_t *repo;
static guint tags_total;
static guint tags_freed;

static const char *cursor_word;
static const char *current_file;
static gboolean jump_allowed;
static guint jumps;
static char *inserted;

static gboolean ui_picks;
static const char *ui_prompt;
static guint ui_calls;
static guint ui_len;
static guint messages;
static guint word_completions;

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
void
message (int flags, const char *title, const char *text, ...)
{
    (void) flags;
    (void) title;
    (void) text;
    messages++;
}

/* @Mock */
void
edit_complete_word_cmd (WEdit *edit)
{
    (void) edit;
    word_completions++;
}

/* @Mock */
ctags_entry_t *
ctags_ui_select (const char *title, GPtrArray *entries, const char *initial_filter)
{
    guint i;

    (void) title;
    (void) initial_filter;
    ui_calls++;
    ui_len = entries != NULL ? entries->len : 0;
    for (i = 0; i < ui_len; i++)
        ck_assert_ptr_nonnull (((ctags_entry_t *) g_ptr_array_index (entries, i))->name);
    return ui_picks && ui_len > 0 ? (ctags_entry_t *) g_ptr_array_index (entries, 0) : NULL;
}

/* @Mock */
ctags_entry_t *
ctags_ui_select_refs (const char *title, GPtrArray *entries)
{
    guint i;

    (void) title;
    ui_calls++;
    ui_len = entries->len;
    // a place of the history may have no name
    for (i = 0; i < ui_len; i++)
        ck_assert_ptr_nonnull (((ctags_entry_t *) g_ptr_array_index (entries, i))->file);
    return ui_picks && ui_len > 0 ? (ctags_entry_t *) g_ptr_array_index (entries, 0) : NULL;
}

/* @Mock */
ctags_entry_t *
ctags_ui_select_members (const char *scope, GPtrArray *rows)
{
    guint i;
    ctags_entry_t *first = NULL;

    (void) scope;
    ui_calls++;
    ui_len = rows->len;
    for (i = 0; i < rows->len; i++)
    {
        const ctags_member_row_t *row = g_ptr_array_index (rows, i);

        ck_assert_ptr_nonnull (row->display_name);
        if (first == NULL && !row->is_group)
            first = (ctags_entry_t *) row->original;
    }
    return ui_picks ? first : NULL;
}

/* @Mock */
char *
ctags_ui_prompt_symbol (const char *initial)
{
    (void) initial;
    return g_strdup (ui_prompt);
}

/* @Mock */
char *
ctags_ui_prompt_file (const char *initial)
{
    (void) initial;
    return g_strdup (ui_prompt);
}

/* @Mock */
char *
ctags_ui_prompt_scope (const char *initial)
{
    (void) initial;
    return g_strdup (ui_prompt);
}

/* @Mock */
char *
ctags_ui_prompt_tags_path (void)
{
    return g_strdup (ui_prompt);
}

/* @Mock */
gboolean
ctags_ui_manage_repos (GSList **repos)
{
    (void) repos;
    return FALSE;
}

/* @Mock */
void
ctags_ui_set_root_dir (const char *root_dir)
{
    (void) root_dir;
}

/* @Mock */
void
ctags_member_row_free (ctags_member_row_t *row)
{
    g_free (row->display_name);
    g_free (row->note);
    g_free (row);
}

/* --------------------------------------------------------------------------------------------- */

static char *
host_word (mc_editor_host_t *h, void *edit)
{
    (void) h;
    (void) edit;
    return g_strdup (cursor_word);
}

static char *
host_file (mc_editor_host_t *h, void *edit)
{
    (void) h;
    (void) edit;
    return g_strdup (current_file);
}

static long
host_line (mc_editor_host_t *h, void *edit)
{
    (void) h;
    (void) edit;
    return 7;
}

static gboolean
host_jump (mc_editor_host_t *h, void *edit, const char *file, long line)
{
    (void) h;
    (void) edit;
    (void) line;
    ck_assert_ptr_nonnull (file);
    if (jump_allowed)
        jumps++;
    return jump_allowed;
}

static void
host_insert (mc_editor_host_t *h, void *edit, const char *text, gsize remove_before)
{
    (void) h;
    (void) edit;
    (void) remove_before;
    g_free (inserted);
    inserted = g_strdup (text);
}

static void
host_redraw (mc_editor_host_t *h)
{
    (void) h;
}

static void
host_message (mc_editor_host_t *h, int flags, const char *title, const char *text)
{
    (void) h;
    (void) flags;
    (void) title;
    (void) text;
    messages++;
}

/* --------------------------------------------------------------------------------------------- */

static void
count_free (gpointer entry)
{
    tags_freed++;
    ctags_entry_free_ptr (entry);
}

static char *
in_tmp (const char *name)
{
    return g_build_filename (tmp_dir, name, (char *) NULL);
}

static void
write_tmp (const char *name, const char *text)
{
    char *path = in_tmp (name);

    ck_assert (g_file_set_contents (path, text, -1, NULL));
    g_free (path);
}

static void
watch (ctags_repo_t *r)
{
    g_ptr_array_set_free_func (r->entries, count_free);
    tags_total = r->entries->len;
    tags_freed = 0;
}

/* The results are gone and the tags of the repository are all there. */
static void
assert_tags_kept (void)
{
    guint i;

    ck_assert_uint_eq (tags_freed, 0);
    for (i = 0; i < repo->entries->len; i++)
        ck_assert_uint_gt (strlen (((ctags_entry_t *) g_ptr_array_index (repo->entries, i))->name),
                           0);
}

static void
load_repo (void)
{
    char *tags = in_tmp ("tags");

    repo = ctags_repo_load (tags);
    g_free (tags);
    ck_assert_ptr_nonnull (repo);
    data.repos = g_slist_prepend (NULL, repo);
    watch (repo);
}

/* Each tag freed once, when the repository goes. */
static void
unload_repo (void)
{
    guint total = tags_total;

    tags_freed = 0;
    ctags_repo_free (repo);
    repo = NULL;
    g_slist_free (data.repos);
    data.repos = NULL;
    ck_assert_uint_eq (tags_freed, total);
}

/* @Before */
static void
setup (void)
{
    tmp_dir = g_dir_make_tmp ("mc-ctags-XXXXXX", NULL);
    ck_assert_ptr_nonnull (tmp_dir);
    write_tmp ("tags", TAGS_TEXT);
    write_tmp ("a.c",
               "int helper (void)\n{\n    return helper_two ();\n}\nint helper_two (void);\n");
    write_tmp ("b.c", "int helper (int x)\n{\n    return x;\n}\n");
    write_tmp ("a.h", "struct Foo {\n    struct { int inner; } field;\n    int member;\n};\n");
    a_c = in_tmp ("a.c");

    memset (&data, 0, sizeof (data));
    memset (&host, 0, sizeof (host));
    host.get_cursor_word = host_word;
    host.get_current_file = host_file;
    host.get_cursor_line = host_line;
    host.jump_to = host_jump;
    host.insert_text = host_insert;
    host.redraw = host_redraw;
    host.message = host_message;
    data.host = &host;
    data.cfg.case_sensitive = TRUE;

    cursor_word = NULL;
    current_file = a_c;
    jump_allowed = TRUE;
    jumps = ui_calls = ui_len = messages = word_completions = 0;
    ui_picks = TRUE;
    ui_prompt = NULL;
    load_repo ();
}

/* @After */
static void
teardown (void)
{
    char *cmd;

    if (repo != NULL)
        unload_repo ();
    g_clear_pointer (&inserted, g_free);
    g_clear_pointer (&a_c, g_free);
    cmd = g_strdup_printf ("rm -rf '%s'", tmp_dir);
    ck_assert_int_eq (system (cmd), 0);
    g_free (cmd);
    g_clear_pointer (&tmp_dir, g_free);
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_goto_declaration)
{
    ck_assert_uint_eq (repo->entries->len, TAGS_COUNT);

    // one definition: no list
    cursor_word = "helper_two";
    ctags_goto_declaration (&data, NULL);
    ck_assert_uint_eq (ui_calls, 0);
    ck_assert_uint_eq (jumps, 1);
    assert_tags_kept ();

    // two: the list, and a choice
    cursor_word = "helper";
    ctags_goto_declaration (&data, NULL);
    ck_assert_uint_eq (ui_calls, 1);
    ck_assert_uint_eq (ui_len, 2);
    ck_assert_uint_eq (jumps, 2);
    assert_tags_kept ();

    // the list cancelled
    ui_picks = FALSE;
    ctags_goto_declaration (&data, NULL);
    ck_assert_uint_eq (jumps, 2);
    assert_tags_kept ();

    // an empty result
    cursor_word = "nothing";
    ctags_goto_declaration (&data, NULL);
    ck_assert_uint_eq (messages, 1);
    assert_tags_kept ();

    // no word, and the prompt cancelled
    cursor_word = NULL;
    ctags_goto_declaration (&data, NULL);
    assert_tags_kept ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_find_symbol)
{
    // by the whole name, and by its beginning when no tag has it
    cursor_word = "helper";
    ctags_find_symbol (&data, NULL);
    ck_assert_uint_eq (ui_len, 2);
    cursor_word = "hel";
    ctags_find_symbol (&data, NULL);
    ck_assert_uint_eq (ui_len, 3);
    ck_assert_uint_eq (jumps, 2);
    assert_tags_kept ();

    ui_picks = FALSE;
    ctags_find_symbol (&data, NULL);
    ck_assert_uint_eq (jumps, 2);

    // an empty result goes to the list as NULL
    cursor_word = "nothing";
    ctags_find_symbol (&data, NULL);
    ck_assert_uint_eq (ui_len, 0);
    assert_tags_kept ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_search_commands)
{
    ctags_search_symbol (&data, NULL);
    ck_assert_uint_eq (ui_len, TAGS_COUNT);
    ck_assert_uint_eq (jumps, 1);
    assert_tags_kept ();

    ctags_search_current_file (&data, NULL);
    ck_assert_uint_eq (ui_len, 2);
    ck_assert_uint_eq (jumps, 2);
    assert_tags_kept ();

    ui_picks = FALSE;
    ctags_search_symbol (&data, NULL);
    ctags_search_current_file (&data, NULL);
    ck_assert_uint_eq (jumps, 2);
    assert_tags_kept ();

    ui_picks = TRUE;
    ui_prompt = "a.c";
    ctags_search_file (&data, NULL);
    ck_assert_uint_eq (ui_len, 2);
    ck_assert_uint_eq (jumps, 3);
    assert_tags_kept ();

    // no such file, a cancelled prompt, and a file without tags
    ui_prompt = "none.c";
    ctags_search_file (&data, NULL);
    ck_assert_uint_eq (messages, 1);
    ui_prompt = NULL;
    ctags_search_file (&data, NULL);
    current_file = "/nowhere/x.c";
    ctags_search_current_file (&data, NULL);
    ck_assert_uint_eq (messages, 2);
    assert_tags_kept ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_complete_symbol)
{
    // one name, though two tags have it: inserted at once
    cursor_word = "helper_";
    ctags_complete_symbol (&data, NULL);
    ck_assert_uint_eq (ui_calls, 0);
    mctest_assert_str_eq (inserted, "helper_two");
    assert_tags_kept ();

    // several names: the list has each once
    cursor_word = "hel";
    ctags_complete_symbol (&data, NULL);
    ck_assert_uint_eq (ui_len, 2);
    mctest_assert_str_eq (inserted, "helper");
    assert_tags_kept ();

    ui_picks = FALSE;
    g_clear_pointer (&inserted, g_free);
    ctags_complete_symbol (&data, NULL);
    ck_assert_ptr_null (inserted);

    // nothing to complete from the tags: the words of the file do it
    cursor_word = "nothing";
    ctags_complete_symbol (&data, NULL);
    cursor_word = NULL;
    ctags_complete_symbol (&data, NULL);
    ck_assert_uint_eq (word_completions, 2);
    assert_tags_kept ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_show_members)
{
    // field and member of Foo, field an anonymous struct with one child
    cursor_word = "Foo";
    ctags_show_members (&data, NULL);
    ck_assert_uint_eq (ui_len, 3);
    ck_assert_uint_eq (jumps, 1);
    assert_tags_kept ();

    ui_picks = FALSE;
    ctags_show_members (&data, NULL);
    ck_assert_uint_eq (jumps, 1);

    // the scope asked for, not found, and cancelled
    ui_picks = TRUE;
    cursor_word = "nothing";
    ui_prompt = "Foo";
    ctags_show_members (&data, NULL);
    ck_assert_uint_eq (ui_len, 3);
    ck_assert_uint_eq (jumps, 2);
    ui_prompt = "none";
    ctags_show_members (&data, NULL);
    ck_assert_uint_eq (messages, 1);
    ui_prompt = NULL;
    ctags_show_members (&data, NULL);
    assert_tags_kept ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_show_history)
{
    ctags_show_history (&data, NULL);
    ck_assert_uint_eq (messages, 1);

    cursor_word = "helper_two";
    ctags_goto_declaration (&data, NULL);
    ctags_show_history (&data, NULL);
    ck_assert_uint_eq (ui_len, 2);
    ck_assert_uint_eq (jumps, 2);
    assert_tags_kept ();

    ui_picks = FALSE;
    ctags_show_history (&data, NULL);
    ck_assert_uint_eq (jumps, 2);

    // the jump refused: the result is freed all the same
    ui_picks = TRUE;
    jump_allowed = FALSE;
    ctags_show_history (&data, NULL);
    ck_assert_uint_eq (messages, 2);
    assert_tags_kept ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_grep_refs)
{
    cursor_word = "helper_two";
    ctags_grep_refs (&data, NULL);
    ck_assert_uint_eq (ui_calls, 1);
    ck_assert_uint_eq (ui_len, 2);
    ck_assert_uint_eq (jumps, 1);
    assert_tags_kept ();

    ui_picks = FALSE;
    ctags_grep_refs (&data, NULL);
    ck_assert_uint_eq (jumps, 1);

    cursor_word = "nothing";
    ctags_grep_refs (&data, NULL);
    ck_assert_uint_eq (messages, 1);
    assert_tags_kept ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The program of ctags of the test copies the file new_tags to what -f names */
static void
fake_ctags (const char *new_tags, int status)
{
    char *script;
    char *path = in_tmp ("fake_ctags");

    if (new_tags != NULL)
        write_tmp ("new_tags", new_tags);
    script = g_strdup_printf ("#!/bin/sh\n"
                              "[ %d = 0 ] || exit %d\n"
                              "while [ $# -gt 0 ]; do\n"
                              "  [ \"$1\" = -f ] && { shift; cp new_tags \"$1\"; }\n"
                              "  shift\n"
                              "done\n",
                              status, status);
    ck_assert (g_file_set_contents (path, script, -1, NULL));
    ck_assert_int_eq (chmod (path, 0700), 0);
    data.cfg.ctags_cmd = path;
    g_free (script);
}

/* @Test */
START_TEST (test_search_reindex_search_unload)
{
    guint i;

    cursor_word = "helper";
    for (i = 0; i < 3; i++)
    {
        ctags_goto_declaration (&data, NULL);
        ctags_search_symbol (&data, NULL);
        assert_tags_kept ();
    }

    fake_ctags (NEW_TAGS_TEXT, 0);
    ctags_reindex (&data, NULL);
    ck_assert_uint_eq (tags_freed, TAGS_COUNT);
    ck_assert_uint_eq (repo->entries->len, 1);
    g_free (data.cfg.ctags_cmd);
    data.cfg.ctags_cmd = NULL;
    watch (repo);

    ctags_goto_declaration (&data, NULL);
    ck_assert_uint_eq (jumps, 7);
    ctags_search_symbol (&data, NULL);
    ck_assert_uint_eq (ui_len, 1);
    assert_tags_kept ();
    unload_repo ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_reindex_that_fails_keeps_the_tags)
{
    // ctags fails
    fake_ctags (NULL, 1);
    ctags_reindex (&data, NULL);
    ck_assert_uint_eq (messages, 1);
    assert_tags_kept ();
    g_free (data.cfg.ctags_cmd);

    // ctags writes nothing that can be read
    fake_ctags ("", 0);
    ctags_reindex (&data, NULL);
    ck_assert_uint_eq (repo->entries->len, TAGS_COUNT);
    assert_tags_kept ();
    g_free (data.cfg.ctags_cmd);
    data.cfg.ctags_cmd = NULL;
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_broken_tags_files)
{
    char *path = in_tmp ("other");
    ctags_repo_t *other;
    GSList *repos;
    GPtrArray *found;

    // no file, an empty one, and one with nothing of a tags file
    ck_assert_ptr_null (ctags_repo_load (path));
    write_tmp ("other", "");
    ck_assert_ptr_null (ctags_repo_load (path));
    write_tmp ("other", "\x01\xff garbage\n\n\t\t\t\nname\n");
    ck_assert_ptr_null (ctags_repo_load (path));

    // a good line among bad ones, the last line cut
    write_tmp ("other",
               "\xff\xfe\n"
               "good\ta.c\t/^int good;$/;\"\tv\tline:1\n"
               "bad\n"
               "cut\ta.c\t/^int c");
    other = ctags_repo_load (path);
    ck_assert_ptr_nonnull (other);
    repos = g_slist_prepend (NULL, other);
    found = ctags_repos_find_exact (repos, "good");
    ck_assert_ptr_nonnull (found);
    ck_assert_uint_eq (found->len, 1);
    g_ptr_array_free (found, TRUE);
    ck_assert_ptr_null (ctags_repos_find_exact (repos, "absent"));
    g_slist_free (repos);
    ctags_repo_free (other);
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_repository_found_by_the_plugin)
{
    unload_repo ();
    data.cfg.auto_discover = TRUE;

    ctags_search_symbol (&data, NULL);
    ck_assert_uint_eq (g_slist_length (data.repos), 1);
    ck_assert_uint_eq (ui_len, TAGS_COUNT);

    // found once
    ctags_search_symbol (&data, NULL);
    ck_assert_uint_eq (g_slist_length (data.repos), 1);

    repo = (ctags_repo_t *) data.repos->data;
    watch (repo);
    unload_repo ();

    // an empty tags file gives no repository
    write_tmp ("tags", "");
    ctags_search_symbol (&data, NULL);
    ck_assert_ptr_null (data.repos);
    ck_assert_uint_eq (messages, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_goto_declaration);
    tcase_add_test (tc_core, test_find_symbol);
    tcase_add_test (tc_core, test_search_commands);
    tcase_add_test (tc_core, test_complete_symbol);
    tcase_add_test (tc_core, test_show_members);
    tcase_add_test (tc_core, test_show_history);
    tcase_add_test (tc_core, test_grep_refs);
    tcase_add_test (tc_core, test_search_reindex_search_unload);
    tcase_add_test (tc_core, test_reindex_that_fails_keeps_the_tags);
    tcase_add_test (tc_core, test_broken_tags_files);
    tcase_add_test (tc_core, test_repository_found_by_the_plugin);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
