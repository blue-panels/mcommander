/*
   Interface to the terminal controlling library for the M-Commander
   The protocol the terminal draws pictures with

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

#include "graphics.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static tty_graphics_t backend = TTY_GRAPHICS_NONE;
static guint epoch = 0;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
term_is (const char *term, const char *name)
{
    return term != NULL && strncmp (term, name, strlen (name)) == 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

tty_graphics_t
tty_graphics_request (const char *graphics_env, const char *sixel_env, const char *term)
{
    if (graphics_env != NULL && graphics_env[0] != '\0')
    {
        if (strcmp (graphics_env, "none") == 0)
            return TTY_GRAPHICS_NONE;
        if (strcmp (graphics_env, "sixel") == 0)
            return TTY_GRAPHICS_SIXEL;
        if (strcmp (graphics_env, "kitty") == 0)
            return TTY_GRAPHICS_KITTY;
    }
    else if (sixel_env != NULL)
    {
        if (sixel_env[0] == '0')
            return TTY_GRAPHICS_NONE;
        if (sixel_env[0] == '1')
            return TTY_GRAPHICS_SIXEL;
    }

    // screen and tmux do not pass the pictures through
    if (term_is (term, "screen") || term_is (term, "tmux"))
        return TTY_GRAPHICS_NONE;

    return TTY_GRAPHICS_AUTO;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_graphics_ask_kitty (tty_graphics_t request, const char *term)
{
    return request == TTY_GRAPHICS_AUTO && !term_is (term, "linux");
}

/* --------------------------------------------------------------------------------------------- */

tty_graphics_t
tty_graphics_pick (tty_graphics_t request, gboolean sixel_answer, gboolean kitty_answer)
{
    if (request != TTY_GRAPHICS_AUTO)
        return request;
    if (sixel_answer)
        return TTY_GRAPHICS_SIXEL;
    if (kitty_answer)
        return TTY_GRAPHICS_KITTY;
    return TTY_GRAPHICS_NONE;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_graphics_set_backend (tty_graphics_t b)
{
    backend = b == TTY_GRAPHICS_AUTO ? TTY_GRAPHICS_NONE : b;
    epoch++;
}

/* --------------------------------------------------------------------------------------------- */

tty_graphics_t
tty_graphics_backend (void)
{
    return backend;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_draws_pictures (void)
{
    return backend == TTY_GRAPHICS_SIXEL || backend == TTY_GRAPHICS_KITTY;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_graphics_reset_images (void)
{
    epoch++;
}

/* --------------------------------------------------------------------------------------------- */

guint
tty_graphics_epoch (void)
{
    return epoch;
}

/* --------------------------------------------------------------------------------------------- */
