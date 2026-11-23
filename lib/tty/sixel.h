/*
   Interface to the terminal controlling library for the M-Commander
   Sixel data: its size, and its pixels

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

#ifndef MC__TTY_SIXEL_H
#define MC__TTY_SIXEL_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* A few bytes of raster attributes can declare any size: a bigger picture is not decoded */
#define MC_SIXEL_MAX_PIXELS (4096 * 1024)

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* The size in pixels of the sixel data after the 'q'. The raster attributes name it outright;
   without them it is what the data draws: six rows per band, a column per sixel character,
   repeats counted. */
void mc_sixel_measure (const char *data, gsize len, int *width, int *height);

/* The whole DCS, ESC P ... q ... ESC \, as RGBA, 4 bytes a pixel, rows top down.
   Pixels the data does not draw are transparent when P2 is 1, else of color 0.
   NULL for no picture or one over MC_SIXEL_MAX_PIXELS. g_free() the result. */
guint8 *mc_sixel_decode (const char *dcs, gsize len, int *width, int *height);

/*** inline functions ****************************************************************************/

#endif
