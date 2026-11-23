/*
   Interface to the terminal controlling library for the M-Commander
   Commands of the kitty graphics protocol

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

#ifndef MC__TTY_KITTY_GRAPHICS_H
#define MC__TTY_KITTY_GRAPHICS_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* base64 bytes in one APC */
#define TTY_KITTY_CHUNK 4096

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* All commands have q=2: the terminal does not answer */

/* zlib compressed when mc is built with zlib */
void tty_kitty_transmit (GString *out, guint32 id, const guint8 *rgba, int width, int height);
/* At the cursor, in its own size; the cursor does not move. The same ids move it. */
void tty_kitty_put (GString *out, guint32 id, guint32 pid);
/* The image stays with the terminal */
void tty_kitty_delete_placement (GString *out, guint32 id, guint32 pid);
void tty_kitty_free (GString *out, guint32 id);

/*** inline functions ****************************************************************************/

#endif
