/*
   Midnight Commander - mcterm text selection.

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
#include "lib/event.h"

#include "mcterm_select.h"

/*** file scope variables ************************************************************************/

// What a double click stops at: the break characters of the editor.
static const char mcterm_word_break[] = "{}[]()<>=|/\\!?~-+`'\",.;:#$%^&*";

/*** file scope functions ************************************************************************/

/* Whether the cell at @col of @row is one a word is made of. */
static gboolean
mcterm_sel_is_word_cell (mcview_vterm_t *vt, gint64 row, int col)
{
    const mcview_vterm_cell_t *cell = mcterm_sel_cell_at (vt, row, col);

    if (cell == NULL || cell->ch == 0 || g_unichar_isspace (cell->ch))
        return FALSE;

    return cell->ch >= 0x80 || strchr (mcterm_word_break, (int) cell->ch) == NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* The region with its ends in reading order. */
static void
mcterm_sel_bounds (const mcterm_sel_t *sel, gint64 *first_row, int *first_col, gint64 *last_row,
                   int *last_col)
{
    if (sel->anchor_row < sel->point_row
        || (sel->anchor_row == sel->point_row && sel->anchor_col <= sel->point_col))
    {
        *first_row = sel->anchor_row;
        *first_col = sel->anchor_col;
        *last_row = sel->point_row;
        *last_col = sel->point_col;
    }
    else
    {
        *first_row = sel->point_row;
        *first_col = sel->point_col;
        *last_row = sel->anchor_row;
        *last_col = sel->anchor_col;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mcterm_sel_clear (mcterm_sel_t *sel)
{
    if (sel != NULL)
        memset (sel, 0, sizeof (*sel));
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_sel_start (mcterm_sel_t *sel, gint64 row, int col)
{
    if (sel == NULL)
        return;

    sel->anchored = TRUE;
    sel->active = FALSE;
    sel->anchor_row = row;
    sel->anchor_col = col;
    sel->point_row = row;
    sel->point_col = col;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_sel_extend (mcterm_sel_t *sel, gint64 row, int col)
{
    if (sel == NULL || !sel->anchored)
        return;

    sel->active = TRUE;
    sel->point_row = row;
    sel->point_col = col;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_sel_word (mcterm_sel_t *sel, mcview_vterm_t *vt, gint64 row, int col, int cols)
{
    int from, to;

    if (sel == NULL || vt == NULL || cols <= 0)
        return;

    col = CLAMP (col, 0, cols - 1);
    from = col;
    to = col;

    if (mcterm_sel_is_word_cell (vt, row, col))
    {
        while (from > 0 && mcterm_sel_is_word_cell (vt, row, from - 1))
            from--;
        while (to < cols - 1 && mcterm_sel_is_word_cell (vt, row, to + 1))
            to++;
    }

    mcterm_sel_start (sel, row, from);
    mcterm_sel_extend (sel, row, to);
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_sel_line (mcterm_sel_t *sel, mcview_vterm_t *vt, gint64 row, int cols)
{
    int last;

    if (sel == NULL || vt == NULL || cols <= 0)
        return;

    for (last = cols - 1; last > 0; last--)
    {
        const mcview_vterm_cell_t *cell = mcterm_sel_cell_at (vt, row, last);

        if (cell != NULL && cell->ch != 0 && cell->ch != ' ')
            break;
    }

    mcterm_sel_start (sel, row, 0);
    mcterm_sel_extend (sel, row, last);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_sel_row_span (const mcterm_sel_t *sel, gint64 row, int cols, int *from, int *to)
{
    gint64 first_row, last_row;
    int first_col, last_col;
    int a, b;

    if (sel == NULL || !sel->active || cols <= 0)
        return FALSE;

    mcterm_sel_bounds (sel, &first_row, &first_col, &last_row, &last_col);

    if (row < first_row || row > last_row)
        return FALSE;

    a = (row == first_row) ? first_col : 0;
    // The cell the point is on belongs to the region.
    b = (row == last_row) ? last_col + 1 : cols;

    a = CLAMP (a, 0, cols);
    b = CLAMP (b, 0, cols);
    if (a >= b)
        return FALSE;

    *from = a;
    *to = b;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

const mcview_vterm_cell_t *
mcterm_sel_cell_at (mcview_vterm_t *vt, gint64 row, int col)
{
    gint64 scrolled;

    if (vt == NULL || col < 0)
        return NULL;

    scrolled = mcview_vterm_scrolled_rows (vt);

    if (row >= scrolled)
        return mcview_terminal_buffer_get (mcview_vterm_buf (vt), (int) (row - scrolled), col);

    {
        // The history keeps its newest rows only, so the oldest ones are gone.
        const int len = mcview_vterm_history_len (vt);
        const gint64 index = row - (scrolled - len);
        const GArray *cells;

        if (index < 0 || index >= len)
            return NULL;

        cells = mcview_vterm_history_row (vt, (int) index);
        if (cells == NULL || col >= (int) cells->len)
            return NULL;

        return &g_array_index (cells, mcview_vterm_cell_t, col);
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
mcterm_sel_text (const mcterm_sel_t *sel, mcview_vterm_t *vt, int cols, gint64 skip_row)
{
    GString *text;
    gint64 first_row, last_row, row;
    int first_col, last_col;
    gboolean first = TRUE;

    if (sel == NULL || !sel->active || vt == NULL || cols <= 0)
        return NULL;

    mcterm_sel_bounds (sel, &first_row, &first_col, &last_row, &last_col);
    text = g_string_new (NULL);

    for (row = first_row; row <= last_row; row++)
    {
        int from, to, col;
        gsize row_start;
        gsize last_word;

        if (row == skip_row)
            continue;

        if (!first)
            g_string_append_c (text, '\n');
        first = FALSE;
        row_start = text->len;
        last_word = text->len;

        if (!mcterm_sel_row_span (sel, row, cols, &from, &to))
        {
            from = 0;
            to = 0;
        }

        for (col = from; col < to; col++)
        {
            const mcview_vterm_cell_t *cell = mcterm_sel_cell_at (vt, row, col);
            const gunichar ch = (cell == NULL || cell->ch == 0) ? ' ' : cell->ch;

            if (ch == MCVIEW_VTERM_WIDE_TAIL)
                continue;
            g_string_append_unichar (text, ch);
            if (ch != ' ')
                last_word = text->len;
        }

        // A terminal row is padded with blanks up to its width; they are not text.
        g_string_truncate (text, MAX (row_start, last_word));
    }

    return g_string_free (text, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_sel_copy (const mcterm_sel_t *sel, mcview_vterm_t *vt, int cols, gint64 skip_row)
{
    char *text;

    text = mcterm_sel_text (sel, vt, cols, skip_row);
    if (text == NULL)
        return FALSE;

    if (*text == '\0')
    {
        g_free (text);
        return FALSE;
    }

    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_text_to_file", text);
    // try use external clipboard utility
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_file_to_ext_clip", NULL);

    g_free (text);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
