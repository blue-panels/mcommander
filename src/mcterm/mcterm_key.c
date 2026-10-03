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

/* Modified cursor and editing keys use xterm CSI sequences. Ctrl with a digit has no
   legacy control byte, so pass its distinct key code as kitty CSI u. */
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

    if ((mods & KEY_M_CTRL) != 0 && base >= '0' && base <= '9')
    {
        m = 1 + ((mods & KEY_M_SHIFT) != 0 ? 1 : 0) + ((mods & KEY_M_ALT) != 0 ? 2 : 0) + 4;
        g_snprintf (seq, sizeof (seq), "\x1b[%d;%du", base, m);
        return mcterm_copy_seq (buf, bufsz, seq);
    }

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

    if ((key & KEY_M_MASK) == 0 && app_cursor)
        switch (key)
        {
        case KEY_END:
            return mcterm_copy_seq (buf, bufsz, "\x1bOF");
        case KEY_UP:
            return mcterm_copy_seq (buf, bufsz, "\x1bOA");
        case KEY_DOWN:
            return mcterm_copy_seq (buf, bufsz, "\x1bOB");
        case KEY_LEFT:
            return mcterm_copy_seq (buf, bufsz, "\x1bOD");
        case KEY_RIGHT:
            return mcterm_copy_seq (buf, bufsz, "\x1bOC");
        case KEY_HOME:
            return mcterm_copy_seq (buf, bufsz, "\x1bOH");
        default:
            break;
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

/* Virtual key and shift state of a printable ASCII character on a US keyboard */
static gboolean
mcterm_far2l_ascii (int c, unsigned int *vk, gboolean *shift)
{
    static const char plain[] = "`-=[]\\;',./";
    static const unsigned int plain_vk[] = { 0xC0, 0xBD, 0xBB, 0xDB, 0xDD, 0xDC,
                                             0xBA, 0xDE, 0xBC, 0xBE, 0xBF };
    static const char shifted[] = "~_+{}|:\"<>?";
    static const char digits_shifted[] = ")!@#$%^&*(";
    const char *p;

    *shift = FALSE;
    if (c >= 'a' && c <= 'z')
        *vk = (unsigned int) (c - 'a' + 'A');
    else if (c >= 'A' && c <= 'Z')
    {
        *vk = (unsigned int) c;
        *shift = TRUE;
    }
    else if (c >= '0' && c <= '9')
        *vk = (unsigned int) c;
    else if (c == ' ')
        *vk = 0x20;
    else if (c > 0 && (p = strchr (digits_shifted, c)) != NULL)
    {
        *vk = (unsigned int) ('0' + (p - digits_shifted));
        *shift = TRUE;
    }
    else if (c > 0 && (p = strchr (plain, c)) != NULL)
        *vk = plain_vk[p - plain];
    else if (c > 0 && (p = strchr (shifted, c)) != NULL)
    {
        *vk = plain_vk[p - shifted];
        *shift = TRUE;
    }
    else
        return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_far2l_put (unsigned char *out, unsigned int size, unsigned int value)
{
    unsigned int i;

    for (i = 0; i < size; i++)
        out[i] = (unsigned char) (value >> (8 * i));
}

/* --------------------------------------------------------------------------------------------- */

size_t
mcterm_encode_key_far2l (int key, unsigned char *buf, size_t bufsz)
{
    /* Windows console key state bits */
    enum
    {
        CS_LEFT_ALT = 0x02,
        CS_LEFT_CTRL = 0x08,
        CS_SHIFT = 0x10,
        CS_ENHANCED = 0x100
    };
    const int base = key & ~KEY_M_MASK;
    unsigned int vk = 0, uc = 0, cs = 0;
    gboolean shift = FALSE;
    size_t n = 0;
    int down;

    if ((key & KEY_M_SHIFT) != 0)
        cs |= CS_SHIFT;
    if ((key & KEY_M_ALT) != 0)
        cs |= CS_LEFT_ALT;
    if ((key & KEY_M_CTRL) != 0)
        cs |= CS_LEFT_CTRL;

    if (base == '\n' || base == '\r' || base == KEY_ENTER)
        vk = uc = 13;
    else if (base == '\t')
        vk = uc = 9;
    else if (base == 27)
        vk = uc = 27;
    else if (base == KEY_BACKSPACE || base == 8 || base == 127)
        vk = uc = 8;
    else if (base >= 1 && base <= 26)
    {
        // Ctrl-A to Ctrl-Z, which mc keeps as the control characters
        vk = (unsigned int) (base - 1 + 'A');
        uc = (unsigned int) base;
        cs |= CS_LEFT_CTRL;
    }
    else if (base >= 28 && base <= 31)
    {
        static const unsigned int ctrl_vk[] = { 0xDC, 0xDD, '6', 0xBD };

        vk = ctrl_vk[base - 28];
        uc = (unsigned int) base;
        cs |= CS_LEFT_CTRL;
    }
    else if (base >= 32 && base < 127 && mcterm_far2l_ascii (base, &vk, &shift))
    {
        uc = (unsigned int) base;
        if (shift)
            cs |= CS_SHIFT;
    }
    else if (base >= KEY_F (1) && base <= KEY_F (20))
    {
        /* F13 to F20 are Shift-F3 to Shift-F10 for mc; F11 and F12 are taken for the keys of
           that name, though Shift-F1 and Shift-F2 come as the same codes */
        const int f = base - KEY_F (1);

        if (f < 12)
            vk = 0x70 + (unsigned int) f;
        else
        {
            vk = 0x70 + (unsigned int) (f - 10);
            cs |= CS_SHIFT;
        }
    }
    else
    {
        switch (base)
        {
        case KEY_LEFT:
            vk = 0x25;
            break;
        case KEY_UP:
            vk = 0x26;
            break;
        case KEY_RIGHT:
            vk = 0x27;
            break;
        case KEY_DOWN:
            vk = 0x28;
            break;
        case KEY_PPAGE:
            vk = 0x21;
            break;
        case KEY_NPAGE:
            vk = 0x22;
            break;
        case KEY_END:
            vk = 0x23;
            break;
        case KEY_HOME:
            vk = 0x24;
            break;
        case KEY_IC:
            vk = 0x2D;
            break;
        case KEY_DC:
            vk = 0x2E;
            break;
        default:
            return 0;
        }
        cs |= CS_ENHANCED;
    }

    if (bufsz < 2 * 32)
        return 0;

    for (down = 1; down >= 0; down--)
    {
        unsigned char stack[16];
        gchar *b64;

        mcterm_far2l_put (stack, 2, 1);  // repeat count
        mcterm_far2l_put (stack + 2, 2, vk);
        mcterm_far2l_put (stack + 4, 2, 0);  // scan code
        mcterm_far2l_put (stack + 6, 4, cs);
        mcterm_far2l_put (stack + 10, 4, uc);
        stack[14] = down != 0 ? 'K' : 'k';

        b64 = g_base64_encode (stack, 15);
        n += (size_t) g_snprintf ((char *) buf + n, bufsz - n, ESC_STR "_f2l:%s\a", b64);
        g_free (b64);
    }

    return n;
}

/* --------------------------------------------------------------------------------------------- */
