/*
   lib - tty function testing

   Copyright (C) 2011-2025
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

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"

#include <stdio.h>

#include "lib/strutil.h"
#include "lib/util.h"

#include "lib/tty/tty.h"

/* --------------------------------------------------------------------------------------------- */
/* @CapturedValue */
static int my_exit__status__captured;

/* @Mock */
void
my_exit (int status)
{
    my_exit__status__captured = status;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_check_term_unset)
{
    // given
    setenv ("TERM", "", 1);

    // when
    tty_check_xterm_compat (FALSE);

    // then
    ck_assert_int_eq (my_exit__status__captured, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_check_term_non_xterm)
{
    // given
    setenv ("TERM", "gnome-terminal", 1);

    // when
    const gboolean actual_result_force_false = tty_check_xterm_compat (FALSE);
    const gboolean actual_result_force_true = tty_check_xterm_compat (TRUE);

    // then
    ck_assert_int_eq (my_exit__status__captured, 0);
    ck_assert_int_eq (actual_result_force_false, 0);
    ck_assert_int_eq (actual_result_force_true, 1);
}
END_TEST
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_check_term_xterm_like)
{
    // given
    setenv ("TERM", "alacritty-terminal", 1);

    // when
    const gboolean actual_result_force_false = tty_check_xterm_compat (FALSE);
    const gboolean actual_result_force_true = tty_check_xterm_compat (TRUE);

    // then
    ck_assert_int_eq (my_exit__status__captured, 0);
    ck_assert_int_eq (actual_result_force_false, 1);
    ck_assert_int_eq (actual_result_force_true, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_osc52_sequence)
{
    // when
    char *actual = tty_osc52_sequence ("hello", 5);
    char *empty = tty_osc52_sequence ("", 0);
    char *none = tty_osc52_sequence (NULL, 0);
    char *nul_and_newline = tty_osc52_sequence ("a\0\n", 3);
    char *too_long_text = g_malloc0 (TTY_OSC52_MAX_TEXT + 1);
    char *at_limit = tty_osc52_sequence (too_long_text, TTY_OSC52_MAX_TEXT);
    char *over_limit = tty_osc52_sequence (too_long_text, TTY_OSC52_MAX_TEXT + 1);

    // then
    ck_assert_str_eq (actual, "\033]52;c;aGVsbG8=\a");
    ck_assert_ptr_null (empty);
    ck_assert_ptr_null (none);
    ck_assert_str_eq (nul_and_newline, "\033]52;c;YQAK\a");
    ck_assert_ptr_nonnull (at_limit);
    ck_assert_ptr_null (over_limit);

    g_free (actual);
    g_free (nul_and_newline);
    g_free (at_limit);
    g_free (too_long_text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_tty_check_term_unset);
    tcase_add_test (tc_core, test_tty_check_term_non_xterm);
    tcase_add_test (tc_core, test_tty_check_term_xterm_like);
    tcase_add_test (tc_core, test_tty_osc52_sequence);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
