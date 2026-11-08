/*
   src/viewer - unit tests for the fast path of the viewer search

   Copyright (C) 2026
   Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "/src/viewer/fast_search"

#include "tests/mctest.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/search.h"
#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "src/vfs/local/local.h"

#include "src/viewer/internal.h"

#include "src/viewer/search.c"

/*** fixtures ********************************************************************************/

#define CHUNK MCVIEW_FAST_SEARCH_CHUNK

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

static gboolean
find_from (off_t start, off_t end, gsize *len)
{
    return mcview_find (&test_ssm, start, end, len);
}

/* Every match, overlapping ones too, as "offset:length " */
static char *
all_matches (gboolean generic)
{
    const off_t size = mcview_get_filesize (&test_view);
    GString *out;
    off_t start = 0;

    out = g_string_new (NULL);

    while (start <= size)
    {
        gsize len = 0;
        gboolean ok;

        ok = generic ? mc_search_run (test_view.search, &test_ssm, start, size, &len)
                     : find_from (start, size, &len);
        if (!ok)
            break;

        g_string_append_printf (out, "%lld:%zu ", (long long) test_view.search->normal_offset, len);
        start = test_view.search->normal_offset + 1;
    }

    return g_string_free (out, FALSE);
}

static void
assert_same_as_generic (const char *content, gsize len, const char *pattern, mc_search_type_t type,
                        gboolean case_sens)
{
    gboolean caseless;
    GString *needle;
    char *fast, *generic;

    open_file (content, len, O_RDONLY);
    start_search (pattern, type, case_sens);

    needle = mcview_fast_search_needle (test_view.search, &caseless);
    ck_assert_msg (needle != NULL, "'%s' does not take the fast path", pattern);
    g_string_free (needle, TRUE);

    fast = all_matches (FALSE);
    generic = all_matches (TRUE);
    ck_assert_msg (strcmp (fast, generic) == 0, "'%s' %s: fast [%s], generic [%s]", pattern,
                   case_sens ? "case sensitive" : "caseless", fast, generic);
    g_free (fast);
    g_free (generic);
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
    { "a1B a1b A1B", "a1b", MC_SEARCH_T_NORMAL, FALSE },
    { "1aB 1Ab 1ab", "1AB", MC_SEARCH_T_NORMAL, FALSE },
    { "ko\xc5\xa1tunica KO\xc5\xa0TUNICA", "ko\xc5\xa1tunica", MC_SEARCH_T_NORMAL, TRUE },
    { "\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82", "\xd1\x80\xd0\xb8", MC_SEARCH_T_NORMAL, TRUE },
    { "the \xe2\x84\xaa sign, k and K", "k", MC_SEARCH_T_NORMAL, FALSE },
    { "long \xc5\xbf and s and S", "s", MC_SEARCH_T_NORMAL, FALSE },
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
    assert_same_as_generic (same_as_generic_ds[_i].content, strlen (same_as_generic_ds[_i].content),
                            same_as_generic_ds[_i].pattern, same_as_generic_ds[_i].type,
                            same_as_generic_ds[_i].case_sens);
}
END_TEST

START_TEST (test_same_as_generic_on_random_data)
{
    static const char *const tokens[] = { "a", "b", "A", "B", " ", "\t", "\r", "\xc3\xa9" };
    const guint ascii_tokens = G_N_ELEMENTS (tokens) - 1;
    GRand *rnd;
    int round;

    rnd = g_rand_new_with_seed (5205);

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
            g_string_append (
                pattern,
                tokens[g_rand_int_range (
                    rnd, 0, (gint32) (case_sens ? G_N_ELEMENTS (tokens) : ascii_tokens))]);

        assert_same_as_generic (content->str, content->len, pattern->str, MC_SEARCH_T_NORMAL,
                                case_sens);

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
        ck_assert_msg (mcview_fast_search_needle (test_view.search, &caseless) == NULL,
                       "'%s' must not take the fast path", ds[i].pattern);
    }
}
END_TEST

START_TEST (test_needle_bytes)
{
    gboolean caseless;
    GString *needle;

    start_search ("AbC", MC_SEARCH_T_NORMAL, FALSE);
    needle = mcview_fast_search_needle (test_view.search, &caseless);
    ck_assert_str_eq (needle->str, "abc");
    ck_assert (caseless);
    g_string_free (needle, TRUE);

    start_search ("AbC", MC_SEARCH_T_NORMAL, TRUE);
    needle = mcview_fast_search_needle (test_view.search, &caseless);
    ck_assert_str_eq (needle->str, "AbC");
    ck_assert (!caseless);
    g_string_free (needle, TRUE);

    start_search (" 41  0a 00 ff ", MC_SEARCH_T_HEX, FALSE);
    needle = mcview_fast_search_needle (test_view.search, &caseless);
    ck_assert_int_eq (needle->len, 4);
    ck_assert_mem_eq (needle->str, "\x41\x0a\x00\xff", 4);
    ck_assert (!caseless);
    g_string_free (needle, TRUE);
}
END_TEST

/* The generic search goes line by line and would not find these. */
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

/* A pattern only the fast path finds tells which path was taken. */
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

/* The bytes kept between chunks must let a partly matched prefix go on matching. */
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

    // the match has to end inside the range
    ck_assert (find_from (0, 9, &len));
    ck_assert_int_eq (test_view.search->normal_offset, 3);
    ck_assert (!find_from (0, 8, &len));
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
    gsize len = 0;

    open_file ("needle", 6, O_WRONLY);
    start_search ("needle", MC_SEARCH_T_NORMAL, TRUE);

    ck_assert (!find_from (0, 6, &len));
    ck_assert_int_eq (test_view.search->error, MC_SEARCH_E_INPUT);
    ck_assert (test_view.search->error_str != NULL);
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

    tcase_add_loop_test (tc, test_same_as_generic, 0, G_N_ELEMENTS (same_as_generic_ds));
    tcase_add_test (tc, test_same_as_generic_on_random_data);
    tcase_add_test (tc, test_needle_is_refused);
    tcase_add_test (tc, test_needle_bytes);
    tcase_add_test (tc, test_hex_finds_across_lines_and_nul);
    tcase_add_test (tc, test_generic_search_is_kept_for);
    tcase_add_loop_test (tc, test_chunk_boundaries, 0, 3);
    tcase_add_test (tc, test_repeated_prefix_across_chunks);
    tcase_add_test (tc, test_range_is_respected);
    tcase_add_test (tc, test_match_at_the_ends_of_the_file);
    tcase_add_test (tc, test_not_found_sets_the_error);
    tcase_add_test (tc, test_progress_is_reported_per_chunk);
    tcase_add_test (tc, test_abort_stops_the_search);
    tcase_add_test (tc, test_read_error_is_not_a_miss);
    tcase_add_test (tc, test_viewer_reads_the_right_bytes_after);

    return mctest_run_all (tc);
}

/* --------------------------------------------------------------------------------------------- */
