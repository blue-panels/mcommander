/*
   mcstruct - value formatting for STL fields

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

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

#include <ctype.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "lib/global.h"

#include "slv.h"
#include "slv_internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static guint64
read_uint (const unsigned char *b, int size, gboolean big_endian)
{
    guint64 v = 0;
    int i;

    if (big_endian)
        for (i = 0; i < size; i++)
            v = (v << 8) | b[i];
    else
        for (i = size - 1; i >= 0; i--)
            v = (v << 8) | b[i];
    return v;
}

/* --------------------------------------------------------------------------------------------- */

static gint64
sign_extend (guint64 v, int size)
{
    if (size >= 8)
        return (gint64) v;
    {
        guint64 sign = (guint64) 1 << (size * 8 - 1);

        return (gint64) ((v ^ sign) - sign);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
append_text (GString *out, const unsigned char *b, gsize len)
{
    gsize i;

    for (i = 0; i < len; i++)
    {
        unsigned char c = b[i];

        if (c >= 0x20 && c < 0x7F)
            g_string_append_c (out, (char) c);
        else
            g_string_append_c (out, '.');
    }
}

/* --------------------------------------------------------------------------------------------- */

/* text in the item's #encoding, converted to UTF-8; control characters become '.' */
static void
append_encoded (GString *out, const slv_item_t *item, const unsigned char *b, gsize len)
{
    char *conv;
    gsize out_len = 0;
    const char *p, *end;

    if (item->encoding == NULL)
    {
        append_text (out, b, len);
        return;
    }
    conv =
        g_convert ((const char *) b, (gssize) len, "UTF-8", item->encoding, NULL, &out_len, NULL);
    if (conv == NULL)
    {
        append_text (out, b, len);
        return;
    }
    for (p = conv, end = conv + out_len; p < end;)
    {
        gunichar u = g_utf8_get_char_validated (p, end - p);

        if (u == (gunichar) -1 || u == (gunichar) -2)
        {
            g_string_append_c (out, '.');
            p++;
            continue;
        }
        if (u < 0x20 || u == 0x7F)
            g_string_append_c (out, '.');
        else
            g_string_append_unichar (out, u);
        p = g_utf8_next_char (p);
    }
    g_free (conv);
}

/* --------------------------------------------------------------------------------------------- */

static void
format_dos_time (GString *out, guint32 v)
{
    unsigned int t = v & 0xFFFF, d = v >> 16;

    g_string_append_printf (out, "%04u-%02u-%02u %02u:%02u:%02u", 1980 + (d >> 9), (d >> 5) & 15,
                            d & 31, t >> 11, (t >> 5) & 63, (t & 31) * 2);
}

/* --------------------------------------------------------------------------------------------- */

static void
format_unix_time (GString *out, gint64 v)
{
    time_t t = (time_t) v;
    struct tm tm;

    if (gmtime_r (&t, &tm) == NULL)
    {
        g_string_append_printf (out, "%lld", (long long) v);
        return;
    }
    g_string_append_printf (out, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1,
                            tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

/* --------------------------------------------------------------------------------------------- */

static double
msbin_to_double (const unsigned char *b, int size)
{
    int exp;
    guint64 mant;
    gboolean neg;
    double v;

    if (size == 4)
    {
        exp = b[3];
        neg = (b[2] & 0x80) != 0;
        mant = ((guint64) (b[2] & 0x7F) << 16) | ((guint64) b[1] << 8) | b[0];
        if (exp == 0)
            return 0.0;
        v = 1.0 + (double) mant / (double) (1 << 23);
        v = ldexp (v, exp - 129);
    }
    else
    {
        int i;

        exp = b[7];
        neg = (b[6] & 0x80) != 0;
        mant = b[6] & 0x7F;
        for (i = 5; i >= 0; i--)
            mant = (mant << 8) | b[i];
        if (exp == 0)
            return 0.0;
        v = 1.0 + (double) mant / ldexp (1.0, 55);
        v = ldexp (v, exp - 129);
    }
    return neg ? -v : v;
}

/* --------------------------------------------------------------------------------------------- */

static double
float_value (const unsigned char *b, int size, gboolean big_endian)
{
    unsigned char le[10];

    if (big_endian && size <= (int) sizeof (le))
    {
        int i;

        for (i = 0; i < size; i++)
            le[i] = b[size - 1 - i];
        b = le;
    }
    if (size == 4)
    {
        float f;

        memcpy (&f, b, 4);
        return f;
    }
    if (size == 8)
    {
        double d;

        memcpy (&d, b, 8);
        return d;
    }
#if defined(__i386__) || defined(__x86_64__)
    if (size == 10 && sizeof (long double) >= 10)
    {
        long double ld = 0;

        memcpy (&ld, b, 10);
        return (double) ld;
    }
#endif
    return 0.0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* SQLite varint: 7 bits per byte, high bit means more, the 9th byte is a full byte.
   LEB128: 7 bits per byte little-endian. Returns the value of the @len bytes. */
gint64
slv_varint_value (gboolean leb128, const unsigned char *buf, gsize len)
{
    guint64 v = 0;
    gsize i;

    if (leb128)
    {
        for (i = 0; i < len && i < 10; i++)
            v |= (guint64) (buf[i] & 0x7F) << (7 * i);
        return (gint64) v;
    }
    for (i = 0; i < len && i < 8; i++)
        v = (v << 7) | (buf[i] & 0x7F);
    if (len >= 9)
        v = (v << 8) | buf[8];
    return (gint64) v;
}

/* --------------------------------------------------------------------------------------------- */

/* how many bytes the varint at @buf takes, @avail bytes available; 0 when unterminated */
gsize
slv_varint_size (int kind, const unsigned char *buf, gsize avail)
{
    gsize i, max = kind != 0 ? 10 : 9;

    for (i = 0; i < avail && i < max; i++)
    {
        gboolean last = (buf[i] & 0x80) == 0;

        if (kind == 2)
            last = !last;
        if (last || (kind == 0 && i == 8))
            return i + 1;
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* zlib polynomial; start with 0xFFFFFFFF and xor the result with 0xFFFFFFFF */
guint32
slv_crc32 (guint32 crc, const unsigned char *buf, gsize len)
{
    static guint32 table[256];
    static gboolean init = FALSE;
    gsize i;

    if (!init)
    {
        guint32 c;
        int n, k;

        for (n = 0; n < 256; n++)
        {
            c = (guint32) n;
            for (k = 0; k < 8; k++)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        init = TRUE;
    }
    for (i = 0; i < len; i++)
        crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

/* --------------------------------------------------------------------------------------------- */

const char *
slv_type_name (slv_type_t type, int size)
{
    switch (type)
    {
    case SLV_TYPE_CHAR:
        return "c";
    case SLV_TYPE_HEX:
        return size == 1 ? "b" : size == 2 ? "w" : size == 4 ? "d" : "q";
    case SLV_TYPE_INT:
        return size == 1 ? "i8" : size == 2 ? "i16" : size == 4 ? "i32" : "i64";
    case SLV_TYPE_UINT:
        return size == 1 ? "u8" : size == 2 ? "u16" : size == 4 ? "u32" : "u64";
    case SLV_TYPE_BITS:
        return size == 1 ? "t8" : size == 2 ? "t16" : size == 4 ? "t32" : "t64";
    case SLV_TYPE_BE_HEX:
        return size == 1 ? "m8" : size == 2 ? "m16" : size == 4 ? "m32" : "m64";
    case SLV_TYPE_FLOAT:
        return size == 4 ? "f32" : size == 8 ? "f64" : "f80";
    case SLV_TYPE_MSBIN:
        return size == 4 ? "e32" : "e64";
    case SLV_TYPE_PTR:
        return size == 4 ? "p32" : "p48";
    case SLV_TYPE_PSTRING:
        return "sp";
    case SLV_TYPE_CSTRING:
        return "sc";
    case SLV_TYPE_TIME_DOS:
        return "td";
    case SLV_TYPE_TIME_UNIX:
        return size == 8 ? "tu64" : "tu";
    case SLV_TYPE_TIME_FILE:
        return "tf";
    case SLV_TYPE_JUMP:
        return size == 1 ? "j8" : size == 2 ? "j16" : size == 4 ? "j32" : "j64";
    case SLV_TYPE_STR8:
        return "s8";
    case SLV_TYPE_STR16:
        return "s16";
    case SLV_TYPE_STR16Z:
        return "sz16";
    case SLV_TYPE_CHECK:
        return "check";
    case SLV_TYPE_VARINT:
        return "v";
    case SLV_TYPE_LEB128:
        return "vl";
    case SLV_TYPE_ZVARINT:
        return "vz";
    default:
        return "?";
    }
}

/* --------------------------------------------------------------------------------------------- */

/* bits of a little-endian value, most significant first, groups separated by '.'
   split: "" or "1" = no groups, "0" = every bit, "745" = 7+4+5 (A = 10 ... Z = 35) */
char *
slv_format_bits (const unsigned char *buf, int size, gboolean big_endian, gboolean lsb_first,
                 const char *split)
{
    GString *out;
    guint64 v;
    int nbits = size * 8, i;
    int group_left = -1;
    const char *sp = split != NULL ? split : "";

    v = read_uint (buf, size, big_endian);
    out = g_string_sized_new (nbits + 8);

    if (strcmp (sp, "0") == 0)
        group_left = 1;
    else if (*sp != '\0' && strcmp (sp, "1") != 0)
    {
        group_left =
            isdigit ((unsigned char) *sp) ? *sp - '0' : toupper ((unsigned char) *sp) - 'A' + 10;
        sp++;
    }
    else
        sp = "";

    for (i = nbits - 1; i >= 0; i--)
    {
        int bit = lsb_first ? nbits - 1 - i : i;

        g_string_append_c (out, (v >> bit) & 1 ? '1' : '0');
        if (group_left > 0)
        {
            group_left--;
            if (group_left == 0 && i > 0)
            {
                g_string_append_c (out, '.');
                if (strcmp (split, "0") == 0)
                    group_left = 1;
                else if (*sp != '\0')
                {
                    group_left = isdigit ((unsigned char) *sp)
                        ? *sp - '0'
                        : toupper ((unsigned char) *sp) - 'A' + 10;
                    sp++;
                }
                else
                    group_left = -1;
            }
        }
    }
    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

char *
slv_format_legend (const slv_def_t *legend, gint64 value)
{
    GString *out = NULL;
    guint i;

    if (legend == NULL || legend->entries == NULL)
        return NULL;

    for (i = 0; i < legend->entries->len; i++)
    {
        const slv_legend_entry_t *ent = &g_array_index (legend->entries, slv_legend_entry_t, i);
        gboolean hit;

        if (legend->kind == SLV_DEF_BIT_LEGEND)
            hit = (value & ent->min) == ent->max;
        else
            hit = value >= ent->min && value <= ent->max;
        if (!hit)
            continue;
        if (legend->kind != SLV_DEF_BIT_LEGEND)
            return g_strdup (ent->text);
        if (out == NULL)
            out = g_string_new (ent->text);
        else
        {
            g_string_append (out, ", ");
            g_string_append (out, ent->text);
        }
    }
    return out != NULL ? g_string_free (out, FALSE) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

char *
slv_format_value (const slv_item_t *item, const unsigned char *buf, gsize len,
                  const char *float_format, gint64 *first_value, double *dvalue)
{
    return slv_format_value_endian (item, item->big_endian, buf, len, float_format, first_value,
                                    dvalue);
}

/* --------------------------------------------------------------------------------------------- */

static void
append_utf8 (GString *out, const char *text, gsize len)
{
    if (len <= G_MAXSSIZE && g_utf8_validate (text, (gssize) len, NULL))
    {
        g_string_append_len (out, text, len);
        return;
    }
    append_text (out, (const unsigned char *) text, len);
}

/* --------------------------------------------------------------------------------------------- */

/* buf holds the whole field: count * size bytes (or the string bytes) */
char *
slv_format_value_endian (const slv_item_t *item, gboolean big_endian, const unsigned char *buf,
                         gsize len, const char *float_format, gint64 *first_value, double *dvalue)
{
    GString *out = g_string_new (NULL);
    int size = item->size > 0 ? item->size : 1;
    gsize n, i;
    const char *ff = float_format != NULL ? float_format : "%g";

    *first_value = 0;
    *dvalue = 0.0;

    switch (item->type)
    {
    case SLV_TYPE_VARINT:
    case SLV_TYPE_LEB128:
    case SLV_TYPE_ZVARINT:
    {
        gint64 v = slv_varint_value (item->type != SLV_TYPE_VARINT, buf, len);

        g_string_append_printf (out, "%lld", (long long) v);
        *first_value = v;
        break;
    }

    case SLV_TYPE_STR8:
    {
        gsize l = 0;

        while (l < len && buf[l] != '\0')
            l++;
        if (item->encoding != NULL)
            append_encoded (out, item, buf, l);
        else
            append_utf8 (out, (const char *) buf, l);
        *first_value = (gint64) l;
        break;
    }

    case SLV_TYPE_STR16:
    case SLV_TYPE_STR16Z:
    {
        gsize units = len / 2, l = 0;
        gunichar2 *u;
        char *conv;

        u = g_new (gunichar2, units + 1);
        for (i = 0; i < units; i++)
            u[i] = (gunichar2) (buf[2 * i] | (buf[2 * i + 1] << 8));
        while (l < units && u[l] != 0)
            l++;
        conv = g_utf16_to_utf8 (u, (glong) l, NULL, NULL, NULL);
        if (conv != NULL)
            g_string_append (out, conv);
        else
            append_text (out, buf, len);
        g_free (conv);
        g_free (u);
        *first_value = (gint64) l;
        break;
    }

    case SLV_TYPE_CHAR:
        append_encoded (out, item, buf, len);
        if (len > 0)
            *first_value = buf[0];
        break;

    case SLV_TYPE_PSTRING:
        if (len > 0)
        {
            gsize l = buf[0];

            if (l > len - 1)
                l = len - 1;
            append_encoded (out, item, buf + 1, l);
            *first_value = buf[0];
        }
        break;

    case SLV_TYPE_CSTRING:
    {
        gsize l = 0;

        while (l < len && buf[l] != (unsigned char) item->terminator)
            l++;
        append_encoded (out, item, buf, l);
        *first_value = (gint64) l;
        break;
    }

    case SLV_TYPE_BITS:
    {
        char *bits = slv_format_bits (buf, size, big_endian, item->lsb_first, item->bits);

        g_string_append (out, bits);
        g_free (bits);
        *first_value = (gint64) read_uint (buf, size, big_endian);
        break;
    }

    default:
        n = len / size;
        for (i = 0; i < n; i++)
        {
            const unsigned char *b = buf + i * size;

            if (i > 0)
                g_string_append_c (out, ' ');
            switch (item->type)
            {
            case SLV_TYPE_HEX:
            case SLV_TYPE_BE_HEX:
            case SLV_TYPE_JUMP:
            {
                guint64 v = read_uint (b, size, big_endian);

                g_string_append_printf (out, "%0*llX", size * 2, (unsigned long long) v);
                if (i == 0)
                    *first_value = (gint64) v;
                break;
            }
            case SLV_TYPE_UINT:
            {
                guint64 v = read_uint (b, size, big_endian);

                g_string_append_printf (out, "%llu", (unsigned long long) v);
                if (i == 0)
                    *first_value = (gint64) v;
                break;
            }
            case SLV_TYPE_INT:
            {
                gint64 v = sign_extend (read_uint (b, size, big_endian), size);

                g_string_append_printf (out, "%lld", (long long) v);
                if (i == 0)
                    *first_value = v;
                break;
            }
            case SLV_TYPE_PTR:
                if (size == 4)
                    g_string_append_printf (out, "%04X:%04X",
                                            (unsigned) read_uint (b + 2, 2, FALSE),
                                            (unsigned) read_uint (b, 2, FALSE));
                else
                    g_string_append_printf (out, "%04X:%08X",
                                            (unsigned) read_uint (b + 4, 2, FALSE),
                                            (unsigned) read_uint (b, 4, FALSE));
                if (i == 0)
                    *first_value = (gint64) read_uint (b, size == 4 ? 2 : 4, FALSE);
                break;
            case SLV_TYPE_TIME_DOS:
                format_dos_time (out, (guint32) read_uint (b, 4, big_endian));
                if (i == 0)
                    *first_value = (gint64) read_uint (b, 4, big_endian);
                break;
            case SLV_TYPE_TIME_UNIX:
                format_unix_time (out, (gint64) read_uint (b, size, big_endian));
                if (i == 0)
                    *first_value = (gint64) read_uint (b, size, big_endian);
                break;
            case SLV_TYPE_TIME_FILE:
            {
                guint64 ft = read_uint (b, 8, big_endian);

                /* 100 ns units since 1601-01-01; 0 is "no time" */
                if (ft == 0)
                    g_string_append (out, "0");
                else
                    format_unix_time (out, (gint64) (ft / 10000000) - 11644473600LL);
                if (i == 0)
                    *first_value = (gint64) ft;
                break;
            }
            case SLV_TYPE_FLOAT:
            case SLV_TYPE_MSBIN:
            {
                double d = item->type == SLV_TYPE_FLOAT ? float_value (b, size, big_endian)
                                                        : msbin_to_double (b, size);

                g_string_append_printf (out, ff, d);
                if (i == 0)
                {
                    *dvalue = d;
                    *first_value = (gint64) d;
                }
                break;
            }
            default:
                g_string_append (out, "?");
                break;
            }
        }
        break;
    }

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

char *
slv_node_dump (const slv_node_t *node, int indent)
{
    GString *out = g_string_new (NULL);
    guint i;

    {
        g_string_append_printf (out, "%08llX %*s", (unsigned long long) node->offset, indent * 2,
                                "");
        switch (node->kind)
        {
        case SLV_NODE_STRUCT:
            g_string_append_printf (out, "/%s", node->def != NULL ? node->def->name : "");
            break;
        case SLV_NODE_ERROR:
            g_string_append_printf (out, "ERROR %s", node->text != NULL ? node->text : "");
            break;
        case SLV_NODE_REMARK:
            g_string_append_printf (out, ": %s", node->key != NULL ? node->key : "");
            break;
        default:
            g_string_append_printf (out, "%-16s %-6s %s", node->key != NULL ? node->key : "",
                                    node->hint != NULL ? node->hint : "",
                                    node->text != NULL ? node->text : "");
            if (node->legend != NULL)
                g_string_append_printf (out, " (%s)", node->legend);
            if (node->kind == SLV_NODE_JUMP)
                g_string_append_printf (out, " -> %08llX", (unsigned long long) node->jump_target);
            if (node->lazy)
                g_string_append (out, " ...");
            break;
        }
        g_string_append_c (out, '\n');
    }

    if (node->children != NULL)
        for (i = 0; i < node->children->len; i++)
        {
            char *s = slv_node_dump (g_ptr_array_index (node->children, i), indent + 1);

            g_string_append (out, s);
            g_free (s);
        }
    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

char *
slv_format_unix_time (gint64 v)
{
    GString *out = g_string_new (NULL);

    format_unix_time (out, v);
    return g_string_free (out, FALSE);
}
