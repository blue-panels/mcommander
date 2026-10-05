/*
   Tests for the user menu that edits itself.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/src/usermenu_ini"

#include "tests/mctest.h"

#include <stdio.h>
#include <unistd.h>

#include "src/usermenu_ini.h"

/* --------------------------------------------------------------------------------------------- */

static char *
write_temp_file (const char *content)
{
    char *name;
    int fd;
    FILE *file;

    name = g_build_filename (g_get_tmp_dir (), "mc-usermenu-test-XXXXXX", (char *) NULL);
    fd = g_mkstemp (name);
    if (fd == -1)
    {
        g_free (name);
        return NULL;
    }

    file = fdopen (fd, "w");
    if (file == NULL)
    {
        close (fd);
        g_free (name);
        return NULL;
    }

    if (content != NULL)
        fputs (content, file);
    fclose (file);

    return name;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
entries_new (void)
{
    return g_ptr_array_new_with_free_func ((GDestroyNotify) user_menu_entry_free);
}

/* --------------------------------------------------------------------------------------------- */

static user_menu_entry_t *
entry_new (const char *label, char hotkey, const char *command)
{
    user_menu_entry_t *entry;

    entry = g_new0 (user_menu_entry_t, 1);
    entry->label = g_strdup (label);
    entry->hotkey = hotkey;
    entry->command = g_strdup (command);

    return entry;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_command_survives_the_single_line)
{
    static const char *commands[] = {
        "", "echo one", "echo one\necho two", "echo \\ and \\n", "\n\n",
    };
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (commands); i++)
    {
        char *line, *back;

        line = user_menu_ini_escape (commands[i]);
        // whatever the command holds, the field it goes into is one line
        ck_assert_ptr_eq (strchr (line, '\n'), NULL);

        back = user_menu_ini_unescape (line);
        ck_assert_str_eq (back, commands[i]);

        g_free (line);
        g_free (back);
    }
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_entries_are_read_in_the_order_of_the_file)
{
    char *file;
    GPtrArray *entries;
    user_menu_entry_t *entry;

    file = write_temp_file ("[Second]\n"
                            "hotkey=b\n"
                            "command=echo two\n"
                            "\n"
                            "[First]\n"
                            "hotkey=a\n"
                            "command=echo one\n"
                            "view=true\n"
                            "silent=true\n");
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 1, NULL));

    ck_assert_uint_eq (entries->len, 2);

    entry = g_ptr_array_index (entries, 0);
    ck_assert_str_eq (entry->label, "Second");
    ck_assert_int_eq (entry->hotkey, 'b');
    ck_assert_str_eq (entry->command, "echo two");
    ck_assert (!entry->view);
    ck_assert (!entry->silent);
    ck_assert_int_eq (entry->level, 1);

    entry = g_ptr_array_index (entries, 1);
    ck_assert_str_eq (entry->label, "First");
    ck_assert (entry->view);
    ck_assert (entry->silent);

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_what_is_written_is_read_back)
{
    char *file;
    GPtrArray *entries;
    user_menu_entry_t *entry;

    file = write_temp_file (NULL);
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    g_ptr_array_add (entries, entry_new ("Pack it", 'p', "tar caf %{Name} %s"));
    g_ptr_array_add (entries, entry_new ("Two lines", 'l', "echo one\necho two"));

    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));

    ck_assert_uint_eq (entries->len, 2);

    entry = g_ptr_array_index (entries, 0);
    ck_assert_str_eq (entry->label, "Pack it");
    ck_assert_int_eq (entry->hotkey, 'p');
    ck_assert_str_eq (entry->command, "tar caf %{Name} %s");

    // A command of several lines is one value of the file, newlines and all.
    entry = g_ptr_array_index (entries, 1);
    ck_assert_str_eq (entry->command, "echo one\necho two");

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_only_the_level_of_the_file_is_written)
{
    char *file;
    GPtrArray *entries;

    file = write_temp_file (NULL);
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    g_ptr_array_add (entries, entry_new ("Mine", 'm', "echo mine"));
    g_ptr_array_add (entries, entry_new ("Theirs", 't', "echo theirs"));
    ((user_menu_entry_t *) g_ptr_array_index (entries, 1))->level = 2;

    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));

    ck_assert_uint_eq (entries->len, 1);
    ck_assert_str_eq (((user_menu_entry_t *) g_ptr_array_index (entries, 0))->label, "Mine");

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_key_of_a_later_version_is_kept)
{
    char *file;
    GPtrArray *entries;
    GKeyFile *keys;
    char *value;

    file = write_temp_file ("[Entry]\n"
                            "hotkey=e\n"
                            "command=echo old\n"
                            "submenu=Tools\n");
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));
    ck_assert_uint_eq (entries->len, 1);

    g_free (((user_menu_entry_t *) g_ptr_array_index (entries, 0))->command);
    ((user_menu_entry_t *) g_ptr_array_index (entries, 0))->command = g_strdup ("echo new");

    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);

    keys = g_key_file_new ();
    ck_assert (g_key_file_load_from_file (keys, file, G_KEY_FILE_NONE, NULL));

    value = g_key_file_get_string (keys, "Entry", "command", NULL);
    ck_assert_str_eq (value, "echo new");
    g_free (value);

    // The key this version knows nothing about is still there.
    value = g_key_file_get_string (keys, "Entry", "submenu", NULL);
    ck_assert_ptr_ne (value, NULL);
    ck_assert_str_eq (value, "Tools");
    g_free (value);

    g_key_file_free (keys);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_file_that_is_not_there_gives_no_entries)
{
    GPtrArray *entries;

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, "/nonexistent/mc6/menu.ini", 0, NULL));
    ck_assert_uint_eq (entries->len, 0);
    g_ptr_array_free (entries, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_file_that_cannot_be_read_is_not_written_over)
{
    static const char *const broken = "[Diff]\n"
                                      "hotkey=d\n"
                                      "command=diff %f %D/%F\n"
                                      "  | less\n";
    char *file;
    char *content = NULL;
    GPtrArray *entries;
    GError *error = NULL;

    file = write_temp_file (broken);
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    ck_assert (!user_menu_ini_load_file (entries, file, 0, &error));
    ck_assert_ptr_ne (error, NULL);
    g_clear_error (&error);
    ck_assert_uint_eq (entries->len, 0);

    g_ptr_array_add (entries, entry_new ("New", 'n', "echo new"));

    ck_assert (!user_menu_ini_save_file (file, entries, 0, &error));
    ck_assert_ptr_ne (error, NULL);
    g_clear_error (&error);

    ck_assert (g_file_get_contents (file, &content, NULL, NULL));
    ck_assert_str_eq (content, broken);

    g_free (content);
    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_menu_written_by_hand_is_imported)
{
    char *file;
    GPtrArray *entries;
    user_menu_entry_t *entry;
    guint added;

    file = write_temp_file ("shell_patterns=0\n"
                            "\n"
                            "# a comment\n"
                            "+ t r & ! t t\n"
                            "c       Compile this file\n"
                            "        make \"`basename %f .c`\"\n"
                            "        echo done\n"
                            "\n"
                            "i       if [] then else\n"
                            "        echo brackets\n"
                            "\n"
                            "d       Same label\n"
                            "        echo one\n"
                            "\n"
                            "D       Same label\n"
                            "        echo two\n");
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    added = user_menu_ini_import_file (entries, file, 1);

    // the directive, the comment and the condition are not entries
    ck_assert_uint_eq (added, 4);
    ck_assert_uint_eq (entries->len, 4);

    entry = g_ptr_array_index (entries, 0);
    ck_assert_int_eq (entry->hotkey, 'c');
    ck_assert_str_eq (entry->label, "Compile this file");
    // the commands lose the indentation and keep their order
    ck_assert_str_eq (entry->command, "make \"`basename %f .c`\"\necho done");

    // a group name holds no brackets, so the label cannot either
    entry = g_ptr_array_index (entries, 1);
    ck_assert_str_eq (entry->label, "if () then else");

    // and two entries cannot share a label
    ck_assert_str_eq (((user_menu_entry_t *) g_ptr_array_index (entries, 2))->label, "Same label");
    ck_assert_str_eq (((user_menu_entry_t *) g_ptr_array_index (entries, 3))->label,
                      "Same label (2)");

    // what was imported is what a file of the new kind can hold
    {
        char *out;

        out = write_temp_file (NULL);
        ck_assert (user_menu_ini_save_file (out, entries, 1, NULL));
        g_ptr_array_free (entries, TRUE);

        entries = entries_new ();
        ck_assert (user_menu_ini_load_file (entries, out, 1, NULL));
        ck_assert_uint_eq (entries->len, 4);

        unlink (out);
        g_free (out);
    }

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_submenu_and_its_entries_are_written_and_read_back)
{
    char *file;
    GPtrArray *entries;
    user_menu_entry_t *box, *child, *top;
    GKeyFile *keys;

    file = write_temp_file (NULL);
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();

    box = entry_new ("Archives", 'a', "");
    box->is_submenu = TRUE;
    g_ptr_array_add (entries, box);

    child = entry_new ("Pack it", 'p', "tar caf x.tgz %s");
    child->parent = g_strdup ("Archives");
    g_ptr_array_add (entries, child);

    g_ptr_array_add (entries, entry_new ("Top one", '1', "echo one"));

    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);

    // a submenu holds no command; an entry at the top has no parent
    keys = g_key_file_new ();
    ck_assert (g_key_file_load_from_file (keys, file, G_KEY_FILE_NONE, NULL));
    ck_assert (g_key_file_get_boolean (keys, "Archives", "submenu", NULL));
    ck_assert (!g_key_file_has_key (keys, "Archives", "command", NULL));
    ck_assert (!g_key_file_has_key (keys, "Top one", "parent", NULL));
    g_key_file_free (keys);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));
    ck_assert_uint_eq (entries->len, 3);

    box = g_ptr_array_index (entries, 0);
    ck_assert (box->is_submenu);
    ck_assert_ptr_eq (box->parent, NULL);

    child = g_ptr_array_index (entries, 1);
    ck_assert (!child->is_submenu);
    ck_assert_str_eq (child->parent, "Archives");
    ck_assert_str_eq (child->command, "tar caf x.tgz %s");

    top = g_ptr_array_index (entries, 2);
    ck_assert_ptr_eq (top->parent, NULL);

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_a_command_survives_the_single_line);
    tcase_add_test (tc_core, test_entries_are_read_in_the_order_of_the_file);
    tcase_add_test (tc_core, test_what_is_written_is_read_back);
    tcase_add_test (tc_core, test_only_the_level_of_the_file_is_written);
    tcase_add_test (tc_core, test_a_key_of_a_later_version_is_kept);
    tcase_add_test (tc_core, test_a_file_that_is_not_there_gives_no_entries);
    tcase_add_test (tc_core, test_a_file_that_cannot_be_read_is_not_written_over);
    tcase_add_test (tc_core, test_a_menu_written_by_hand_is_imported);
    tcase_add_test (tc_core, test_a_submenu_and_its_entries_are_written_and_read_back);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
