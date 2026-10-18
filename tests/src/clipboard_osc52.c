/*
   tests/src/clipboard_osc52.c -- unit tests for the OSC 52 copy to the terminal

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

#define TEST_SUITE_NAME "/src/clipboard_osc52"

#include "tests/mctest.h"

#include <string.h>
#include <unistd.h>

#include "src/clipboard.h"

#define OSC52_START "\033]52;c;"
#define OSC52_END   "\033\\"

static char *clip;

/* --------------------------------------------------------------------------------------------- */

static void
write_clip (const char *text, gssize len)
{
    ck_assert (g_file_set_contents (clip, text, len, NULL));
}

/* --------------------------------------------------------------------------------------------- */

static void
write_info (const char *text, gsize len, const char *codeset)
{
    char *sum;

    sum = g_compute_checksum_for_data (CLIP_DIGEST_TYPE, (const guchar *) text, len);
    clipboard_info_write (clip, sum, FALSE, codeset);
    g_free (sum);
}

/* --------------------------------------------------------------------------------------------- */

/* The text the sequence carries, decoded */
static char *
payload (const char *seq, gsize *len)
{
    char *b64, *text;

    ck_assert (g_str_has_prefix (seq, OSC52_START));
    ck_assert (g_str_has_suffix (seq, OSC52_END));
    b64 = g_strndup (seq + strlen (OSC52_START),
                     strlen (seq) - strlen (OSC52_START) - strlen (OSC52_END));
    text = (char *) g_base64_decode (b64, len);
    g_free (b64);
    return text;
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    int fd;

    clip = g_build_filename (g_get_tmp_dir (), "mc-test-osc52-XXXXXX", NULL);
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

START_TEST (test_sequence)
{
    char *seq;

    seq = clipboard_osc52_sequence ("Hi", 2);
    mctest_assert_str_eq (seq, OSC52_START "SGk=" OSC52_END);
    g_free (seq);

    seq = clipboard_osc52_sequence ("", 0);
    mctest_assert_str_eq (seq, OSC52_START OSC52_END);
    g_free (seq);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_too_long)
{
    char *text, *seq;

    // 75000 bytes are 100000 of base64: the most that is sent
    text = g_malloc0 (75001);

    seq = clipboard_osc52_sequence (text, 75000);
    mctest_assert_not_null (seq);
    ck_assert_uint_eq (strlen (seq), strlen (OSC52_START) + 100000 + strlen (OSC52_END));
    g_free (seq);

    mctest_assert_null (clipboard_osc52_sequence (text, 75001));

    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_file_as_is)
{
    static const char text[] = "line 1\nстрока 2\n";
    char *seq, *got;
    gsize len;

    write_clip (text, -1);
    seq = clipboard_osc52_from_file (clip);
    got = payload (seq, &len);
    ck_assert_uint_eq (len, strlen (text));
    ck_assert_mem_eq (got, text, len);
    g_free (got);
    g_free (seq);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_file_recoded_to_utf8)
{
    // "привет" in KOI8-R
    static const char koi8[] = "\xd0\xd2\xc9\xd7\xc5\xd4";
    char *seq, *got;
    gsize len;

    write_clip (koi8, sizeof (koi8) - 1);
    write_info (koi8, sizeof (koi8) - 1, "KOI8-R");
    seq = clipboard_osc52_from_file (clip);
    got = payload (seq, &len);
    ck_assert_uint_eq (len, strlen ("привет"));
    ck_assert_mem_eq (got, "привет", len);
    g_free (got);
    g_free (seq);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_file_stale_info_kept_as_is)
{
    static const char koi8[] = "\xd0\xd2\xc9\xd7\xc5\xd4";
    char *seq, *got;
    gsize len;

    // the info was written for other text: it says nothing about this one
    write_clip (koi8, sizeof (koi8) - 1);
    write_info ("other", 5, "KOI8-R");
    seq = clipboard_osc52_from_file (clip);
    got = payload (seq, &len);
    ck_assert_uint_eq (len, sizeof (koi8) - 1);
    ck_assert_mem_eq (got, koi8, len);
    g_free (got);
    g_free (seq);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_file_column_mark_dropped)
{
    char *seq, *got;
    gsize len;

    write_clip ("\1\1\1\1\nab\ncd\n", -1);
    seq = clipboard_osc52_from_file (clip);
    got = payload (seq, &len);
    ck_assert_uint_eq (len, 6);
    ck_assert_mem_eq (got, "ab\ncd\n", len);
    g_free (got);
    g_free (seq);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_file_missing)
{
    unlink (clip);
    mctest_assert_null (clipboard_osc52_from_file (clip));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_sequence);
    tcase_add_test (tc_core, test_too_long);
    tcase_add_test (tc_core, test_file_as_is);
    tcase_add_test (tc_core, test_file_recoded_to_utf8);
    tcase_add_test (tc_core, test_file_stale_info_kept_as_is);
    tcase_add_test (tc_core, test_file_column_mark_dropped);
    tcase_add_test (tc_core, test_file_missing);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
