/*
   Widgets for the Midnight Commander

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

/** \file scrollbar.c
 *  \brief Source: WScrollBar widget (a scrollbar)
 */

#include <config.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/color.h"
#include "lib/skin.h"
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/* The thumb in the track: its first cell from 0, and its cells */
typedef struct
{
    int track;
    int start;
    int len;
} thumb_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
scrollbar_len (const WScrollBar *b)
{
    const WRect *r = &CONST_WIDGET (b)->rect;

    return b->orientation == SCROLLBAR_VERTICAL ? r->lines : r->cols;
}

/* --------------------------------------------------------------------------------------------- */

static long
scrollbar_max (const WScrollBar *b)
{
    return MAX (0, b->total - b->visible);
}

/* --------------------------------------------------------------------------------------------- */

/* The look of the bar: its own, or that of the skin as it is now */
static const scrollbar_look_t *
scrollbar_look (const WScrollBar *b)
{
    if (b->own_look)
        return &b->look;
    return b->orientation == SCROLLBAR_VERTICAL ? &mc_skin_scrollbar_vert
                                                : &mc_skin_scrollbar_horiz;
}

/* --------------------------------------------------------------------------------------------- */

/* The thumb: as long as the look has it when it fits with room to move, else half the track; at
   the nearest cell to where the view is, so that a thumb dragged to a cell stays there */
static thumb_t
scrollbar_thumb (const WScrollBar *b)
{
    thumb_t t = { 0, 0, 0 };
    const long max = scrollbar_max (b);

    if (max == 0 || scrollbar_len (b) < 4)
        return t;

    t.track = scrollbar_len (b) - 2;
    t.len = (scrollbar_look (b)->thumb_len < t.track) ? scrollbar_look (b)->thumb_len
                                                      : MAX (1, t.track / 2);
    t.start = (int) (((long) (t.track - t.len) * CLAMP (b->pos, 0, max) + max / 2) / max);
    return t;
}

/* --------------------------------------------------------------------------------------------- */

static void
scrollbar_put (const WScrollBar *b, int at, mc_tty_char_t c)
{
    if (b->orientation == SCROLLBAR_VERTICAL)
        widget_gotoyx (CONST_WIDGET (b), at, 0);
    else
        widget_gotoyx (CONST_WIDGET (b), 0, at);
    tty_print_char (c);
}

/* --------------------------------------------------------------------------------------------- */

static void
scrollbar_draw (const WScrollBar *b)
{
    const scrollbar_look_t *look = scrollbar_look (b);
    const thumb_t t = scrollbar_thumb (b);
    const int len = scrollbar_len (b);
    int i;

    if (scrollbar_max (b) == 0 || len <= 0)
        return;

    tty_setcolor (b->color);

    // too short for a thumb: the arrows and the track, so that the column is not left empty
    if (t.track == 0)
    {
        for (i = 0; i < len; i++)
            scrollbar_put (b, i,
                           len < 2            ? look->track
                               : i == 0       ? look->start
                               : i == len - 1 ? look->end
                                              : look->track);
        return;
    }

    scrollbar_put (b, 0, look->start);
    for (i = 0; i < t.track; i++)
        scrollbar_put (b, 1 + i, (i >= t.start && i < t.start + t.len) ? look->thumb : look->track);
    scrollbar_put (b, t.track + 1, look->end);
}

/* --------------------------------------------------------------------------------------------- */

/* Ask the client to move the view to pos */
static void
scrollbar_move (WScrollBar *b, long pos)
{
    Widget *w = WIDGET (b);
    Widget *client = b->client != NULL ? b->client : WIDGET (w->owner);

    pos = CLAMP (pos, 0, scrollbar_max (b));
    if (pos == b->pos)
        return;

    b->pos = pos;
    if (client != NULL)
        send_message (client, w, MSG_NOTIFY, 0, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
scrollbar_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_DRAW:
        scrollbar_draw (CONST_SCROLLBAR (w));
        return MSG_HANDLED;

    // it takes no focus and no keys
    case MSG_FOCUS:
    case MSG_KEY:
        return MSG_NOT_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
scrollbar_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    WScrollBar *b = SCROLLBAR (w);

    if (!scrollbar_needed (b) && b->drag < 0)
    {
        // nothing to scroll: what is under it takes the event
        event->result.abort = TRUE;
        return;
    }

    scrollbar_mouse (b, msg, b->orientation == SCROLLBAR_VERTICAL ? event->y : event->x);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WScrollBar *
scrollbar_new (int y, int x, int len, scrollbar_orientation_t orientation)
{
    WRect r = { y, x, 1, 1 };
    WScrollBar *b;
    Widget *w;

    if (orientation == SCROLLBAR_VERTICAL)
        r.lines = MAX (1, len);
    else
        r.cols = MAX (1, len);

    b = g_new0 (WScrollBar, 1);
    w = WIDGET (b);
    widget_init (w, &r, scrollbar_callback, scrollbar_mouse_callback);
    b->orientation = orientation;
    b->color = CORE_DEFAULT_COLOR;
    b->drag = -1;

    return b;
}

/* --------------------------------------------------------------------------------------------- */

void
scrollbar_set_look (WScrollBar *b, const scrollbar_look_t *look)
{
    b->look = *look;
    b->look.thumb_len = MAX (1, b->look.thumb_len);
    b->own_look = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Put the bar at r, give it the range and the color and draw it: what a widget that keeps a bar
   to itself does when it draws */

void
scrollbar_show (WScrollBar *b, const WRect *r, long total, long visible, long pos, int color)
{
    WRect place = *r;

    widget_set_size_rect (WIDGET (b), &place);
    scrollbar_set_range (b, total, visible, pos);
    scrollbar_set_color (b, color);
    send_message (WIDGET (b), NULL, MSG_DRAW, 0, NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
scrollbar_set_color (WScrollBar *b, int color)
{
    b->color = color;
}

/* --------------------------------------------------------------------------------------------- */

void
scrollbar_set_client (WScrollBar *b, Widget *client)
{
    b->client = client;
}

/* --------------------------------------------------------------------------------------------- */

void
scrollbar_set_range (WScrollBar *b, long total, long visible, long pos)
{
    b->total = MAX (0, total);
    b->visible = MAX (0, visible);
    b->pos = CLAMP (pos, 0, scrollbar_max (b));
}

/* --------------------------------------------------------------------------------------------- */

long
scrollbar_get_pos (const WScrollBar *b)
{
    return b->pos;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
scrollbar_needed (const WScrollBar *b)
{
    return scrollbar_max (b) != 0 && scrollbar_len (b) > 0;
}

/* --------------------------------------------------------------------------------------------- */

void
scrollbar_mouse (WScrollBar *b, mouse_msg_t msg, int at)
{
    const thumb_t t = scrollbar_thumb (b);

    switch (msg)
    {
    case MSG_MOUSE_DOWN:
        // too short for a thumb: the arrows still step
        if (t.track == 0)
        {
            const int len = scrollbar_len (b);

            if (len >= 2 && at <= 0)
                scrollbar_move (b, b->pos - 1);
            else if (len >= 2 && at >= len - 1)
                scrollbar_move (b, b->pos + 1);
            break;
        }
        if (at <= 0)
            scrollbar_move (b, b->pos - 1);
        else if (at >= t.track + 1)
            scrollbar_move (b, b->pos + 1);
        else if (at - 1 < t.start)
            scrollbar_move (b, b->pos - MAX (1, b->visible - 1));
        else if (at - 1 >= t.start + t.len)
            scrollbar_move (b, b->pos + MAX (1, b->visible - 1));
        else
            b->drag = at - 1 - t.start;
        break;

    case MSG_MOUSE_DRAG:
        if (b->drag >= 0 && t.track > t.len)
        {
            const int room = t.track - t.len;
            const int cell = CLAMP (at - 1 - b->drag, 0, room);

            scrollbar_move (b, (long) cell * scrollbar_max (b) / room);
        }
        break;

    case MSG_MOUSE_UP:
        b->drag = -1;
        break;

    case MSG_MOUSE_SCROLL_UP:
    case MSG_MOUSE_SCROLL_LEFT:
        scrollbar_move (b, b->pos - 2);
        break;

    case MSG_MOUSE_SCROLL_DOWN:
    case MSG_MOUSE_SCROLL_RIGHT:
        scrollbar_move (b, b->pos + 2);
        break;

    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */
