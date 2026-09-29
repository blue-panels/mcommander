/*
   Internal file viewer for the Midnight Commander
   Virtual screen buffer for ANSI terminal replay mode.

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

#include "terminal_buffer.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

struct mcview_terminal_buffer_struct
{
    GHashTable *rows;
    GHashTable *wrapped;  // rows that go on in the row below: the soft line breaks
    int max_row;
};

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

static void
cell_attr_from_ansi (mcview_cell_attr_t *attr, const mcview_ansi_state_t *ansi)
{
    attr->fg = ansi->fg;
    attr->bg = ansi->bg;
    attr->bold = ansi->bold;
    attr->italic = ansi->italic;
    attr->underline = ansi->underline;
    attr->blink = ansi->blink;
    attr->reverse = ansi->reverse;
    attr->conceal = ansi->conceal;
}

/* --------------------------------------------------------------------------------------------- */

static GArray *
get_or_create_row (mcview_terminal_buffer_t *buf, int row)
{
    gpointer key = GINT_TO_POINTER (row);
    GArray *arr;

    arr = (GArray *) g_hash_table_lookup (buf->rows, key);
    if (arr == NULL)
    {
        arr = g_array_new (FALSE, TRUE, sizeof (mcview_vterm_cell_t));
        g_hash_table_insert (buf->rows, key, arr);
    }
    return arr;
}

/* --------------------------------------------------------------------------------------------- */

static void
ensure_col (GArray *arr, int col)
{
    if ((int) arr->len <= col)
    {
        mcview_vterm_cell_t empty;
        memset (&empty, 0, sizeof (empty));
        while ((int) arr->len <= col)
            g_array_append_val (arr, empty);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
free_row_array (gpointer data)
{
    g_array_free ((GArray *) data, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_wrapped (mcview_terminal_buffer_t *buf, gpointer key_src, gpointer key_dst)
{
    if (g_hash_table_contains (buf->wrapped, key_src))
        g_hash_table_insert (buf->wrapped, key_dst, GINT_TO_POINTER (1));
    else
        g_hash_table_remove (buf->wrapped, key_dst);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mcview_terminal_buffer_t *
mcview_terminal_buffer_new (void)
{
    mcview_terminal_buffer_t *buf;

    buf = g_new (mcview_terminal_buffer_t, 1);
    buf->rows = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, free_row_array);
    buf->wrapped = g_hash_table_new (g_direct_hash, g_direct_equal);
    buf->max_row = -1;
    return buf;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_free (mcview_terminal_buffer_t *buf)
{
    if (buf == NULL)
        return;
    g_hash_table_destroy (buf->rows);
    g_hash_table_destroy (buf->wrapped);
    g_free (buf);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_clear (mcview_terminal_buffer_t *buf)
{
    g_hash_table_remove_all (buf->rows);
    g_hash_table_remove_all (buf->wrapped);
    buf->max_row = -1;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_put_char (mcview_terminal_buffer_t *buf, int row, int col, gunichar ch,
                                 const mcview_ansi_state_t *ansi)
{
    GArray *arr;
    mcview_vterm_cell_t *cell;

    if (row < 0 || col < 0)
        return;

    arr = get_or_create_row (buf, row);
    ensure_col (arr, col);

    cell = &g_array_index (arr, mcview_vterm_cell_t, col);
    cell->ch = ch;
    cell_attr_from_ansi (&cell->attr, ansi);

    if (row > buf->max_row)
        buf->max_row = row;
}

/* --------------------------------------------------------------------------------------------- */

const mcview_vterm_cell_t *
mcview_terminal_buffer_get (const mcview_terminal_buffer_t *buf, int row, int col)
{
    GArray *arr;

    arr = (GArray *) g_hash_table_lookup (buf->rows, GINT_TO_POINTER (row));
    if (arr == NULL || col < 0 || (int) arr->len <= col)
        return NULL;
    return &g_array_index (arr, mcview_vterm_cell_t, col);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_fill_range (mcview_terminal_buffer_t *buf, int row, int col_from, int col_to,
                                   gunichar ch, const mcview_ansi_state_t *ansi)
{
    GArray *arr;
    mcview_cell_attr_t attr;
    int i;

    if (row < 0 || col_from > col_to || col_to < 0)
        return;

    cell_attr_from_ansi (&attr, ansi);
    arr = get_or_create_row (buf, row);
    ensure_col (arr, col_to);

    for (i = col_from; i <= col_to; i++)
    {
        mcview_vterm_cell_t *cell = &g_array_index (arr, mcview_vterm_cell_t, i);
        cell->ch = ch;
        cell->attr = attr;
    }

    if (row > buf->max_row)
        buf->max_row = row;
}

/* --------------------------------------------------------------------------------------------- */

/* Cells of @row, or NULL when it is empty. Free with g_array_unref(). */
GArray *
mcview_terminal_buffer_row_copy (const mcview_terminal_buffer_t *buf, int row)
{
    return mcview_terminal_buffer_row_copy_n (buf, row, -1);
}

/* --------------------------------------------------------------------------------------------- */

/* The same, at most @max_cells of the row; negative keeps all of it. */
GArray *
mcview_terminal_buffer_row_copy_n (const mcview_terminal_buffer_t *buf, int row, int max_cells)
{
    GArray *src;
    GArray *dst;
    guint len;

    src = (GArray *) g_hash_table_lookup (buf->rows, GINT_TO_POINTER (row));
    if (src == NULL || src->len == 0)
        return NULL;

    len = src->len;
    if (max_cells >= 0 && len > (guint) max_cells)
        len = (guint) max_cells;
    if (len == 0)
        return NULL;

    dst = g_array_new (FALSE, TRUE, sizeof (mcview_vterm_cell_t));
    g_array_append_vals (dst, src->data, len);

    return dst;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_set_row (mcview_terminal_buffer_t *buf, int row, const GArray *cells)
{
    GArray *dst;

    if (row < 0)
        return;

    g_hash_table_remove (buf->rows, GINT_TO_POINTER (row));
    g_hash_table_remove (buf->wrapped, GINT_TO_POINTER (row));

    if (cells == NULL || cells->len == 0)
        return;

    dst = g_array_new (FALSE, TRUE, sizeof (mcview_vterm_cell_t));
    g_array_append_vals (dst, cells->data, cells->len);
    g_hash_table_insert (buf->rows, GINT_TO_POINTER (row), dst);

    if (row > buf->max_row)
        buf->max_row = row;
}

/* --------------------------------------------------------------------------------------------- */

/* Drop what is below @max_row, as a screen made shorter does. */
void
mcview_terminal_buffer_set_max_row (mcview_terminal_buffer_t *buf, int max_row)
{
    int row;

    if (max_row >= buf->max_row)
        return;

    for (row = max_row + 1; row <= buf->max_row; row++)
    {
        g_hash_table_remove (buf->rows, GINT_TO_POINTER (row));
        g_hash_table_remove (buf->wrapped, GINT_TO_POINTER (row));
    }

    buf->max_row = (max_row < -1) ? -1 : max_row;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_scroll_up (mcview_terminal_buffer_t *buf, int top, int bottom, int cols,
                                  const mcview_ansi_state_t *ansi)
{
    int row;

    if (top >= bottom || cols <= 0)
        return;

    /* Shift rows top..bottom-1: each row gets the content of the row below it. */
    for (row = top; row < bottom; row++)
    {
        gpointer key_dst = GINT_TO_POINTER (row);
        gpointer key_src = GINT_TO_POINTER (row + 1);
        GArray *src_arr;

        g_hash_table_remove (buf->rows, key_dst);
        move_wrapped (buf, key_src, key_dst);

        src_arr = (GArray *) g_hash_table_lookup (buf->rows, key_src);
        if (src_arr != NULL)
        {
            GArray *dst_arr = g_array_new (FALSE, TRUE, sizeof (mcview_vterm_cell_t));
            if (src_arr->len > 0)
                g_array_append_vals (dst_arr, src_arr->data, src_arr->len);
            g_hash_table_insert (buf->rows, key_dst, dst_arr);
        }
    }

    /* Clear the vacated bottom row. */
    g_hash_table_remove (buf->rows, GINT_TO_POINTER (bottom));
    g_hash_table_remove (buf->wrapped, GINT_TO_POINTER (bottom));
    mcview_terminal_buffer_fill_range (buf, bottom, 0, cols - 1, ' ', ansi);

    if (buf->max_row < bottom)
        buf->max_row = bottom;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_scroll_down (mcview_terminal_buffer_t *buf, int top, int bottom, int cols,
                                    const mcview_ansi_state_t *ansi)
{
    int row;

    if (top >= bottom || cols <= 0)
        return;

    for (row = bottom; row > top; row--)
    {
        gpointer key_dst = GINT_TO_POINTER (row);
        gpointer key_src = GINT_TO_POINTER (row - 1);
        GArray *src_arr;

        g_hash_table_remove (buf->rows, key_dst);
        move_wrapped (buf, key_src, key_dst);

        src_arr = (GArray *) g_hash_table_lookup (buf->rows, key_src);
        if (src_arr != NULL)
        {
            GArray *dst_arr = g_array_new (FALSE, TRUE, sizeof (mcview_vterm_cell_t));
            if (src_arr->len > 0)
                g_array_append_vals (dst_arr, src_arr->data, src_arr->len);
            g_hash_table_insert (buf->rows, key_dst, dst_arr);
        }
    }

    g_hash_table_remove (buf->rows, GINT_TO_POINTER (top));
    g_hash_table_remove (buf->wrapped, GINT_TO_POINTER (top));
    mcview_terminal_buffer_fill_range (buf, top, 0, cols - 1, ' ', ansi);

    if (buf->max_row < bottom)
        buf->max_row = bottom;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_erase_eol (mcview_terminal_buffer_t *buf, int row, int col, int term_cols,
                                  const mcview_ansi_state_t *ansi)
{
    if (col >= term_cols || term_cols <= 0)
        return;
    mcview_terminal_buffer_fill_range (buf, row, col, term_cols - 1, ' ', ansi);
    g_hash_table_remove (buf->wrapped, GINT_TO_POINTER (row));
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_erase_bol (mcview_terminal_buffer_t *buf, int row, int col, int term_cols,
                                  const mcview_ansi_state_t *ansi)
{
    if (col < 0)
        return;
    if (col >= term_cols)
        col = term_cols - 1;
    mcview_terminal_buffer_fill_range (buf, row, 0, col, ' ', ansi);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_erase_line (mcview_terminal_buffer_t *buf, int row, int term_cols,
                                   const mcview_ansi_state_t *ansi)
{
    if (term_cols <= 0)
        return;
    mcview_terminal_buffer_fill_range (buf, row, 0, term_cols - 1, ' ', ansi);
    g_hash_table_remove (buf->wrapped, GINT_TO_POINTER (row));
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_delete_chars (mcview_terminal_buffer_t *buf, int row, int col, int count,
                                     int term_cols, const mcview_ansi_state_t *ansi)
{
    GArray *arr;
    int dst, src;

    if (row < 0 || col < 0 || col >= term_cols || count <= 0 || term_cols <= 0)
        return;

    if (count > term_cols - col)
        count = term_cols - col;

    arr = get_or_create_row (buf, row);
    ensure_col (arr, term_cols - 1);

    for (dst = col, src = col + count; src < term_cols; dst++, src++)
        g_array_index (arr, mcview_vterm_cell_t, dst) =
            g_array_index (arr, mcview_vterm_cell_t, src);

    mcview_terminal_buffer_fill_range (buf, row, term_cols - count, term_cols - 1, ' ', ansi);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_insert_chars (mcview_terminal_buffer_t *buf, int row, int col, int count,
                                     int term_cols, const mcview_ansi_state_t *ansi)
{
    GArray *arr;
    int dst, src;

    if (row < 0 || col < 0 || col >= term_cols || count <= 0 || term_cols <= 0)
        return;

    if (count > term_cols - col)
        count = term_cols - col;

    arr = get_or_create_row (buf, row);
    ensure_col (arr, term_cols - 1);

    for (dst = term_cols - 1, src = dst - count; src >= col; dst--, src--)
        g_array_index (arr, mcview_vterm_cell_t, dst) =
            g_array_index (arr, mcview_vterm_cell_t, src);

    mcview_terminal_buffer_fill_range (buf, row, col, col + count - 1, ' ', ansi);
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_terminal_buffer_max_row (const mcview_terminal_buffer_t *buf)
{
    return buf->max_row;
}

/* --------------------------------------------------------------------------------------------- */

mcview_terminal_buffer_t *
mcview_terminal_buffer_copy (const mcview_terminal_buffer_t *src)
{
    mcview_terminal_buffer_t *dst;
    GHashTableIter iter;
    gpointer key, value;

    dst = mcview_terminal_buffer_new ();
    dst->max_row = src->max_row;

    g_hash_table_iter_init (&iter, (GHashTable *) src->rows);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        GArray *src_arr = (GArray *) value;
        GArray *dst_arr = g_array_new (FALSE, TRUE, sizeof (mcview_vterm_cell_t));
        if (src_arr->len > 0)
            g_array_append_vals (dst_arr, src_arr->data, src_arr->len);
        g_hash_table_insert (dst->rows, key, dst_arr);
    }

    g_hash_table_iter_init (&iter, (GHashTable *) src->wrapped);
    while (g_hash_table_iter_next (&iter, &key, &value))
        g_hash_table_insert (dst->wrapped, key, value);

    return dst;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_terminal_buffer_set_wrapped (mcview_terminal_buffer_t *buf, int row, gboolean wrapped)
{
    if (row < 0)
        return;
    if (wrapped)
        g_hash_table_insert (buf->wrapped, GINT_TO_POINTER (row), GINT_TO_POINTER (1));
    else
        g_hash_table_remove (buf->wrapped, GINT_TO_POINTER (row));
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_terminal_buffer_is_wrapped (const mcview_terminal_buffer_t *buf, int row)
{
    return g_hash_table_contains (buf->wrapped, GINT_TO_POINTER (row));
}

/* --------------------------------------------------------------------------------------------- */
