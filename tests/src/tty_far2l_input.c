/*
   src - unit tests for the far2l keyboard extensions decoder

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

#define TEST_SUITE_NAME "/src/tty_far2l_input"

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

static int test_input[400];
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
    far2l_input_active = TRUE;
    use_mouse_p = MOUSE_XTERM_BUTTON_EVENT_TRACKING;
    mouse_enabled = TRUE;
    define_sequence (MCKEY_EXTENDED_MOUSE, "\033[<", MCKEY_NOACTION);
}

static void
teardown (void)
{
    far2l_input_active = FALSE;
    mouse_enabled = FALSE;
    use_mouse_p = MOUSE_NONE;
    done_key ();
    close (input_fd);
    input_fd = -1;
}

/* Append bytes to what the test feeds to the keyboard. */
static void
append (const char *seq, size_t len)
{
    size_t i;

    ck_assert_uint_lt (test_input_len + len, G_N_ELEMENTS (test_input));
    for (i = 0; i < len; i++)
        test_input[test_input_len++] = (unsigned char) seq[i];
}

static void
append_str (const char *seq)
{
    append (seq, strlen (seq));
}

/* The values in the order they are pushed on the far2l stack, little endian, the command last;
   @term is the terminator, BEL or ESC \ */
static void
append_packet (const char *term, char cmd, const unsigned char *pushed, size_t len)
{
    guchar stack[17];
    gchar *b64;

    memcpy (stack, pushed, len);
    stack[len] = (guchar) cmd;
    b64 = g_base64_encode (stack, len + 1);
    append_str ("\033_f2l");
    append_str (b64);
    append_str (term);
    g_free (b64);
}

/* Little endian bytes of @value, @size of them, at @out */
static void
put_le (unsigned char *out, unsigned int size, unsigned int value)
{
    unsigned int i;

    for (i = 0; i < size; i++)
        out[i] = (unsigned char) (value >> (8 * i));
}

/* A key packet, 'K' pressed or 'k' released */
static void
append_key (const char *term, char cmd, unsigned int vk, unsigned int uc, unsigned int cs)
{
    unsigned char pushed[14];

    put_le (pushed, 2, 1);  // repeat count
    put_le (pushed + 2, 2, vk);
    put_le (pushed + 4, 2, 0);  // scan code
    put_le (pushed + 6, 4, cs);
    put_le (pushed + 10, 4, uc);
    append_packet (term, cmd, pushed, sizeof (pushed));
}

/* The short key packet, 'C' pressed or 'c' released */
static void
append_short_key (char cmd, unsigned int vk, unsigned int uc, unsigned int cs)
{
    unsigned char pushed[5];

    put_le (pushed, 1, vk);
    put_le (pushed + 1, 2, cs);
    put_le (pushed + 3, 2, uc);
    append_packet ("\033\\", cmd, pushed, sizeof (pushed));
}

/* A mouse packet: 'M' with 32 bit fields, or the compact 'm' with the button state squeezed */
static void
append_mouse (gboolean compact, unsigned int flags, unsigned int buttons, int x, int y)
{
    unsigned char pushed[16];

    put_le (pushed, 2, (unsigned int) x);
    put_le (pushed + 2, 2, (unsigned int) y);
    if (compact)
    {
        put_le (pushed + 4, 2, (buttons & 0xFF) | ((buttons >> 8) & 0xFF00));
        put_le (pushed + 6, 1, 0);  // control key state
        put_le (pushed + 7, 1, flags);
        append_packet ("\033\\", 'm', pushed, 8);
    }
    else
    {
        put_le (pushed + 4, 4, buttons);
        put_le (pushed + 8, 4, 0);  // control key state
        put_le (pushed + 12, 4, flags);
        append_packet ("\033\\", 'M', pushed, 16);
    }
}

/* The report the keyboard got back, after the ESC [ < that made the mouse key */
static const char *
mouse_report (void)
{
    static char report[32];
    size_t n = 0;
    int c;

    ck_assert_int_eq (get_key_code (1), MCKEY_EXTENDED_MOUSE);
    do
    {
        c = test_tty_lowlevel_getch ();
        ck_assert_int_ne (c, -1);
        report[n++] = (char) c;
    }
    while (c != 'M' && c != 'm' && n < sizeof (report) - 1);
    report[n] = '\0';
    return report;
}

static void
reset_input (void)
{
    test_input_len = test_input_pos = 0;
}

/* --------------------------------------------------------------------------------------------- */

static const struct decode_ds
{
    unsigned int vk;
    unsigned int uc;
    unsigned int cs;
    int code;
} decode_ds[] = {
    { 65, 97, 0, 'a' },                      // a
    { 65, 65, 16, 'A' },                     // Shift-A
    { 49, 33, 16, '!' },                     // Shift-1
    { 65, 1, 8, XCTRL ('a') },               // Ctrl-A
    { 65, 1, 24, XCTRL ('a') },              // Ctrl-Shift-A, as the keymap names it
    { 65, 97, 2, ALT ('a') },                // Alt-A
    { 81, 64, 10, '@' },                     // AltGr-Q is @ on some layouts
    { 13, 13, 0, '\n' },                     // Enter
    { 13, 13, 8, KEY_M_CTRL | '\n' },        // Ctrl-Enter
    { 9, 9, 16, KEY_M_SHIFT | '\t' },        // Shift-Tab
    { 27, 27, 0, ESC_CHAR },                 // Esc
    { 8, 8, 2, KEY_M_ALT | KEY_BACKSPACE },  // Alt-Backspace
    { 32, 0, 8, XCTRL (' ') },               // Ctrl-Space
    { 39, 0, 264, KEY_M_CTRL | KEY_RIGHT },  // Ctrl-Right
    { 38, 0, 272, KEY_M_SHIFT | KEY_UP },    // Shift-Up
    { 36, 0, 256, KEY_HOME },                // Home
    { 46, 0, 256, KEY_DC },                  // Delete
    { 112, 0, 0, KEY_F (1) },                // F1
    { 116, 0, 16, KEY_F (15) },              // Shift-F5, F15 as the keymap names it
    { 115, 0, 2, KEY_M_ALT | KEY_F (4) },    // Alt-F4
    { 16, 0, 16, -1 },                       // Shift on its own
    { 20, 0, 128, -1 },                      // Caps Lock
};

START_PARAMETRIZED_TEST (test_far2l_decode, decode_ds)
{
    reset_input ();
    append_key ("\033\\", 'K', data->vk, data->uc, data->cs);
    ck_assert_int_eq (get_key_code (1), data->code);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_bel_terminator)
{
    append_key ("\a", 'K', 65, 97, 0);
    ck_assert_int_eq (get_key_code (1), 'a');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_short_packet)
{
    append_short_key ('C', 65, 1, 8);
    append_short_key ('c', 65, 1, 8);
    append_short_key ('C', 114, 0, 16);
    ck_assert_int_eq (get_key_code (1), XCTRL ('a'));
    ck_assert_int_eq (get_key_code (1), -1);          // the release is nothing
    ck_assert_int_eq (get_key_code (1), KEY_F (13));  // Shift-F3
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_press_and_release)
{
    append_key ("\033\\", 'K', 65, 97, 0);
    append_key ("\033\\", 'k', 65, 97, 0);
    append_key ("\033\\", 'K', 66, 98, 0);
    ck_assert_int_eq (get_key_code (1), 'a');
    ck_assert_int_eq (get_key_code (1), -1);
    ck_assert_int_eq (get_key_code (1), 'b');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_other_script_goes_back_as_utf8)
{
    // U+0444 is D1 84 in UTF-8; the bytes come back to the keyboard and are read as such
    append_key ("\033\\", 'K', 65, 1092, 0);
    append_str ("z");
    ck_assert_int_eq (get_key_code (1), 0xD1);
    ck_assert_int_eq (get_key_code (1), 0x84);
    ck_assert_int_eq (get_key_code (1), 'z');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_ignored_packets)
{
    // a reply, a mouse packet, a packet that is not ours and one that is not base64: no key,
    // and the keys after them are still read
    append_str ("\033_far2l:AAAA\033\\");
    append_packet ("\033\\", 'M', (const unsigned char *) "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 15);
    append_str ("\033_something else\033\\");
    append_str ("\033_f2l!!!!\033\\");
    append_key ("\033\\", 'K', 66, 98, 0);
    ck_assert_int_eq (get_key_code (1), -1);
    ck_assert_int_eq (get_key_code (1), -1);
    ck_assert_int_eq (get_key_code (1), -1);
    ck_assert_int_eq (get_key_code (1), -1);
    ck_assert_int_eq (get_key_code (1), 'b');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_truncated_packet)
{
    // a short stack, and a packet cut short: no key, no hang
    append_packet ("\033\\", 'K', (const unsigned char *) "\1\0", 2);
    ck_assert_int_eq (get_key_code (1), -1);
    reset_input ();
    append_str ("\033_f2lAAAA");
    ck_assert_int_eq (get_key_code (1), -1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_mouse_click)
{
    // left button down at column 11, row 4 (from 0), and up; the terminal counts from 0, xterm from
    // 1
    append_mouse (FALSE, 0, 1, 11, 4);
    append_mouse (FALSE, 0, 0, 11, 4);
    ck_assert_str_eq (mouse_report (), "0;12;5M");
    ck_assert_str_eq (mouse_report (), "0;12;5m");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_mouse_buttons_drag_and_wheel)
{
    append_mouse (FALSE, 0, 2, 0, 0);                      // right down
    append_mouse (TRUE, 1, 2, 3, 1);                       // moved with it held, in the short form
    append_mouse (FALSE, 0, 0, 3, 1);                      // up
    append_mouse (FALSE, 0, 4, 0, 0);                      // middle down
    append_mouse (FALSE, 0, 0, 0, 0);                      // up
    append_mouse (FALSE, 1, 0, 5, 5);                      // moved with nothing held: no event
    append_mouse (FALSE, 4, 120u << 16, 1, 1);             // wheel up
    append_mouse (FALSE, 4, (unsigned) -120 << 16, 1, 1);  // wheel down
    append_mouse (FALSE, 4, 120u << 16, 1, 1);
    ck_assert_str_eq (mouse_report (), "2;1;1M");
    ck_assert_str_eq (mouse_report (), "34;4;2M");
    ck_assert_str_eq (mouse_report (), "0;4;2m");
    ck_assert_str_eq (mouse_report (), "1;1;1M");
    ck_assert_str_eq (mouse_report (), "0;1;1m");
    ck_assert_int_eq (get_key_code (1), -1);  // the move with nothing held
    ck_assert_str_eq (mouse_report (), "64;2;2M");
    ck_assert_str_eq (mouse_report (), "65;2;2M");
    ck_assert_str_eq (mouse_report (), "64;2;2M");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_mouse_compact_wheel_and_off)
{
    append_mouse (TRUE, 4, 120u << 16, 1, 1);
    ck_assert_str_eq (mouse_report (), "64;2;2M");

    // mc did not ask for the mouse: nothing goes back to the keyboard, and the next key is read
    mouse_enabled = FALSE;
    append_mouse (FALSE, 0, 1, 1, 1);
    append_key ("\033\\", 'K', 66, 98, 0);
    ck_assert_int_eq (get_key_code (1), -1);
    ck_assert_int_eq (get_key_code (1), 'b');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_inactive)
{
    // no far2l: ESC _ stays what it was, Alt-_, and the text after it is not swallowed
    far2l_input_active = FALSE;
    append_key ("\033\\", 'K', 65, 97, 0);
    ck_assert_int_ne (get_key_code (1), 'a');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    mctest_add_parameterized_test (tc_core, test_far2l_decode, decode_ds);
    tcase_add_test (tc_core, test_far2l_bel_terminator);
    tcase_add_test (tc_core, test_far2l_short_packet);
    tcase_add_test (tc_core, test_far2l_press_and_release);
    tcase_add_test (tc_core, test_far2l_other_script_goes_back_as_utf8);
    tcase_add_test (tc_core, test_far2l_ignored_packets);
    tcase_add_test (tc_core, test_far2l_truncated_packet);
    tcase_add_test (tc_core, test_far2l_mouse_click);
    tcase_add_test (tc_core, test_far2l_mouse_buttons_drag_and_wheel);
    tcase_add_test (tc_core, test_far2l_mouse_compact_wheel_and_off);
    tcase_add_test (tc_core, test_far2l_inactive);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
