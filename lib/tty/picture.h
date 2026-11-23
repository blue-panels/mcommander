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

#ifndef MC__TTY_PICTURE_H
#define MC__TTY_PICTURE_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

typedef struct mc_picture_struct mc_picture_t;

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* The DCS ESC P ... q ... ESC \ as it came, and its size in pixels. The bytes are copied. */
mc_picture_t *mc_picture_new_sixel (const char *data, gsize len, int width, int height);
mc_picture_t *mc_picture_ref (mc_picture_t *pic);
void mc_picture_unref (mc_picture_t *pic);

int mc_picture_width (const mc_picture_t *pic);
int mc_picture_height (const mc_picture_t *pic);
/* The sixel source, whole */
const char *mc_picture_sixel (const mc_picture_t *pic, gsize *len);
/* Bytes the picture keeps, for the memory caps */
gsize mc_picture_bytes (const mc_picture_t *pic);

/*** inline functions ****************************************************************************/

#endif
