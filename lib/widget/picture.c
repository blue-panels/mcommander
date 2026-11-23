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
#include "lib/tty/kitty_graphics.h"
#include "lib/widget.h"

#include "picture.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct
{
    Widget *owner;
    guint slot;
    guint32 pid;  // the kitty placement id
    mc_picture_t *pic;
    WRect cells;
    tty_graphics_t shown;  // how it was written after the last refresh; NONE when it was not
} placement_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static GList *placements = NULL;
static gboolean painter_added = FALSE;

/* Kitty: the id of every picture the terminal has, by the address of the picture. Ids come
   from a high range with a base of its own for each run, so that a program run under Ctrl-O
   does not meet them. */
static GHashTable *kitty_images = NULL;
static guint32 kitty_next_id = 0;
static guint32 kitty_next_pid = 1;
static guint kitty_epoch = 0;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
flush (GString *out)
{
    if (out->len > 0)
        tty_raw_write (out->str, out->len);
    g_string_truncate (out, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_to (GString *out, const WRect *cells)
{
    g_string_append_printf (out, ESC_STR "[%d;%dH", cells->y + 1, cells->x + 1);
}

/* --------------------------------------------------------------------------------------------- */

static guint32
kitty_image_id (const mc_picture_t *pic)
{
    gpointer id;

    /* Also after the terminal may have lost them: a delete for what is gone costs nothing,
       and the table is emptied before anything is sent again */
    if (kitty_images == NULL)
        return 0;
    id = g_hash_table_lookup (kitty_images, pic);
    return GPOINTER_TO_UINT (id);
}

/* --------------------------------------------------------------------------------------------- */

/* The terminal may have lost everything: what it still has goes, and what is needed from
   now on is sent again */
static void
kitty_forget (void)
{
    GList *l;

    if (kitty_images != NULL)
    {
        if (tty_graphics_backend () == TTY_GRAPHICS_KITTY && g_hash_table_size (kitty_images) > 0)
        {
            GString *out = g_string_new (NULL);
            GHashTableIter iter;
            gpointer value;

            g_hash_table_iter_init (&iter, kitty_images);
            while (g_hash_table_iter_next (&iter, NULL, &value))
                tty_kitty_free (out, GPOINTER_TO_UINT (value));
            flush (out);
            g_string_free (out, TRUE);
        }
        g_hash_table_remove_all (kitty_images);
    }
    kitty_epoch = tty_graphics_epoch ();

    for (l = placements; l != NULL; l = g_list_next (l))
    {
        placement_t *p = l->data;

        if (p->shown == TTY_GRAPHICS_KITTY)
            p->shown = TTY_GRAPHICS_NONE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
kitty_picture_freed (const mc_picture_t *pic)
{
    const guint32 id = kitty_image_id (pic);

    if (id != 0 && tty_graphics_backend () == TTY_GRAPHICS_KITTY)
    {
        GString *out = g_string_new (NULL);

        tty_kitty_free (out, id);
        flush (out);
        g_string_free (out, TRUE);
    }
    if (kitty_images != NULL)
        g_hash_table_remove (kitty_images, pic);
}

/* --------------------------------------------------------------------------------------------- */

/* The id of the picture with the terminal, sent now if it was not; 0 when it cannot be */
static guint32
kitty_transmit (const mc_picture_t *pic, GString *out)
{
    guint32 id;
    guint8 *rgba;
    int width, height;

    id = kitty_image_id (pic);
    if (id != 0)
        return id;

    rgba = mc_picture_rgba (pic, &width, &height);
    if (rgba == NULL)
        return 0;

    if (kitty_next_id == 0)
        kitty_next_id = 0x40000000u + (g_random_int () & 0x0fff0000u);
    id = kitty_next_id++;

    tty_kitty_transmit (out, id, rgba, width, height);
    g_free (rgba);
    g_hash_table_insert (kitty_images, (gpointer) pic, GUINT_TO_POINTER (id));

    return id;
}

/* --------------------------------------------------------------------------------------------- */

/* Take the picture off the screen. Returns TRUE when its cells have to be written again. */
static gboolean
placement_unshow (placement_t *p, GString *out)
{
    const tty_graphics_t shown = p->shown;

    p->shown = TTY_GRAPHICS_NONE;

    switch (shown)
    {
    case TTY_GRAPHICS_SIXEL:
        // the pixels stay on the terminal until the cells under them are written again
        tty_touch_area (p->cells.y, p->cells.x, p->cells.lines, p->cells.cols);
        return TRUE;

    case TTY_GRAPHICS_KITTY:
    {
        const guint32 id = kitty_image_id (p->pic);

        if (id != 0)
            tty_kitty_delete_placement (out, id, p->pid);
        return FALSE;
    }

    default:
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
placement_free (placement_t *p)
{
    GString *out = g_string_new (NULL);

    (void) placement_unshow (p, out);
    flush (out);
    g_string_free (out, TRUE);
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
show_sixel (const placement_t *p, GString *out)
{
    const char *data;
    gsize len;

    data = mc_picture_sixel (p->pic, &len);
    g_string_append (out, ESC_STR "7");
    move_to (out, &p->cells);
    g_string_append_len (out, data, (gssize) len);
    g_string_append (out, ESC_STR "8");
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
show_kitty (const placement_t *p, GString *out)
{
    const guint32 id = kitty_transmit (p->pic, out);

    if (id == 0)
        return FALSE;

    g_string_append (out, ESC_STR "7");
    move_to (out, &p->cells);
    tty_kitty_put (out, id, p->pid, p->cells.lines, p->cells.cols);
    g_string_append (out, ESC_STR "8");
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* After every refresh, every picture that can be seen whole is shown again. Sixel: any cell
   the refresh wrote under it has erased that part of it. Kitty: putting the same ids again
   costs a few bytes, moves nothing that is in place and brings back what the terminal lost.
   A sixel picture that can no longer be seen gets its cells written again over the pixels;
   a kitty one is deleted. */
static void
picture_painter (void *data)
{
    const tty_graphics_t backend =
        tty_draws_pictures () ? tty_graphics_backend () : TTY_GRAPHICS_NONE;
    GString *out;
    gboolean rewrite = FALSE;
    GList *l;

    (void) data;

    if (placements == NULL)
        return;

    if (kitty_epoch != tty_graphics_epoch ())
        kitty_forget ();

    out = g_string_new (NULL);

    for (l = placements; l != NULL; l = g_list_next (l))
    {
        placement_t *p = l->data;
        const gboolean visible =
            backend != TTY_GRAPHICS_NONE && widget_picture_visible (p->owner, &p->cells);

        if (p->shown != TTY_GRAPHICS_NONE && (!visible || p->shown != backend))
            rewrite = placement_unshow (p, out) || rewrite;

        if (!visible)
            continue;

        if (backend == TTY_GRAPHICS_SIXEL)
        {
            show_sixel (p, out);
            p->shown = TTY_GRAPHICS_SIXEL;
        }
        else if (show_kitty (p, out))
            p->shown = TTY_GRAPHICS_KITTY;
    }

    flush (out);
    g_string_free (out, TRUE);

    // the painters run again after this refresh, over the new screen
    if (rewrite)
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
        kitty_images = g_hash_table_new (g_direct_hash, g_direct_equal);
        kitty_epoch = tty_graphics_epoch ();
        mc_picture_set_free_hook (kitty_picture_freed);
        painter_added = TRUE;
    }

    l = placement_find (owner, slot);
    if (l != NULL)
    {
        p = l->data;
        if (p->pic == pic && rects_are_equal (&p->cells, cells))
            return;

        /* Moved or replaced. The old sixel pixels go with the next refresh. A kitty placement
           that only moved is put again where it is now, with the same ids. */
        if (p->shown == TTY_GRAPHICS_SIXEL || (p->shown == TTY_GRAPHICS_KITTY && p->pic != pic))
        {
            GString *out = g_string_new (NULL);

            (void) placement_unshow (p, out);
            flush (out);
            g_string_free (out, TRUE);
        }
        mc_picture_unref (p->pic);
    }
    else
    {
        p = g_new0 (placement_t, 1);
        p->owner = owner;
        p->slot = slot;
        p->pid = kitty_next_pid++;
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
