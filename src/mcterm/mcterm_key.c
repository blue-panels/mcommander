/*
   Midnight Commander - mcterm key encoding.

   Translates MC keycodes back to terminal byte sequences for forwarding
   to the PTY child process.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

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

#include <config.h>

#include <string.h>

#include "lib/global.h"
#include "lib/mcconfig.h"
#include "lib/terminal.h"
#include "lib/tty/key.h"

#include "mcterm_key.h"

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static GHashTable *mcterm_enc_map = NULL;

/* What xterm sends for the keys it has one sequence for. The [terminal:xterm] lists are what
   outer terminals may send, and the user's own ini adds learned keys to them. */
static const struct
{
    int key;
    const char *normal;
    const char *app;  // with the cursor keys in application mode
} xterm_keys[] = {
    { KEY_UP, "\x1b[A", "\x1bOA" },
    { KEY_DOWN, "\x1b[B", "\x1bOB" },
    { KEY_RIGHT, "\x1b[C", "\x1bOC" },
    { KEY_LEFT, "\x1b[D", "\x1bOD" },
    { KEY_HOME, "\x1b[H", "\x1bOH" },
    { KEY_END, "\x1b[F", "\x1bOF" },
    { KEY_IC, "\x1b[2~", NULL },
    { KEY_DC, "\x1b[3~", NULL },
    { KEY_PPAGE, "\x1b[5~", NULL },
    { KEY_NPAGE, "\x1b[6~", NULL },
    { KEY_F (1), "\x1bOP", NULL },
    { KEY_F (2), "\x1bOQ", NULL },
    { KEY_F (3), "\x1bOR", NULL },
    { KEY_F (4), "\x1bOS", NULL },
    { KEY_F (5), "\x1b[15~", NULL },
    { KEY_F (6), "\x1b[17~", NULL },
    { KEY_F (7), "\x1b[18~", NULL },
    { KEY_F (8), "\x1b[19~", NULL },
    { KEY_F (9), "\x1b[20~", NULL },
    { KEY_F (10), "\x1b[21~", NULL },
    { KEY_F (11), "\x1b[23~", NULL },
    { KEY_F (12), "\x1b[24~", NULL },
    { KEY_M_SHIFT | '\t', "\x1b[Z", NULL },
};

/*** file scope functions ************************************************************************/

static void
mcterm_remember_sequence (int key_code, char *raw)
{
    if (raw != NULL && *raw != '\0')
        g_hash_table_replace (mcterm_enc_map, GINT_TO_POINTER (tty_normalize_keycode (key_code)),
                              raw);
    else
        g_free (raw);
}

/* --------------------------------------------------------------------------------------------- */

static size_t
mcterm_copy_seq (unsigned char *buf, size_t bufsz, const char *seq)
{
    size_t len;

    if (seq == NULL)
        return 0;

    len = strlen (seq);
    if (len == 0 || len > bufsz)
        return 0;

    memcpy (buf, seq, len);
    return len;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_load_section_rec (const char *terminal, mc_config_t *cfg, GHashTable *visited)
{
    char *section_name;
    gchar **profile_keys, **keys;

    if (terminal == NULL || cfg == NULL || g_hash_table_contains (visited, terminal))
        return;

    g_hash_table_add (visited, g_strdup (terminal));

    section_name = g_strconcat ("terminal:", terminal, (char *) NULL);
    keys = mc_config_get_keys (cfg, section_name, NULL);

    for (profile_keys = keys; *profile_keys != NULL; profile_keys++)
    {
        if (g_ascii_strcasecmp (*profile_keys, "copy") == 0)
        {
            char *valcopy = mc_config_get_string (cfg, section_name, *profile_keys, "");
            mcterm_load_section_rec (valcopy, cfg, visited);
            g_free (valcopy);
            continue;
        }

        {
            int key_code = tty_keyname_to_keycode (*profile_keys, NULL);

            if (key_code != 0)
            {
                gchar **values = mc_config_get_string_list (cfg, section_name, *profile_keys, NULL);

                if (values != NULL)
                {
                    /* A list means decoder aliases; the encoder sends one
                       sequence -- use the first value as the canonical one. */
                    char *raw = convert_controls (values[0]);

                    mcterm_remember_sequence (key_code, raw);
                    g_strfreev (values);
                }
                else
                {
                    char *value = mc_config_get_string (cfg, section_name, *profile_keys, "");
                    char *raw = convert_controls (value);

                    g_free (value);
                    mcterm_remember_sequence (key_code, raw);
                }
            }
        }
    }

    g_strfreev (keys);
    g_free (section_name);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_load_terminal (mc_config_t *cfg)
{
    GHashTable *visited;

    /* Load both base and 256-colour variant under one visited set so that
       if xterm-256color has copy=xterm, the xterm section is not walked twice. */
    visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    mcterm_load_section_rec ("xterm", cfg, visited);
    mcterm_load_section_rec ("xterm-256color", cfg, visited);
    g_hash_table_destroy (visited);
}

/* --------------------------------------------------------------------------------------------- */

/* Modified cursor and editing keys use xterm CSI sequences. */
static size_t
mcterm_encode_modified_key (int key, unsigned char *buf, size_t bufsz)
{
    const int mods = key & KEY_M_MASK;
    const int base = key & ~KEY_M_MASK;
    const char *num = NULL;
    char final = '\0';
    char seq[16];
    int m;

    if (mods == 0)
        return 0;

    switch (base)
    {
    case KEY_UP:
        final = 'A';
        break;
    case KEY_DOWN:
        final = 'B';
        break;
    case KEY_RIGHT:
        final = 'C';
        break;
    case KEY_LEFT:
        final = 'D';
        break;
    case KEY_HOME:
        final = 'H';
        break;
    case KEY_END:
        final = 'F';
        break;
    case KEY_IC:
        num = "2";
        break;
    case KEY_DC:
        num = "3";
        break;
    case KEY_PPAGE:
        num = "5";
        break;
    case KEY_NPAGE:
        num = "6";
        break;
    default:
        return 0;
    }

    m = 1 + ((mods & KEY_M_SHIFT) != 0 ? 1 : 0) + ((mods & KEY_M_ALT) != 0 ? 2 : 0)
        + ((mods & KEY_M_CTRL) != 0 ? 4 : 0);

    if (num != NULL)
        g_snprintf (seq, sizeof (seq), "\x1b[%s;%d~", num, m);
    else
        g_snprintf (seq, sizeof (seq), "\x1b[1;%d%c", m, final);

    return mcterm_copy_seq (buf, bufsz, seq);
}

/* --------------------------------------------------------------------------------------------- */

static size_t
mcterm_copy_enc_seq (int key, unsigned char *buf, size_t bufsz)
{
    const char *raw;

    if (mcterm_enc_map == NULL)
        return 0;

    raw = g_hash_table_lookup (mcterm_enc_map, GINT_TO_POINTER (tty_normalize_keycode (key)));
    return mcterm_copy_seq (buf, bufsz, raw);
}

/* --------------------------------------------------------------------------------------------- */
/* The kitty keyboard protocol, for a program that asked for it with CSI > flags u */

#define KITTY_DISAMBIGUATE 0x01
#define KITTY_ALTERNATES   0x04
#define KITTY_ALL_KEYS     0x08
#define KITTY_TEXT         0x10

#define KITTY_KEY_F13      57376

static int
mcterm_kitty_mods (int mods)
{
    return 1 + ((mods & KEY_M_SHIFT) != 0 ? 1 : 0) + ((mods & KEY_M_ALT) != 0 ? 2 : 0)
        + ((mods & KEY_M_CTRL) != 0 ? 4 : 0);
}

/* --------------------------------------------------------------------------------------------- */
/* CSI key[:shifted] ; mods ; text u */

static size_t
mcterm_kitty_csi_u (unsigned char *buf, size_t bufsz, guint flags, gunichar code, gunichar shifted,
                    int mods, gunichar text)
{
    GString *seq;
    const int m = mcterm_kitty_mods (mods);
    size_t n;

    seq = g_string_new ("\x1b[");
    g_string_append_printf (seq, "%u", (unsigned int) code);
    if ((flags & KITTY_ALTERNATES) != 0 && shifted != 0 && shifted != code)
        g_string_append_printf (seq, ":%u", (unsigned int) shifted);
    if ((flags & KITTY_TEXT) != 0 && (flags & KITTY_ALL_KEYS) != 0 && text != 0)
        g_string_append_printf (seq, ";%d;%u", m, (unsigned int) text);
    else if (m != 1)
        g_string_append_printf (seq, ";%d", m);
    g_string_append_c (seq, 'u');

    n = mcterm_copy_seq (buf, bufsz, seq->str);
    g_string_free (seq, TRUE);
    return n;
}

/* --------------------------------------------------------------------------------------------- */
/* Function and cursor keys: the legacy forms with the modifiers, F3 as CSI 13 ~ and F13 and up
   as CSI u. Returns 0 for a key that is not one of them. */

static size_t
mcterm_kitty_function_key (int key, unsigned char *buf, size_t bufsz, gboolean app_cursor)
{
    static const int tilde_f[] = { 0, 0, 0, 13, 0, 15, 17, 18, 19, 20, 21, 23, 24 };
    const int mods = key & KEY_M_MASK;
    const int base = key & ~KEY_M_MASK;
    const int m = mcterm_kitty_mods (mods);
    char seq[24];
    char final = '\0';
    int num = 0;

    if (base >= KEY_F (13) && base <= KEY_F (35))
        return mcterm_kitty_csi_u (buf, bufsz, 0, (gunichar) (KITTY_KEY_F13 + base - KEY_F (13)), 0,
                                   mods, 0);

    if (base >= KEY_F (1) && base <= KEY_F (12))
    {
        const int f = base - KEY_F (1) + 1;

        if (f == 1 || f == 2 || f == 4)
        {
            final = f == 1 ? 'P' : (f == 2 ? 'Q' : 'S');
            if (m == 1)
            {
                g_snprintf (seq, sizeof (seq), "\x1bO%c", final);
                return mcterm_copy_seq (buf, bufsz, seq);
            }
        }
        else
            num = tilde_f[f];
    }
    else
        switch (base)
        {
        case KEY_IC:
            num = 2;
            break;
        case KEY_DC:
            num = 3;
            break;
        case KEY_PPAGE:
            num = 5;
            break;
        case KEY_NPAGE:
            num = 6;
            break;
        case KEY_UP:
        case KEY_DOWN:
        case KEY_RIGHT:
        case KEY_LEFT:
        case KEY_HOME:
        case KEY_END:
            final = base == KEY_UP  ? 'A'
                : base == KEY_DOWN  ? 'B'
                : base == KEY_RIGHT ? 'C'
                : base == KEY_LEFT  ? 'D'
                : base == KEY_HOME  ? 'H'
                                    : 'F';
            if (m == 1)
            {
                g_snprintf (seq, sizeof (seq), "\x1b%c%c", app_cursor ? 'O' : '[', final);
                return mcterm_copy_seq (buf, bufsz, seq);
            }
            break;
        default:
            return 0;
        }

    if (num != 0)
    {
        if (m == 1)
            g_snprintf (seq, sizeof (seq), "\x1b[%d~", num);
        else
            g_snprintf (seq, sizeof (seq), "\x1b[%d;%d~", num, m);
    }
    else
        g_snprintf (seq, sizeof (seq), "\x1b[1;%d%c", m, final);

    return mcterm_copy_seq (buf, bufsz, seq);
}

/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/

void
mcterm_key_table_init (const char *global_config_path, mc_config_t *cfg)
{
    g_clear_pointer (&mcterm_enc_map, g_hash_table_destroy);
    mcterm_enc_map = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

    if (global_config_path != NULL)
    {
        mc_config_t *global_cfg = mc_config_init (global_config_path, TRUE);

        mcterm_load_terminal (global_cfg);
        mc_config_deinit (global_cfg);
    }

    mcterm_load_terminal (cfg);
    /* Do NOT load the outer $TERM key file here: this table encodes keys for
     * the embedded xterm-256color child, not for decoding the outer terminal. */
}

/* --------------------------------------------------------------------------------------------- */

size_t
mcterm_encode_key_xterm (int key, unsigned char *buf, size_t bufsz, gboolean app_cursor)
{
    if (bufsz == 0)
        return 0;

    if ((key & ~0x1F) == KEY_M_CTRL)
        key &= 0x1F;

    /* Ctrl with a digit, as xterm sends it without the kitty protocol */
    if ((key & ~KEY_M_SHIFT) == (KEY_M_CTRL | (key & 0xFF)) && (key & 0xFF) >= '0'
        && (key & 0xFF) <= '9')
    {
        static const unsigned char ctrl_digit[10] = { '0',  '1',  0x00, 0x1B, 0x1C,
                                                      0x1D, 0x1E, 0x1F, 0x7F, '9' };

        buf[0] = ctrl_digit[(key & 0xFF) - '0'];
        return 1;
    }

    if (key == '\n' || key == '\r')
    {
        buf[0] = '\r';
        return 1;
    }

    if (key >= 0x01 && key <= 0x1F) /* C0: Ctrl+A..Z plus Ctrl+\, ], ^, _ and ESC */
    {
        buf[0] = (unsigned char) key;
        return 1;
    }
    if (key == 0x7F)
    {
        buf[0] = 0x7F;
        return 1;
    }
    if (key >= 0x20 && key < 0x80) /* printable ASCII */
    {
        buf[0] = (unsigned char) key;
        return 1;
    }
    if (key >= 0x80 && key <= 0xFF)
    {
        buf[0] = (unsigned char) key;
        return 1;
    }

    if (key == KEY_BACKSPACE)
    {
        buf[0] = 0x7F;
        return 1;
    }
    if (key == KEY_ENTER)
    {
        buf[0] = '\r';
        return 1;
    }

    {
        const int plain = tty_normalize_keycode (key);
        size_t i;

        for (i = 0; i < G_N_ELEMENTS (xterm_keys); i++)
            if (xterm_keys[i].key == plain)
                return mcterm_copy_seq (buf, bufsz,
                                        app_cursor && xterm_keys[i].app != NULL
                                            ? xterm_keys[i].app
                                            : xterm_keys[i].normal);
    }

    {
        size_t n = mcterm_copy_enc_seq (key, buf, bufsz);

        if (n == 0)
            n = mcterm_encode_modified_key (key, buf, bufsz);
        if (n > 0)
            return n;
    }

    if ((key & KEY_M_ALT) != 0 && bufsz >= 2)
    {
        size_t n = mcterm_encode_key_xterm (key & ~KEY_M_ALT, buf + 1, bufsz - 1, app_cursor);

        if (n > 0)
        {
            buf[0] = 0x1B;
            return n + 1;
        }
        return 0;
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

size_t
mcterm_encode_kitty_codepoint (gunichar cp, int mods, guint flags, unsigned char *buf, size_t bufsz)
{
    const gunichar code = g_unichar_tolower (cp);
    const gboolean shifted = code != cp;

    if (shifted)
        mods |= KEY_M_SHIFT;
    if ((flags & KITTY_ALL_KEYS) == 0
        && ((flags & KITTY_DISAMBIGUATE) == 0 || (mods & (KEY_M_CTRL | KEY_M_ALT)) == 0))
        return 0;

    return mcterm_kitty_csi_u (buf, bufsz, flags, code, shifted ? cp : 0, mods,
                               (mods & (KEY_M_CTRL | KEY_M_ALT)) == 0 ? cp : 0);
}

/* --------------------------------------------------------------------------------------------- */

size_t
mcterm_encode_key (int key, guint kitty_flags, unsigned char *buf, size_t bufsz,
                   gboolean app_cursor)
{
    int mods = key & KEY_M_MASK;
    int base = key & ~KEY_M_MASK;
    gunichar code = 0;
    size_t n;

    if ((kitty_flags & (KITTY_DISAMBIGUATE | KITTY_ALL_KEYS)) == 0 || bufsz == 0)
        return mcterm_encode_key_xterm (key, buf, bufsz, app_cursor);

    n = mcterm_kitty_function_key (key, buf, bufsz, app_cursor);
    if (n > 0)
        return n;

    if (base == '\n' || base == '\r' || base == KEY_ENTER)
        code = 13;
    else if (base == '\t')
        code = 9;
    else if (base == KEY_BACKSPACE || base == 0x7F)
        code = 127;
    else if (base == ESC_CHAR)
        code = 27;
    else if (base >= 0x00 && base < 0x20)
    {
        /* Ctrl with a letter or one of @ [ \ ] ^ _ came as its control byte */
        mods |= KEY_M_CTRL;
        code = (gunichar) (base == 0 ? ' ' : (base <= 0x1A ? base + 0x60 : base + 0x40));
    }
    else if (base >= 0x20 && base < 0x7F)
    {
        n = mcterm_encode_kitty_codepoint ((gunichar) base, mods, kitty_flags, buf, bufsz);
        return n > 0 ? n : mcterm_encode_key_xterm (key, buf, bufsz, app_cursor);
    }
    else
        return mcterm_encode_key_xterm (key, buf, bufsz, app_cursor);

    /* Enter, Tab and Backspace stay legacy without modifiers unless every key is asked for */
    if ((kitty_flags & KITTY_ALL_KEYS) == 0 && mods == 0 && code != 27)
        return mcterm_encode_key_xterm (key, buf, bufsz, app_cursor);

    return mcterm_kitty_csi_u (buf, bufsz, kitty_flags, code, 0, mods, 0);
}
