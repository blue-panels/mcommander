/** \file dnd.h
 *  \brief Header: files dropped on the terminal window (far2l drag and drop)
 */

#ifndef MC__FILEMANAGER_DND_H
#define MC__FILEMANAGER_DND_H

#include "lib/tty/far2l.h"

/*** declarations of public functions ************************************************************/

/* The handler of far2l_dnd_set_handler(): take the files of a drop into the panel it landed on. */
void filemanager_dnd_drop (const far2l_drop_t *drop);

#endif
