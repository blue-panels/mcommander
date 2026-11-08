/*
   src/viewer - unit tests for the fast path of the viewer search

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

#define TEST_SUITE_NAME "/src/viewer/fast_search"

#include "tests/mctest.h"

#include <fcntl.h>
#include <locale.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/charsets.h"
#include "lib/global.h"
#include "lib/search.h"
#include "lib/search/internal.h"
#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "src/vfs/local/local.h"

#include "src/viewer/internal.h"

#include "src/viewer/search.c"

/*** mocks ***********************************************************************************/

void
message (int flags, const char *title, const char *text, ...)
{
    (void) flags;
    (void) title;
    (void) text;
}

int
query_dialog (const char *header, const char *text, int flags, int count, ...)
{
    (void) header;
    (void) text;
    (void) flags;
    (void) count;
    return 1;
}

void
status_msg_init (status_msg_t *sm, const char *title, double delay, status_msg_cb init_cb,
                 status_msg_update_cb update_cb, status_msg_cb deinit_cb)
{
    (void) sm;
    (void) title;
    (void) delay;
    (void) init_cb;
    (void) update_cb;
    (void) deinit_cb;
}

void
status_msg_deinit (status_msg_t *sm)
{
    (void) sm;
}

/*** fixtures ********************************************************************************/

#define CHUNK MC_SEARCH__PLAIN_CHUNK

static WView test_view;
static mcview_search_status_msg_t test_ssm;
static char *test_path;
static int update_calls;
static off_t last_update_pos;
static gboolean abort_search;

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);
    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    memset (&test_view, 0, sizeof (test_view));
    mcview_init (&test_view);
    memset (&test_ssm, 0, sizeof (test_ssm));
    test_ssm.view = &test_view;
    test_path = NULL;
    update_calls = 0;
    last_update_pos = 0;
    abort_search = FALSE;

    mcview_search_options.type = MC_SEARCH_T_NORMAL;
    mcview_search_options.case_sens = TRUE;
    mcview_search_options.backwards = FALSE;
    mcview_search_options.whole_words = FALSE;
    mcview_search_options.all_codepages = FALSE;
}

/* @After */
static void
teardown (void)
{
    mc_global.utf8_display = FALSE;
    cp_source = NULL;
    test_view.filter_active = FALSE;
    if (test_view.filter_offsets != NULL)
        g_array_free (test_view.filter_offsets, TRUE);
    mcview_search_deinit (&test_view);
    mcview_close_datasource (&test_view);
    mcview_selection_done (&test_view);
    if (test_path != NULL)
    {
        unlink (test_path);
        g_free (test_path);
    }
    vfs_shut ();
    str_uninit_strings ();
}

/*** helpers *********************************************************************************/

static void
set_utf8_mode (gboolean utf8)
{
    mc_global.utf8_display = utf8;
    cp_source = utf8 ? "UTF-8" : NULL;
}

static mc_search_cbret_t
test_update_cb (const void *user_data, off_t char_offset)
{
    (void) user_data;

    update_calls++;
    last_update_pos = char_offset;
    return abort_search ? MC_SEARCH_CB_ABORT : MC_SEARCH_CB_OK;
}

static void
close_file (void)
{
    mcview_close_datasource (&test_view);
    if (test_path != NULL)
    {
        unlink (test_path);
        g_free (test_path);
        test_path = NULL;
    }
}

static void
open_file (const char *content, gsize len, int flags)
{
    GError *error = NULL;
    vfs_path_t *vpath;
    struct stat st;
    int fd;

    close_file ();

    fd = g_file_open_tmp ("mcview_fast_search_XXXXXX", &test_path, &error);
    ck_assert_msg (fd != -1, "cannot create a file");
    close (fd);
    ck_assert (g_file_set_contents (test_path, content, (gssize) len, NULL));

    vpath = vfs_path_from_str (test_path);
    fd = mc_open (vpath, flags);
    vfs_path_free (vpath, TRUE);
    ck_assert_int_ne (fd, -1);
    ck_assert_int_eq (stat (test_path, &st), 0);
    mcview_set_datasource_file (&test_view, fd, &st);
}

static void
start_search (const char *pattern, mc_search_type_t type, gboolean case_sens)
{
    mcview_search_deinit (&test_view);
    test_view.search = NULL;
    test_view.search_nroff_seq = NULL;

    mcview_search_options.type = type;
    mcview_search_options.case_sens = case_sens;
    test_view.last_search_string = g_strdup (pattern);
    ck_assert (mcview_search_init (&test_view));
    test_view.search->update_fn = test_update_cb;
}

/* What mcview_do_search() does before it searches. */
static void
choose_source (gboolean blocks)
{
    test_view.search->block_fn =
        (blocks && test_view.datasource == DS_FILE && !test_view.mode_flags.nroff)
        ? mcview_search_block_cmd_callback
        : NULL;
}

static gboolean
find_from (off_t start, off_t end, gsize *len)
{
    choose_source (TRUE);
    return mcview_find (&test_ssm, start, end, len);
}

/* Every match, overlapping ones too, as "offset:length " */
static char *
all_matches (gboolean blocks, off_t end)
{
    GString *out;
    off_t start = 0;

    out = g_string_new (NULL);
    choose_source (blocks);

    while (start <= end)
    {
        gsize len = 0;

        if (!mc_search_run (test_view.search, &test_ssm, start, end, &len))
            break;

        g_string_append_printf (out, "%lld:%zu ", (long long) test_view.search->normal_offset, len);
        start = test_view.search->normal_offset + 1;
    }

    return g_string_free (out, FALSE);
}

/* The match a backward search stops at, from every start. */
static char *
all_backward_matches (gboolean blocks, off_t size)
{
    GString *out;
    off_t start;

    out = g_string_new (NULL);
    choose_source (blocks);
    mcview_search_options.backwards = TRUE;

    for (start = 0; start <= size; start++)
    {
        gsize len = 0;

        if (mcview_find (&test_ssm, start, size, &len))
            g_string_append_printf (out, "%lld:%zu ", (long long) test_view.search->normal_offset,
                                    len);
        else
            g_string_append (out, "- ");
    }

    mcview_search_options.backwards = FALSE;
    return g_string_free (out, FALSE);
}

static void
assert_same_results (char *blocks, char *generic, const char *pattern, gboolean case_sens,
                     off_t end)
{
    ck_assert_msg (strcmp (blocks, generic) == 0, "'%s' %s, up to %lld: blocks [%s], generic [%s]",
                   pattern, case_sens ? "case sensitive" : "caseless", (long long) end, blocks,
                   generic);
    g_free (blocks);
    g_free (generic);
}

static void
assert_same_as_generic (const char *content, gsize len, off_t end, const char *pattern,
                        mc_search_type_t type, gboolean case_sens)
{
    gboolean caseless;
    GString *needle;

    open_file (content, len, O_RDONLY);
    start_search (pattern, type, case_sens);

    needle = mc_search__plain_needle (test_view.search, &caseless);
    ck_assert_msg (needle != NULL, "'%s' is not a plain search", pattern);
    g_string_free (needle, TRUE);

    assert_same_results (all_matches (TRUE, end), all_matches (FALSE, end), pattern, case_sens,
                         end);
}

static char *
filler (gsize size)
{
    char *buf = g_malloc (size + 1);

    memset (buf, 'z', size);
    buf[size] = '\0';
    return buf;
}

/*** tests ***********************************************************************************/

/* clang-format off */
static const struct
{
    const char *content;
    const char *pattern;
    mc_search_type_t type;
    gboolean case_sens;
} same_as_generic_ds[] = {
    { "alma AlMa ALMA alm", "alma", MC_SEARCH_T_NORMAL, TRUE },
    { "alma AlMa ALMA alm", "alma", MC_SEARCH_T_NORMAL, FALSE },
    { "alma AlMa ALMA alm", "AlMa", MC_SEARCH_T_NORMAL, FALSE },
    { "line one\nline two\r\nline", "line", MC_SEARCH_T_NORMAL, FALSE },
    { "aaaa", "aa", MC_SEARCH_T_NORMAL, TRUE },
    { "aaabaaaab aab", "aaab", MC_SEARCH_T_NORMAL, TRUE },
    { "abababac ababac", "ababac", MC_SEARCH_T_NORMAL, TRUE },
    { "aAaAb AAAB aaab", "AAAB", MC_SEARCH_T_NORMAL, FALSE },
    { "abcabcabd abcabd", "ABCABD", MC_SEARCH_T_NORMAL, FALSE },
    { "a1B a1b A1B", "a1b", MC_SEARCH_T_NORMAL, FALSE },
    { "1aB 1Ab 1ab", "1AB", MC_SEARCH_T_NORMAL, FALSE },
    { "ko\xc5\xa1tunica KO\xc5\xa0TUNICA", "ko\xc5\xa1tunica", MC_SEARCH_T_NORMAL, TRUE },
    { "\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82", "\xd1\x80\xd0\xb8", MC_SEARCH_T_NORMAL, TRUE },
    { "the \xe2\x84\xaa sign, k and K", "k", MC_SEARCH_T_NORMAL, FALSE },
    { "the \xe2\x84\xaa sign, k and K", "K", MC_SEARCH_T_NORMAL, TRUE },
    { "long \xc5\xbf and s and S", "s", MC_SEARCH_T_NORMAL, FALSE },
    { "ma\xc5\xbf and mas", "AS", MC_SEARCH_T_NORMAL, FALSE },
    { "alma", "almafa", MC_SEARCH_T_NORMAL, TRUE },
    { "", "x", MC_SEARCH_T_NORMAL, TRUE },
    { "alma ALMA", "61 6c", MC_SEARCH_T_HEX, TRUE },
    { "alma ALMA", "61 6c", MC_SEARCH_T_HEX, FALSE },
    { "alma ALMA", "0x61 0X6C", MC_SEARCH_T_HEX, TRUE },
    { "alma\xff\xfe\xff", "ff fe", MC_SEARCH_T_HEX, FALSE },
};
/* clang-format on */

START_TEST (test_same_as_generic)
{
    const size_t i = (size_t) _i / 2;
    const gsize len = strlen (same_as_generic_ds[i].content);

    set_utf8_mode (_i % 2 != 0);
    assert_same_as_generic (same_as_generic_ds[i].content, len, (off_t) len,
                            same_as_generic_ds[i].pattern, same_as_generic_ds[i].type,
                            same_as_generic_ds[i].case_sens);
}
END_TEST

START_TEST (test_backward_same_as_generic)
{
    const size_t i = (size_t) _i / 2;
    const gsize len = strlen (same_as_generic_ds[i].content);

    set_utf8_mode (_i % 2 != 0);
    open_file (same_as_generic_ds[i].content, len, O_RDONLY);
    start_search (same_as_generic_ds[i].pattern, same_as_generic_ds[i].type,
                  same_as_generic_ds[i].case_sens);

    assert_same_results (all_backward_matches (TRUE, (off_t) len),
                         all_backward_matches (FALSE, (off_t) len), same_as_generic_ds[i].pattern,
                         same_as_generic_ds[i].case_sens, (off_t) len);
}
END_TEST

START_TEST (test_same_as_generic_on_random_data)
{
    static const char *const tokens[] = {
        "a", "k", "S", "K", "s", " ", "\t", "\r", "\xc3\xa9", "\xe2\x84\xaa", "\xc5\xbf",
    };
    const gint32 ascii_tokens = 8;
    GRand *rnd;
    int round;

    rnd = g_rand_new_with_seed (5205);
    set_utf8_mode (_i != 0);

    for (round = 0; round < 400; round++)
    {
        const gboolean case_sens = g_rand_boolean (rnd);
        const gint32 content_len = g_rand_int_range (rnd, 0, 120);
        const gint32 needle_len = g_rand_int_range (rnd, 1, 5);
        GString *content, *pattern;
        gint32 i;

        content = g_string_new (NULL);
        pattern = g_string_new (NULL);

        for (i = 0; i < content_len; i++)
        {
            if (g_rand_int_range (rnd, 0, 10) == 0)
                g_string_append_c (content, '\n');
            else
                g_string_append (content, tokens[g_rand_int_range (rnd, 0, G_N_ELEMENTS (tokens))]);
        }

        for (i = 0; i < needle_len; i++)
            g_string_append (pattern,
                             tokens[g_rand_int_range (
                                 rnd, 0, case_sens ? G_N_ELEMENTS (tokens) : ascii_tokens)]);

        assert_same_as_generic (content->str, content->len,
                                g_rand_int_range (rnd, 0, (gint32) content->len + 1), pattern->str,
                                MC_SEARCH_T_NORMAL, case_sens);

        g_string_free (content, TRUE);
        g_string_free (pattern, TRUE);
    }

    g_rand_free (rnd);
}
END_TEST

/* A hex pattern must stop at the end of a line that is shown, not go on to a hidden one. */
START_TEST (test_filter_does_not_reach_a_hidden_line)
{
    off_t bol = 0;
    gsize len = 0;

    open_file ("A\nB hidden\n", 11, O_RDONLY);
    test_view.filter_offsets = g_array_new (FALSE, FALSE, sizeof (off_t));
    g_array_append_val (test_view.filter_offsets, bol);
    test_view.filter_active = TRUE;
    test_view.dpy_bbar_dirty = FALSE;
    test_view.dirty = 0;

    start_search ("41 0a 42", MC_SEARCH_T_HEX, TRUE);
    mcview_do_search (&test_view, 0);
    ck_assert_int_eq (test_view.search->error, MC_SEARCH_E_NOTFOUND);

    // the newline of the line that is shown is part of it
    start_search ("41 0a", MC_SEARCH_T_HEX, TRUE);
    choose_source (TRUE);
    ck_assert (mc_search_run (test_view.search, &test_ssm, 0, 1, &len));
    ck_assert_int_eq (test_view.search->normal_offset, 0);
}
END_TEST

/* In a Turkish single byte locale I goes with a dotless i: not a plain search. */
START_TEST (test_locale_case_rules)
{
    static const char content[] = "i I \xfd";
    char *old_locale;
    gboolean caseless;
    GString *needle;
    char *matches;

    old_locale = g_strdup (setlocale (LC_CTYPE, NULL));
    if (setlocale (LC_CTYPE, "tr_TR.ISO-8859-9") == NULL)
    {
        g_free (old_locale);
        return;  // the locale is not there on this machine
    }

    str_uninit_strings ();
    str_init_strings ("ISO-8859-9");
    cp_source = "ISO-8859-9";
    cp_display = "ISO-8859-9";

    open_file (content, strlen (content), O_RDONLY);
    start_search ("I", MC_SEARCH_T_NORMAL, FALSE);
    ck_assert (mc_search__plain_needle (test_view.search, &caseless) == NULL);

    matches = all_matches (TRUE, (off_t) strlen (content));
    ck_assert_str_eq (matches, "2:1 4:1 ");
    g_free (matches);

    start_search ("alma", MC_SEARCH_T_NORMAL, FALSE);
    needle = mc_search__plain_needle (test_view.search, &caseless);
    ck_assert (needle != NULL);
    g_string_free (needle, TRUE);

    cp_display = NULL;
    setlocale (LC_CTYPE, old_locale);
    g_free (old_locale);
    str_uninit_strings ();
    str_init_strings (NULL);
}
END_TEST

/* A needle that keeps almost matching: one scan, not a comparison from every position. */
START_TEST (test_repeated_needle_in_repeated_data)
{
    const gsize size = 512 * 1024;
    const gsize needle_len = 10000;
    char *buf = g_malloc (size);
    char *pattern = g_malloc (needle_len + 1);
    gsize len = 0;
    int caseless;

    memset (buf, 'a', size);
    buf[size - 1] = 'b';
    memset (pattern, 'a', needle_len);
    pattern[needle_len - 1] = 'b';
    pattern[needle_len] = '\0';
    open_file (buf, size, O_RDONLY);

    for (caseless = 0; caseless <= 1; caseless++)
    {
        start_search (pattern, MC_SEARCH_T_NORMAL, caseless == 0);
        ck_assert (find_from (0, (off_t) size, &len));
        ck_assert_int_eq (test_view.search->normal_offset, (off_t) (size - needle_len));
        ck_assert_int_eq (len, needle_len);
    }

    g_free (buf);
    g_free (pattern);
}
END_TEST

/* Two letters make the needle overlap itself in every way. */
START_TEST (test_same_as_generic_on_two_letters)
{
    GRand *rnd;
    int round;

    rnd = g_rand_new_with_seed (5206);

    for (round = 0; round < 600; round++)
    {
        const gboolean case_sens = g_rand_boolean (rnd);
        const gint32 content_len = g_rand_int_range (rnd, 1, 160);
        const gint32 needle_len = g_rand_int_range (rnd, 1, 13);
        GString *content, *pattern;
        gint32 i;

        content = g_string_new (NULL);
        pattern = g_string_new (NULL);

        for (i = 0; i < content_len; i++)
            g_string_append_c (content, "abAB"[g_rand_int_range (rnd, 0, case_sens ? 2 : 4)]);
        for (i = 0; i < needle_len; i++)
            g_string_append_c (pattern, "ab"[g_rand_int_range (rnd, 0, 2)]);

        // planted, as a random needle of this length is rarely there
        if (content->len >= pattern->len && g_rand_boolean (rnd))
            g_string_overwrite (
                content,
                (gsize) g_rand_int_range (rnd, 0, (gint32) (content->len - pattern->len + 1)),
                pattern->str);

        assert_same_as_generic (content->str, content->len, (off_t) content->len, pattern->str,
                                MC_SEARCH_T_NORMAL, case_sens);

        g_string_free (content, TRUE);
        g_string_free (pattern, TRUE);
    }

    g_rand_free (rnd);
}
END_TEST

START_TEST (test_needle_is_refused)
{
    static const struct
    {
        const char *pattern;
        mc_search_type_t type;
        gboolean case_sens;
    } ds[] = {
        { "a.c", MC_SEARCH_T_REGEX, TRUE },        { "a*c", MC_SEARCH_T_GLOB, TRUE },
        { "\"abc\"", MC_SEARCH_T_HEX, TRUE },      { "41 \"abc\"", MC_SEARCH_T_HEX, TRUE },
        { "100", MC_SEARCH_T_HEX, TRUE },          { "zz", MC_SEARCH_T_HEX, TRUE },
        { "   ", MC_SEARCH_T_HEX, TRUE },          { "a\nb", MC_SEARCH_T_NORMAL, TRUE },
        { "\xc5\xa1", MC_SEARCH_T_NORMAL, FALSE },
    };
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (ds); i++)
    {
        gboolean caseless;

        start_search (ds[i].pattern, ds[i].type, ds[i].case_sens);
        ck_assert_msg (mc_search__plain_needle (test_view.search, &caseless) == NULL,
                       "'%s' must not be a plain search", ds[i].pattern);
    }
}
END_TEST

START_TEST (test_needle_bytes)
{
    gboolean caseless;
    GString *needle;

    start_search ("AbC", MC_SEARCH_T_NORMAL, FALSE);
    needle = mc_search__plain_needle (test_view.search, &caseless);
    ck_assert_str_eq (needle->str, "abc");
    ck_assert (caseless);
    g_string_free (needle, TRUE);

    start_search ("AbC", MC_SEARCH_T_NORMAL, TRUE);
    needle = mc_search__plain_needle (test_view.search, &caseless);
    ck_assert_str_eq (needle->str, "AbC");
    ck_assert (!caseless);
    g_string_free (needle, TRUE);

    start_search (" 41  0a 00 ff ", MC_SEARCH_T_HEX, FALSE);
    needle = mc_search__plain_needle (test_view.search, &caseless);
    ck_assert_int_eq (needle->len, 4);
    ck_assert_mem_eq (needle->str, "\x41\x0a\x00\xff", 4);
    ck_assert (!caseless);
    g_string_free (needle, TRUE);
}
END_TEST

/* The search by lines would not find these. */
START_TEST (test_hex_finds_across_lines_and_nul)
{
    gsize len = 0;

    open_file ("first\nAl", 8, O_RDONLY);
    start_search ("0a 41 6c", MC_SEARCH_T_HEX, TRUE);
    ck_assert (find_from (0, 8, &len));
    ck_assert_int_eq (test_view.search->normal_offset, 5);
    ck_assert_int_eq (len, 3);

    open_file ("ab\0\0cd", 6, O_RDONLY);
    start_search ("00 63", MC_SEARCH_T_HEX, TRUE);
    ck_assert (find_from (0, 6, &len));
    ck_assert_int_eq (test_view.search->normal_offset, 3);
    ck_assert_int_eq (len, 2);
}
END_TEST

/* A pattern only the block search finds tells which path was taken. */
START_TEST (test_generic_search_is_kept_for)
{
    gsize len = 0;

    open_file ("first\nAl", 8, O_RDONLY);

    load_codepages_list ();
    start_search ("0a 41", MC_SEARCH_T_HEX, TRUE);
    test_view.search->is_all_charsets = TRUE;
    ck_assert (!find_from (0, 8, &len));
    free_codepages_list ();

    start_search ("0a 41", MC_SEARCH_T_HEX, TRUE);
    test_view.mode_flags.nroff = TRUE;
    ck_assert (!find_from (0, 8, &len));
    test_view.mode_flags.nroff = FALSE;

    open_file ("xfoo foo.", 9, O_RDONLY);
    start_search ("foo", MC_SEARCH_T_NORMAL, TRUE);
    test_view.search->whole_words = TRUE;
    ck_assert (find_from (0, 9, &len));
    ck_assert_int_eq (test_view.search->normal_offset, 5);
}
END_TEST

START_TEST (test_chunk_boundaries)
{
    static const struct
    {
        const char *needle;
        const char *text;
        gboolean case_sens;
    } ds[] = {
        { "needle", "needle", TRUE },
        { "needle", "NeEdLe", FALSE },
        { "\xc5\xa1\xc5\xa0", "\xc5\xa1\xc5\xa0", TRUE },
    };
    const size_t text_len = strlen (ds[_i].text);
    const gsize size = (gsize) CHUNK * 3;
    char *buf = filler (size);
    int boundary, delta;

    for (boundary = 1; boundary <= 2; boundary++)
        for (delta = -(int) text_len - 1; delta <= 1; delta++)
        {
            const gsize at = (gsize) (boundary * CHUNK + delta);
            gsize len = 0;

            memcpy (buf + at, ds[_i].text, text_len);
            open_file (buf, size, O_RDONLY);
            start_search (ds[_i].needle, MC_SEARCH_T_NORMAL, ds[_i].case_sens);

            ck_assert_msg (find_from (0, (off_t) size, &len), "not found at %zu", at);
            ck_assert_int_eq (test_view.search->normal_offset, (off_t) at);
            ck_assert_int_eq (len, text_len);

            memset (buf + at, 'z', text_len);
        }

    g_free (buf);
}
END_TEST

/* The kept bytes must hold enough to see such a character split by the end of a chunk. */
START_TEST (test_folded_character_across_chunks)
{
    const gsize size = (gsize) CHUNK * 2;
    char *buf = filler (size);
    gsize len = 0;
    int delta;

    set_utf8_mode (TRUE);

    for (delta = -3; delta <= 0; delta++)
    {
        const gsize at = (gsize) (CHUNK + delta);

        memcpy (buf + at, "\xe2\x84\xaa", 3);
        open_file (buf, size, O_RDONLY);
        start_search ("k", MC_SEARCH_T_NORMAL, FALSE);

        ck_assert_msg (find_from (0, (off_t) size, &len), "not found at %zu", at);
        ck_assert_int_eq (test_view.search->normal_offset, (off_t) at);
        ck_assert_int_eq (len, 3);

        memset (buf + at, 'z', 3);
    }

    g_free (buf);
}
END_TEST

/* The bytes of the match before a folded character split by a chunk end must not be dropped. */
START_TEST (test_match_ending_in_folded_character_across_chunks)
{
    static const struct
    {
        const char *needle;
        const char *text;
    } ds[] = {
        { "ak", "a\xe2\x84\xaa" },
        { "aaas", "aaa\xc5\xbf" },
        { "akb",
          "a\xe2\x84\xaa"
          "b" },
        { "ss", "\xc5\xbf\xc5\xbf" },
    };
    const size_t text_len = strlen (ds[_i].text);
    const gsize size = (gsize) CHUNK * 2;
    char *buf = filler (size);
    int delta;

    set_utf8_mode (TRUE);

    for (delta = -(int) text_len - 1; delta <= 1; delta++)
    {
        const gsize at = (gsize) (CHUNK + delta);
        gsize len = 0;

        memcpy (buf + at, ds[_i].text, text_len);
        open_file (buf, size, O_RDONLY);
        start_search (ds[_i].needle, MC_SEARCH_T_NORMAL, FALSE);

        ck_assert_msg (find_from (0, (off_t) size, &len), "'%s' not found at %zu", ds[_i].needle,
                       at);
        ck_assert_int_eq (test_view.search->normal_offset, (off_t) at);
        ck_assert_int_eq (len, text_len);

        memset (buf + at, 'z', text_len);
    }

    g_free (buf);
}
END_TEST

/* A plain match in a chunk before the one with the folded character is found by bytes. */
START_TEST (test_plain_match_before_folded_character)
{
    const gsize size = (gsize) CHUNK * 3;
    char *buf = filler (size);
    gsize len = 0;

    set_utf8_mode (TRUE);
    buf[10] = 'K';
    memcpy (buf + CHUNK * 2, "\xe2\x84\xaa", 3);
    open_file (buf, size, O_RDONLY);
    start_search ("k", MC_SEARCH_T_NORMAL, FALSE);

    ck_assert (find_from (0, (off_t) size, &len));
    ck_assert_int_eq (test_view.search->normal_offset, 10);
    ck_assert_int_eq (len, 1);

    g_free (buf);
}
END_TEST

START_TEST (test_repeated_prefix_across_chunks)
{
    const gsize size = (gsize) CHUNK + 2;
    char *buf = g_malloc (size);
    gsize len = 0;

    memset (buf, 'a', size);
    buf[size - 1] = 'b';
    open_file (buf, size, O_RDONLY);
    start_search ("aaab", MC_SEARCH_T_NORMAL, TRUE);

    ck_assert (find_from (0, (off_t) size, &len));
    ck_assert_int_eq (test_view.search->normal_offset, (off_t) size - 4);

    g_free (buf);
}
END_TEST

START_TEST (test_range_is_respected)
{
    gsize len = 0;

    open_file ("xx needle needle", 16, O_RDONLY);
    start_search ("needle", MC_SEARCH_T_NORMAL, TRUE);

    ck_assert (find_from (0, 16, &len));
    ck_assert_int_eq (test_view.search->normal_offset, 3);

    ck_assert (find_from (4, 16, &len));
    ck_assert_int_eq (test_view.search->normal_offset, 10);

    // the byte at the end of the range is in it
    ck_assert (find_from (0, 8, &len));
    ck_assert_int_eq (test_view.search->normal_offset, 3);
    ck_assert (!find_from (0, 7, &len));
    ck_assert_int_eq (test_view.search->error, MC_SEARCH_E_NOTFOUND);

    ck_assert (!find_from (16, 16, &len));
    ck_assert_int_eq (test_view.search->error, MC_SEARCH_E_NOTFOUND);
}
END_TEST

START_TEST (test_match_at_the_ends_of_the_file)
{
    gsize len = 0;

    open_file ("needle in the middle needle", 27, O_RDONLY);
    start_search ("needle", MC_SEARCH_T_NORMAL, TRUE);

    ck_assert (find_from (0, 27, &len));
    ck_assert_int_eq (test_view.search->normal_offset, 0);
    ck_assert_int_eq (test_view.search->error, MC_SEARCH_E_OK);

    ck_assert (find_from (1, 27, &len));
    ck_assert_int_eq (test_view.search->normal_offset, 21);
}
END_TEST

START_TEST (test_not_found_sets_the_error)
{
    gsize len = 0;

    open_file ("nothing here", 12, O_RDONLY);
    start_search ("needle", MC_SEARCH_T_NORMAL, TRUE);

    ck_assert (!find_from (0, 12, &len));
    ck_assert_int_eq (test_view.search->error, MC_SEARCH_E_NOTFOUND);
    ck_assert (test_view.search->error_str != NULL);
}
END_TEST

START_TEST (test_progress_is_reported_per_chunk)
{
    const gsize size = (gsize) CHUNK * 3;
    char *buf = filler (size);
    gsize len = 0;

    open_file (buf, size, O_RDONLY);
    start_search ("needle", MC_SEARCH_T_NORMAL, TRUE);

    ck_assert (!find_from (0, (off_t) size, &len));
    ck_assert_int_eq (update_calls, 3);
    ck_assert_int_eq (last_update_pos, (off_t) size);

    g_free (buf);
}
END_TEST

START_TEST (test_abort_stops_the_search)
{
    const gsize size = (gsize) CHUNK * 3;
    char *buf = filler (size);
    gsize len = 0;

    memcpy (buf + size - 6, "needle", 6);
    open_file (buf, size, O_RDONLY);
    start_search ("needle", MC_SEARCH_T_NORMAL, TRUE);
    abort_search = TRUE;

    ck_assert (!find_from (0, (off_t) size, &len));
    ck_assert_int_eq (test_view.search->error, MC_SEARCH_E_ABORT);
    ck_assert_int_eq (update_calls, 1);

    g_free (buf);
}
END_TEST

START_TEST (test_read_error_is_not_a_miss)
{
    const gsize size = (gsize) CHUNK;
    char *buf = filler (size);
    gsize len = 0;

    open_file (buf, size, O_WRONLY);
    start_search ("needle", MC_SEARCH_T_NORMAL, TRUE);

    ck_assert (!find_from (0, (off_t) size, &len));
    ck_assert_int_eq (test_view.search->error, MC_SEARCH_E_INPUT);
    ck_assert (test_view.search->error_str != NULL);

    g_free (buf);
}
END_TEST

START_TEST (test_read_error_in_a_short_range_is_not_a_miss)
{
    gsize len = 0;

    open_file ("nothing here", 12, O_WRONLY);
    start_search ("needle", MC_SEARCH_T_NORMAL, TRUE);

    ck_assert (!find_from (0, 12, &len));
    ck_assert_int_eq (test_view.search->error, MC_SEARCH_E_INPUT);
    ck_assert (test_view.search->error_str != NULL);
}
END_TEST

/* A file that got shorter is the end of the data, not an error. */
START_TEST (test_truncated_file_is_not_an_error)
{
    gsize len = 0;

    open_file ("nothing here", 12, O_RDONLY);
    ck_assert_int_eq (truncate (test_path, 0), 0);
    start_search ("needle", MC_SEARCH_T_NORMAL, TRUE);

    ck_assert (!find_from (0, 12, &len));
    ck_assert_int_eq (test_view.search->error, MC_SEARCH_E_NOTFOUND);
}
END_TEST

/* The viewer sticks to the size it saw when it opened the file. */
START_TEST (test_bytes_added_to_the_file_are_not_seen)
{
    const gsize size = 8192;
    char *buf = filler (size);
    gsize len = 0;
    FILE *f;

    open_file (buf, size, O_RDONLY);
    f = fopen (test_path, "a");
    ck_assert (f != NULL);
    fputs ("Q", f);
    fclose (f);

    start_search ("Q", MC_SEARCH_T_NORMAL, TRUE);
    ck_assert (!find_from (0, (off_t) size, &len));

    g_free (buf);
}
END_TEST

/* The search moves the descriptor under the block the viewer has loaded. */
START_TEST (test_viewer_reads_the_right_bytes_after)
{
    const gsize size = (gsize) CHUNK * 2;
    char *buf = filler (size);
    gsize len = 0;
    int c = 0;

    buf[0] = 'A';
    buf[size / 2] = 'B';
    memcpy (buf + size - 6, "needle", 6);
    open_file (buf, size, O_RDONLY);

    ck_assert (mcview_get_byte (&test_view, 0, &c));
    ck_assert_int_eq (c, 'A');

    start_search ("needle", MC_SEARCH_T_NORMAL, TRUE);
    ck_assert (find_from (0, (off_t) size, &len));

    ck_assert (mcview_get_byte (&test_view, 0, &c));
    ck_assert_int_eq (c, 'A');
    ck_assert (mcview_get_byte (&test_view, (off_t) size / 2, &c));
    ck_assert_int_eq (c, 'B');

    g_free (buf);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc;

    tc = tcase_create ("fast_search");
    tcase_set_timeout (tc, 60);
    tcase_add_checked_fixture (tc, setup, teardown);

    tcase_add_loop_test (tc, test_same_as_generic, 0, 2 * G_N_ELEMENTS (same_as_generic_ds));
    tcase_add_loop_test (tc, test_backward_same_as_generic, 0,
                         2 * G_N_ELEMENTS (same_as_generic_ds));
    tcase_add_loop_test (tc, test_same_as_generic_on_random_data, 0, 2);
    tcase_add_test (tc, test_same_as_generic_on_two_letters);
    tcase_add_test (tc, test_filter_does_not_reach_a_hidden_line);
    tcase_add_test (tc, test_locale_case_rules);
    tcase_add_test (tc, test_repeated_needle_in_repeated_data);
    tcase_add_test (tc, test_needle_is_refused);
    tcase_add_test (tc, test_needle_bytes);
    tcase_add_test (tc, test_hex_finds_across_lines_and_nul);
    tcase_add_test (tc, test_generic_search_is_kept_for);
    tcase_add_loop_test (tc, test_chunk_boundaries, 0, 3);
    tcase_add_test (tc, test_folded_character_across_chunks);
    tcase_add_loop_test (tc, test_match_ending_in_folded_character_across_chunks, 0, 4);
    tcase_add_test (tc, test_plain_match_before_folded_character);
    tcase_add_test (tc, test_repeated_prefix_across_chunks);
    tcase_add_test (tc, test_range_is_respected);
    tcase_add_test (tc, test_match_at_the_ends_of_the_file);
    tcase_add_test (tc, test_not_found_sets_the_error);
    tcase_add_test (tc, test_progress_is_reported_per_chunk);
    tcase_add_test (tc, test_abort_stops_the_search);
    tcase_add_test (tc, test_read_error_is_not_a_miss);
    tcase_add_test (tc, test_read_error_in_a_short_range_is_not_a_miss);
    tcase_add_test (tc, test_truncated_file_is_not_an_error);
    tcase_add_test (tc, test_bytes_added_to_the_file_are_not_seen);
    tcase_add_test (tc, test_viewer_reads_the_right_bytes_after);

    return mctest_run_all (tc);
}

/* --------------------------------------------------------------------------------------------- */
