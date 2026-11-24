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

/* Raster attributes can declare any size in a few bytes */
#define MC_SIXEL_MAX_PIXELS (4096 * 1024)

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* Data after the 'q': size from the raster attributes, else from what the data draws */
void mc_sixel_measure (const char *data, gsize len, int *width, int *height);

/* @dcs is the whole DCS; NULL if empty or too big. g_free() the result. */
guint8 *mc_sixel_decode (const char *dcs, gsize len, int *width, int *height);

/*** inline functions ****************************************************************************/

#endif
