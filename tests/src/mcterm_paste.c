/*
   src - unit tests for what mcterm gives a program as a paste

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

#define TEST_SUITE_NAME "/src/mcterm_paste"

#include "tests/mctest.h"

#include "src/mcterm/mcterm.h"

/* --------------------------------------------------------------------------------------------- */

static void
assert_bytes (const char *text, gboolean bracketed, const char *expected)
{
    size_t n;
    char *bytes = mcterm_paste_bytes (text, strlen (text), bracketed, &n);

    ck_assert_uint_eq (n, strlen (expected));
    ck_assert_mem_eq (bytes, expected, n);
    g_free (bytes);
}

/* --------------------------------------------------------------------------------------------- */

/* A program that asked for bracketed paste gets the text whole, in the marks. */
START_TEST (test_bracketed_wraps_the_block)
{
    assert_bytes ("touch CCC\ntouch DDD", TRUE, "\033[200~touch CCC\ntouch DDD\033[201~");
    assert_bytes ("a\r\nb", TRUE, "\033[200~a\nb\033[201~");
    assert_bytes ("a\tb", TRUE, "\033[200~a\tb\033[201~");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The text cannot end the paste early, nor act as a key. */
START_TEST (test_bracketed_strips_esc)
{
    assert_bytes ("ab\033[201~touch EVIL\n", TRUE, "\033[200~ab[201~touch EVIL\n\033[201~");
    assert_bytes ("a\001b\177c", TRUE, "\033[200~abc\033[201~");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Without the marks no line break goes out: the lines are one line, and Enter is the user's. */
START_TEST (test_plain_joins_lines)
{
    assert_bytes ("touch CCC\ntouch DDD\n", FALSE, "touch CCC touch DDD");
    assert_bytes ("a\r\nb\rc", FALSE, "a b c");
    assert_bytes ("a\033[201~b", FALSE, "a [201~b");
    assert_bytes ("\xd0\xbc\xd0\xb8", FALSE, "\xd0\xbc\xd0\xb8");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_bracketed_wraps_the_block);
    tcase_add_test (tc_core, test_bracketed_strips_esc);
    tcase_add_test (tc_core, test_plain_joins_lines);

    return mctest_run_all (tc_core);
}
