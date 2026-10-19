/*
   Widgets for the Midnight Commander

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

/** \file table.c
 *  \brief Source: WTable widget
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h" /* KEY_M_CTRL */
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/widget.h"
#include "lib/widget/quicksearch.h"

#include "table.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

static void table_go (WTable *t, int pos);

static int table_max_first_col (const WTable *t);

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
table_source_nrows (const WTable *t)
{
    if (t->datasource.get_nrows == NULL)
        return 0;
    return t->datasource.get_nrows (t->datasource.data);
}

/* --------------------------------------------------------------------------------------------- */

/* The rows the table shows: all the rows of the data source, or those a filter left */
static int
table_get_nrows (const WTable *t)
{
    if (t->qs_rows != NULL)
        return (int) t->qs_rows->len;
    return table_source_nrows (t);
}

/* --------------------------------------------------------------------------------------------- */

/* The row of the data source that row @row of the table shows */
static int
table_data_row (const WTable *t, int row)
{
    if (t->qs_rows == NULL || row < 0 || (guint) row >= t->qs_rows->len)
        return row;
    return g_array_index (t->qs_rows, int, row);
}

/* --------------------------------------------------------------------------------------------- */

static int
table_col_width (const WTable *t, int c)
{
    return t->widths != NULL ? t->widths[c] : t->col_defs[c].width;
}

/* --------------------------------------------------------------------------------------------- */

/* Left, Right and the space act on one cell of the row. */
static gboolean
table_has_col_cursor (const WTable *t)
{
    return t->has_choice_cols || (t->check_cursor && t->has_check_cols);
}

/* --------------------------------------------------------------------------------------------- */

/* The last column that has a width: no separator after it. */
static int
table_last_col (const WTable *t)
{
    int last_col = t->ncols - 1;

    while (last_col > 0 && table_col_width (t, last_col) <= 0)
        last_col--;
    return last_col;
}

/* --------------------------------------------------------------------------------------------- */

/* The x after the last cell: the scrollbar has the last column of the widget. */
static int
table_cells_right (const WTable *t)
{
    return CONST_WIDGET (t)->rect.cols - ((t->scrollbar || t->scrollbar_on_frame) ? 1 : 0);
}

/* --------------------------------------------------------------------------------------------- */

/* Step from the cell @c at @x to the next one that is drawn, with its width clipped at the
   right edge.  Start with @c = first_col - 1, @x = 1 and @width = 0. */
static gboolean
table_next_cell (const WTable *t, int *c, int *x, int *width)
{
    int right = table_cells_right (t);

    if (*c >= t->first_col)
        *x += *width + (*c < table_last_col (t) ? 1 : 0);

    for ((*c)++; *c < t->ncols && *x < right; (*c)++)
    {
        int w = table_col_width (t, *c);

        if (w > 0)
        {
            *width = MIN (w, right - *x);
            return TRUE;
        }
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* The column under x of the widget, or -1. */
static int
table_col_at (const WTable *t, int x)
{
    int c = t->first_col - 1, col_x = 1, width = 0;

    while (table_next_cell (t, &c, &col_x, &width))
        if (x >= col_x && x < col_x + width)
            return c;
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

/* Whether column @col is drawn whole. */
static gboolean
table_col_shown (const WTable *t, int col)
{
    int c = t->first_col - 1, col_x = 1, width = 0;

    while (table_next_cell (t, &c, &col_x, &width))
        if (c == col)
            return width == table_col_width (t, c);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* Scroll the columns so that the cell the column cursor is on is drawn. */
static void
table_show_current_col (WTable *t)
{
    if (!t->scroll_columns)
        return;
    if (t->current_col < t->first_col)
        t->first_col = t->current_col;
    while (t->first_col < table_max_first_col (t) && !table_col_shown (t, t->current_col))
        t->first_col++;
}

/* --------------------------------------------------------------------------------------------- */

/* The line a data row is drawn on: below the header when there is one. */
static int
table_row_y (const WTable *t, int i)
{
    return i + (t->titles != NULL ? 1 : 0);
}

/* --------------------------------------------------------------------------------------------- */

/* The first column that shows every column after it whole; scrolling further
   would only leave the right side empty. */
static int
table_max_first_col (const WTable *t)
{
    int cols = CONST_WIDGET (t)->rect.cols - 1;
    int used = 0;
    int c;

    for (c = t->ncols - 1; c >= 0; c--)
    {
        used += table_col_width (t, c) + (c < t->ncols - 1 ? 1 : 0);
        if (used > cols)
            return MIN (c + 1, t->ncols - 1);
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* One line of cells from first_col, clipped at the right edge. */
static void
table_draw_cells (WTable *t, int y, int row_idx, int nrows, int row_color, gboolean focused,
                  gboolean disabled)
{
    int right = table_cells_right (t);
    int last_col = table_last_col (t);
    int c = t->first_col - 1, col_x = 1, width = 0;

    while (table_next_cell (t, &c, &col_x, &width))
    {
        int cell_color = row_color;

        if (row_idx >= 0 && row_idx != t->current && t->cell_color != NULL)
        {
            int color = t->cell_color (t->datasource.data, table_data_row (t, row_idx), c);

            if (color >= 0)
                cell_color = color;
        }
        tty_setcolor (cell_color);
        widget_gotoyx (t, y, col_x);

        if (row_idx < 0)
            tty_print_string (str_fit_to_term (t->titles[c] != NULL ? t->titles[c] : "", width,
                                               t->col_defs[c].align));
        else if (t->col_defs[c].type == TABLE_COL_CHECK && row_idx < nrows
                 && t->datasource.get_checked != NULL)
        {
            gboolean checked =
                t->datasource.get_checked (t->datasource.data, table_data_row (t, row_idx), c);

            /* the box the space acts on */
            if (t->check_cursor && focused && !disabled && row_idx == t->current
                && c == t->current_col)
                tty_setcolor (DIALOG_CELL_FOCUS_COLOR);
            tty_print_string (str_fit_to_term (checked ? "[x]" : "[ ]", width, J_LEFT));
        }
        else
        {
            const char *cell_text = "";
            int text_width = width;

            if (row_idx < nrows && t->datasource.get_text != NULL)
                cell_text =
                    t->datasource.get_text (t->datasource.data, table_data_row (t, row_idx), c);

            /* mark the cell space acts on; a character, not a color: the current
               cell and the current row may share it */
            if (t->col_defs[c].type == TABLE_COL_CHOICE)
            {
                gboolean is_current = (focused && !disabled && row_idx == t->current
                                       && c == t->current_col && row_idx < nrows);

                tty_print_char (is_current ? '>' : ' ');
                text_width--;
            }

            tty_print_string (str_fit_to_term (cell_text, text_width, t->col_defs[c].align));
        }

        /* draw column separator except after last column */
        if (c < last_col && col_x + width < right)
        {
            tty_setcolor (row_color);
            widget_gotoyx (t, y, col_x + width);
            tty_print_one_vline (TRUE);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The line style of the right column: that of the frame of the dialog when the column lies
   on it, else a single line */
static gboolean
table_frame_single (const WTable *t)
{
    const Widget *w = CONST_WIDGET (t);
    const Widget *top;
    const Widget *bg;
    int d;

    if (!t->scrollbar_on_frame)
        return TRUE;

    // a dialog is its own owner; a group without a dialog above has none
    for (top = w; top->owner != NULL && CONST_WIDGET (top->owner) != top;
         top = CONST_WIDGET (top->owner))
        ;
    if (top == w || top->owner == NULL)
        return TRUE;

    bg = CONST_DIALOG (top)->bg;
    if (bg == NULL || bg->callback != frame_callback)
        return TRUE;

    d = CONST_FRAME (bg)->compact ? 0 : 1;
    if (w->rect.x + w->rect.cols != bg->rect.x + bg->rect.cols - d)
        return TRUE;
    return CONST_FRAME (bg)->single;
}

/* --------------------------------------------------------------------------------------------- */

/* Where the scrollbar of the table is: down its last column, beside the rows */
static WRect
table_bar_rect (const WTable *t)
{
    const WRect *w = &CONST_WIDGET (t)->rect;
    WRect r = { w->y + table_row_y (t, 0), w->x + w->cols - 1, MAX (1, table_data_lines (t)), 1 };

    return r;
}

/* --------------------------------------------------------------------------------------------- */

/* The scrollbar placed and given the range of the rows, for the mouse */
static void
table_place_bar (WTable *t)
{
    WRect r = table_bar_rect (t);

    widget_set_size_rect (WIDGET (t->bar), &r);
    scrollbar_set_range (t->bar, table_get_nrows (t), table_data_lines (t), t->top);
}

/* --------------------------------------------------------------------------------------------- */

/* Next column the user can act on, `from` when there is none. */
static int
table_next_active_col (const WTable *t, int from, int dir)
{
    int c;

    for (c = from + dir; c >= 0 && c < t->ncols; c += dir)
        if (t->col_defs[c].type == TABLE_COL_CHECK || t->col_defs[c].type == TABLE_COL_CHOICE)
            return c;

    return MAX (from, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
table_draw (WTable *t, gboolean focused)
{
    Widget *wt = WIDGET (t);
    const WRect *w = &CONST_WIDGET (t)->rect;
    const int *colors;
    gboolean disabled;
    int normalc, selc, scrollbarc;
    int nrows, lines;
    int i;
    int sel_line = -1;

    nrows = table_get_nrows (t);
    lines = table_data_lines (t);
    colors = widget_get_colors (wt);

    disabled = widget_get_state (wt, WST_DISABLED);
    if (t->normal_color >= 0)
        normalc = disabled ? CORE_DISABLED_COLOR : t->normal_color;
    else if (t->color_idx >= 0)
        normalc = disabled ? CORE_DISABLED_COLOR : colors[t->color_idx];
    else
        normalc = disabled ? CORE_DISABLED_COLOR : colors[DLG_COLOR_NORMAL];
    if (t->selected_color >= 0)
        selc = disabled ? CORE_DISABLED_COLOR : t->selected_color;
    else
        selc = disabled ? CORE_DISABLED_COLOR
                        : colors[focused ? DLG_COLOR_SELECTED_FOCUS : DLG_COLOR_SELECTED_NORMAL];
    scrollbarc = disabled         ? CORE_DISABLED_COLOR
        : t->scrollbar_color >= 0 ? t->scrollbar_color
                                  : colors[DLG_COLOR_FRAME];

    if (t->prefetch != NULL && nrows > 0 && t->top < nrows)
    {
        if (t->qs_rows == NULL)
            t->prefetch (t->datasource.data, t->top, MIN (lines, nrows - t->top));
        else
            // the rows a filter left are not in one run
            for (i = t->top; i < MIN (t->top + lines, nrows); i++)
                t->prefetch (t->datasource.data, table_data_row (t, i), 1);
    }

    if (t->titles != NULL)
    {
        int headc = disabled ? CORE_DISABLED_COLOR : colors[DLG_COLOR_TITLE];

        tty_setcolor (headc);
        widget_gotoyx (t, 0, 0);
        tty_print_string (str_fit_to_term ("", w->cols, J_LEFT));
        table_draw_cells (t, 0, -1, nrows, headc, focused, disabled);
    }

    for (i = 0; i < lines; i++)
    {
        int row_idx = t->top + i;
        int y = table_row_y (t, i);
        int row_color;

        if (row_idx == t->current && sel_line == -1)
        {
            sel_line = y;
            row_color = selc;
        }
        else
            row_color = normalc;

        tty_setcolor (row_color);

        /* clear the line first */
        widget_gotoyx (t, y, 0);
        tty_print_string (str_fit_to_term ("", w->cols, J_LEFT));

        table_draw_cells (t, y, row_idx, nrows, row_color, focused, disabled);
    }

    t->cursor_y = sel_line;

    // the text of a quick search on the last line below the rows, when it has no label
    if (t->qs_active && t->qs_label == NULL)
    {
        char *text;

        if (t->qs_filtering)
            text = g_strdup_printf ("%s %s", _ ("Filter:"), t->qs_text->str);
        else
            text = g_strconcat ("/", t->qs_text->str, (char *) NULL);
        tty_setcolor (normalc);
        widget_gotoyx (t, w->lines - 1, 0);
        tty_print_string (str_fit_to_term (text, w->cols - (t->scrollbar ? 1 : 0), J_LEFT_FIT));
        g_free (text);
    }

    if (t->scrollbar && nrows > lines)
    {
        const WRect r = table_bar_rect (t);

        scrollbar_show (t->bar, &r, nrows, lines, t->top, scrollbarc);
        /* the header row above the scrollbar and the search line below it are still the frame,
           in its line */
        if (t->scrollbar_on_frame)
        {
            const gboolean single = table_frame_single (t);

            tty_setcolor (scrollbarc);
            for (i = 0; i < w->lines; i++)
                if (i < table_row_y (t, 0) || i >= table_row_y (t, 0) + lines)
                {
                    widget_gotoyx (t, i, w->cols - 1);
                    tty_print_one_vline (single);
                }
        }
    }
    else if (t->scrollbar_on_frame)
    {
        /* nothing to scroll: the column is the frame, in its line */
        const gboolean single = table_frame_single (t);

        tty_setcolor (scrollbarc);
        for (i = 0; i < w->lines; i++)
        {
            widget_gotoyx (t, i, w->cols - 1);
            tty_print_one_vline (single);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
table_fwd (WTable *t, gboolean wrap)
{
    int nrows = table_get_nrows (t);

    if (nrows == 0)
        return;

    if (t->current + 1 < nrows)
        table_go (t, t->current + 1);
    else if (wrap)
        table_go (t, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
table_back (WTable *t, gboolean wrap)
{
    int nrows = table_get_nrows (t);

    if (nrows == 0)
        return;

    if (t->current > 0)
        table_go (t, t->current - 1);
    else if (wrap)
        table_go (t, nrows - 1);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
table_execute_cmd (WTable *t, long command)
{
    cb_ret_t ret = MSG_HANDLED;
    int lines = table_data_lines (t);
    int nrows = table_get_nrows (t);

    if (nrows == 0)
        return MSG_NOT_HANDLED;

    switch (command)
    {
    case CK_Up:
        table_back (t, TRUE);
        break;
    case CK_Down:
        table_fwd (t, TRUE);
        break;
    case CK_Top:
        table_go (t, 0);
        break;
    case CK_Bottom:
        table_go (t, nrows - 1);
        break;
    case CK_PageUp:
        table_go (t, MAX (t->current - (lines - 1), 0));
        break;
    case CK_PageDown:
        table_go (t, MIN (t->current + (lines - 1), nrows - 1));
        break;
    case CK_Enter:
        ret = send_message (WIDGET (t)->owner, t, MSG_NOTIFY, command, NULL);
        break;
    default:
        ret = MSG_NOT_HANDLED;
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
table_key (WTable *t, int key)
{
    int lines = table_data_lines (t);
    int nrows = table_get_nrows (t);

    /* the columns scroll even over an empty table; with a column cursor Left and Right
       move the cursor and the columns follow it */
    if (t->scroll_columns)
    {
        int page = MAX (t->ncols / 2, 1);

        switch (key)
        {
        case KEY_LEFT:
            if (table_has_col_cursor (t))
                break;
            table_scroll_columns (t, -1);
            return MSG_HANDLED;
        case KEY_RIGHT:
            if (table_has_col_cursor (t))
                break;
            table_scroll_columns (t, 1);
            return MSG_HANDLED;
        case KEY_M_CTRL | KEY_LEFT:
            table_scroll_columns (t, -page);
            return MSG_HANDLED;
        case KEY_M_CTRL | KEY_RIGHT:
            table_scroll_columns (t, page);
            return MSG_HANDLED;
        default:
            break;
        }
    }

    if (nrows == 0)
        return MSG_NOT_HANDLED;

    switch (key)
    {
    case KEY_UP:
        table_back (t, TRUE);
        return MSG_HANDLED;
    case KEY_DOWN:
        table_fwd (t, TRUE);
        return MSG_HANDLED;
    case KEY_HOME:
        table_go (t, 0);
        return MSG_HANDLED;
    case KEY_END:
        table_go (t, nrows - 1);
        return MSG_HANDLED;
    case KEY_PPAGE:
        table_go (t, MAX (t->current - (lines - 1), 0));
        return MSG_HANDLED;
    case KEY_NPAGE:
        table_go (t, MIN (t->current + (lines - 1), nrows - 1));
        return MSG_HANDLED;
    case '\n':
    case KEY_ENTER:
        /* unhandled by the owner, the key falls through to the default button */
        return send_message (WIDGET (t)->owner, t, MSG_NOTIFY, CK_Enter, NULL);
    case KEY_LEFT:
        if (!table_has_col_cursor (t))
            return MSG_NOT_HANDLED;
        t->current_col = table_next_active_col (t, t->current_col, -1);
        table_show_current_col (t);
        return MSG_HANDLED;
    case KEY_RIGHT:
        if (!table_has_col_cursor (t))
            return MSG_NOT_HANDLED;
        t->current_col = table_next_active_col (t, t->current_col, 1);
        table_show_current_col (t);
        return MSG_HANDLED;
    case ' ':
        if (t->current >= nrows)
            return MSG_NOT_HANDLED;

        /* with a column cursor the space acts on the cell it points at */
        if (table_has_col_cursor (t))
        {
            int c = t->current_col;

            /* Ctrl-Left and Ctrl-Right may have scrolled the cell away: show what changes */
            table_show_current_col (t);

            if (c >= 0 && c < t->ncols)
            {
                if (t->col_defs[c].type == TABLE_COL_CHOICE && t->datasource.cycle_choice != NULL)
                {
                    t->datasource.cycle_choice (t->datasource.data, table_data_row (t, t->current),
                                                c, 1);
                    return MSG_HANDLED;
                }

                if (t->col_defs[c].type == TABLE_COL_CHECK && t->datasource.get_checked != NULL
                    && t->datasource.set_checked != NULL)
                {
                    gboolean val = t->datasource.get_checked (t->datasource.data,
                                                              table_data_row (t, t->current), c);

                    t->datasource.set_checked (t->datasource.data, table_data_row (t, t->current),
                                               c, !val);
                    return MSG_HANDLED;
                }
            }
            return MSG_NOT_HANDLED;
        }

        if (t->has_check_cols && t->datasource.get_checked != NULL
            && t->datasource.set_checked != NULL)
        {
            int c;

            for (c = 0; c < t->ncols; c++)
                if (t->col_defs[c].type == TABLE_COL_CHECK)
                {
                    gboolean val = t->datasource.get_checked (t->datasource.data,
                                                              table_data_row (t, t->current), c);
                    t->datasource.set_checked (t->datasource.data, table_data_row (t, t->current),
                                               c, !val);
                    break;
                }
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;
    default:
        return MSG_NOT_HANDLED;
    }
}

/* --------------------------------------------------------------------------------------------- */
/* quick search and quick filter, as in the panels */

/* The text of data row @row for the quick search */
static const char *
table_qs_text (const WTable *t, int row)
{
    if (t->qs_text_of != NULL)
        return t->qs_text_of (t->datasource.data, row);
    if (t->datasource.get_text != NULL)
        return t->datasource.get_text (t->datasource.data, row, t->qs_col);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
table_qs_matches (const WTable *t, mc_search_t *search, int data_row)
{
    const char *text;

    text = table_qs_text (t, data_row);
    return text != NULL && mc_search_run (search, text, 0, strlen (text), NULL);
}

/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    int row;
    int group;
    int score;
} table_rank_t;

static gint
table_rank_compare (gconstpointer a, gconstpointer b)
{
    const table_rank_t *x = (const table_rank_t *) a;
    const table_rank_t *y = (const table_rank_t *) b;

    if (x->group != y->group)
        return x->group < y->group ? -1 : 1;
    if (x->score != y->score)
        return x->score > y->score ? -1 : 1;
    return x->row - y->row;
}

/* --------------------------------------------------------------------------------------------- */

/* A fuzzy filter: the rows that have the letters in their order, best match first in each
   group; the cursor goes to the best one */
static gboolean
table_qs_filter_fuzzy (WTable *t)
{
    GArray *ranks, *rows;
    int n = table_source_nrows (t);
    int i, best = 0, found = 0;

    ranks = g_array_new (FALSE, FALSE, sizeof (table_rank_t));
    for (i = 0; i < n; i++)
    {
        table_rank_t r;

        r.row = i;
        r.group = t->qs_group_of != NULL ? t->qs_group_of (t->datasource.data, i) : 0;
        r.score = t->qs_text->len == 0
            ? 1
            : quick_search_fuzzy_score (table_qs_text (t, i), t->qs_text->str,
                                        t->qs_case_sensitive);
        if (r.score > 0)
        {
            g_array_append_val (ranks, r);
            found++;
        }
        else if (r.group < 0)
            g_array_append_val (ranks, r);
    }

    if (found == 0)
    {
        g_array_free (ranks, TRUE);
        return FALSE;
    }

    if (t->qs_text->len != 0)
        g_array_sort (ranks, table_rank_compare);
    // the cursor goes to the best match of all the groups
    rows = g_array_sized_new (FALSE, FALSE, sizeof (int), ranks->len);
    for (i = 0; i < (int) ranks->len; i++)
    {
        const table_rank_t *r = &g_array_index (ranks, table_rank_t, i);

        g_array_append_val (rows, r->row);
        if (t->qs_text->len != 0 && r->score > g_array_index (ranks, table_rank_t, best).score)
            best = i;
    }
    g_array_free (ranks, TRUE);

    if (t->qs_rows != NULL)
        g_array_free (t->qs_rows, TRUE);
    t->qs_rows = rows;
    t->top = 0;
    t->current = 0;
    table_go (t, best);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The rows that have the text, the data row that was current kept current when it is there */
static gboolean
table_qs_filter (WTable *t)
{
    mc_search_t *search;
    GArray *rows;
    int current = table_get_current (t);
    int n = table_source_nrows (t);
    int i;

    if (t->qs_fuzzy)
        return table_qs_filter_fuzzy (t);

    search = quick_search_new (t->qs_text->str, TRUE, t->qs_case_sensitive);
    rows = g_array_new (FALSE, FALSE, sizeof (int));
    for (i = 0; i < n; i++)
        if (t->qs_text->len == 0 || table_qs_matches (t, search, i))
            g_array_append_val (rows, i);
    mc_search_free (search);

    if (rows->len == 0)
    {
        g_array_free (rows, TRUE);
        return FALSE;
    }

    if (t->qs_rows != NULL)
        g_array_free (t->qs_rows, TRUE);
    t->qs_rows = rows;
    t->top = 0;
    t->current = 0;
    table_set_current (t, current);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* From row @from on, wrapping around, the first row that begins with the text */
static gboolean
table_qs_find (WTable *t, int from)
{
    mc_search_t *search;
    int n = table_get_nrows (t);
    int i;
    gboolean found = FALSE;

    if (n == 0)
        return FALSE;

    search = quick_search_new (t->qs_text->str, FALSE, t->qs_case_sensitive);
    for (i = 0; i < n && !found; i++)
    {
        const int row = (from + i) % n;

        if (table_qs_matches (t, search, table_data_row (t, row)))
        {
            table_go (t, row);
            found = TRUE;
        }
    }
    mc_search_free (search);
    return found;
}

/* --------------------------------------------------------------------------------------------- */

/* The text of the quick search onto its label, if the table has one */
static void
table_qs_show_label (WTable *t)
{
    char *text = NULL;

    if (t->qs_label == NULL)
        return;

    if (t->qs_active && t->qs_filtering)
        text = g_strdup_printf (" %s %s ", _ ("Filter:"), t->qs_text->str);
    else if (t->qs_active)
        text = g_strconcat (" /", t->qs_text->str, " ", (char *) NULL);
    if (g_strcmp0 (text, t->qs_label->text) != 0)
        hline_set_text (t->qs_label, text);
    g_free (text);
}

/* --------------------------------------------------------------------------------------------- */

static void
table_qs_start (WTable *t, gboolean filtering)
{
    if (t->qs_text == NULL)
        t->qs_text = g_string_new (NULL);
    g_string_truncate (t->qs_text, 0);
    t->qs_chpoint = 0;
    t->qs_active = TRUE;
    t->qs_filtering = filtering;
    // a letter is the text of the search, not the hotkey of a button, as in WListbox
    widget_set_options (WIDGET (t), WOP_IS_INPUT, TRUE);
    // the line of the text is taken from the rows
    table_go (t, t->current);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
table_is_search_key (int key)
{
    return key == XCTRL ('s') || key == ALT ('s');
}

/* --------------------------------------------------------------------------------------------- */

/* The keys of a quick search or filter; MSG_NOT_HANDLED for the keys the table handles as
   usual.  Any such key ends a search, while a filter stays for the keys that move in it. */
static cb_ret_t
table_qs_key (WTable *t, int key)
{
    const gboolean typed = key == KEY_BACKSPACE || (key >= ' ' && key <= 255);

    if (!t->qs_active)
    {
        if (table_is_search_key (key))
            table_qs_start (t, FALSE);
        else if (key == ALT ('S'))
            table_qs_start (t, TRUE);
        else if (t->qs_letters && typed && key != KEY_BACKSPACE && key != ' ')
        {
            table_qs_start (t, t->qs_letters_filter);
            return table_qs_key (t, key);
        }
        else
            return MSG_NOT_HANDLED;
        return MSG_HANDLED;
    }

    if (typed)
    {
        if (!quick_search_take_key (t->qs_text, t->qs_ch, sizeof (t->qs_ch), &t->qs_chpoint, key))
            return MSG_HANDLED;

        // a character that finds nothing is dropped again, as in the panels; the rows and the
        // cursor stay as they were
        if ((t->qs_filtering ? !table_qs_filter (t) : !table_qs_find (t, t->current))
            && key != KEY_BACKSPACE)
            quick_search_drop_char (t->qs_text);
        return MSG_HANDLED;
    }

    if (table_is_search_key (key))
    {
        if (!t->qs_filtering)
            (void) table_qs_find (t, t->current + 1);
        return MSG_HANDLED;
    }

    if (key == ALT ('S'))
    {
        t->qs_filtering = TRUE;
        (void) table_qs_filter (t);
        return MSG_HANDLED;
    }

    if (key == ESC_CHAR)
    {
        table_quick_search_stop (t);
        return MSG_HANDLED;
    }

    if (!t->qs_filtering)
        table_quick_search_stop (t);
    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static void
table_on_change (WTable *t)
{
    // a table no dialog holds is not on the screen yet
    if (WIDGET (t)->owner == NULL)
        return;
    table_draw (t, TRUE);
    send_message (WIDGET (t)->owner, t, MSG_NOTIFY, 0, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
table_destroy (WTable *t)
{
    g_free (t->col_defs);
    t->col_defs = NULL;
    g_strfreev (t->titles);
    t->titles = NULL;
    g_free (t->min_widths);
    t->min_widths = NULL;
    g_free (t->expands);
    t->expands = NULL;
    g_free (t->widths);
    t->widths = NULL;
    if (t->qs_text != NULL)
        g_string_free (t->qs_text, TRUE);
    t->qs_text = NULL;
    if (t->qs_rows != NULL)
        g_array_free (t->qs_rows, TRUE);
    t->qs_rows = NULL;
    widget_destroy (WIDGET (t->bar));
    t->bar = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
table_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WTable *t = TABLE (w);

    switch (msg)
    {
    case MSG_KEY:
    {
        cb_ret_t ret_code = MSG_NOT_HANDLED;
        const gboolean was_active = t->qs_active;

        if (t->qs_enabled)
        {
            ret_code = table_qs_key (t, parm);
            table_qs_show_label (t);
        }
        // a key that ended the search and goes on unhandled still takes its line away
        if (ret_code == MSG_NOT_HANDLED && was_active && !t->qs_active)
            table_on_change (t);
        if (ret_code == MSG_NOT_HANDLED)
            ret_code = table_key (t, parm);
        else
        {
            table_on_change (t);
            return ret_code;
        }
        if (ret_code != MSG_NOT_HANDLED)
            table_on_change (t);
        return ret_code;
    }

    case MSG_UNFOCUS:
        // a search typed here ends when the focus goes, as in a panel
        table_quick_search_stop (t);
        return widget_default_callback (w, sender, msg, parm, data);

    case MSG_HOTKEY:
        /* Enter reaches every widget as a hotkey before the focused one gets it as
           a key, so the default button would grab it */
        if (w->owner == NULL || GROUP (w->owner)->current == NULL
            || WIDGET (GROUP (w->owner)->current->data) != w)
            return MSG_NOT_HANDLED;
        if (parm == '\n')
            return send_message (w->owner, w, MSG_NOTIFY, CK_Enter, NULL);
        // Alt-S and Alt-Shift-S as they are, see WOP_WANT_ALT_KEYS
        if (t->qs_enabled && (table_is_search_key (parm) || parm == ALT ('S')))
        {
            const cb_ret_t ret = table_qs_key (t, parm);

            table_qs_show_label (t);
            table_on_change (t);
            return ret;
        }
        return MSG_NOT_HANDLED;

    case MSG_ACTION:
        return table_execute_cmd (t, parm);

    case MSG_NOTIFY:
        // the scrollbar has moved: the rows go with it, the current one kept among them
        if (sender == WIDGET (t->bar))
        {
            const int old_current = t->current;
            const int lines = table_data_lines (t);

            t->top = (int) scrollbar_get_pos (t->bar);
            // a row as it is shown, among the rows a filter left: not table_set_current()
            table_go (t, CLAMP (t->current, t->top, t->top + lines - 1));
            if (t->current != old_current)
                table_on_change (t);
            else
                table_draw (t, widget_get_state (w, WST_FOCUSED));
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case MSG_CURSOR:
        if (t->cursor_y >= 0)
            widget_gotoyx (t, t->cursor_y, 0);
        return MSG_HANDLED;

    case MSG_DRAW:
        table_draw (t, widget_get_state (w, WST_FOCUSED));
        return MSG_HANDLED;

    case MSG_DESTROY:
        table_destroy (t);
        return MSG_HANDLED;

    case MSG_RESIZE:
    {
        cb_ret_t ret = widget_default_callback (w, sender, msg, parm, data);

        table_layout (t);
        table_go (t, t->current);
        return ret;
    }

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
table_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    WTable *t = TABLE (w);
    int old_current, old_col;
    int nrows = table_get_nrows (t);

    old_current = t->current;
    old_col = t->current_col;

    // the last column is the scrollbar's while there is something to scroll, and so is a drag of
    // its thumb wherever it goes
    if (t->scrollbar && t->bar != NULL
        && (t->bar->drag >= 0
            || (event->x == w->rect.cols - 1 && event->y >= table_row_y (t, 0)
                && event->y < table_row_y (t, 0) + table_data_lines (t)
                && nrows > table_data_lines (t))))
    {
        table_place_bar (t);
        scrollbar_mouse (t->bar, msg, event->y - table_row_y (t, 0));
        if (msg == MSG_MOUSE_DOWN)
            widget_select (w);
        return;
    }

    switch (msg)
    {
    case MSG_MOUSE_DOWN:
        widget_select (w);
        if (nrows > 0 && event->y >= table_row_y (t, 0))
            table_go (t, MIN (t->top + event->y - table_row_y (t, 0), nrows - 1));
        if (table_has_col_cursor (t))
        {
            int c = table_col_at (t, event->x);

            if (c >= 0
                && (t->col_defs[c].type == TABLE_COL_CHECK
                    || t->col_defs[c].type == TABLE_COL_CHOICE))
                t->current_col = c;
        }
        break;

    case MSG_MOUSE_SCROLL_UP:
        table_back (t, FALSE);
        break;

    case MSG_MOUSE_SCROLL_DOWN:
        table_fwd (t, FALSE);
        break;

    case MSG_MOUSE_CLICK:
    {
        int row = t->top + event->y - table_row_y (t, 0);
        int c = table_col_at (t, event->x);

        /* a click on a box switches it, also as the second click of a double one */
        if (t->check_cursor && event->y >= table_row_y (t, 0) && row < nrows && c >= 0
            && t->col_defs[c].type == TABLE_COL_CHECK && t->datasource.get_checked != NULL
            && t->datasource.set_checked != NULL)
        {
            gboolean val =
                t->datasource.get_checked (t->datasource.data, table_data_row (t, row), c);

            t->datasource.set_checked (t->datasource.data, table_data_row (t, row), c, !val);
            table_on_change (t);
        }
        else if (event->count == GPM_DOUBLE)
            table_execute_cmd (t, CK_Enter);
        break;
    }

    case MSG_MOUSE_DRAG:
        event->result.repeat = TRUE;
        if (nrows > 0)
            table_go (t, MIN (t->top + MAX (event->y - table_row_y (t, 0), 0), nrows - 1));
        break;

    default:
        break;
    }

    if (t->current != old_current || t->current_col != old_col)
        table_on_change (t);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Let Left, Right and the space walk and switch the check columns one cell at a
 * time, and a click switch a box.  The current box gets the foreground of the
 * skin's [dialog] dcellfocus on the background of its row.
 */
void
table_set_check_cursor (WTable *t, gboolean on)
{
    t->check_cursor = on;
    if (table_has_col_cursor (t))
        t->current_col = table_next_active_col (t, -1, 1);
}

/* --------------------------------------------------------------------------------------------- */

WTable *
table_new (int y, int x, int height, int width, int ncols, const table_column_def_t *col_defs)
{
    WRect r = { y, x, 1, width };
    WTable *t;
    Widget *w;

    t = g_new0 (WTable, 1);
    w = WIDGET (t);
    r.lines = height > 0 ? height : 1;
    widget_init (w, &r, table_callback, table_mouse_callback);
    /* WANT_HOTKEY: else the hotkey round skips the table and Enter goes to the button */
    w->options |= WOP_SELECTABLE | WOP_WANT_HOTKEY;

    t->ncols = ncols;
    t->col_defs = g_new (table_column_def_t, ncols);
    memcpy (t->col_defs, col_defs, sizeof (table_column_def_t) * (size_t) ncols);

    t->top = 0;
    t->current = 0;
    t->current_col = 0;
    t->cursor_y = 0;
    t->scrollbar = !mc_global.tty.slow_terminal;
    t->scrollbar_on_frame = FALSE;
    // the table's own, in no group: it draws it and gives it the mouse of its last column
    t->bar = scrollbar_new (y, x + width - 1, MAX (1, r.lines), SCROLLBAR_VERTICAL);
    scrollbar_set_client (t->bar, w);
    t->color_idx = -1;
    t->normal_color = -1;
    t->selected_color = -1;
    t->scrollbar_color = -1;

    /* detect CHECK and CHOICE columns */
    t->has_check_cols = FALSE;
    t->has_choice_cols = FALSE;
    {
        int c;

        for (c = 0; c < ncols; c++)
        {
            if (col_defs[c].type == TABLE_COL_CHECK)
                t->has_check_cols = TRUE;
            else if (col_defs[c].type == TABLE_COL_CHOICE)
                t->has_choice_cols = TRUE;
        }
    }

    if (table_has_col_cursor (t))
        t->current_col = table_next_active_col (t, -1, 1);

    return t;
}

/* --------------------------------------------------------------------------------------------- */

void
table_set_datasource (WTable *t, table_datasource_t ds)
{
    int nrows;

    // new rows: a filter of the old ones means nothing
    table_quick_search_stop (t);
    t->datasource = ds;

    /* clamp selection and viewport to new data bounds */
    nrows = table_get_nrows (t);
    if (nrows == 0)
    {
        t->current = 0;
        t->top = 0;
    }
    else
        table_go (t, MIN (t->current, nrows - 1));
}

/* --------------------------------------------------------------------------------------------- */

int
table_get_current (const WTable *t)
{
    return table_data_row (t, t->current);
}

/* --------------------------------------------------------------------------------------------- */

void
table_set_current (WTable *t, int pos)
{
    guint i;

    // a row a filter hides cannot be made current; the others are found where it left them
    if (t->qs_rows != NULL)
    {
        for (i = 0; i < t->qs_rows->len; i++)
            if (g_array_index (t->qs_rows, int, i) == pos)
            {
                table_go (t, (int) i);
                return;
            }
        return;
    }
    table_go (t, pos);
}

/* --------------------------------------------------------------------------------------------- */

static void
table_go (WTable *t, int pos)
{
    int nrows;
    int lines;
    int max_top;

    nrows = table_get_nrows (t);

    if (nrows == 0)
        return;

    if (pos < 0)
        pos = 0;
    if (pos >= nrows)
        pos = nrows - 1;

    t->current = pos;

    lines = table_data_lines (t);
    max_top = MAX (nrows - lines, 0);

    /* adjust top so current is visible */
    if (t->current < t->top)
        t->top = t->current;
    else if (t->current - t->top >= lines)
        t->top = t->current - lines + 1;

    if (t->top > max_top)
        t->top = max_top;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

int
table_data_lines (const WTable *t)
{
    int lines = CONST_WIDGET (t)->rect.lines - (t->titles != NULL ? 1 : 0)
        - (t->qs_active && t->qs_label == NULL ? 1 : 0);

    return MAX (lines, 1);
}

/* --------------------------------------------------------------------------------------------- */

void
table_set_header (WTable *t, const char *const *titles)
{
    g_strfreev (t->titles);
    t->titles = NULL;
    if (titles != NULL)
    {
        int c;

        t->titles = g_new0 (char *, t->ncols + 1);
        for (c = 0; c < t->ncols; c++)
            t->titles[c] = g_strdup (titles[c] != NULL ? titles[c] : "");
    }
    table_layout (t);
    table_go (t, t->current);
}

/* --------------------------------------------------------------------------------------------- */

void
table_set_column_sizing (WTable *t, int col, int min_width, gboolean expands)
{
    if (col < 0 || col >= t->ncols)
        return;
    if (t->min_widths == NULL)
    {
        t->min_widths = g_new0 (int, t->ncols);
        t->expands = g_new0 (gboolean, t->ncols);
    }
    t->min_widths[col] = MAX (min_width, 1);
    t->expands[col] = expands;
    table_layout (t);
}

/* --------------------------------------------------------------------------------------------- */

void
table_set_scroll_columns (WTable *t, gboolean enable)
{
    t->scroll_columns = enable;
    if (!enable)
        t->first_col = 0;
}

/* --------------------------------------------------------------------------------------------- */

void
table_set_prefetch (WTable *t, void (*prefetch) (void *data, int first, int count))
{
    t->prefetch = prefetch;
}

/* --------------------------------------------------------------------------------------------- */

void
table_set_cell_color (WTable *t, int (*cell_color) (void *data, int row, int col))
{
    t->cell_color = cell_color;
}

/* --------------------------------------------------------------------------------------------- */

/* Fixed columns keep their col_defs width.  A column sized here starts at its
   floor (or its title, whichever is wider); when everything fits, the columns
   that expand share the spare width.  When it does not, the columns scroll. */
void
table_layout (WTable *t)
{
    // the cells start at x = 1 and end before the scrollbar
    int cols = table_cells_right (t) - 1;
    int total = 0, spare, nexpand = 0;
    int c;

    if (t->min_widths == NULL)
    {
        g_free (t->widths);
        t->widths = NULL;
    }
    else
    {
        if (t->widths == NULL)
            t->widths = g_new0 (int, t->ncols);
        for (c = 0; c < t->ncols; c++)
        {
            int width = t->col_defs[c].width;

            if (width <= 0)
            {
                width = t->min_widths[c];
                if (t->titles != NULL && t->titles[c] != NULL)
                    width = MAX (width, str_term_width1 (t->titles[c]));
                if (t->col_defs[c].type == TABLE_COL_CHECK)
                    width = MAX (width, 3);
            }
            t->widths[c] = MAX (width, 1);
            total += t->widths[c] + (c < t->ncols - 1 ? 1 : 0);
            if (t->expands[c] && t->col_defs[c].width <= 0)
                nexpand++;
        }
        spare = cols - total;
        if (spare > 0 && nexpand > 0)
            for (c = 0; c < t->ncols; c++)
                if (t->expands[c] && t->col_defs[c].width <= 0)
                {
                    int share = spare / nexpand;

                    t->widths[c] += share;
                    spare -= share;
                    nexpand--;
                }
    }

    if (t->first_col > table_max_first_col (t))
        t->first_col = table_max_first_col (t);
}

/* --------------------------------------------------------------------------------------------- */

int
table_get_first_column (const WTable *t)
{
    return t->first_col;
}

/* --------------------------------------------------------------------------------------------- */

void
table_scroll_columns (WTable *t, int delta)
{
    int max_first = table_max_first_col (t);

    t->first_col = CLAMP (t->first_col + delta, 0, max_first);
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

void
table_set_quick_search (WTable *t, int col, gboolean letters, gboolean case_sensitive)
{
    t->qs_enabled = TRUE;
    t->qs_col = col;
    t->qs_letters = letters;
    t->qs_case_sensitive = case_sensitive;
    // letters that start a search are no hotkeys of the buttons, nor are Alt-S and Alt-Shift-S
    widget_set_options (WIDGET (t), WOP_IS_INPUT, letters);
    widget_set_options (WIDGET (t), WOP_WANT_ALT_KEYS, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
table_quick_search_stop (WTable *t)
{
    int current;

    if (!t->qs_active)
        return;

    current = table_get_current (t);
    t->qs_active = FALSE;
    t->qs_filtering = FALSE;
    widget_set_options (WIDGET (t), WOP_IS_INPUT, t->qs_letters);
    t->qs_chpoint = 0;
    if (t->qs_text != NULL)
        g_string_truncate (t->qs_text, 0);
    if (t->qs_rows != NULL)
    {
        // all the rows are back, the one that was current still is
        g_array_free (t->qs_rows, TRUE);
        t->qs_rows = NULL;
    }
    table_go (t, current);
    table_qs_show_label (t);
}

/* --------------------------------------------------------------------------------------------- */

void
table_set_quick_search_text (WTable *t, const char *(*text_of) (void *data, int row))
{
    t->qs_text_of = text_of;
}

/* --------------------------------------------------------------------------------------------- */

void
table_set_quick_filter_fuzzy (WTable *t, gboolean letters_filter,
                              int (*group_of) (void *data, int row))
{
    t->qs_fuzzy = TRUE;
    t->qs_letters_filter = letters_filter;
    t->qs_group_of = group_of;
}

/* --------------------------------------------------------------------------------------------- */

void
table_set_quick_search_label (WTable *t, WHLine *label)
{
    t->qs_label = label;
}

/* --------------------------------------------------------------------------------------------- */
