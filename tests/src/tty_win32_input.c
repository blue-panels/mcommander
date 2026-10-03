/*
   src - unit tests for the kitty keyboard protocol decoder

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

#define TEST_SUITE_NAME "/src/tty_win32_input"

#include "tests/mctest.h"

#include <fcntl.h>

/* Run the real decoder on bytes given by the test. */
#define tty_lowlevel_getch test_tty_lowlevel_getch
#define tty_nodelay        test_tty_nodelay
#define tty_unget_input    test_tty_unget_input
#include "lib/tty/key.c"
#undef tty_lowlevel_getch
#undef tty_nodelay
#undef tty_unget_input

static int test_input[64];
static size_t test_input_len;
static size_t test_input_pos;

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

/* The bytes go back in front of what is still to be read. */
void
test_tty_unget_input (const unsigned char *data, size_t len)
{
    size_t i;

    ck_assert_uint_lt (test_input_len + len, G_N_ELEMENTS (test_input));
    memmove (test_input + test_input_pos + len, test_input + test_input_pos,
             (test_input_len - test_input_pos) * sizeof (test_input[0]));
    for (i = 0; i < len; i++)
        test_input[test_input_pos + i] = data[i];
    test_input_len += len;
}

static void
setup (void)
{
    setenv ("TERM", "xterm", 1);
    mc_global.tty.xterm_flag = TRUE;
    mc_global.tty.disable_x11 = TRUE;
    mc_global.tty.alternate_plus_minus = FALSE;
    input_fd = open ("/dev/null", O_RDONLY);
    ck_assert_int_ge (input_fd, 0);
    test_input_len = test_input_pos = 0;
    init_key ();
    win32_input_active = TRUE;
}

static void
teardown (void)
{
    win32_input_active = FALSE;
    done_key ();
    close (input_fd);
    input_fd = -1;
}

static void
feed (const char *seq)
{
    size_t i;

    test_input_len = strlen (seq);
    ck_assert_uint_lt (test_input_len, G_N_ELEMENTS (test_input));
    test_input_pos = 0;
    for (i = 0; i < test_input_len; i++)
        test_input[i] = (unsigned char) seq[i];
}

static int
decode (const char *seq)
{
    feed (seq);
    return get_key_code (1);
}

/* --------------------------------------------------------------------------------------------- */

/* CSI Vk ; Sc ; Uc ; Kd ; Cs ; Rc _ */
static const struct decode_ds
{
    const char *seq;
    int code;
} decode_ds[] = {
    { "\033[65;30;97;1;0;1_", 'a' },                      // a
    { "\033[65;30;65;1;16;1_", 'A' },                     // Shift-A
    { "\033[49;2;33;1;16;1_", '!' },                      // Shift-1
    { "\033[65;30;1;1;8;1_", XCTRL ('a') },               // Ctrl-A, left Ctrl
    { "\033[65;30;1;1;4;1_", XCTRL ('a') },               // Ctrl-A, right Ctrl
    { "\033[65;30;1;1;24;1_", XCTRL ('a') },              // Ctrl-Shift-A, as the keymap names it
    { "\033[65;30;97;1;2;1_", ALT ('a') },                // Alt-A
    { "\033[65;30;65;1;18;1_", ALT ('A') },               // Alt-Shift-A
    { "\033[81;16;64;1;10;1_", '@' },                     // AltGr-Q is @ on some layouts
    { "\033[13;28;13;1;0;1_", '\n' },                     // Enter
    { "\033[13;28;13;1;8;1_", KEY_M_CTRL | '\n' },        // Ctrl-Enter
    { "\033[9;15;9;1;0;1_", '\t' },                       // Tab
    { "\033[9;15;9;1;16;1_", KEY_M_SHIFT | '\t' },        // Shift-Tab
    { "\033[27;1;27;1;0;1_", ESC_CHAR },                  // Esc
    { "\033[8;14;8;1;0;1_", KEY_BACKSPACE },              // Backspace
    { "\033[8;14;8;1;2;1_", KEY_M_ALT | KEY_BACKSPACE },  // Alt-Backspace
    { "\033[32;57;32;1;0;1_", ' ' },                      // Space
    { "\033[32;57;0;1;8;1_", XCTRL (' ') },               // Ctrl-Space
    { "\033[37;75;0;1;256;1_", KEY_LEFT },                // Left
    { "\033[39;77;0;1;264;1_", KEY_M_CTRL | KEY_RIGHT },  // Ctrl-Right
    { "\033[38;72;0;1;272;1_", KEY_M_SHIFT | KEY_UP },    // Shift-Up
    { "\033[40;80;0;1;256;1_", KEY_DOWN },                // Down
    { "\033[36;71;0;1;256;1_", KEY_HOME },                // Home
    { "\033[35;79;0;1;256;1_", KEY_END },                 // End
    { "\033[33;73;0;1;256;1_", KEY_PPAGE },               // Page Up
    { "\033[34;81;0;1;256;1_", KEY_NPAGE },               // Page Down
    { "\033[45;82;0;1;256;1_", KEY_IC },                  // Insert
    { "\033[46;83;0;1;256;1_", KEY_DC },                  // Delete
    { "\033[112;59;0;1;0;1_", KEY_F (1) },                // F1
    { "\033[121;68;0;1;0;1_", KEY_F (10) },               // F10
    { "\033[116;63;0;1;16;1_", KEY_F (15) },              // Shift-F5 is F15, as in the kitty tests
    { "\033[115;62;0;1;2;1_", KEY_M_ALT | KEY_F (4) },    // Alt-F4
    { "\033[97;30;97;1;0;", -1 },                         // cut short
    { "\033[65;30;97;0;0;1_", -1 },                       // a released
    { "\033[16;42;0;1;16;1_", -1 },                       // Shift on its own
    { "\033[17;29;0;1;8;1_", -1 },                        // Ctrl on its own
    { "\033[20;58;0;1;128;1_", -1 },                      // Caps Lock
    { "\033[91;91;0;1;0;1_", -1 },                        // Windows key
};

START_PARAMETRIZED_TEST (test_win32_decode, decode_ds)
{
    ck_assert_int_eq (decode (data->seq), data->code);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_key_after_sequence)
{
    feed ("\033[65;30;1;1;8;1_x\033[13;28;13;1;0;1_");
    ck_assert_int_eq (get_key_code (1), XCTRL ('a'));
    ck_assert_int_eq (get_key_code (1), 'x');
    ck_assert_int_eq (get_key_code (1), '\n');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_press_and_release)
{
    // a key press and its release: one key, the release is nothing
    feed ("\033[65;30;97;1;0;1_\033[65;30;97;0;0;1_\033[66;48;98;1;0;1_");
    ck_assert_int_eq (get_key_code (1), 'a');
    ck_assert_int_eq (get_key_code (1), -1);
    ck_assert_int_eq (get_key_code (1), 'b');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_other_script_goes_back_as_utf8)
{
    // U+0444 is D1 84 in UTF-8; the bytes come back to the keyboard and are read as such
    feed ("\033[65;30;1092;1;0;1_z");
    ck_assert_int_eq (get_key_code (1), 0xD1);
    ck_assert_int_eq (get_key_code (1), 0x84);
    ck_assert_int_eq (get_key_code (1), 'z');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_other_script_with_modifier_is_no_key)
{
    ck_assert_int_eq (decode ("\033[65;30;1092;1;8;1_"), -1);
    ck_assert_int_eq (decode ("\033[65;30;55357;1;0;1_"), -1);  // half of a surrogate pair
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_inactive)
{
    win32_input_active = FALSE;
    ck_assert_int_ne (decode ("\033[65;30;97;1;0;1_"), 'a');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    mctest_add_parameterized_test (tc_core, test_win32_decode, decode_ds);
    tcase_add_test (tc_core, test_win32_key_after_sequence);
    tcase_add_test (tc_core, test_win32_press_and_release);
    tcase_add_test (tc_core, test_win32_other_script_goes_back_as_utf8);
    tcase_add_test (tc_core, test_win32_other_script_with_modifier_is_no_key);
    tcase_add_test (tc_core, test_win32_inactive);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */