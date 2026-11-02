/*
   Library of the M-Commander
   A table of text as psql prints it

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

#include "lib/global.h"
#include "lib/strutil.h"

#include "lib/text_table.h"

/*** file scope macro definitions ****************************************************************/

/* a column is not made narrower than this to fit the screen, unless all its texts are */
#define TABLE_MIN_WIDTH 8

/* a text is written up to about this many bytes */
#define TABLE_TEXT_MAX 256

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *title;       // in the codeset of the terminal, as the cells
    gboolean numeric;  // every cell set in it is a number
} column_t;

struct mc_text_table_t
{
    GArray *columns;  // column_t
    GPtrArray *rows;  // GPtrArray of char *, NULL: an empty cell
};

/*** file scope functions ************************************************************************/

static void
column_clear (gpointer p)
{
    g_free (((column_t *) p)->title);
}

/* --------------------------------------------------------------------------------------------- */

static void
row_free (gpointer p)
{
    g_ptr_array_free ((GPtrArray *) p, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/* @length bytes of UTF-8 @text on one line in the codeset of the terminal, a character it has
   not as "?"; @cut_long: cut with "~" at TABLE_TEXT_MAX */
static char *
cell_text (const char *text, gsize length, gboolean cut_long)
{
    gboolean cut = cut_long && length > TABLE_TEXT_MAX;
    const char *codeset = str_get_codeset ();
    char *valid;
    char *converted = NULL;
    GString *out;

    if (cut)
    {
        length = TABLE_TEXT_MAX;
        // not in the middle of a character, which is 4 bytes at most
        for (int i = 0; i < 3 && ((unsigned char) text[length] & 0xC0) == 0x80; i++)
            length--;
    }
    valid = g_utf8_make_valid (text, (gssize) length);
    for (char *p = valid; *p != '\0'; p++)
        if ((unsigned char) *p < ' ' || *p == '\177')
            *p = ' ';

    if (codeset != NULL && !str_isutf8 (codeset))
        converted = g_convert_with_fallback (valid, -1, codeset, "UTF-8", "?", NULL, NULL, NULL);
    out = g_string_new (converted != NULL ? converted : valid);
    g_free (converted);
    g_free (valid);
    if (cut)
        g_string_append_c (out, '~');
    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static int
total_width (const int *widths, const int *mins, int ncols, int limit)
{
    int total = 3 * ncols - 1;  // " text " and "|" between them

    for (int c = 0; c < ncols; c++)
        total += MAX (mins[c], MIN (widths[c], limit));
    return total;
}

/* --------------------------------------------------------------------------------------------- */

/* Narrow the widest columns, none below its @mins, until a line fits @screen_width: every
   column wider than a limit is cut to it, the limit as high as the screen allows; what is left
   goes, one each, to the last of the columns cut. */
static void
fit_widths (int *widths, const int *mins, int ncols, int screen_width)
{
    int lo = 0, hi = 0;
    int slack;

    for (int c = 0; c < ncols; c++)
        hi = MAX (hi, widths[c]);
    if (total_width (widths, mins, ncols, hi) <= screen_width)
        return;

    // the highest limit that fits, or 0 when even the minimums do not
    while (lo < hi)
    {
        int mid = (lo + hi + 1) / 2;

        if (total_width (widths, mins, ncols, mid) <= screen_width)
            lo = mid;
        else
            hi = mid - 1;
    }

    slack = screen_width - total_width (widths, mins, ncols, lo);
    for (int c = ncols - 1; c >= 0; c--)
    {
        gboolean was_cut = widths[c] > lo && lo >= mins[c];

        widths[c] = MAX (mins[c], MIN (widths[c], lo));
        if (was_cut && slack > 0)
        {
            widths[c]++;
            slack--;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/* A text wider than its column keeps its start and ends with "~" */
static void
append_cell (GString *out, const char *text, int width, gboolean right)
{
    g_string_append_c (out, ' ');
    if (str_term_width1 (text) > width)
    {
        g_string_append (out, str_fit_to_term (text, width - 1, J_LEFT));
        g_string_append_c (out, '~');
    }
    else
        g_string_append (out, str_fit_to_term (text, width, right ? J_RIGHT : J_LEFT));
    g_string_append_c (out, ' ');
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mc_text_table_t *
mc_text_table_new (void)
{
    mc_text_table_t *table = g_new0 (mc_text_table_t, 1);

    table->columns = g_array_new (FALSE, TRUE, sizeof (column_t));
    g_array_set_clear_func (table->columns, column_clear);
    table->rows = g_ptr_array_new_with_free_func (row_free);
    return table;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_text_table_free (mc_text_table_t *table)
{
    if (table == NULL)
        return;
    g_array_free (table->columns, TRUE);
    g_ptr_array_free (table->rows, TRUE);
    g_free (table);
}

/* --------------------------------------------------------------------------------------------- */

int
mc_text_table_add_column (mc_text_table_t *table, const char *title)
{
    column_t column;

    column.title = cell_text (title, strlen (title), TRUE);
    column.numeric = TRUE;
    g_array_append_val (table->columns, column);
    return (int) table->columns->len - 1;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_text_table_add_row (mc_text_table_t *table)
{
    g_ptr_array_add (table->rows, g_ptr_array_new_with_free_func (g_free));
}

/* --------------------------------------------------------------------------------------------- */

guint
mc_text_table_rows (const mc_text_table_t *table)
{
    return table->rows->len;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_text_table_set_cell (mc_text_table_t *table, int column, const char *text, gssize length,
                        gboolean numeric)
{
    GPtrArray *row;

    g_return_if_fail (table->rows->len > 0);
    g_return_if_fail (column >= 0 && (guint) column < table->columns->len);

    row = (GPtrArray *) g_ptr_array_index (table->rows, table->rows->len - 1);
    if ((guint) column >= row->len)
        g_ptr_array_set_size (row, column + 1);
    g_free (g_ptr_array_index (row, column));
    g_ptr_array_index (row, column) =
        cell_text (text, length < 0 ? strlen (text) : (gsize) length, TRUE);
    if (!numeric)
        g_array_index (table->columns, column_t, column).numeric = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

char *
mc_text_table_render (const mc_text_table_t *table, const char *caption, int screen_width)
{
    int ncols = (int) table->columns->len;
    int *widths, *mins;
    GString *out;

    widths = g_new0 (int, ncols);
    mins = g_new0 (int, ncols);
    for (int c = 0; c < ncols; c++)
        widths[c] = str_term_width1 (g_array_index (table->columns, column_t, c).title);
    for (guint r = 0; r < table->rows->len; r++)
    {
        const GPtrArray *row = (const GPtrArray *) g_ptr_array_index (table->rows, r);

        for (guint c = 0; c < row->len; c++)
            if (g_ptr_array_index (row, c) != NULL)
                widths[c] = MAX (widths[c], str_term_width1 (g_ptr_array_index (row, c)));
    }
    for (int c = 0; c < ncols; c++)
        mins[c] = MIN (widths[c], TABLE_MIN_WIDTH);
    if (screen_width > 0)
        fit_widths (widths, mins, ncols, screen_width);

    out = g_string_new (NULL);
    if (caption != NULL)
    {
        // not cut: it is one, and the end of it says which rows these are
        char *line = cell_text (caption, strlen (caption), FALSE);

        g_string_append_printf (out, "-- %s\n\n", line);
        g_free (line);
    }

    for (int c = 0; c < ncols; c++)
    {
        if (c > 0)
            g_string_append_c (out, '|');
        append_cell (out, g_array_index (table->columns, column_t, c).title, widths[c], FALSE);
    }
    g_string_append_c (out, '\n');
    for (int c = 0; c < ncols; c++)
    {
        if (c > 0)
            g_string_append_c (out, '+');
        for (int i = 0; i < widths[c] + 2; i++)
            g_string_append_c (out, '-');
    }
    g_string_append_c (out, '\n');
    for (guint r = 0; r < table->rows->len; r++)
    {
        const GPtrArray *row = (const GPtrArray *) g_ptr_array_index (table->rows, r);

        for (int c = 0; c < ncols; c++)
        {
            const char *text = (guint) c < row->len ? g_ptr_array_index (row, c) : NULL;

            if (c > 0)
                g_string_append_c (out, '|');
            append_cell (out, text != NULL ? text : "", widths[c],
                         g_array_index (table->columns, column_t, c).numeric);
        }
        g_string_append_c (out, '\n');
    }
    g_string_append_printf (out, "(%u %s)\n", table->rows->len,
                            table->rows->len == 1 ? "row" : "rows");

    g_free (widths);
    g_free (mins);
    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
