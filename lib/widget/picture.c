/*
   Widgets for the M-Commander
   Pictures on the screen: where they are and whether they can be seen

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
#include "lib/tty/tty.h"
#include "lib/widget.h"

#include "picture.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct
{
    Widget *owner;
    guint slot;
    mc_picture_t *pic;
    WRect cells;
    gboolean shown;  // written to the terminal after the last refresh
} placement_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static GList *placements = NULL;
static gboolean painter_added = FALSE;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
placement_free (placement_t *p)
{
    /* the pixels stay on the terminal until the cells under them are written again */
    if (p->shown)
        tty_touch_area (p->cells.y, p->cells.x, p->cells.lines, p->cells.cols);
    mc_picture_unref (p->pic);
    g_free (p);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
rect_inside (const WRect *inner, const WRect *outer)
{
    return inner->y >= outer->y && inner->x >= outer->x
        && inner->y + inner->lines <= outer->y + outer->lines
        && inner->x + inner->cols <= outer->x + outer->cols;
}

/* --------------------------------------------------------------------------------------------- */

static void
write_sixel (const placement_t *p)
{
    const char *data;
    gsize len;
    char move[32];

    data = mc_picture_sixel (p->pic, &len);
    g_snprintf (move, sizeof (move), ESC_STR "7" ESC_STR "[%d;%dH", p->cells.y + 1, p->cells.x + 1);
    tty_raw_write (move, strlen (move));
    tty_raw_write (data, len);
    tty_raw_write (ESC_STR "8", 2);
}

/* --------------------------------------------------------------------------------------------- */

/* After every refresh. A sixel picture is written again every time: any cell the refresh
   wrote under it has erased that part of it. One that can no longer be seen gets its cells
   written again over the pixels. */
static void
picture_painter (void *data)
{
    const gboolean draws = tty_graphics_backend () == TTY_GRAPHICS_SIXEL && tty_draws_pictures ();
    gboolean hidden = FALSE;
    GList *l;

    (void) data;

    for (l = placements; l != NULL; l = g_list_next (l))
    {
        placement_t *p = l->data;

        if (draws && widget_picture_visible (p->owner, &p->cells))
        {
            write_sixel (p);
            p->shown = TRUE;
        }
        else if (p->shown)
        {
            tty_touch_area (p->cells.y, p->cells.x, p->cells.lines, p->cells.cols);
            p->shown = FALSE;
            hidden = TRUE;
        }
    }

    // the painters run again after this refresh, over the new screen
    if (hidden)
        tty_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

static GList *
placement_find (const Widget *owner, guint slot)
{
    GList *l;

    for (l = placements; l != NULL; l = g_list_next (l))
    {
        const placement_t *p = l->data;

        if (p->owner == owner && p->slot == slot)
            return l;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
placement_remove_if (const Widget *owner, guint first_slot)
{
    GList *l = placements;

    while (l != NULL)
    {
        GList *next = g_list_next (l);
        placement_t *p = l->data;

        if (p->owner == owner && p->slot >= first_slot)
        {
            placements = g_list_delete_link (placements, l);
            placement_free (p);
        }
        l = next;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
widget_picture_set (Widget *owner, guint slot, mc_picture_t *pic, const WRect *cells)
{
    GList *l;
    placement_t *p;

    if (!painter_added)
    {
        tty_painter_add (picture_painter, NULL);
        painter_added = TRUE;
    }

    l = placement_find (owner, slot);
    if (l != NULL)
    {
        p = l->data;
        if (p->pic == pic && rects_are_equal (&p->cells, cells))
            return;

        // moved or replaced: the old pixels go with the next refresh
        if (p->shown)
            tty_touch_area (p->cells.y, p->cells.x, p->cells.lines, p->cells.cols);
        p->shown = FALSE;
        mc_picture_unref (p->pic);
    }
    else
    {
        p = g_new0 (placement_t, 1);
        p->owner = owner;
        p->slot = slot;
        placements = g_list_append (placements, p);
    }

    p->pic = mc_picture_ref (pic);
    p->cells = *cells;
}

/* --------------------------------------------------------------------------------------------- */

void
widget_picture_clear (Widget *owner, guint slot)
{
    GList *l;

    l = placement_find (owner, slot);
    if (l != NULL)
    {
        placement_t *p = l->data;

        placements = g_list_delete_link (placements, l);
        placement_free (p);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
widget_picture_trim (Widget *owner, guint first_slot)
{
    placement_remove_if (owner, first_slot);
}

/* --------------------------------------------------------------------------------------------- */

void
widget_picture_clear_all (Widget *owner)
{
    placement_remove_if (owner, 0);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
widget_picture_visible (const Widget *owner, const WRect *cells)
{
    const Widget *w;

    if (!rect_inside (cells, &owner->rect))
        return FALSE;

    // a dialog is its own owner
    for (w = owner; w->owner != NULL && CONST_WIDGET (w->owner) != w; w = CONST_WIDGET (w->owner))
    {
        GList *l;

        if (!widget_get_state (w, WST_VISIBLE))
            return FALSE;

        // the widgets of the group are drawn in the order of the list
        l = g_list_find (w->owner->widgets, w);
        for (l = l != NULL ? g_list_next (l) : NULL; l != NULL; l = g_list_next (l))
        {
            const Widget *above = CONST_WIDGET (l->data);

            if (widget_get_state (above, WST_VISIBLE) && rects_are_overlapped (&above->rect, cells))
                return FALSE;
        }
    }

    // w is the dialog now: it has to be the top one, and not put aside
    return top_dlg != NULL && top_dlg->data == w && !widget_get_state (w, WST_SUSPENDED);
}

/* --------------------------------------------------------------------------------------------- */
