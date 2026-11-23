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

#ifndef MC__TTY_GRAPHICS_H
#define MC__TTY_GRAPHICS_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* Any answer with this id, OK or an error, means the terminal knows kitty graphics */
#define TTY_GRAPHICS_KITTY_ID "31"
#define TTY_GRAPHICS_KITTY_QUERY                                                                   \
    ESC_STR "_Gi=" TTY_GRAPHICS_KITTY_ID ",s=1,v=1,a=q,t=d,f=24;AAAA" ESC_STR "\\"

/*** enums ***************************************************************************************/

typedef enum
{
    TTY_GRAPHICS_NONE = 0,
    TTY_GRAPHICS_SIXEL,
    TTY_GRAPHICS_KITTY,
    TTY_GRAPHICS_AUTO  // in a request only: ask the terminal
} tty_graphics_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* MC_GRAPHICS, or the old MC_SIXEL; NONE in screen or tmux unless a protocol is set */
tty_graphics_t tty_graphics_request (const char *graphics_env, const char *sixel_env,
                                     const char *term);
/* Not on the Linux console: it prints an APC it does not know */
gboolean tty_graphics_ask_kitty (tty_graphics_t request, const char *term);
/* AUTO: sixel if DA1 has it, else kitty if the query got an answer */
tty_graphics_t tty_graphics_pick (tty_graphics_t request, gboolean sixel_answer,
                                  gboolean kitty_answer);

void tty_graphics_set_backend (tty_graphics_t backend);
tty_graphics_t tty_graphics_backend (void);
gboolean tty_draws_pictures (void);

/* The terminal may have lost the pictures: resize, alternate screen, new backend */
void tty_graphics_reset_images (void);
guint tty_graphics_epoch (void);

/*** inline functions ****************************************************************************/

#endif
