/*
   Interface to the terminal controlling library for the M-Commander
   A picture as a program drew it, for any backend to show

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

#include "lib/global.h"

#include "picture.h"
#include "sixel.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

struct mc_picture_struct
{
    int refs;
    int width;
    int height;
    GBytes *sixel;
};

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static void (*free_hook) (const mc_picture_t *pic) = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mc_picture_t *
mc_picture_new_sixel (const char *data, gsize len, int width, int height)
{
    mc_picture_t *pic;

    pic = g_new0 (mc_picture_t, 1);
    pic->refs = 1;
    pic->width = width;
    pic->height = height;
    pic->sixel = g_bytes_new (data, len);

    return pic;
}

/* --------------------------------------------------------------------------------------------- */

mc_picture_t *
mc_picture_ref (mc_picture_t *pic)
{
    pic->refs++;
    return pic;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_picture_unref (mc_picture_t *pic)
{
    if (pic == NULL || --pic->refs > 0)
        return;

    if (free_hook != NULL)
        free_hook (pic);
    g_bytes_unref (pic->sixel);
    g_free (pic);
}

/* --------------------------------------------------------------------------------------------- */

int
mc_picture_width (const mc_picture_t *pic)
{
    return pic->width;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_picture_height (const mc_picture_t *pic)
{
    return pic->height;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mc_picture_sixel (const mc_picture_t *pic, gsize *len)
{
    return g_bytes_get_data (pic->sixel, len);
}

/* --------------------------------------------------------------------------------------------- */

gsize
mc_picture_bytes (const mc_picture_t *pic)
{
    return g_bytes_get_size (pic->sixel);
}

/* --------------------------------------------------------------------------------------------- */

guint8 *
mc_picture_rgba (const mc_picture_t *pic, int *width, int *height)
{
    const char *data;
    gsize len;

    data = mc_picture_sixel (pic, &len);
    return mc_sixel_decode (data, len, width, height);
}

/* --------------------------------------------------------------------------------------------- */

void
mc_picture_set_free_hook (void (*hook) (const mc_picture_t *pic))
{
    free_hook = hook;
}

/* --------------------------------------------------------------------------------------------- */
