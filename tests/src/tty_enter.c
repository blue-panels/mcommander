/*
   Regression tests for Enter decoding, including Alacritty keypad Enter.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   This file is part of M-Commander.

   M-Commander is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation, either version 3 of the License, or (at your
   option) any later version.

   M-Commander is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/src/tty_enter"

#include "tests/mctest.h"

#ifdef HAVE_PTY_H
#include <pty.h>
#endif
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif

/* Exercise the real decoder with deterministic input from either backend. */
#define tty_lowlevel_getch test_tty_lowlevel_getch
#define tty_nodelay        test_tty_nodelay
#include "lib/tty/key.c"
#undef tty_lowlevel_getch
#undef tty_nodelay

static int test_input[SEQ_BUFFER_LEN];
static size_t test_input_len;
static size_t test_input_pos;
static int master = -1;
static int slave = -1;

int
test_tty_lowlevel_getch (void)
{
    return test_input_pos < test_input_len ? test_input[test_input_pos++] : -1;
}

void
test_tty_nodelay (gboolean set)
{
    (void) set;
}

static void
setup (void)
{
    struct termios mode;

    setenv ("TERM", "xterm", 1);
    mc_global.tty.xterm_flag = TRUE;
    mc_global.tty.disable_x11 = TRUE;
    ck_assert_int_eq (openpty (&master, &slave, NULL, NULL, NULL), 0);
    ck_assert_int_eq (tcgetattr (slave, &mode), 0);
    mode.c_iflag &= ~ICRNL;
    ck_assert_int_eq (tcsetattr (slave, TCSANOW, &mode), 0);
    input_fd = slave;
    test_input_len = test_input_pos = 0;
    init_key ();
}

static void
teardown (void)
{
    done_key ();
    close (slave);
    close (master);
    slave = master = -1;
    input_fd = -1;
}

static int
decode_key (int key)
{
    test_input[0] = key;
    test_input_len = 1;
    test_input_pos = 0;
    return get_key_code (1);
}

static int
decode_sequence (const char *seq)
{
    size_t i;

    test_input_len = strlen (seq);
    ck_assert_uint_lt (test_input_len, G_N_ELEMENTS (test_input));
    test_input_pos = 0;
    for (i = 0; i < test_input_len; i++)
        test_input[i] = (unsigned char) seq[i];
    return get_key_code (1);
}

START_TEST (test_enter_without_modifiers)
{
    ck_assert_int_eq (decode_key ('\r'), '\n');
    ck_assert_int_eq (decode_key (KEY_ENTER), '\n');
}
END_TEST

START_TEST (test_legacy_ctrl_enter) { ck_assert_int_eq (decode_key ('\n'), KEY_M_CTRL | '\n'); }
END_TEST

START_TEST (test_lf_in_bracketed_paste)
{
    bracketed_pasting_in_progress = TRUE;
    ck_assert_int_eq (decode_key ('\n'), '\n');
    bracketed_pasting_in_progress = FALSE;
    ck_assert_int_eq (decode_key ('\n'), KEY_M_CTRL | '\n');
}
END_TEST

START_TEST (test_lf_with_icrnl)
{
    struct termios mode;

    ck_assert_int_eq (tcgetattr (slave, &mode), 0);
    mode.c_iflag |= ICRNL;
    ck_assert_int_eq (tcsetattr (slave, TCSANOW, &mode), 0);
    ck_assert_int_eq (decode_key ('\n'), '\n');
}
END_TEST

START_TEST (test_keypad_enter_sequence)
{
    char *seq;

    ck_assert_int_eq (decode_sequence ("\033OM"), '\n');
    seq = tty_key_lookup_sequence ('\n');
    mctest_assert_str_eq (seq, "\\eOM");
    g_free (seq);
}
END_TEST

START_TEST (test_keypad_enter_capture_name)
{
    int code;
    char *name;

    code = tty_match_seq_to_keycode ("\033OM", 3);
    name = tty_keycode_to_keyname (code);
    ck_assert_ptr_ne (name, NULL);
    ck_assert_int_eq (tty_keyname_to_keycode (name, NULL), '\n');
    ck_assert_int_eq (code, '\n');
    g_free (name);
}
END_TEST

START_TEST (test_saved_keypad_enter)
{
    int code = tty_normalize_keycode (tty_keyname_to_keycode ("kpenter", NULL));

    /* Both Learn keys and the config loader register this canonical code. */
    ck_assert (define_sequence (code, "\033OM", MCKEY_NOACTION));
    ck_assert_int_eq (decode_sequence ("\033OM"), '\n');
    ck_assert_int_eq (tty_match_seq_to_keycode ("\033OM", 3), '\n');
    /* A saved escape sequence must not change the treatment of a raw LF. */
    ck_assert_int_eq (decode_key ('\n'), KEY_M_CTRL | '\n');
}
END_TEST

START_TEST (test_enter_with_explicit_modifiers)
{
    ck_assert (define_sequence (KEY_M_CTRL | '\n', "\033[13;5u", MCKEY_NOACTION));
    ck_assert (define_sequence (KEY_M_ALT | '\n', "\033[13;3u", MCKEY_NOACTION));
    ck_assert (define_sequence (KEY_M_SHIFT | '\n', "\033[13;2u", MCKEY_NOACTION));
    ck_assert_int_eq (decode_sequence ("\033[13;5u"), KEY_M_CTRL | '\n');
    ck_assert_int_eq (decode_sequence ("\033[13;3u"), KEY_M_ALT | '\n');
    ck_assert_int_eq (decode_sequence ("\033[13;2u"), KEY_M_SHIFT | '\n');
}
END_TEST

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_enter_without_modifiers);
    tcase_add_test (tc_core, test_legacy_ctrl_enter);
    tcase_add_test (tc_core, test_lf_in_bracketed_paste);
    tcase_add_test (tc_core, test_lf_with_icrnl);
    tcase_add_test (tc_core, test_keypad_enter_sequence);
    tcase_add_test (tc_core, test_keypad_enter_capture_name);
    tcase_add_test (tc_core, test_saved_keypad_enter);
    tcase_add_test (tc_core, test_enter_with_explicit_modifiers);

    return mctest_run_all (tc_core);
}
