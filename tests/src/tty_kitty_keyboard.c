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

#define TEST_SUITE_NAME "/src/tty_kitty_keyboard"

#include "tests/mctest.h"

#include <fcntl.h>

/* Run the real decoder on bytes given by the test. */
#define tty_lowlevel_getch test_tty_lowlevel_getch
#define tty_nodelay        test_tty_nodelay
#include "lib/tty/key.c"
#undef tty_lowlevel_getch
#undef tty_nodelay

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
    kitty_keyboard_active = TRUE;
}

static void
teardown (void)
{
    kitty_keyboard_active = FALSE;
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

static const struct decode_ds
{
    const char *seq;
    int code;
} decode_ds[] = {
    { "\033[57414u", '\n' },                       // keypad Enter
    { "\033[13;5u", KEY_M_CTRL | '\n' },           // Ctrl-Enter
    { "\033[13;2u", KEY_M_SHIFT | '\n' },          // Shift-Enter
    { "\033[13;3u", KEY_M_ALT | '\n' },            // Alt-Enter
    { "\033[27u", ESC_CHAR },                      // Esc
    { "\033[9;2u", KEY_M_SHIFT | '\t' },           // Shift-Tab
    { "\033[127;3u", KEY_M_ALT | KEY_BACKSPACE },  // Alt-Backspace
    { "\033[97;5u", XCTRL ('a') },                 // Ctrl-A
    { "\033[97;6u", XCTRL ('a') },                 // Ctrl-Shift-A, as the keymap names it
    { "\033[109;5u", '\n' },                       // Ctrl-M stays Enter
    { "\033[105;5u", '\t' },                       // Ctrl-I stays Tab
    { "\033[32;5u", XCTRL (' ') },                 // Ctrl-Space
    { "\033[49;5u", KEY_M_CTRL | '1' },            // Ctrl-1, not Ctrl-Q
    { "\033[49:33;6u", KEY_M_CTRL | '1' },         // Ctrl-Shift-1 keeps the digit
    { "\033[97;3u", ALT ('a') },                   // Alt-A
    { "\033[97;4u", ALT ('A') },                   // Alt-Shift-A
    { "\033[49:33;4u", ALT ('!') },                // Alt-Shift-1 with the shifted key
    { "\033[1092::97;5u", XCTRL ('a') },           // Ctrl-ef with the base layout key
    { "\033[57417u", KEY_LEFT },                   // keypad Left
    { "\033[57403u", '4' },                        // keypad 4
    { "\033[57413u", '+' },                        // keypad +
    { "\033[57426;2u", KEY_M_SHIFT | KEY_DC },     // Shift-keypad Delete
    { "\033[P", KEY_F (1) },                       // F1
    { "\033[Q", KEY_F (2) },                       // F2
    { "\033[13~", KEY_F (3) },                     // F3, from the xterm table
    { "\033[S", KEY_F (4) },                       // F4
    { "\033[1;2P", KEY_F (11) },                   // Shift-F1
    { "\033[1;5S", KEY_M_CTRL | KEY_F (4) },       // Ctrl-F4
    { "\033[1;5A", KEY_M_CTRL | KEY_UP },          // Ctrl-Up, from the xterm table
    { "\033[1092;5u", KEY_KITTY_EVENT },           // Ctrl-ef without the base layout key
    { "\033[97;9u", KEY_KITTY_EVENT },             // Super-A
    { "\033[57358u", KEY_KITTY_EVENT },            // Caps Lock
    { "\033[57376u", KEY_KITTY_EVENT },            // F13
    { "\033[97;5:2u", XCTRL ('a') },               // Ctrl-A repeated
    { "\033[97;5:3u", KEY_KITTY_EVENT },           // Ctrl-A released
    { "\033[1;5:2A", KEY_M_CTRL | KEY_UP },        // Ctrl-Up repeated
    { "\033[1;1:3A", KEY_KITTY_EVENT },            // Up released
    { "\033[13;1:2~", KEY_F (3) },                 // F3 repeated
};

START_PARAMETRIZED_TEST (test_kitty_decode, decode_ds)
{
    ck_assert_int_eq (decode (data->seq), data->code);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_key_after_sequence)
{
    feed ("\033[97;5ux\033[57414u");
    ck_assert_int_eq (get_key_code (1), XCTRL ('a'));
    ck_assert_int_eq (get_key_code (1), 'x');
    ck_assert_int_eq (get_key_code (1), '\n');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_ctrl_digit_keyname)
{
    ck_assert_int_eq (tty_keyname_to_keycode ("ctrl-1", NULL), decode ("\033[49;5u"));
    ck_assert_int_eq (tty_keyname_to_keycode ("ctrl-shift-1", NULL), decode ("\033[49:33;6u"));
    ck_assert_int_eq (tty_keyname_to_keycode ("ctrl-0", NULL), decode ("\033[48;5u"));
    ck_assert_int_eq (tty_keyname_to_keycode ("ctrl-q", NULL), XCTRL ('q'));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static const struct learned_ds
{
    const char *seq;
    int code;
} learned_ds[] = {
    { "\033[49;5u", KEY_M_CTRL | '1' },  // Ctrl-1 of the kitty protocol
    { "\033[97;5u", XCTRL ('a') },       // Ctrl-A of the kitty protocol
    { "\001", XCTRL ('a') },             // Ctrl-A of a legacy terminal
    { "\033a", ALT ('a') },              // Alt-A
    { "a", 'a' },                        //
    { "\033[13~", KEY_F (3) },           // F3, from the xterm table
    { "\033[97;9u", 0 },                 // Super-A
    { "\033[1;5Az", 0 },                 // more than one key
    { "\033", ESC_CHAR },                // Esc
    { "\033O", ALT ('O') },              // Alt-Shift-O, a head of other sequences
    { "\177", KEY_BACKSPACE },           // Backspace
    { "\035", XCTRL (']') },             // Ctrl-]
};

START_PARAMETRIZED_TEST (test_kitty_learned_seq, learned_ds)
{
    ck_assert_int_eq (tty_decode_key_seq (data->seq, (int) strlen (data->seq)), data->code);
    ck_assert_uint_eq (test_input_pos, test_input_len);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_learned_after_cut_sequence)
{
    // Decoding a capture must neither use nor discard the live decoder's partial sequence
    feed ("\033[1;5");
    ck_assert_int_eq (get_key_code (1), -1);
    ck_assert_int_eq (tty_decode_key_seq ("\033[13~", 5), KEY_F (3));
    feed ("A");
    ck_assert_int_eq (get_key_code (1), KEY_M_CTRL | KEY_UP);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_learned_preserves_live_input)
{
    feed ("z");
    ck_assert_int_eq (tty_decode_key_seq ("\033[49;5u", 7), KEY_M_CTRL | '1');
    ck_assert_int_eq (get_key_code (1), 'z');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_ctrl_digit_name)
{
    char *name;

    name = tty_keycode_to_keyname (KEY_M_CTRL | '1');
    ck_assert_str_eq (name, "Ctrl-1");
    g_free (name);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_event_fields)
{
    tty_key_event_t ev;

    ck_assert_int_eq (decode ("\033[97:65;6:2;65u"), XCTRL ('a'));
    ck_assert (tty_key_event (&ev));
    ck_assert_int_eq (ev.final, 'u');
    ck_assert_uint_eq (ev.key, 97);
    ck_assert_uint_eq (ev.shifted, 65);
    ck_assert_uint_eq (ev.mods, TTY_KITTY_MOD_SHIFT | TTY_KITTY_MOD_CTRL);
    ck_assert_int_eq (ev.event, TTY_KITTY_REPEAT);
    ck_assert_int_eq (ev.text_len, 1);
    ck_assert_uint_eq (ev.text[0], 65);

    ck_assert_int_eq (decode ("\033[1;3:3D"), KEY_KITTY_EVENT);
    ck_assert (tty_key_event (&ev));
    ck_assert_int_eq (ev.final, 'D');
    ck_assert_uint_eq (ev.key, 1);
    ck_assert_uint_eq (ev.mods, TTY_KITTY_MOD_ALT);
    ck_assert_int_eq (ev.event, TTY_KITTY_RELEASE);

    ck_assert_int_eq (decode ("\033[57441;9u"), KEY_KITTY_EVENT);
    ck_assert (tty_key_event (&ev));
    ck_assert_uint_eq (ev.key, 57441);
    ck_assert_uint_eq (ev.mods, TTY_KITTY_MOD_SUPER);

    /* a key that did not come as a kitty event has none */
    ck_assert_int_eq (decode ("x"), 'x');
    ck_assert (!tty_key_event (&ev));
    ck_assert_int_eq (decode ("\033[13~"), KEY_F (3));
    ck_assert (!tty_key_event (&ev));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_inactive)
{
    kitty_keyboard_active = FALSE;
    ck_assert_int_ne (decode ("\033[57414u"), '\n');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    mctest_add_parameterized_test (tc_core, test_kitty_decode, decode_ds);
    tcase_add_test (tc_core, test_kitty_key_after_sequence);
    tcase_add_test (tc_core, test_kitty_ctrl_digit_keyname);
    mctest_add_parameterized_test (tc_core, test_kitty_learned_seq, learned_ds);
    tcase_add_test (tc_core, test_kitty_learned_after_cut_sequence);
    tcase_add_test (tc_core, test_kitty_learned_preserves_live_input);
    tcase_add_test (tc_core, test_kitty_ctrl_digit_name);
    tcase_add_test (tc_core, test_kitty_event_fields);
    tcase_add_test (tc_core, test_kitty_inactive);

    return mctest_run_all (tc_core);
}
