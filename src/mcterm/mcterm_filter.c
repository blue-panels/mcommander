/*
   Terminal widget mcterm for the M-Commander
   The output of the terminal cut down to the rows that match, and searched

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
#include "lib/search.h"
#include "lib/util.h"  // MC_PTR_FREE

#include "mcterm_filter.h"
#include "mcterm_select.h"

/*** file scope functions ************************************************************************/

/* The text of a row has one character where a wide one takes two cells. */
static int
mcterm_filter_col_to_char (mcview_vterm_t *vt, gint64 row, int col)
{
    int n = 0;
    int c;

    for (c = 0; c < col; c++)
    {
        const mcview_vterm_cell_t *cell = mcterm_sel_cell_at (vt, row, c);

        if (cell == NULL || cell->ch != MCVIEW_VTERM_WIDE_TAIL)
            n++;
    }

    return n;
}

/* --------------------------------------------------------------------------------------------- */

static int
mcterm_filter_char_to_col (mcview_vterm_t *vt, gint64 row, int cols, int index)
{
    int n = 0;
    int c;

    for (c = 0; c < cols; c++)
    {
        const mcview_vterm_cell_t *cell = mcterm_sel_cell_at (vt, row, c);

        if (cell != NULL && cell->ch == MCVIEW_VTERM_WIDE_TAIL)
            continue;
        if (n == index)
            return c;
        n++;
    }

    return cols;
}

/* --------------------------------------------------------------------------------------------- */

/* What a log is filtered by is looked for the way the viewer looks for it:
   a plain string, of either case. */
static mc_search_t *
mcterm_filter_search_new (const char *pattern)
{
    mc_search_t *search;

    search = mc_search_new (pattern, NULL);
    if (search != NULL)
    {
        search->search_type = MC_SEARCH_T_NORMAL;
        search->is_case_sensitive = FALSE;
    }

    return search;
}

/* --------------------------------------------------------------------------------------------- */

/* The last match in @text that starts at byte @limit or before it, as its first byte and its
   length in bytes. */
static gboolean
mcterm_filter_last_match (mc_search_t *search, const char *text, gsize limit, gsize *start,
                          gsize *len)
{
    const gsize text_len = strlen (text);
    gsize from = 0;
    gboolean found = FALSE;

    while (from <= limit && from < text_len)
    {
        gsize found_len = 0;
        gsize at;

        if (!mc_search_run (search, text, (off_t) from, (off_t) text_len, &found_len))
            break;

        at = (gsize) search->normal_offset;
        if (at > limit)
            break;

        *start = at;
        *len = found_len;
        found = TRUE;
        from = (gsize) (g_utf8_next_char (text + at) - text);
    }

    return found;
}

/* --------------------------------------------------------------------------------------------- */

/* The first match in @text that starts at byte @limit or after it, as its first byte and its
   length in bytes. */
static gboolean
mcterm_filter_first_match (mc_search_t *search, const char *text, gsize limit, gsize *start,
                           gsize *len)
{
    const gsize text_len = strlen (text);
    gsize found_len = 0;

    if (limit >= text_len)
        return FALSE;

    if (!mc_search_run (search, text, (off_t) limit, (off_t) text_len, &found_len))
        return FALSE;

    *start = (gsize) search->normal_offset;
    *len = found_len;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_filter_active (const mcterm_filter_t *f)
{
    return (f != NULL && f->pattern != NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_filter_clear (mcterm_filter_t *f)
{
    if (f == NULL)
        return;

    MC_PTR_FREE (f->pattern);
    g_clear_pointer (&f->rows, g_array_unref);
    f->top = 0;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_filter_apply (mcterm_filter_t *f, mcview_vterm_t *vt, int cols, gint64 newest,
                     const char *pattern)
{
    mc_search_t *search;
    GArray *rows;
    gint64 row;
    gint64 oldest;

    if (f == NULL || vt == NULL || cols <= 0 || pattern == NULL || *pattern == '\0')
        return FALSE;

    search = mcterm_filter_search_new (pattern);
    if (search == NULL)
        return FALSE;

    rows = g_array_new (FALSE, FALSE, sizeof (gint64));
    oldest = mcview_vterm_scrolled_rows (vt) - mcview_vterm_history_len (vt);

    for (row = oldest; row <= newest; row++)
    {
        char *text;

        text = mcterm_filter_row_text (vt, row, cols);
        if (text == NULL)
            continue;

        if (*text != '\0' && mc_search_run (search, text, 0, strlen (text), NULL))
            g_array_append_val (rows, row);

        g_free (text);
    }

    mc_search_free (search);

    if (rows->len == 0)
    {
        g_array_unref (rows);
        return FALSE;
    }

    mcterm_filter_clear (f);
    f->pattern = g_strdup (pattern);
    f->rows = rows;
    f->top = 0;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

int
mcterm_filter_len (const mcterm_filter_t *f)
{
    if (f == NULL || f->rows == NULL)
        return 0;

    return (int) f->rows->len;
}

/* --------------------------------------------------------------------------------------------- */

gint64
mcterm_filter_row (const mcterm_filter_t *f, int index)
{
    if (index < 0 || index >= mcterm_filter_len (f))
        return -1;

    return g_array_index (f->rows, gint64, index);
}

/* --------------------------------------------------------------------------------------------- */

int
mcterm_filter_index (const mcterm_filter_t *f, gint64 row)
{
    int lo = 0;
    int hi = mcterm_filter_len (f) - 1;

    if (hi < 0)
        return 0;

    while (lo < hi)
    {
        const int mid = lo + (hi - lo) / 2;

        if (g_array_index (f->rows, gint64, mid) < row)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_filter_find (mcview_vterm_t *vt, int cols, gint64 newest, const char *pattern, gint64 row,
                    int col, gboolean up, gint64 *found_row, int *found_col, int *found_width)
{
    mc_search_t *search;
    gint64 oldest, left;
    gboolean found = FALSE;

    if (vt == NULL || cols <= 0 || pattern == NULL || *pattern == '\0')
        return FALSE;

    oldest = mcview_vterm_scrolled_rows (vt) - mcview_vterm_history_len (vt);
    // The cursor is off the output: start from the end the search runs away from.
    if (row < oldest || row > newest)
    {
        row = up ? newest : oldest;
        col = up ? cols : 0;
    }

    search = mcterm_filter_search_new (pattern);
    if (search == NULL)
        return FALSE;

    /* Every row once, and the row it starts on a second time at the end of the round: the part
       of it the search has already run over comes last. */
    for (left = newest - oldest + 2; left > 0 && !found; left--)
    {
        char *text;

        text = mcterm_filter_row_text (vt, row, cols);
        if (text != NULL && col >= 0)
        {
            const glong chars = g_utf8_strlen (text, -1);
            const int at = mcterm_filter_col_to_char (vt, row, col);
            const gsize limit = (at >= chars)
                ? strlen (text)
                : (gsize) (g_utf8_offset_to_pointer (text, at) - text);
            gsize start, len;
            gboolean hit;

            hit = up ? mcterm_filter_last_match (search, text, limit, &start, &len)
                     : mcterm_filter_first_match (search, text, limit, &start, &len);
            if (hit)
            {
                const int first = (int) g_utf8_pointer_to_offset (text, text + start);

                *found_row = row;
                *found_col = mcterm_filter_char_to_col (vt, row, cols, first);
                *found_width =
                    mcterm_filter_char_to_col (
                        vt, row, cols, first + (int) g_utf8_strlen (text + start, (gssize) len))
                    - *found_col;
                found = TRUE;
            }
        }
        g_free (text);

        // The next row on that side, and past the last one round to the other end.
        if (up)
        {
            row = (row > oldest) ? row - 1 : newest;
            col = cols;
        }
        else
        {
            row = (row < newest) ? row + 1 : oldest;
            col = 0;
        }
    }

    mc_search_free (search);

    return found;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcterm_filter_row_text (mcview_vterm_t *vt, gint64 row, int cols)
{
    GString *text;
    gsize last_word = 0;
    int col;

    if (vt == NULL || cols <= 0)
        return NULL;

    text = g_string_sized_new (cols);

    for (col = 0; col < cols; col++)
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
    g_string_truncate (text, last_word);

    return g_string_free (text, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
