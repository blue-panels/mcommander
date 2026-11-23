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

#ifndef MC__WIDGET_PICTURE_H
#define MC__WIDGET_PICTURE_H

#include "lib/tty/picture.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* A placement lives until its owner replaces or clears it, or the owner is destroyed.
   After every screen refresh it is shown when it can be seen whole, and hidden whole
   otherwise. @cells are screen coordinates. */
void widget_picture_set (Widget *owner, guint slot, mc_picture_t *pic, const WRect *cells);
void widget_picture_clear (Widget *owner, guint slot);
/* Clear the slots from @first_slot on */
void widget_picture_trim (Widget *owner, guint first_slot);
void widget_picture_clear_all (Widget *owner);

/* Nothing covers @cells of @owner: the owner is visible in the top dialog, no widget drawn
   after it overlaps the cells, and the cells are inside the owner. */
gboolean widget_picture_visible (const Widget *owner, const WRect *cells);

/*** inline functions ****************************************************************************/

#endif
