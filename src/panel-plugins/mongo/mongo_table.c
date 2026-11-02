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
#include "lib/strutil.h"  // str_fit_to_term

#include "mongo_render.h"

/*** file scope macros ***************************************************************************/

/* the widest a column is written, in columns of the terminal */
#define TABLE_CELL_WIDTH 40

/* a cell is written up to about this many bytes, the terminal shows TABLE_CELL_WIDTH anyway */
#define TABLE_TEXT_MAX 256

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *name;
    char *title;       // the name on one line
    gboolean numeric;  // every value of the column is a number
    int width;
} table_column_t;

/*** file scope functions ************************************************************************/

static gboolean
is_numeric_type (bson_type_t t)
{
    return t == BSON_TYPE_INT32 || t == BSON_TYPE_INT64 || t == BSON_TYPE_DOUBLE
        || t == BSON_TYPE_DECIMAL128;
}

/* --------------------------------------------------------------------------------------------- */

/* The control characters of @text are spaces */
static char *
one_line (char *text)
{
    for (char *p = text; *p != '\0'; p++)
        if ((unsigned char) *p < ' ' || *p == '\177')
            *p = ' ';
    return text;
}

/* --------------------------------------------------------------------------------------------- */

/* @len bytes of UTF-8 @text, cut with "..." where the cell reaches TABLE_TEXT_MAX */
static void
append_text (GString *out, const char *text, gsize len)
{
    gsize room = out->len < TABLE_TEXT_MAX ? TABLE_TEXT_MAX - out->len : 0;

    if (len <= room)
    {
        g_string_append_len (out, text, (gssize) len);
        return;
    }
    // not in the middle of a character
    while (room > 0 && ((unsigned char) text[room] & 0xC0) == 0x80)
        room--;
    g_string_append_len (out, text, (gssize) room);
    g_string_append (out, "...");
}

/* --------------------------------------------------------------------------------------------- */

static void append_value (GString *out, const bson_value_t *v, gboolean nested);

/* {a: 1, b: "x"} or [1, "x"], cut with "..." past TABLE_TEXT_MAX */
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
            if (out->len >= TABLE_TEXT_MAX)
            {
                g_string_append (out, ", ...");
                break;
            }
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

/* A value on one line */
static char *
cell_text (const bson_value_t *v)
{
    GString *out = g_string_new (NULL);

    append_value (out, v, FALSE);
    return one_line (g_string_free (out, FALSE));
}

/* --------------------------------------------------------------------------------------------- */

static void
append_cell (GString *out, const char *text, int width, gboolean right)
{
    g_string_append_c (out, ' ');
    g_string_append (out, str_fit_to_term (text, width, right ? J_RIGHT_FIT : J_LEFT_FIT));
    g_string_append_c (out, ' ');
}

/* --------------------------------------------------------------------------------------------- */

static void
column_free (gpointer p)
{
    table_column_t *c = (table_column_t *) p;

    g_free (c->name);
    g_free (c->title);
    g_free (c);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
mongo_table_render (const GPtrArray *docs)
{
    GPtrArray *columns;
    GHashTable *index;  // name of a field -> 1 + its column
    char ***cells;
    GString *out;
    guint ncols;

    columns = g_ptr_array_new_with_free_func (column_free);
    index = g_hash_table_new (g_str_hash, g_str_equal);

    // the columns: the fields of the first level, in the order they are first met
    for (guint r = 0; r < docs->len; r++)
    {
        bson_iter_t it;

        if (!bson_iter_init (&it, (const bson_t *) g_ptr_array_index (docs, r)))
            continue;
        while (bson_iter_next (&it))
        {
            const char *key = bson_iter_key (&it);
            gpointer pos = g_hash_table_lookup (index, key);
            table_column_t *c;

            if (pos == NULL)
            {
                c = g_new0 (table_column_t, 1);
                c->name = g_strdup (key);
                c->title = one_line (g_strdup (key));
                c->numeric = TRUE;
                c->width = MIN (str_term_width1 (c->title), TABLE_CELL_WIDTH);
                g_ptr_array_add (columns, c);
                g_hash_table_insert (index, c->name, GUINT_TO_POINTER (columns->len));
            }
            else
                c = (table_column_t *) g_ptr_array_index (columns, GPOINTER_TO_UINT (pos) - 1);
            if (!is_numeric_type (bson_iter_type (&it)))
                c->numeric = FALSE;
        }
    }
    ncols = columns->len;

    cells = g_new0 (char **, docs->len);
    for (guint r = 0; r < docs->len; r++)
    {
        bson_iter_t it;

        cells[r] = g_new0 (char *, ncols + 1);
        if (!bson_iter_init (&it, (const bson_t *) g_ptr_array_index (docs, r)))
            continue;
        while (bson_iter_next (&it))
        {
            gpointer pos = g_hash_table_lookup (index, bson_iter_key (&it));
            guint col = GPOINTER_TO_UINT (pos) - 1;
            table_column_t *c = (table_column_t *) g_ptr_array_index (columns, col);

            // a key met twice in a document keeps its first value
            if (cells[r][col] != NULL)
                continue;
            cells[r][col] = cell_text (bson_iter_value (&it));
            c->width = MAX (c->width, MIN (str_term_width1 (cells[r][col]), TABLE_CELL_WIDTH));
        }
    }

    out = g_string_new (NULL);
    for (guint col = 0; col < ncols; col++)
    {
        const table_column_t *c = (const table_column_t *) g_ptr_array_index (columns, col);

        if (col > 0)
            g_string_append_c (out, '|');
        append_cell (out, c->title, c->width, FALSE);
    }
    g_string_append_c (out, '\n');
    for (guint col = 0; col < ncols; col++)
    {
        const table_column_t *c = (const table_column_t *) g_ptr_array_index (columns, col);

        if (col > 0)
            g_string_append_c (out, '+');
        for (int i = 0; i < c->width + 2; i++)
            g_string_append_c (out, '-');
    }
    g_string_append_c (out, '\n');
    for (guint r = 0; r < docs->len; r++)
    {
        for (guint col = 0; col < ncols; col++)
        {
            const table_column_t *c = (const table_column_t *) g_ptr_array_index (columns, col);

            if (col > 0)
                g_string_append_c (out, '|');
            append_cell (out, cells[r][col] != NULL ? cells[r][col] : "", c->width, c->numeric);
            g_free (cells[r][col]);
        }
        g_string_append_c (out, '\n');
        g_free (cells[r]);
    }
    g_string_append_printf (out, "(%u %s)\n", docs->len, docs->len == 1 ? "row" : "rows");

    g_free (cells);
    g_hash_table_destroy (index);
    g_ptr_array_free (columns, TRUE);
    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
