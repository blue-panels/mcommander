/*
   Panel plugin mongo for the M-Commander
   Documents as a table of text, for F3

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

#include <config.h>

#include <string.h>

#include <bson/bson.h>

#include "lib/global.h"
#include "lib/text_table.h"

#include "mongo_render.h"

/*** file scope macros ***************************************************************************/

/* a cell is made up to about this many bytes: more than the table writes, so that its "~" shows
   where the text was cut, and a value of megabytes is not copied */
#define CELL_TEXT_MAX 512

/*** file scope functions ************************************************************************/

static gboolean
is_numeric_type (bson_type_t t)
{
    return t == BSON_TYPE_INT32 || t == BSON_TYPE_INT64 || t == BSON_TYPE_DOUBLE
        || t == BSON_TYPE_DECIMAL128;
}

/* --------------------------------------------------------------------------------------------- */

/* @len bytes of UTF-8 @text, as many as the cell has room for up to CELL_TEXT_MAX */
static void
append_text (GString *out, const char *text, gsize len)
{
    gsize room = out->len < CELL_TEXT_MAX ? CELL_TEXT_MAX - out->len : 0;

    if (len > room)
    {
        len = room;
        // not in the middle of a character, which is 4 bytes at most
        for (int i = 0; i < 3 && len > 0 && ((unsigned char) text[len] & 0xC0) == 0x80; i++)
            len--;
    }
    g_string_append_len (out, text, (gssize) len);
}

/* --------------------------------------------------------------------------------------------- */

static void append_value (GString *out, const bson_value_t *v, gboolean nested);

/* {a: 1, b: "x"} or [1, "x"], up to about CELL_TEXT_MAX */
static void
append_nested (GString *out, const bson_value_t *v)
{
    gboolean array = v->value_type == BSON_TYPE_ARRAY;
    bson_t sub;
    bson_iter_t it;
    gboolean first = TRUE;

    g_string_append_c (out, array ? '[' : '{');
    if (bson_init_static (&sub, v->value.v_doc.data, v->value.v_doc.data_len)
        && bson_iter_init (&it, &sub))
        while (bson_iter_next (&it))
        {
            if (out->len >= CELL_TEXT_MAX)
                break;
            if (!first)
                g_string_append (out, ", ");
            first = FALSE;
            if (!array)
            {
                append_text (out, bson_iter_key (&it), bson_iter_key_len (&it));
                g_string_append (out, ": ");
            }
            append_value (out, bson_iter_value (&it), TRUE);
        }
    g_string_append_c (out, array ? ']' : '}');
}

/* --------------------------------------------------------------------------------------------- */

/* A string is quoted inside a document or an array only */
static void
append_value (GString *out, const bson_value_t *v, gboolean nested)
{
    char buf[64];

    switch (v->value_type)
    {
    case BSON_TYPE_UTF8:
        if (nested)
            g_string_append_c (out, '"');
        append_text (out, v->value.v_utf8.str, v->value.v_utf8.len);
        if (nested)
            g_string_append_c (out, '"');
        break;
    case BSON_TYPE_DOCUMENT:
    case BSON_TYPE_ARRAY:
        append_nested (out, v);
        break;
    case BSON_TYPE_DATE_TIME:
    {
        gint64 secs = v->value.v_datetime / 1000;
        GDateTime *dt;

        // the second the moment is in, also before 1970
        if (v->value.v_datetime % 1000 < 0)
            secs--;
        dt = g_date_time_new_from_unix_utc (secs);

        if (dt != NULL)
        {
            char *s = g_date_time_format (dt, "%Y-%m-%d %H:%M:%S");

            g_string_append (out, s);
            g_free (s);
            g_date_time_unref (dt);
        }
        else
            g_string_append_printf (out, "Date(%" PRId64 ")", v->value.v_datetime);
        break;
    }
    case BSON_TYPE_DECIMAL128:
    {
        char dec[BSON_DECIMAL128_STRING];

        bson_decimal128_to_string (&v->value.v_decimal128, dec);
        g_string_append (out, dec);
        break;
    }
    default:
        mongo_render_value (v, buf, sizeof (buf));
        g_string_append (out, buf);
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* A value as the text of a cell */
static char *
cell_text (const bson_value_t *v)
{
    GString *out = g_string_new (NULL);

    append_value (out, v, FALSE);
    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
mongo_table_render (const GPtrArray *docs, const char *caption, int screen_width)
{
    mc_text_table_t *table;
    GHashTable *index;  // name of a field -> 1 + its column
    GArray *set_in;     // per column: 1 + the row its cell was set in
    char *out;

    table = mc_text_table_new ();
    index = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    set_in = g_array_new (FALSE, TRUE, sizeof (guint));

    for (guint r = 0; r < docs->len; r++)
    {
        bson_iter_t it;

        mc_text_table_add_row (table);
        if (!bson_iter_init (&it, (const bson_t *) g_ptr_array_index (docs, r)))
            continue;
        while (bson_iter_next (&it))
        {
            const char *key = bson_iter_key (&it);
            gpointer pos = g_hash_table_lookup (index, key);
            int col;
            char *text;

            // the columns: the fields of the first level, in the order they are first met
            if (pos == NULL)
            {
                col = mc_text_table_add_column (table, key);
                g_hash_table_insert (index, g_strdup (key), GINT_TO_POINTER (col + 1));
                g_array_set_size (set_in, (guint) col + 1);
            }
            else
                col = GPOINTER_TO_INT (pos) - 1;

            // a key met twice in a document keeps its first value
            if (g_array_index (set_in, guint, col) == r + 1)
                continue;
            g_array_index (set_in, guint, col) = r + 1;

            text = cell_text (bson_iter_value (&it));
            mc_text_table_set_cell (table, col, text, -1, is_numeric_type (bson_iter_type (&it)));
            g_free (text);
        }
    }

    out = mc_text_table_render (table, caption, screen_width);
    g_array_free (set_in, TRUE);
    g_hash_table_destroy (index);
    mc_text_table_free (table);
    return out;
}

/* --------------------------------------------------------------------------------------------- */
