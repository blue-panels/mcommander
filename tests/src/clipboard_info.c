/*
   tests/src/clipboard_info.c -- unit tests for the info file of the clipfile

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

#define TEST_SUITE_NAME "/src/clipboard_info"

#include "tests/mctest.h"

#include <unistd.h>

#include "lib/util.h"  // mc_realpath()

#include "src/clipboard.h"

static char *clip;
static const char digest_a[] = "0123456789abcdef0123456789abcdef";
static const char digest_b[] = "fedcba9876543210fedcba9876543210";

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    int fd;

    clip = g_build_filename (g_get_tmp_dir (), "mc-test-clip-XXXXXX", NULL);
    fd = g_mkstemp (clip);
    if (fd >= 0)
        close (fd);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    clipboard_info_drop (clip);
    unlink (clip);
    g_free (clip);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_info_roundtrip)
{
    char digest[CLIP_DIGEST_LEN + 1];
    char codeset[CLIP_CODESET_MAX + 1];
    gboolean vertical;

    clipboard_info_write (clip, digest_a, FALSE, "KOI8-R");
    mctest_assert_true (clipboard_info_read (clip, digest, &vertical, codeset));
    mctest_assert_str_eq (digest, digest_a);
    mctest_assert_false (vertical);
    mctest_assert_str_eq (codeset, "KOI8-R");

    clipboard_info_write (clip, digest_b, TRUE, "UTF-8");
    mctest_assert_true (clipboard_info_read (clip, digest, &vertical, codeset));
    mctest_assert_str_eq (digest, digest_b);
    mctest_assert_true (vertical);
    mctest_assert_str_eq (codeset, "UTF-8");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_info_no_codeset)
{
    char digest[CLIP_DIGEST_LEN + 1];
    char codeset[CLIP_CODESET_MAX + 1];
    gboolean vertical;

    clipboard_info_write (clip, digest_a, TRUE, NULL);
    mctest_assert_true (clipboard_info_read (clip, digest, &vertical, codeset));
    mctest_assert_true (vertical);
    mctest_assert_str_eq (codeset, "");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_info_missing_and_dropped)
{
    char digest[CLIP_DIGEST_LEN + 1];
    char codeset[CLIP_CODESET_MAX + 1];
    gboolean vertical = TRUE;

    mctest_assert_false (clipboard_info_read (clip, digest, &vertical, codeset));
    mctest_assert_false (vertical);
    mctest_assert_str_eq (codeset, "");

    clipboard_info_write (clip, digest_a, TRUE, "KOI8-R");
    clipboard_info_drop (clip);
    mctest_assert_false (clipboard_info_read (clip, digest, &vertical, codeset));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_info_garbage)
{
    char digest[CLIP_DIGEST_LEN + 1];
    char codeset[CLIP_CODESET_MAX + 1];
    gboolean vertical;
    char real[MC_MAXPATHLEN];
    char *path;

    // the info is next to the real file, and the temporary directory may be a symlink
    mctest_assert_not_null (mc_realpath (clip, real));
    path = g_strconcat (real, CLIP_INFO_SUFFIX, (char *) NULL);
    g_file_set_contents (path, "not an info line\n", -1, NULL);
    mctest_assert_false (clipboard_info_read (clip, digest, &vertical, codeset));

    g_file_set_contents (path, "", -1, NULL);
    mctest_assert_false (clipboard_info_read (clip, digest, &vertical, codeset));
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Two programs share the clipfile by a symlink: they must share its info too */
START_TEST (test_info_through_symlink)
{
    char digest[CLIP_DIGEST_LEN + 1];
    char codeset[CLIP_CODESET_MAX + 1];
    gboolean vertical;
    char *link;

    link = g_strconcat (clip, "-link", (char *) NULL);
    ck_assert_int_eq (symlink (clip, link), 0);

    clipboard_info_write (clip, digest_a, TRUE, "UTF-8");
    mctest_assert_true (clipboard_info_read (link, digest, &vertical, codeset));
    mctest_assert_str_eq (digest, digest_a);
    mctest_assert_true (vertical);

    clipboard_info_write (link, digest_b, FALSE, "KOI8-R");
    mctest_assert_true (clipboard_info_read (clip, digest, &vertical, codeset));
    mctest_assert_str_eq (digest, digest_b);
    mctest_assert_false (vertical);

    clipboard_info_drop (link);
    mctest_assert_false (clipboard_info_read (clip, digest, &vertical, codeset));

    unlink (link);
    g_free (link);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Write the clipfile and its info as the editor does */
static void
put_clip (const char *data, gboolean vertical, const char *codeset)
{
    char *sum;

    g_file_set_contents (clip, data, -1, NULL);
    sum = g_compute_checksum_for_string (CLIP_DIGEST_TYPE, data, -1);
    clipboard_info_write (clip, sum, vertical, codeset);
    g_free (sum);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_paste_column_matches)
{
    // the terminal sends CR line breaks and drops the blanks that pad the column
    static const char pasted[] = "ab\rgh\rmn";
    gboolean column = FALSE;

    put_clip ("ab  \ngh  \nmn  \n", TRUE, "UTF-8");
    mctest_assert_true (clipboard_paste_is_clip (clip, pasted, strlen (pasted), &column));
    mctest_assert_true (column);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_paste_text_block)
{
    static const char pasted[] = "ab\ngh";
    gboolean column = TRUE;

    put_clip ("ab\ngh", FALSE, "UTF-8");
    mctest_assert_true (clipboard_paste_is_clip (clip, pasted, strlen (pasted), &column));
    mctest_assert_false (column);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_paste_other_text)
{
    static const char pasted[] = "ab\ngx";
    gboolean column = TRUE;

    put_clip ("ab\ngh", TRUE, "UTF-8");
    mctest_assert_false (clipboard_paste_is_clip (clip, pasted, strlen (pasted), &column));
    mctest_assert_false (column);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_paste_stale_info)
{
    static const char pasted[] = "ab\ngh";
    gboolean column = TRUE;

    // the clipfile was replaced after the info was written
    clipboard_info_write (clip, digest_a, TRUE, "UTF-8");
    g_file_set_contents (clip, "ab\ngh", -1, NULL);
    mctest_assert_false (clipboard_paste_is_clip (clip, pasted, strlen (pasted), &column));
    mctest_assert_false (column);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_paste_recoded_clip)
{
    // "\xd0\xd2" is two Cyrillic letters in KOI8-R; the terminal pastes them in UTF-8
    static const char pasted[] = "\xd0\xbf\xd1\x80\nab";
    gboolean column = FALSE;

    put_clip ("\xd0\xd2\nab", TRUE, "KOI8-R");
    mctest_assert_true (clipboard_paste_is_clip (clip, pasted, strlen (pasted), &column));
    mctest_assert_true (column);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_info_roundtrip);
    tcase_add_test (tc_core, test_info_no_codeset);
    tcase_add_test (tc_core, test_info_missing_and_dropped);
    tcase_add_test (tc_core, test_info_garbage);
    tcase_add_test (tc_core, test_info_through_symlink);
    tcase_add_test (tc_core, test_paste_column_matches);
    tcase_add_test (tc_core, test_paste_text_block);
    tcase_add_test (tc_core, test_paste_other_text);
    tcase_add_test (tc_core, test_paste_stale_info);
    tcase_add_test (tc_core, test_paste_recoded_clip);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
