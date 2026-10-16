/*
   Line up three files for the merge view of the diff viewer.

   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "lib/global.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/* A diff statement as two ranges of lines, counted from 0 */
typedef struct
{
    int side_start;
    int side_count;
    int mid_start;
    int mid_count;
} hunk_t;

/* Where the walk over the statements of one side stands */
typedef struct
{
    const GArray *ops;
    guint next;
    int delta;  // side line minus middle line in the lines both have
} side_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static hunk_t
op_to_hunk (const DIFFCMD *op)
{
    hunk_t h;

    switch (op->cmd)
    {
    case 'a':
        h.side_start = op->a[0][0];
        h.side_count = 0;
        h.mid_start = op->a[1][0] - 1;
        h.mid_count = op->a[1][1] - op->a[1][0] + 1;
        break;
    case 'd':
        h.side_start = op->a[0][0] - 1;
        h.side_count = op->a[0][1] - op->a[0][0] + 1;
        h.mid_start = op->a[1][0];
        h.mid_count = 0;
        break;
    default:
        h.side_start = op->a[0][0] - 1;
        h.side_count = op->a[0][1] - op->a[0][0] + 1;
        h.mid_start = op->a[1][0] - 1;
        h.mid_count = op->a[1][1] - op->a[1][0] + 1;
        break;
    }

    return h;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
side_peek (const side_t *s, hunk_t *h)
{
    if (s->next >= s->ops->len)
        return FALSE;
    *h = op_to_hunk (&g_array_index (s->ops, DIFFCMD, s->next));
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The lines of one side in the chunk [mid_start, mid_end) of the middle: the statements of
   that side inside the chunk are taken, the lines between them map one to one. */
static gboolean
side_take (side_t *s, int mid_start, int mid_end, int last, int *start, int *count)
{
    gboolean changed = FALSE;
    hunk_t h;

    *start = mid_start + s->delta;
    while ((int) s->next <= last)
    {
        h = op_to_hunk (&g_array_index (s->ops, DIFFCMD, s->next));
        s->delta += h.side_count - h.mid_count;
        s->next++;
        changed = TRUE;
    }
    *count = mid_end + s->delta - *start;

    return changed;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Line up two files against a third one in the middle.
 *
 * Statements of either side that overlap in the middle file make one chunk, so do insertions
 * at one place.  Outside the chunks the three files have the same lines.
 *
 * @param ops_left statements from the left file to the middle one
 * @param ops_right statements from the right file to the middle one
 * @param same tells whether the left and right lines of a chunk are the same text
 * @param ctx passed to @same
 * @param chunks MERGE3_CHUNK list to fill
 */

void
dview_align3 (const GArray *ops_left, const GArray *ops_right, merge3_same_fn same, void *ctx,
              GArray *chunks)
{
    side_t side[2] = { { ops_left, 0, 0 }, { ops_right, 0, 0 } };
    int row = 0;
    int mid_done = 0;

    while (TRUE)
    {
        MERGE3_CHUNK c;
        hunk_t h[2];
        gboolean has[2];
        int last[2];
        int mid_start, mid_end;
        int k;
        gboolean grew;

        has[0] = side_peek (&side[0], &h[0]);
        has[1] = side_peek (&side[1], &h[1]);
        if (!has[0] && !has[1])
            break;

        k = (!has[1] || (has[0] && h[0].mid_start <= h[1].mid_start)) ? 0 : 1;
        mid_start = h[k].mid_start;
        mid_end = h[k].mid_start + h[k].mid_count;
        last[k] = (int) side[k].next;
        last[k ^ 1] = (int) side[k ^ 1].next - 1;

        do
        {
            grew = FALSE;
            for (k = 0; k < 2; k++)
            {
                guint n = (guint) (last[k] + 1);

                if (n < side[k].ops->len)
                {
                    hunk_t t = op_to_hunk (&g_array_index (side[k].ops, DIFFCMD, n));

                    /* overlapping, or both at one point of the middle; changes that
                       only touch stay apart, so lines put next to each other from
                       both sides are no conflict */
                    if (t.mid_start < mid_end || (t.mid_start == mid_start && mid_start == mid_end))
                    {
                        mid_end = MAX (mid_end, t.mid_start + t.mid_count);
                        last[k]++;
                        grew = TRUE;
                    }
                }
            }
        }
        while (grew);

        has[0] = side_take (&side[0], mid_start, mid_end, last[0], &c.start[DIFF_LEFT],
                            &c.count[DIFF_LEFT]);
        has[1] = side_take (&side[1], mid_start, mid_end, last[1], &c.start[DIFF_RIGHT],
                            &c.count[DIFF_RIGHT]);
        c.start[DIFF_MERGED] = mid_start;
        c.count[DIFF_MERGED] = mid_end - mid_start;

        if (!has[1])
            c.kind = MERGE3_LEFT;
        else if (!has[0])
            c.kind = MERGE3_RIGHT;
        else if (same != NULL
                 && same (ctx, c.start[DIFF_LEFT], c.count[DIFF_LEFT], c.start[DIFF_RIGHT],
                          c.count[DIFF_RIGHT]))
            c.kind = MERGE3_SAME;
        else
            c.kind = MERGE3_CONFLICT;

        c.first_row = row + mid_start - mid_done;
        c.rows = MAX (c.count[DIFF_LEFT], MAX (c.count[DIFF_MERGED], c.count[DIFF_RIGHT]));
        row = c.first_row + c.rows;
        mid_done = mid_end;

        g_array_append_val (chunks, c);
    }
}

/* --------------------------------------------------------------------------------------------- */
