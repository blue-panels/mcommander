/*
   tests/src/mcterm_key_encode.c -- test mcterm key encoding

   Copyright (C) 2026
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

#define TEST_SUITE_NAME "/src/mcterm_key_encode"

#include "tests/mctest.h"

#include <string.h>

#include "lib/strutil.h"
#include "lib/terminal.h"
#include "lib/mcconfig.h"
#include "lib/tty/key.h"
#include "src/mcterm/mcterm_key.h"

/* --------------------------------------------------------------------------------------------- */

static void
init_mcterm_key_table (void)
{
    mc_config_t *cfg;
    const gchar *f13[] = { "\\e[25~", "\\e[1;2R" };
    const gchar *f15[] = { "\\e[15;2~" };
    const gchar *alt_f3[] = { "\\e[1;3R" };
    const gchar *f20[] = { "\\e[19;2~" };

    cfg = mc_config_init (NULL, FALSE);
    if (cfg == NULL)
        return;

    mc_config_set_string_list (cfg, "terminal:xterm", "f13", f13, G_N_ELEMENTS (f13));
    mc_config_set_string_list (cfg, "terminal:xterm", "f15", f15, G_N_ELEMENTS (f15));
    mc_config_set_string_list (cfg, "terminal:xterm", "alt-f3", alt_f3, G_N_ELEMENTS (alt_f3));
    mc_config_set_string_list (cfg, "terminal:xterm", "f20", f20, G_N_ELEMENTS (f20));
    mc_config_set_string (cfg, "terminal:xterm-256color", "copy", "xterm");

    mcterm_key_table_init (NULL, cfg);
    mc_config_deinit (cfg);
}

/* --------------------------------------------------------------------------------------------- */

static void
assert_encoded (int key, gboolean app_cursor, const char *expected)
{
    unsigned char buf[32];
    char *raw;
    size_t len;

    raw = convert_controls (expected);
    ck_assert_ptr_ne (raw, NULL);

    memset (buf, 0, sizeof (buf));
    len = mcterm_encode_key_xterm (key, buf, sizeof (buf), app_cursor);

    ck_assert_uint_eq (len, strlen (raw));
    ck_assert_mem_eq (buf, raw, len);

    g_free (raw);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_function_keys_use_encoding_map)
{
    init_mcterm_key_table ();

    /* The encoder picks the first value from a multi-value list. */
    assert_encoded (KEY_F (13), FALSE, "\\e[25~");
    assert_encoded (KEY_F (15), FALSE, "\\e[15;2~");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_alt_function_key_uses_encoding_map)
{
    init_mcterm_key_table ();

    assert_encoded (KEY_M_ALT | KEY_F (3), FALSE, "\\e[1;3R");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_unknown_function_key_has_no_builtin_fallback)
{
    unsigned char buf[32];
    size_t len;

    init_mcterm_key_table ();

    memset (buf, 0, sizeof (buf));
    len = mcterm_encode_key_xterm (KEY_F (17), buf, sizeof (buf), FALSE);
    ck_assert_uint_eq (len, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_application_cursor_plain_arrows)
{
    init_mcterm_key_table ();

    assert_encoded (KEY_UP, TRUE, "\\eOA");
    assert_encoded (KEY_DOWN, TRUE, "\\eOB");
    assert_encoded (KEY_RIGHT, TRUE, "\\eOC");
    assert_encoded (KEY_LEFT, TRUE, "\\eOD");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_xterm_keys_ignore_encoding_map)
{
    mc_config_t *cfg;

    cfg = mc_config_init (NULL, FALSE);
    mc_config_set_string (cfg, "terminal:xterm", "home", "\\e[1~");
    mc_config_set_string (cfg, "terminal:xterm", "end", "\\e[4~");
    mc_config_set_string (cfg, "terminal:xterm", "up", "\\eOA");
    mc_config_set_string (cfg, "terminal:xterm", "f1", "\\e[11~");
    mc_config_set_string (cfg, "terminal:xterm", "f7", "\\e[99~");
    mcterm_key_table_init (NULL, cfg);
    mc_config_deinit (cfg);

    assert_encoded (KEY_HOME, FALSE, "\\e[H");
    assert_encoded (KEY_END, FALSE, "\\e[F");
    assert_encoded (KEY_UP, FALSE, "\\e[A");
    assert_encoded (KEY_HOME, TRUE, "\\eOH");
    assert_encoded (KEY_END, TRUE, "\\eOF");
    assert_encoded (KEY_DC, TRUE, "\\e[3~");
    assert_encoded (KEY_F (1), FALSE, "\\eOP");
    assert_encoded (KEY_F (7), FALSE, "\\e[18~");
    assert_encoded (KEY_F (0), FALSE, "\\e[21~");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_enter_maps_to_cr)
{
    unsigned char buf[4];
    size_t len;

    len = mcterm_encode_key_xterm ('\n', buf, sizeof (buf), FALSE);
    ck_assert_uint_eq (len, 1);
    ck_assert_uint_eq (buf[0], '\r');

    len = mcterm_encode_key_xterm ('\r', buf, sizeof (buf), FALSE);
    ck_assert_uint_eq (len, 1);
    ck_assert_uint_eq (buf[0], '\r');

    len = mcterm_encode_key_xterm (KEY_ENTER, buf, sizeof (buf), FALSE);
    ck_assert_uint_eq (len, 1);
    ck_assert_uint_eq (buf[0], '\r');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_shift_tab_is_back_tab) { assert_encoded (KEY_M_SHIFT | '\t', FALSE, "\\e[Z"); }
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_backspace_maps_to_del)
{
    unsigned char buf[4];
    size_t len;

    len = mcterm_encode_key_xterm (KEY_BACKSPACE, buf, sizeof (buf), FALSE);
    ck_assert_uint_eq (len, 1);
    ck_assert_uint_eq (buf[0], 0x7F);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_utf8_bytes_pass_through)
{
    unsigned char buf[4];
    size_t len;

    len = mcterm_encode_key_xterm (0x80, buf, sizeof (buf), FALSE);
    ck_assert_uint_eq (len, 1);
    ck_assert_uint_eq (buf[0], 0x80);

    len = mcterm_encode_key_xterm (0xC3, buf, sizeof (buf), FALSE);
    ck_assert_uint_eq (len, 1);
    ck_assert_uint_eq (buf[0], 0xC3);

    len = mcterm_encode_key_xterm (0xFF, buf, sizeof (buf), FALSE);
    ck_assert_uint_eq (len, 1);
    ck_assert_uint_eq (buf[0], 0xFF);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_alt_ascii_uses_esc_prefix)
{
    unsigned char buf[8];
    size_t len;

    len = mcterm_encode_key_xterm (KEY_M_ALT | 'x', buf, sizeof (buf), FALSE);
    ck_assert_uint_eq (len, 2);
    ck_assert_uint_eq (buf[0], 0x1B);
    ck_assert_uint_eq (buf[1], 'x');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_modified_cursor_keys_use_csi_form)
{
    init_mcterm_key_table ();

    assert_encoded (KEY_M_ALT | KEY_LEFT, FALSE, "\\e[1;3D");
    assert_encoded (KEY_M_ALT | KEY_RIGHT, FALSE, "\\e[1;3C");
    assert_encoded (KEY_M_CTRL | KEY_LEFT, FALSE, "\\e[1;5D");
    assert_encoded (KEY_M_CTRL | KEY_RIGHT, FALSE, "\\e[1;5C");
    assert_encoded (KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, FALSE, "\\e[1;6C");
    assert_encoded (KEY_M_CTRL | KEY_HOME, FALSE, "\\e[1;5H");
    assert_encoded (KEY_M_CTRL | KEY_END, FALSE, "\\e[1;5F");
    assert_encoded (KEY_M_CTRL | KEY_DC, FALSE, "\\e[3;5~");
    assert_encoded (KEY_M_ALT | KEY_NPAGE, FALSE, "\\e[6;3~");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_ctrl_digits_without_kitty_are_xterm_bytes)
{
    unsigned char buf[8];

    init_mcterm_key_table ();

    assert_encoded (KEY_M_CTRL | '1', FALSE, "1");
    assert_encoded (KEY_M_CTRL | '3', FALSE, "\\e");
    assert_encoded (KEY_M_CTRL | '8', FALSE, "\x7f");
    ck_assert_uint_eq (mcterm_encode_key_xterm (KEY_M_CTRL | '2', buf, sizeof (buf), FALSE), 1);
    ck_assert_uint_eq (buf[0], 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static void
assert_kitty (int key, guint flags, const char *expected)
{
    unsigned char buf[32];
    char *raw;
    size_t len;

    raw = convert_controls (expected);
    memset (buf, 0, sizeof (buf));
    len = mcterm_encode_key (key, flags, buf, sizeof (buf), FALSE);

    ck_assert_msg (len == strlen (raw) && memcmp (buf, raw, len) == 0,
                   "key 0x%x flags %u: got %.*s, expected %s", (unsigned) key, flags, (int) len,
                   (const char *) buf, expected);
    g_free (raw);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_disambiguate)
{
    init_mcterm_key_table ();

    assert_kitty ('a', 1, "a");
    assert_kitty ('A', 1, "A");
    assert_kitty (KEY_M_CTRL | '1', 1, "\\e[49;5u");
    assert_kitty (KEY_M_ALT | KEY_M_CTRL | '1', 1, "\\e[49;7u");
    assert_kitty (XCTRL ('a'), 1, "\\e[97;5u");
    assert_kitty (ALT ('a'), 1, "\\e[97;3u");
    assert_kitty (ESC_CHAR, 1, "\\e[27u");
    assert_kitty ('\n', 1, "\r");
    assert_kitty (KEY_M_CTRL | '\n', 1, "\\e[13;5u");
    assert_kitty (KEY_M_SHIFT | '\t', 1, "\\e[9;2u");
    assert_kitty (KEY_BACKSPACE, 1, "\x7f");
    assert_kitty (KEY_M_ALT | KEY_BACKSPACE, 1, "\\e[127;3u");
    assert_kitty (KEY_F (1), 1, "\\eOP");
    assert_kitty (KEY_M_CTRL | KEY_F (1), 1, "\\e[1;5P");
    assert_kitty (KEY_F (3), 1, "\\e[13~");
    assert_kitty (KEY_M_ALT | KEY_F (3), 1, "\\e[13;3~");
    assert_kitty (KEY_F (5), 1, "\\e[15~");
    assert_kitty (KEY_F (12), 1, "\\e[24~");
    assert_kitty (KEY_F (13), 1, "\\e[57376u");
    assert_kitty (KEY_UP, 1, "\\e[A");
    assert_kitty (KEY_M_CTRL | KEY_UP, 1, "\\e[1;5A");
    assert_kitty (KEY_DC, 1, "\\e[3~");
    assert_kitty (KEY_M_SHIFT | KEY_NPAGE, 1, "\\e[6;2~");
    assert_kitty (KEY_M_SUPER | 'a', 1, "\\e[97;9u");
    assert_kitty (KEY_M_SUPER | 'a', 0, "a");
    assert_kitty (KEY_M_SUPER | KEY_UP, 1, "\\e[1;9A");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_all_keys)
{
    unsigned char buf[32];
    size_t len;

    init_mcterm_key_table ();

    assert_kitty ('a', 8, "\\e[97u");
    assert_kitty ('A', 8, "\\e[97;2u");
    assert_kitty ('A', 8 | 4, "\\e[97:65;2u");
    assert_kitty ('a', 8 | 16, "\\e[97;1;97u");
    assert_kitty ('A', 8 | 16 | 4, "\\e[97:65;2;65u");
    assert_kitty ('\n', 8, "\\e[13u");
    assert_kitty ('\t', 8, "\\e[9u");
    assert_kitty (KEY_BACKSPACE, 8, "\\e[127u");
    assert_kitty (XCTRL ('a'), 8 | 16, "\\e[97;5u");
    /* flags that change nothing on their own keep the legacy keys */
    assert_kitty (XCTRL ('a'), 4, "\x01");

    len = mcterm_encode_kitty_codepoint (0x0444, 0, 8, buf, sizeof (buf));
    ck_assert_uint_eq (len, strlen ("\x1b[1092u"));
    ck_assert_mem_eq (buf, "\x1b[1092u", len);
    len = mcterm_encode_kitty_codepoint (0x0424, 0, 8 | 4, buf, sizeof (buf));
    ck_assert_mem_eq (buf, "\x1b[1092:1060;2u", len);
    ck_assert_uint_eq (mcterm_encode_kitty_codepoint (0x0444, 0, 1, buf, sizeof (buf)), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_small_buffer_returns_zero)
{
    unsigned char buf[2];
    size_t len;

    init_mcterm_key_table ();

    len = mcterm_encode_key_xterm (KEY_F (20), buf, sizeof (buf), FALSE);
    ck_assert_uint_eq (len, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A section that copies itself must not loop. */
START_TEST (test_copy_self_does_not_loop)
{
    mc_config_t *cfg;
    const gchar *f13[] = { "\\e[1;2R" };

    cfg = mc_config_init (NULL, FALSE);
    ck_assert_ptr_ne (cfg, NULL);
    mc_config_set_string_list (cfg, "terminal:loop", "f13", f13, G_N_ELEMENTS (f13));
    mc_config_set_string (cfg, "terminal:loop", "copy", "loop");
    mcterm_key_table_init (NULL, cfg);
    mc_config_deinit (cfg);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A <-> B mutual copy must not loop. */
START_TEST (test_copy_cycle_does_not_loop)
{
    mc_config_t *cfg;
    const gchar *f13[] = { "\\e[1;2R" };

    cfg = mc_config_init (NULL, FALSE);
    ck_assert_ptr_ne (cfg, NULL);
    mc_config_set_string_list (cfg, "terminal:cycleA", "f13", f13, G_N_ELEMENTS (f13));
    mc_config_set_string (cfg, "terminal:cycleA", "copy", "cycleB");
    mc_config_set_string (cfg, "terminal:cycleB", "copy", "cycleA");
    mcterm_key_table_init (NULL, cfg);
    mc_config_deinit (cfg);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A chain longer than the old depth-4 limit must still resolve fully. */
START_TEST (test_copy_chain_beyond_old_depth_limit)
{
    mc_config_t *cfg;
    const gchar *f13[] = { "\\e[1;2R" };
    unsigned char buf[32];
    size_t len;

    /* Build: xterm -> a -> b -> c -> d -> e -> base, base defines f13. */
    cfg = mc_config_init (NULL, FALSE);
    ck_assert_ptr_ne (cfg, NULL);
    mc_config_set_string (cfg, "terminal:xterm", "copy", "chain_a");
    mc_config_set_string (cfg, "terminal:chain_a", "copy", "chain_b");
    mc_config_set_string (cfg, "terminal:chain_b", "copy", "chain_c");
    mc_config_set_string (cfg, "terminal:chain_c", "copy", "chain_d");
    mc_config_set_string (cfg, "terminal:chain_d", "copy", "chain_e");
    mc_config_set_string (cfg, "terminal:chain_e", "copy", "chain_base");
    mc_config_set_string_list (cfg, "terminal:chain_base", "f13", f13, G_N_ELEMENTS (f13));
    mc_config_set_string (cfg, "terminal:xterm-256color", "copy", "xterm");

    mcterm_key_table_init (NULL, cfg);
    mc_config_deinit (cfg);

    memset (buf, 0, sizeof (buf));
    len = mcterm_encode_key_xterm (KEY_F (13), buf, sizeof (buf), FALSE);
    ck_assert_uint_gt (len, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static void
assert_event (char final, unsigned int key, unsigned int shifted, unsigned int base,
              unsigned int mods, int event, gunichar text, guint flags, const char *expected)
{
    tty_key_event_t ev;
    unsigned char buf[64];
    char *raw;
    size_t len;

    memset (&ev, 0, sizeof (ev));
    ev.final = final;
    ev.key = key;
    ev.shifted = shifted;
    ev.base = base;
    ev.mods = mods;
    ev.event = event;
    if (text != 0)
    {
        ev.text[0] = text;
        ev.text_len = 1;
    }

    raw = convert_controls (expected);
    memset (buf, 0, sizeof (buf));
    len = mcterm_encode_kitty_event (&ev, flags, buf, sizeof (buf), FALSE);
    ck_assert_msg (len == strlen (raw) && memcmp (buf, raw, len) == 0,
                   "key %u event %d flags %u: got %.*s, expected %s", key, event, flags, (int) len,
                   (const char *) buf, expected);
    g_free (raw);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_event_encoding)
{
    const unsigned int S = TTY_KITTY_MOD_SHIFT, A = TTY_KITTY_MOD_ALT, C = TTY_KITTY_MOD_CTRL;
    const unsigned int SUPER = TTY_KITTY_MOD_SUPER, CAPS = TTY_KITTY_MOD_CAPS_LOCK;
    const int P = TTY_KITTY_PRESS, R = TTY_KITTY_REPEAT, X = TTY_KITTY_RELEASE;

    /* a release goes only to a program that asked for the event types */
    assert_event ('u', 97, 0, 0, C, X, 0, 1, "");
    assert_event ('u', 97, 0, 0, C, X, 0, 1 | 2, "\\\\e[97;5:3u");
    assert_event ('u', 97, 0, 0, C, R, 0, 1, "\\\\e[97;5u");
    assert_event ('u', 97, 0, 0, C, R, 0, 1 | 2, "\\\\e[97;5:2u");

    /* Super goes along */
    assert_event ('u', 97, 0, 0, SUPER, P, 0, 1, "\\\\e[97;9u");

    /* with flag 1 alone a character is text, and so is its repeat; its release is not sent */
    assert_event ('u', 1092, 1060, 0, S, P, 0, 1, "\xd0\xa4");
    assert_event ('u', 97, 0, 0, 0, R, 0, 1 | 2, "a");
    assert_event ('u', 97, 0, 0, 0, X, 0, 1 | 2, "");
    assert_event ('u', 13, 0, 0, 0, P, 0, 1, "\r");
    assert_event ('u', 13, 0, 0, S, P, 0, 1, "\\\\e[13;2u");
    assert_event ('u', 27, 0, 0, 0, P, 0, 1, "\\\\e[27u");

    /* every key as CSI u, with the alternates and the text; the lock keys count then */
    assert_event ('u', 97, 65, 0, S | CAPS, P, 65, 8 | 4 | 16, "\\\\e[97:65;66;65u");
    assert_event ('u', 1092, 0, 97, 0, P, 1092, 8 | 4, "\\\\e[1092::97u");
    assert_event ('u', 97, 0, 0, CAPS, P, 0, 1, "a");

    /* the modifier and lock keys themselves only with flag 8 */
    assert_event ('u', 57441, 0, 0, S, P, 0, 1, "");
    assert_event ('u', 57441, 0, 0, S, X, 0, 8 | 2, "\\\\e[57441;2:3u");

    /* F13 and up and the media keys go with flag 1 */
    assert_event ('u', 57376, 0, 0, 0, P, 0, 1, "\\\\e[57376u");
    assert_event ('u', 57428, 0, 0, 0, P, 0, 1, "\\\\e[57428u");

    /* the legacy forms keep their shape, with the modifiers and the event type */
    assert_event ('A', 1, 0, 0, 0, P, 0, 1, "\\\\e[A");
    assert_event ('A', 1, 0, 0, C, R, 0, 1 | 2, "\\\\e[1;5:2A");
    assert_event ('A', 1, 0, 0, 0, X, 0, 1 | 2, "\\\\e[1;1:3A");
    assert_event ('P', 1, 0, 0, 0, P, 0, 1, "\\\\eOP");
    assert_event ('~', 3, 0, 0, A, P, 0, 1, "\\\\e[3;3~");
    assert_event ('~', 15, 0, 0, 0, X, 0, 1 | 2, "\\\\e[15;1:3~");

    /* the event types alone: the legacy forms carry them */
    assert_event ('A', 1, 0, 0, 0, P, 0, 2, "\\\\e[A");
    assert_event ('A', 1, 0, 0, 0, X, 0, 2, "\\\\e[1;1:3A");
    assert_event ('~', 15, 0, 0, 0, R, 0, 2, "\\\\e[15;1:2~");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_event_long_text)
{
    tty_key_event_t ev;
    unsigned char buf[TTY_KITTY_TEXT_MAX * 8 + 64];
    GString *expected;
    size_t len;
    int i;

    memset (&ev, 0, sizeof (ev));
    ev.final = 'u';
    ev.key = 97;
    ev.event = TTY_KITTY_PRESS;
    ev.text_len = 20;
    expected = g_string_new ("\x1b[97;1;");
    for (i = 0; i < ev.text_len; i++)
    {
        ev.text[i] = 0x430 + (gunichar) i;
        g_string_append_printf (expected, i == 0 ? "%u" : ":%u", 0x430 + (unsigned int) i);
    }
    g_string_append_c (expected, 'u');

    len = mcterm_encode_kitty_event (&ev, 8 | 16, buf, sizeof (buf), FALSE);
    ck_assert_uint_eq (len, expected->len);
    ck_assert (memcmp (buf, expected->str, len) == 0);

    /* with flag 1 alone the whole text goes as UTF-8 */
    len = mcterm_encode_kitty_event (&ev, 1, buf, sizeof (buf), FALSE);
    ck_assert_uint_eq (len, 20 * 2);
    g_string_free (expected, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    str_init_strings ("UTF-8");

    tc_core = tcase_create ("Core");
    tcase_add_test (tc_core, test_function_keys_use_encoding_map);
    tcase_add_test (tc_core, test_alt_function_key_uses_encoding_map);
    tcase_add_test (tc_core, test_unknown_function_key_has_no_builtin_fallback);
    tcase_add_test (tc_core, test_application_cursor_plain_arrows);
    tcase_add_test (tc_core, test_xterm_keys_ignore_encoding_map);
    tcase_add_test (tc_core, test_enter_maps_to_cr);
    tcase_add_test (tc_core, test_shift_tab_is_back_tab);
    tcase_add_test (tc_core, test_backspace_maps_to_del);
    tcase_add_test (tc_core, test_utf8_bytes_pass_through);
    tcase_add_test (tc_core, test_alt_ascii_uses_esc_prefix);
    tcase_add_test (tc_core, test_modified_cursor_keys_use_csi_form);
    tcase_add_test (tc_core, test_ctrl_digits_without_kitty_are_xterm_bytes);
    tcase_add_test (tc_core, test_kitty_disambiguate);
    tcase_add_test (tc_core, test_kitty_all_keys);
    tcase_add_test (tc_core, test_kitty_event_encoding);
    tcase_add_test (tc_core, test_kitty_event_long_text);
    tcase_add_test (tc_core, test_small_buffer_returns_zero);
    tcase_add_test (tc_core, test_copy_self_does_not_loop);
    tcase_add_test (tc_core, test_copy_cycle_does_not_loop);
    tcase_add_test (tc_core, test_copy_chain_beyond_old_depth_limit);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
