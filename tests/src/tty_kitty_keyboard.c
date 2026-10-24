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
#define tty_unget_input            test_tty_unget_input
#include "lib/tty/key.c"
#undef tty_lowlevel_getch
#undef tty_lowlevel_input_pending
#undef tty_nodelay
#undef tty_unget_input

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

void
test_tty_unget_input (const unsigned char *data, size_t len)
{
    size_t i;

    ck_assert_uint_ge (test_input_pos, len);
    test_input_pos -= len;
    for (i = 0; i < len; i++)
        test_input[test_input_pos + i] = data[i];
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
    win32_input_active = FALSE;
    win32_high_surrogate = 0;
    kitty_flags_wanted = 0;
    kitty_flags_sent = 0;
    kitty_mod_keys = 0;
    kitty_text_len = kitty_text_pos = 0;
    key_repeat_left = 0;
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
    { "\033[57414u", '\n' },                             // keypad Enter
    { "\033[13;5u", KEY_M_CTRL | '\n' },                 // Ctrl-Enter
    { "\033[13;2u", KEY_M_SHIFT | '\n' },                // Shift-Enter
    { "\033[13;3u", KEY_M_ALT | '\n' },                  // Alt-Enter
    { "\033[27u", ESC_CHAR },                            // Esc
    { "\033[9;2u", KEY_M_SHIFT | '\t' },                 // Shift-Tab
    { "\033[127;3u", KEY_M_ALT | KEY_BACKSPACE },        // Alt-Backspace
    { "\033[97;5u", XCTRL ('a') },                       // Ctrl-A
    { "\033[97;6u", XCTRL ('a') },                       // Ctrl-Shift-A, as the keymap names it
    { "\033[109;5u", '\n' },                             // Ctrl-M stays Enter
    { "\033[105;5u", '\t' },                             // Ctrl-I stays Tab
    { "\033[32;5u", XCTRL (' ') },                       // Ctrl-Space
    { "\033[49;5u", KEY_M_CTRL | '1' },                  // Ctrl-1, not Ctrl-Q
    { "\033[49:33;6u", KEY_M_CTRL | '1' },               // Ctrl-Shift-1 keeps the digit
    { "\033[97;3u", ALT ('a') },                         // Alt-A
    { "\033[97;4u", ALT ('A') },                         // Alt-Shift-A
    { "\033[49:33;4u", ALT ('!') },                      // Alt-Shift-1 with the shifted key
    { "\033[1092::97;5u", XCTRL ('a') },                 // Ctrl-ef with the base layout key
    { "\033[57417u", KEY_LEFT },                         // keypad Left
    { "\033[57403u", '4' },                              // keypad 4
    { "\033[57413u", '+' },                              // keypad +
    { "\033[57413;3u", ALT ('+') },                      // Alt-keypad +: Alt-plus, as before
    { "\033[57413;2u", KEY_M_SHIFT | KEY_KP_ADD },       // Shift-keypad +
    { "\033[57413;5u", KEY_M_CTRL | KEY_KP_ADD },        // Ctrl-keypad +
    { "\033[57412;2u", KEY_M_SHIFT | KEY_KP_SUBTRACT },  // Shift-keypad -
    { "\033[57426;2u", KEY_M_SHIFT | KEY_DC },           // Shift-keypad Delete
    { "\033[P", KEY_F (1) },                             // F1
    { "\033[Q", KEY_F (2) },                             // F2
    { "\033[13~", KEY_F (3) },                           // F3, from the xterm table
    { "\033[S", KEY_F (4) },                             // F4
    { "\033[1;2P", KEY_F (11) },                         // Shift-F1
    { "\033[1;5S", KEY_M_CTRL | KEY_F (4) },             // Ctrl-F4
    { "\033[1;5A", KEY_M_CTRL | KEY_UP },                // Ctrl-Up, from the xterm table
    { "\033[1092;5u", KEY_KITTY_EVENT },                 // Ctrl-ef without the base layout key
    { "\033[97;9u", KEY_M_SUPER | 'a' },                 // Super-A
    { "\033[97;13u", KEY_M_SUPER | XCTRL ('a') },        // Super-Ctrl-A
    { "\033[97;17u", KEY_KITTY_EVENT },                  // Hyper-A
    { "\033[57358u", KEY_KITTY_EVENT },                  // Caps Lock
    { "\033[57376u", KEY_KITTY_EVENT },                  // F13
    { "\033[97;5:2u", XCTRL ('a') },                     // Ctrl-A repeated
    { "\033[97;5:3u", -1 },                              // Ctrl-A released, no one asked for it
    { "\033[57442;5u", MCKEY_MODIFIERS },                // Left Ctrl pressed
    { "\033[57442;1:3u", MCKEY_MODIFIERS },              // Left Ctrl released
    { "\033[57447;2:2u", MCKEY_MODIFIERS },              // Right Shift repeated
    { "\033[1;5:2A", KEY_M_CTRL | KEY_UP },              // Ctrl-Up repeated
    { "\033[1;1:3A", -1 },                               // Up released
    { "\033[97;;97u", 'a' },                             // a, every key as an escape code
    { "\033[97:65;2;65u", 'A' },                         // Shift-A with its text
    { "\033[32;;32u", ' ' },                             // Space
    { "\033[97;5;1u", XCTRL ('a') },                     // Ctrl-A: control text is not text
    { "\033[97;3;97u", ALT ('a') },                      // Alt-A: the text does not count
    { "\033[57403;129;52u", '4' },                       // keypad 4 with Num Lock
    { "\033[13;1:2~", KEY_F (3) },                       // F3 repeated
    { "\033[1;129D", KEY_LEFT },                         // Left with Num Lock
    { "\033[6;129~", KEY_NPAGE },                        // PgDn with Num Lock
    { "\033[1;133A", KEY_M_CTRL | KEY_UP },              // Ctrl-Up with Num Lock
    { "\033[1;65P", KEY_F (1) },                         // F1 with Caps Lock
    { "\033[13;193~", KEY_F (3) },                       // F3 with Caps and Num Lock
    { "\033[1;129:3D", -1 },                             // Left released with Num Lock
    { "\033[97;65;65u", 'A' },                           // a with Caps Lock: its text
    { "\033[97;69u", XCTRL ('a') },                      // Ctrl-A with Caps Lock
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

static gboolean
seq_event (const char *seq, tty_key_event_t *ev)
{
    return tty_kitty_seq_event (seq, (int) strlen (seq), ev);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_event_text)
{
    tty_key_event_t ev;
    char text[16];

    /* the whole text of the event: a letter with a combining mark is three bytes of UTF-8 */
    ck_assert (seq_event (ESC_STR "[97;3;97:776u", &ev));
    ck_assert_uint_eq (tty_key_event_text (&ev, text, sizeof (text)), 3);
    ck_assert (memcmp (text, "a\xCC\x88", 3) == 0);

    /* a text that does not fit is not cut */
    ck_assert_uint_eq (tty_key_event_text (&ev, text, 2), 0);

    /* the first character of the text, not the key of the base layout: Alt with the key of ';'
       in a Russian layout types a letter */
    ck_assert (seq_event (ESC_STR "[1078::59;3;1078u", &ev));
    ck_assert_uint_eq (tty_key_event_char (&ev), 0x436);

    /* Alt with a Cyrillic letter and no key of the base layout: the letter of the key */
    ck_assert (seq_event (ESC_STR "[1092;3u", &ev));
    ck_assert_uint_eq (tty_key_event_text (&ev, text, sizeof (text)), 2);
    ck_assert (memcmp (text, "\xD1\x84", 2) == 0);

    /* with Shift, the shifted key */
    ck_assert (seq_event (ESC_STR "[97:65;4u", &ev));
    ck_assert_uint_eq (tty_key_event_text (&ev, text, sizeof (text)), 1);
    ck_assert_int_eq (text[0], 'A');

    /* with Shift and no alternate keys reported, the capital of the letter */
    ck_assert (seq_event (ESC_STR "[97;4u", &ev));
    ck_assert_uint_eq (tty_key_event_text (&ev, text, sizeof (text)), 1);
    ck_assert_int_eq (text[0], 'A');
    ck_assert (seq_event (ESC_STR "[1092;4u", &ev));
    ck_assert_uint_eq (tty_key_event_text (&ev, text, sizeof (text)), 2);
    ck_assert (memcmp (text, "\xD0\xA4", 2) == 0);
    /* a key that is not a letter types what the layout says, which the terminal does not tell */
    ck_assert (seq_event (ESC_STR "[49;4u", &ev));
    ck_assert_uint_eq (tty_key_event_text (&ev, text, sizeof (text)), 0);
    /* it does with the alternate keys */
    ck_assert (seq_event (ESC_STR "[49:33;4u", &ev));
    ck_assert_uint_eq (tty_key_event_text (&ev, text, sizeof (text)), 1);
    ck_assert_int_eq (text[0], '!');

    /* a functional key types nothing: the plus of the keypad, Enter, a release */
    ck_assert (seq_event (ESC_STR "[57413;3u", &ev));
    ck_assert_uint_eq (tty_key_event_text (&ev, text, sizeof (text)), 0);
    ck_assert_uint_eq (tty_key_event_char (&ev), 0);
    ck_assert (seq_event (ESC_STR "[13;3u", &ev));
    ck_assert_uint_eq (tty_key_event_text (&ev, text, sizeof (text)), 0);
    ck_assert (seq_event (ESC_STR "[97;3:3u", &ev));
    ck_assert_uint_eq (tty_key_event_text (&ev, text, sizeof (text)), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_event_long_text)
{
    tty_key_event_t ev;
    char text[TTY_KITTY_TEXT_BYTES];
    GString *seq;
    int i;

    /* a text longer than any small buffer comes whole, not as the letter of the key */
    seq = g_string_new (ESC_STR "[97;3;98");
    for (i = 1; i < 65; i++)
        g_string_append (seq, ":98");
    g_string_append_c (seq, 'u');
    ck_assert (seq_event (seq->str, &ev));
    g_string_free (seq, TRUE);
    ck_assert_uint_eq (tty_key_event_text (&ev, text, sizeof (text)), 65);
    for (i = 0; i < 65; i++)
        ck_assert_int_eq (text[i], 'b');
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

/* Win32 input mode: CSI Vk ; Sc ; Uc ; Kd ; Cs ; Rc _ */

static const struct win32_ds
{
    const char *seq;
    int code;
} win32_ds[] = {
    { "\033[65;30;97;1;0;1_", 'a' },                     // a
    { "\033[65;30;97;0;0;1_", -1 },                      // a released
    { "\033[65;30;65;1;16;1_", 'A' },                    // Shift-A
    { "\033[65;30;1;1;8;1_", XCTRL ('a') },              // Ctrl-A
    { "\033[65;30;1;1;24;1_", XCTRL ('a') },             // Ctrl-Shift-A
    { "\033[65;30;97;1;2;1_", ALT ('a') },               // Alt-A
    { "\033[65;30;1092;1;2;1_", ALT ('a') },             // Alt-ef: the key of the base layout
    { "\033[65;30;1;1;8;1_", XCTRL ('a') },              // Ctrl-ef: Uc is the control code
    { "\033[81;16;64;1;9;1_", '@' },                     // AltGr-Q of a German layout
    { "\033[49;2;33;1;16;1_", '!' },                     // Shift-1
    { "\033[49;2;49;1;8;1_", KEY_M_CTRL | '1' },         // Ctrl-1
    { "\033[37;75;0;1;256;1_", KEY_LEFT },               // Left
    { "\033[37;75;0;1;288;1_", KEY_LEFT },               // Left with Num Lock
    { "\033[37;75;0;1;264;1_", KEY_M_CTRL | KEY_LEFT },  // Ctrl-Left
    { "\033[34;81;0;1;256;1_", KEY_NPAGE },              // PgDn
    { "\033[46;83;0;1;256;1_", KEY_DC },                 // Delete
    { "\033[112;59;0;1;0;1_", KEY_F (1) },               // F1
    { "\033[114;61;0;1;0;1_", KEY_F (3) },               // F3
    { "\033[116;63;0;1;16;1_", KEY_F (15) },             // Shift-F5
    { "\033[116;63;0;1;8;1_", KEY_M_CTRL | KEY_F (5) },  // Ctrl-F5
    { "\033[13;28;13;1;0;1_", '\n' },                    // Enter
    { "\033[13;28;13;1;256;1_", '\n' },                  // keypad Enter
    { "\033[27;1;27;1;0;1_", ESC_CHAR },                 // Esc
    { "\033[8;14;8;1;0;1_", KEY_BACKSPACE },             // Backspace
    { "\033[9;15;9;1;16;1_", KEY_M_SHIFT | '\t' },       // Shift-Tab
    { "\033[32;57;32;1;0;1_", ' ' },                     // Space
    { "\033[100;75;52;1;32;1_", '4' },                   // keypad 4 with Num Lock
    { "\033[188;51;44;1;0;1_", ',' },                    // comma
    { "\033[17;29;0;1;8;1_", MCKEY_MODIFIERS },          // Ctrl alone
    { "\033[17;29;0;0;0;1_", MCKEY_MODIFIERS },          // Ctrl released
    { "\033[124;100;0;1;0;1_", KEY_KITTY_EVENT },        // F13: no mc key
    { "\033[19;69;0;1;0;1_", KEY_KITTY_EVENT },          // Pause: no mc key, but a key
    { "\033[44;55;0;1;256;1_", KEY_KITTY_EVENT },        // Print Screen
    { "\033[93;93;0;1;256;1_", KEY_KITTY_EVENT },        // Menu
    { "\033[49;2;33;1;18;1_", ALT ('!') },               // Alt-Shift-1
    { "\033[190;52;1102;1;2;1_", ALT ('.') },            // Alt-period, Russian layout
    { "\033[188;51;1073;1;2;1_", ALT (',') },            // Alt-comma, Russian layout
    { "\033[190;52;46;1;2;1_", ALT ('.') },              // Alt-period, Latin layout
    { "\033[190;52;0;1;8;1_", XCTRL ('.') },             // Ctrl-period: no character
    { "\033[18;56;169;0;0;1_", 0xC2 },                   // Alt+0169: the character on Alt up
};

START_PARAMETRIZED_TEST (test_win32_decode, win32_ds)
{
    kitty_keyboard_active = FALSE;
    win32_input_active = TRUE;
    ck_assert_int_eq (decode (data->seq), data->code);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* A key of the Win32 input mode, pressed and released, as the Windows console sends a paste */

static void
win32_type (GString *seq, unsigned int vk, unsigned int uc)
{
    g_string_append_printf (seq, "\033[%u;0;%u;1;0;1_\033[%u;0;%u;0;0;1_", vk, uc, vk, uc);
}

static void
win32_type_text (GString *seq, const char *text)
{
    for (; *text != '\0'; text++)
        win32_type (seq, 0, (unsigned char) *text);
}

/* --------------------------------------------------------------------------------------------- */

static void
check_paste (const char *seq, const char *text)
{
    GString *paste;

    ck_assert_int_eq (decode (seq), MCKEY_BRACKETED_PASTING_START);
    ck_assert_int_eq (tty_paste_next (), MCKEY_PASTE);
    paste = tty_paste_take ();
    ck_assert_ptr_nonnull (paste);
    ck_assert_str_eq (paste->str, text);
    g_string_free (paste, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_paste_typed_as_keys)
{
    GString *seq = g_string_new (NULL);

    /* the Windows console: the marks and the text all as keys, Enter as a key with CR; the
       characters of the marks with no key */
    kitty_keyboard_active = FALSE;
    win32_input_active = TRUE;
    win32_type (seq, 0, 27);
    win32_type_text (seq, "[200~ab");
    win32_type (seq, 13, 13);
    win32_type_text (seq, "c");
    g_string_append (seq, "\033[0;0;55357;1;0;1_\033[0;0;56832;1;0;1_");  // U+1F600
    win32_type (seq, 0, 27);
    win32_type_text (seq, "[201~");
    check_paste (seq->str, "ab\nc\xf0\x9f\x98\x80");
    g_string_free (seq, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_paste_from_windows_console)
{
    // a'[$> Enter, two spaces and cd, pasted with Shift held, as conhost of Windows 11 sent it
    static const char seq[] =
        "\033[0;0;27;1;0;1_\033[0;0;27;0;0;1_\033[0;0;91;1;0;1_\033[0;0;91;0;0;1_"
        "\033[0;0;50;1;0;1_\033[0;0;50;0;0;1_\033[0;0;48;1;0;1_\033[0;0;48;0;0;1_"
        "\033[0;0;48;1;0;1_\033[0;0;48;0;0;1_\033[0;0;126;1;0;1_\033[0;0;126;0;0;1_"
        "\033[65;30;97;1;0;1_\033[65;30;97;0;0;1_\033[222;40;39;1;0;1_\033[222;40;39;0;0;1_"
        "\033[219;26;91;1;0;1_\033[219;26;91;0;0;1_\033[16;42;0;1;16;1_\033[52;5;36;1;16;1_"
        "\033[52;5;36;0;16;1_\033[16;42;0;0;0;1_\033[16;42;0;1;16;1_\033[190;52;62;1;16;1_"
        "\033[190;52;62;0;16;1_\033[16;42;0;0;0;1_\033[13;28;13;1;0;1_\033[13;28;13;0;0;1_"
        "\033[32;57;32;1;0;1_\033[32;57;32;0;0;1_\033[32;57;32;1;0;1_\033[32;57;32;0;0;1_"
        "\033[67;46;99;1;0;1_\033[67;46;99;0;0;1_\033[68;32;100;1;0;1_\033[68;32;100;0;0;1_"
        "\033[0;0;27;1;0;1_\033[0;0;27;0;0;1_\033[0;0;91;1;0;1_\033[0;0;91;0;0;1_"
        "\033[0;0;50;1;0;1_\033[0;0;50;0;0;1_\033[0;0;48;1;0;1_\033[0;0;48;0;0;1_"
        "\033[0;0;49;1;0;1_\033[0;0;49;0;0;1_\033[0;0;126;1;0;1_\033[0;0;126;0;0;1_";

    kitty_keyboard_active = FALSE;
    win32_input_active = TRUE;
    check_paste (seq, "a'[$>\n  cd");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_paste_of_a_frame)
{
    GString *seq = g_string_new (NULL);
    static const char alt_code[] = "\033[18;56;0;1;2;1_\033[18;56;%u;0;0;1_";

    /* the Windows console types a line of a frame as Alt and the digits of its code: the
       character comes with the release of Alt */
    kitty_keyboard_active = FALSE;
    win32_input_active = TRUE;
    win32_type (seq, 0, 27);
    win32_type_text (seq, "[200~");
    g_string_append_printf (seq, alt_code, 0x2554U);                      // a corner
    g_string_append_printf (seq, alt_code, 0x2550U);                      // a line
    g_string_append (seq, "\033[78;49;110;1;0;1_\033[78;49;110;0;0;1_");  // n
    win32_type (seq, 13, 13);
    g_string_append_printf (seq, alt_code, 0x2551U);
    win32_type (seq, 0, 27);
    win32_type_text (seq, "[201~");
    check_paste (seq->str, "\xe2\x95\x94\xe2\x95\x90n\n\xe2\x95\x91");
    g_string_free (seq, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_enter_in_paste)
{
    GString *seq = g_string_new ("\033[200~ab");

    // Windows Terminal: the text as it is, but Enter as a key
    kitty_keyboard_active = FALSE;
    win32_input_active = TRUE;
    win32_type (seq, 13, 13);
    g_string_append (seq, "c\033[201~");
    check_paste (seq->str, "ab\nc");
    g_string_free (seq, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_esc_is_a_key)
{
    GString *seq = g_string_new (NULL);

    kitty_keyboard_active = FALSE;
    win32_input_active = TRUE;

    ck_assert_int_eq (decode ("\033[27;1;27;1;0;1_"), ESC_CHAR);

    // the Esc key is a key even with the keys of a paste mark behind it
    win32_type (seq, 27, 27);
    win32_type_text (seq, "[200~");
    ck_assert_int_eq (decode (seq->str), ESC_CHAR);
    ck_assert_int_eq (get_key_code (1), '[');

    // an Esc typed by the console with no mark behind it: the keys come after it, as they were
    g_string_truncate (seq, 0);
    win32_type (seq, 0, 27);
    win32_type_text (seq, "[2x");
    ck_assert_int_eq (decode (seq->str), ESC_CHAR);
    ck_assert_int_eq (get_key_code (1), '[');
    ck_assert_int_eq (get_key_code (1), '2');
    ck_assert_int_eq (get_key_code (1), 'x');
    g_string_free (seq, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_paste_without_win32)
{
    kitty_keyboard_active = FALSE;
    // a record in a paste of another terminal is text, as the terminal sent it
    check_paste ("\033[200~ab\r\nc\033[201~", "ab\nc");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_keys_without_code)
{
    tty_key_event_t ev;

    kitty_keyboard_active = FALSE;
    win32_input_active = TRUE;

    ck_assert_int_eq (decode ("\033[19;69;0;1;0;1_"), KEY_KITTY_EVENT);
    ck_assert (tty_key_event (KEY_KITTY_EVENT, &ev));
    ck_assert_uint_eq (ev.key, 57362);  // Pause
    ck_assert_int_eq (decode ("\033[44;55;0;1;256;1_"), KEY_KITTY_EVENT);
    ck_assert (tty_key_event (KEY_KITTY_EVENT, &ev));
    ck_assert_uint_eq (ev.key, 57361);  // Print Screen
    ck_assert_int_eq (decode ("\033[93;93;0;1;256;1_"), KEY_KITTY_EVENT);
    ck_assert (tty_key_event (KEY_KITTY_EVENT, &ev));
    ck_assert_uint_eq (ev.key, 57363);  // Menu
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_capture_pair)
{
    char *seq;
    char *after;

    kitty_keyboard_active = FALSE;
    win32_input_active = TRUE;

    /* both halves of a UTF-16 pair are one key, the releases come after */
    feed ("\033[0;0;55357;1;0;1_\033[0;0;56832;1;0;1_\033[0;0;56832;0;0;1_");
    seq = learn_key_ex (&after);
    ck_assert_str_eq (seq, "\\e[0;0;55357;1;0;1_\\e[0;0;56832;1;0;1_");
    ck_assert_str_eq (after, "\\e[0;0;56832;0;0;1_");
    g_free (seq);
    g_free (after);

    /* a key with no character between the halves is no second half */
    ck_assert_int_eq (decode ("\033[0;0;55357;1;0;1_\033[255;0;0;1;0;1_"), -1);
    ck_assert_int_eq (decode ("\033[0;0;56832;1;0;1_"), -1);

    /* the capture leaves nothing half done for the live input */
    ck_assert_int_eq (decode ("\033[0;0;56832;1;0;1_"), -1);
    ck_assert_int_eq (decode ("\033[65;30;97;1;0;1_"), 'a');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_win32_text_and_modifiers)
{
    tty_key_event_t ev;

    kitty_keyboard_active = FALSE;
    win32_input_active = TRUE;

    /* ef: its UTF-8 bytes, and the event says the key and the base layout */
    feed ("\033[65;30;1092;1;0;1_x");
    ck_assert_int_eq (get_key_code (1), 0xD1);
    ck_assert (tty_key_event (0xD1, &ev));
    ck_assert_uint_eq (ev.key, 'a');
    ck_assert_uint_eq (ev.base, 'a');
    ck_assert_uint_eq (ev.text[0], 1092);
    ck_assert_int_eq (get_key_code (1), 0x84);
    ck_assert_int_eq (get_key_code (1), 'x');

    /* a character outside the BMP comes as the two halves of its UTF-16 pair */
    feed ("\033[0;0;55357;1;0;1_\033[0;0;56832;1;0;1_");
    ck_assert_int_eq (get_key_code (1), 0xF0);
    ck_assert_int_eq (get_key_code (1), 0x9F);
    ck_assert_int_eq (get_key_code (1), 0x98);
    ck_assert_int_eq (get_key_code (1), 0x80);

    /* the held modifiers, the left and the right Ctrl apart */
    decode ("\033[17;29;0;1;8;1_");
    ck_assert_uint_eq (tty_kitty_modifiers (), TTY_KITTY_MOD_CTRL);
    decode ("\033[17;29;0;1;268;1_");  // the right Ctrl too: an enhanced key
    decode ("\033[17;29;0;0;4;1_");    // the left one up, the right one held
    ck_assert_uint_eq (tty_kitty_modifiers (), TTY_KITTY_MOD_CTRL);
    decode ("\033[17;29;0;0;256;1_");
    ck_assert_uint_eq (tty_kitty_modifiers (), 0);

    /* a record with a repeat count is that many keys: three Left, then x */
    feed ("\033[37;75;0;1;256;3_x");
    ck_assert_int_eq (get_key_code (1), KEY_LEFT);
    ck_assert_int_eq (get_key_code (1), KEY_LEFT);
    ck_assert_int_eq (get_key_code (1), KEY_LEFT);
    ck_assert_int_eq (get_key_code (1), 'x');

    /* the repeats are repeat events, as kitty gives them */
    feed ("\033[37;75;0;1;256;2_");
    ck_assert_int_eq (get_key_code (1), KEY_LEFT);
    ck_assert (tty_key_event (KEY_LEFT, &ev));
    ck_assert_int_eq (ev.event, TTY_KITTY_PRESS);
    ck_assert_int_eq (get_key_code (1), KEY_LEFT);
    ck_assert (tty_key_event (KEY_LEFT, &ev));
    ck_assert_int_eq (ev.event, TTY_KITTY_REPEAT);

    /* a key with no mc code is repeated too: F13 for a program in mcterm */
    feed ("\033[124;100;0;1;0;3_x");
    ck_assert_int_eq (get_key_code (1), KEY_KITTY_EVENT);
    ck_assert_int_eq (get_key_code (1), KEY_KITTY_EVENT);
    ck_assert_int_eq (get_key_code (1), KEY_KITTY_EVENT);
    ck_assert_int_eq (get_key_code (1), 'x');

    /* a count of more than 256 is not cut */
    {
        int n;

        feed ("\033[37;75;0;1;256;300_x");
        for (n = 0; get_key_code (1) == KEY_LEFT; n++)
            ;
        ck_assert_int_eq (n, 300);
    }

    /* and a character as many times, each with all its bytes */
    feed ("\033[65;30;1092;1;0;2_y");
    ck_assert_int_eq (get_key_code (1), 0xD1);
    ck_assert_int_eq (get_key_code (1), 0x84);
    ck_assert_int_eq (get_key_code (1), 0xD1);
    ck_assert_int_eq (get_key_code (1), 0x84);
    ck_assert_int_eq (get_key_code (1), 'y');

    /* not decoded when the mode is off */
    win32_input_active = FALSE;
    kitty_keyboard_active = TRUE;
    ck_assert_int_ne (decode ("\033[65;30;97;1;0;1_"), 'a');
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
    tcase_add_test (tc_core, test_kitty_event_text);
    tcase_add_test (tc_core, test_kitty_event_long_text);
    tcase_add_test (tc_core, test_kitty_flags_base);
    tcase_add_test (tc_core, test_kitty_held_modifiers);
    tcase_add_test (tc_core, test_getch_skips_modifiers);
    tcase_add_test (tc_core, test_learn_skips_leading_release);
    mctest_add_parameterized_test (tc_core, test_win32_decode, win32_ds);
    tcase_add_test (tc_core, test_win32_keys_without_code);
    tcase_add_test (tc_core, test_win32_paste_typed_as_keys);
    tcase_add_test (tc_core, test_win32_paste_from_windows_console);
    tcase_add_test (tc_core, test_win32_paste_of_a_frame);
    tcase_add_test (tc_core, test_win32_enter_in_paste);
    tcase_add_test (tc_core, test_win32_esc_is_a_key);
    tcase_add_test (tc_core, test_paste_without_win32);
    tcase_add_test (tc_core, test_win32_capture_pair);
    tcase_add_test (tc_core, test_win32_text_and_modifiers);
    tcase_add_test (tc_core, test_super_key_names);
    tcase_add_test (tc_core, test_kitty_inactive);

    return mctest_run_all (tc_core);
}
