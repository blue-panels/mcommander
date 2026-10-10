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
#define tty_lowlevel_getch         test_tty_lowlevel_getch
#define tty_lowlevel_input_pending test_tty_lowlevel_input_pending
#define tty_nodelay                test_tty_nodelay
#include "lib/tty/key.c"
#undef tty_lowlevel_getch
#undef tty_lowlevel_input_pending
#undef tty_nodelay

static int test_input[KITTY_CSI_MAX + 256];
static size_t test_input_len;
static size_t test_input_pos;

int
test_tty_lowlevel_getch (void)
{
    return test_input_pos < test_input_len ? test_input[test_input_pos++] : -1;
}

gboolean
test_tty_lowlevel_input_pending (void)
{
    return test_input_pos < test_input_len;
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
    mc_global.utf8_display = TRUE;
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
    kitty_flags_wanted = 0;
    kitty_flags_sent = 0;
    kitty_mod_keys = 0;
    kitty_text_len = kitty_text_pos = 0;
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
    { "\033[97;9u", KEY_M_SUPER | 'a' },           // Super-A
    { "\033[97;13u", KEY_M_SUPER | XCTRL ('a') },  // Super-Ctrl-A
    { "\033[97;17u", KEY_KITTY_EVENT },            // Hyper-A
    { "\033[57358u", KEY_KITTY_EVENT },            // Caps Lock
    { "\033[57376u", KEY_KITTY_EVENT },            // F13
    { "\033[97;5:2u", XCTRL ('a') },               // Ctrl-A repeated
    { "\033[97;5:3u", -1 },                        // Ctrl-A released, no one asked for it
    { "\033[57442;5u", MCKEY_MODIFIERS },          // Left Ctrl pressed
    { "\033[57442;1:3u", MCKEY_MODIFIERS },        // Left Ctrl released
    { "\033[57447;2:2u", MCKEY_MODIFIERS },        // Right Shift repeated
    { "\033[1;5:2A", KEY_M_CTRL | KEY_UP },        // Ctrl-Up repeated
    { "\033[1;1:3A", -1 },                         // Up released
    { "\033[97;;97u", 'a' },                       // a, every key as an escape code
    { "\033[97:65;2;65u", 'A' },                   // Shift-A with its text
    { "\033[32;;32u", ' ' },                       // Space
    { "\033[97;5;1u", XCTRL ('a') },               // Ctrl-A: control text is not text
    { "\033[97;3;97u", ALT ('a') },                // Alt-A: the text does not count
    { "\033[57403;129;52u", '4' },                 // keypad 4 with Num Lock
    { "\033[13;1:2~", KEY_F (3) },                 // F3 repeated
    { "\033[1;129D", KEY_LEFT },                   // Left with Num Lock
    { "\033[6;129~", KEY_NPAGE },                  // PgDn with Num Lock
    { "\033[1;133A", KEY_M_CTRL | KEY_UP },        // Ctrl-Up with Num Lock
    { "\033[1;65P", KEY_F (1) },                   // F1 with Caps Lock
    { "\033[13;193~", KEY_F (3) },                 // F3 with Caps and Num Lock
    { "\033[1;129:3D", -1 },                       // Left released with Num Lock
    { "\033[97;65;65u", 'A' },                     // a with Caps Lock: its text
    { "\033[97;69u", XCTRL ('a') },                // Ctrl-A with Caps Lock
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
    { "\033[49;5u", KEY_M_CTRL | '1' },   // Ctrl-1 of the kitty protocol
    { "\033[97;5u", XCTRL ('a') },        // Ctrl-A of the kitty protocol
    { "\001", XCTRL ('a') },              // Ctrl-A of a legacy terminal
    { "\033a", ALT ('a') },               // Alt-A
    { "a", 'a' },                         //
    { "\033[13~", KEY_F (3) },            // F3, from the xterm table
    { "\033[97;9u", KEY_M_SUPER | 'a' },  // Super-A
    { "\033[97;17u", 0 },                 // Hyper-A
    { "\033[1;5Az", 0 },                  // more than one key
    { "\033", ESC_CHAR },                 // Esc
    { "\033O", ALT ('O') },               // Alt-Shift-O, a head of other sequences
    { "\177", KEY_BACKSPACE },            // Backspace
    { "\035", XCTRL (']') },              // Ctrl-]
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
    ck_assert (tty_key_event (XCTRL ('a'), &ev));
    ck_assert (!tty_key_event (KEY_LEFT, &ev));
    ck_assert_int_eq (ev.final, 'u');
    ck_assert_uint_eq (ev.key, 97);
    ck_assert_uint_eq (ev.shifted, 65);
    ck_assert_uint_eq (ev.mods, TTY_KITTY_MOD_SHIFT | TTY_KITTY_MOD_CTRL);
    ck_assert_int_eq (ev.event, TTY_KITTY_REPEAT);
    ck_assert_int_eq (ev.text_len, 1);
    ck_assert_uint_eq (ev.text[0], 65);

    kitty_flags_wanted = 2;
    ck_assert_int_eq (decode ("\033[1;3:3D"), KEY_KITTY_EVENT);
    ck_assert (tty_key_event (KEY_KITTY_EVENT, &ev));
    ck_assert_int_eq (ev.final, 'D');
    ck_assert_uint_eq (ev.key, 1);
    ck_assert_uint_eq (ev.mods, TTY_KITTY_MOD_ALT);
    ck_assert_int_eq (ev.event, TTY_KITTY_RELEASE);

    kitty_flags_wanted = 8;
    ck_assert_int_eq (decode ("\033[57441;9u"), KEY_KITTY_EVENT);
    ck_assert (tty_key_event (KEY_KITTY_EVENT, &ev));
    ck_assert_uint_eq (ev.key, 57441);
    ck_assert_uint_eq (ev.mods, TTY_KITTY_MOD_SUPER);

    /* a key that did not come as a kitty event has none */
    ck_assert_int_eq (decode ("x"), 'x');
    ck_assert (!tty_key_event ('x', &ev));
    ck_assert_int_eq (decode ("\033[13~"), KEY_F (3));
    ck_assert (!tty_key_event (KEY_F (3), &ev));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_long_text)
{
    tty_key_event_t ev;
    GString *seq;
    int i, c;

    /* 20 code points of text are all kept */
    seq = g_string_new ("\033[97;;");
    for (i = 0; i < 20; i++)
        g_string_append_printf (seq, i == 0 ? "%d" : ":%d", 0x430 + i);
    g_string_append_c (seq, 'u');
    ck_assert (tty_kitty_seq_event (seq->str, (int) seq->len, &ev));
    ck_assert_int_eq (ev.text_len, 20);
    ck_assert_uint_eq (ev.text[19], 0x430 + 19);

    /* a text longer than TTY_KITTY_TEXT_MAX is not kept cut */
    g_string_truncate (seq, seq->len - 1);
    for (i = 20; i < TTY_KITTY_TEXT_MAX + 5; i++)
        g_string_append_printf (seq, ":%d", 0x430 + i);
    g_string_append_c (seq, 'u');
    ck_assert (tty_kitty_seq_event (seq->str, (int) seq->len, &ev));
    ck_assert_int_eq (ev.text_len, 0);
    ck_assert_uint_eq (ev.key, 97);
    g_string_free (seq, TRUE);

    /* a sequence too long to keep is read to its end: the key after it is not its tail */
    seq = g_string_new ("\033[0;;");
    for (i = 0; seq->len < KITTY_CSI_MAX + 16; i++)
        g_string_append_printf (seq, i == 0 ? "%d" : ":%d", 0x430 + i % 32);
    g_string_append (seq, "ux");
    feed (seq->str);
    // the dropped sequence may give -1 first, or the next key at once
    for (i = 0; (c = get_key_code (1)) == -1 && i < 2; i++)
        ;
    ck_assert_int_eq (c, 'x');
    g_string_free (seq, TRUE);

    /* all of a long text comes as bytes: 30 Cyrillic letters, 60 bytes, then x */
    seq = g_string_new ("\033[97;;");
    for (i = 0; i < 30; i++)
        g_string_append_printf (seq, i == 0 ? "%d" : ":%d", 0x430 + i);
    g_string_append (seq, "ux");
    ck_assert_uint_lt (seq->len, G_N_ELEMENTS (test_input));
    feed (seq->str);
    for (i = 0; i < 60; i++)
        ck_assert_int_ge (get_key_code (1), 0x80);
    ck_assert_int_eq (get_key_code (1), 'x');
    g_string_free (seq, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_release_wanted)
{
    kitty_flags_wanted = 2;
    ck_assert_int_eq (decode ("\033[97;1:3u"), KEY_KITTY_EVENT);
    kitty_flags_wanted = 0;
    ck_assert_int_eq (decode ("\033[97;1:3u"), -1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_release_dropped_before_key)
{
    tty_key_event_t ev;

    feed ("\033[97;1:3uz");
    ck_assert_int_eq (get_key_code (0), 'z');
    ck_assert (!tty_key_event ('z', &ev));

    /* a key from the table after it has no kitty event either */
    feed ("\033[97;1:3u\033[1;5A");
    ck_assert_int_eq (get_key_code (0), KEY_M_CTRL | KEY_UP);
    ck_assert (!tty_key_event (KEY_M_CTRL | KEY_UP, &ev));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_text_bytes)
{
    tty_key_event_t ev;

    /* ef, a Cyrillic letter, with no base layout key: its UTF-8 bytes one by one */
    feed ("\033[1092;;1092ux");
    ck_assert_int_eq (get_key_code (1), 0xD1);
    ck_assert (tty_key_event (0xD1, &ev));
    ck_assert_uint_eq (ev.key, 1092);
    ck_assert_int_eq (get_key_code (1), 0x84);
    ck_assert (!tty_key_event (0x84, &ev));
    ck_assert_int_eq (get_key_code (1), 'x');

    /* the caller that takes the whole event gets no more bytes of it */
    feed ("\033[1092;;1092ux");
    ck_assert_int_eq (get_key_code (1), 0xD1);
    ck_assert (tty_key_event_take (0xD1, &ev));
    ck_assert_int_eq (get_key_code (1), 'x');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_flags_base)
{
    keybar_modifiers = FALSE;
    ck_assert_uint_eq (kitty_flags_base (), 5);
    keybar_modifiers = TRUE;
    ck_assert_uint_eq (kitty_flags_base (), 31);
    keybar_modifiers = FALSE;
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_held_modifiers)
{
    kitty_flags_sent = 31;

    ck_assert_int_eq (decode ("\033[57442;5u"), MCKEY_MODIFIERS);  // Left Ctrl down
    ck_assert_uint_eq (tty_kitty_modifiers (), TTY_KITTY_MOD_CTRL);
    ck_assert_int_eq (decode ("\033[57448;5u"), MCKEY_MODIFIERS);    // Right Ctrl down
    ck_assert_int_eq (decode ("\033[57442;5:3u"), MCKEY_MODIFIERS);  // Left Ctrl up
    ck_assert_uint_eq (tty_kitty_modifiers (), TTY_KITTY_MOD_CTRL);
    ck_assert_int_eq (decode ("\033[57448;1:3u"), MCKEY_MODIFIERS);  // Right Ctrl up
    ck_assert_uint_eq (tty_kitty_modifiers (), 0);

    /* Shift and Alt together, then Shift up: the modifier field keeps Alt */
    decode ("\033[57441;2u");
    decode ("\033[57443;4u");
    ck_assert_uint_eq (tty_kitty_modifiers (), TTY_KITTY_MOD_SHIFT | TTY_KITTY_MOD_ALT);
    decode ("\033[57441;3:3u");
    ck_assert_uint_eq (tty_kitty_modifiers (), TTY_KITTY_MOD_ALT);

    /* a release that never came is fixed by the next key */
    ck_assert_int_eq (decode ("\033[97;;97u"), 'a');
    ck_assert_uint_eq (tty_kitty_modifiers (), 0);
    ck_assert_int_eq (decode ("\033[1;5A"), KEY_M_CTRL | KEY_UP);
    ck_assert_uint_eq (tty_kitty_modifiers (), 0);  // a legacy form is no kitty event
    ck_assert_int_eq (decode ("\033[97;5u"), XCTRL ('a'));
    ck_assert_uint_eq (tty_kitty_modifiers (), TTY_KITTY_MOD_CTRL);

    /* the focus goes away with Ctrl held */
    ck_assert_int_eq (decode ("\033[O"), MCKEY_MODIFIERS);
    ck_assert (!tty_key_event (MCKEY_MODIFIERS, NULL));
    ck_assert_uint_eq (tty_kitty_modifiers (), 0);
    ck_assert_int_eq (decode ("\033[I"), -1);

    /* without the flags 2 and 8 nothing is reported */
    decode ("\033[57442;5u");
    kitty_flags_sent = 5;
    ck_assert_uint_eq (tty_kitty_modifiers (), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_getch_skips_modifiers)
{
    /* Ctrl-Q in an input line takes the next key: the release of Ctrl is not that key */
    kitty_flags_sent = 31;
    feed ("\033[57442;1:3u\033[57441;2u\033[57441;1:3ux");
    ck_assert_int_eq (tty_getch (), 'x');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_learn_skips_leading_release)
{
    char *seq;
    char *after;

    /* the release of Enter that pressed the button, then Left with its release */

    feed ("\033[13;1:3u\033[1;129D\033[1;129:3D");
    seq = learn_key_ex (&after);
    ck_assert_str_eq (seq, "\\e[1;129D");
    ck_assert_str_eq (after, "\\e[1;129:3D");
    g_free (seq);
    g_free (after);

    /* a, its repeat and its release; a key with a byte after it stays whole */
    feed ("\033[97;;97u\033[97;1:2u\033[97;1:3u");
    seq = learn_key_ex (&after);
    ck_assert_str_eq (seq, "\\e[97;;97u");
    ck_assert_str_eq (after, "\\e[97;1:2u\\e[97;1:3u");
    g_free (seq);
    g_free (after);

    feed ("\033[13~x");
    seq = learn_key ();
    ck_assert_str_eq (seq, "\\e[13~x");
    g_free (seq);

    /* Ctrl-Shift-X: the modifiers before the X are skipped */
    feed ("\033[57442;5u\033[57441;6u\033[120;6u\033[120;6:3u");
    seq = learn_key_ex (&after);
    ck_assert_str_eq (seq, "\\e[120;6u");
    ck_assert_str_eq (after, "\\e[120;6:3u");
    g_free (seq);
    g_free (after);

    /* Ctrl and Shift alone: the first of them is the key */
    feed ("\033[57442;5u\033[57441;6u\033[57441;5:3u\033[57442;1:3u");
    seq = learn_key_ex (&after);
    ck_assert_str_eq (seq, "\\e[57442;5u");
    ck_assert_str_eq (after, "\\e[57441;6u\\e[57441;5:3u\\e[57442;1:3u");
    g_free (seq);
    g_free (after);

    /* a legacy terminal: no split */
    kitty_keyboard_active = FALSE;
    feed ("\033[D\033[D");
    seq = learn_key ();
    ck_assert_str_eq (seq, "\\e[D\\e[D");
    g_free (seq);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_super_key_names)
{
    char *name;

    ck_assert_int_eq (tty_keyname_to_keycode ("super-a", NULL), KEY_M_SUPER | 'a');
    ck_assert_int_eq (tty_keyname_to_keycode ("super-ctrl-a", NULL), KEY_M_SUPER | XCTRL ('a'));
    ck_assert_int_eq (tty_keyname_to_keycode ("super-f5", NULL), KEY_M_SUPER | KEY_F (5));

    name = tty_keycode_to_keyname (KEY_M_SUPER | XCTRL ('a'));
    ck_assert_int_eq (tty_keyname_to_keycode (name, NULL), KEY_M_SUPER | XCTRL ('a'));
    g_free (name);
    name = tty_keycode_to_keyname (KEY_M_SUPER | KEY_F (5));
    ck_assert_int_eq (tty_keyname_to_keycode (name, NULL), KEY_M_SUPER | KEY_F (5));
    g_free (name);
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
    tcase_add_test (tc_core, test_kitty_long_text);
    tcase_add_test (tc_core, test_kitty_release_wanted);
    tcase_add_test (tc_core, test_kitty_release_dropped_before_key);
    tcase_add_test (tc_core, test_kitty_text_bytes);
    tcase_add_test (tc_core, test_kitty_flags_base);
    tcase_add_test (tc_core, test_kitty_held_modifiers);
    tcase_add_test (tc_core, test_getch_skips_modifiers);
    tcase_add_test (tc_core, test_learn_skips_leading_release);
    tcase_add_test (tc_core, test_super_key_names);
    tcase_add_test (tc_core, test_kitty_inactive);

    return mctest_run_all (tc_core);
}
