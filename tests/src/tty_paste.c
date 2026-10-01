/*
   src - unit tests for a bracketed paste taken as one block

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

#define TEST_SUITE_NAME "/src/tty_paste"

#include "tests/mctest.h"

#include <fcntl.h>

/* Run the real reader on bytes given by the test. */
#define tty_lowlevel_getch test_tty_lowlevel_getch
#define tty_nodelay        test_tty_nodelay
#include "lib/tty/key.c"
#undef tty_lowlevel_getch
#undef tty_nodelay

static char *test_input = NULL;
static size_t test_input_len;
static size_t test_input_pos;

int
test_tty_lowlevel_getch (void)
{
    return test_input_pos < test_input_len ? (unsigned char) test_input[test_input_pos++] : -1;
}

void
test_tty_nodelay (gboolean set)
{
    (void) set;
}

static void
setup (void)
{
    input_fd = open ("/dev/null", O_RDONLY);
    ck_assert_int_ge (input_fd, 0);
    test_input = NULL;
    test_input_len = test_input_pos = 0;
    tty_paste_gap_usec = 100 * 1000;
}

static void
teardown (void)
{
    g_free (test_input);
    test_input = NULL;
    close (input_fd);
    input_fd = -1;
}

static void
feed (const char *bytes, size_t len)
{
    g_free (test_input);
    test_input = g_memdup2 (bytes, len);
    test_input_len = len;
    test_input_pos = 0;
}

/* Sanitized text of what the terminal sent, up to the end mark; NULL when it never ended. */
static char *
paste_of (const char *bytes)
{
    GString *text;

    feed (bytes, strlen (bytes));
    text = tty_paste_collect ();
    return text == NULL ? NULL : g_string_free (text, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static const struct paste_ds
{
    const char *sent;
    const char *text;
} paste_ds[] = {
    { "hello\033[201~", "hello" },
    { "\033[201~", "" },
    { "one\ntwo\033[201~", "one\ntwo" },
    { "one\r\ntwo\rthree\033[201~", "one\ntwo\nthree" },
    { "a\tb\033[201~", "a\tb" },
    // control bytes are not commands and not text
    { "a\001b\002c\177d\033[201~", "abcd" },
    // ESC never gets through: no key, no escape sequence, no early end of the paste
    { "x\033[Ay\033[201~", "x[Ay" },
    { "\xd0\xbc\xd0\xb8\xd1\x80\033[201~", "\xd0\xbc\xd0\xb8\xd1\x80" },
};

START_PARAMETRIZED_TEST (test_paste_collect, paste_ds)
{
    char *text = paste_of (data->sent);

    ck_assert_ptr_ne (text, NULL);
    ck_assert_str_eq (text, data->text);
    g_free (text);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* A paste that does not end is given up after the gap, and nothing of it is used. */
START_TEST (test_paste_unterminated)
{
    char *text = paste_of ("no end mark");

    ck_assert_ptr_eq (text, NULL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The bytes after the end mark are not taken; they are what the user typed next. */
START_TEST (test_paste_stops_at_end)
{
    char *text = paste_of ("abc\033[201~xyz");

    ck_assert_str_eq (text, "abc");
    ck_assert_int_eq (test_tty_lowlevel_getch (), 'x');
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A big paste is cut at the limit, and the rest is read and dropped up to the end mark. */
START_TEST (test_paste_size_limit)
{
    const size_t big = TTY_PASTE_MAX_BYTES + 1000;
    GString *in = g_string_new ("");
    GString *text;

    g_string_set_size (in, big);
    memset (in->str, 'a', big);
    g_string_append (in, "\033[201~tail");
    feed (in->str, in->len);
    g_string_free (in, TRUE);

    text = tty_paste_collect ();
    ck_assert_ptr_ne (text, NULL);
    ck_assert_uint_le (text->len, TTY_PASTE_MAX_BYTES);
    ck_assert_int_eq (test_tty_lowlevel_getch (), 't');
    g_string_free (text, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_paste_take_once) { ck_assert_ptr_eq (tty_paste_take (), NULL); }
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    mctest_add_parameterized_test (tc_core, test_paste_collect, paste_ds);
    tcase_add_test (tc_core, test_paste_unterminated);
    tcase_add_test (tc_core, test_paste_stops_at_end);
    tcase_add_test (tc_core, test_paste_size_limit);
    tcase_add_test (tc_core, test_paste_take_once);

    return mctest_run_all (tc_core);
}
