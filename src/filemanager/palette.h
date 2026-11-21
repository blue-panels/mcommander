/** \file palette.h
 *  \brief Header: file manager command palette
 */

#ifndef MC__PALETTE_H
#define MC__PALETTE_H

#include "lib/widget.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    long command;       /* CK_IgnoreKey for a directory */
    const menu_t *menu; /* the menu of the command */
    char *directory;    /* a hotlist directory (owned by the caller), else NULL */
} palette_choice_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* The commands of the menus of @menubar but @skip, and the hotlist; FALSE when nothing is chosen */
gboolean command_palette_run (const WMenuBar *menubar, const menu_t *skip,
                              palette_choice_t *choice);

/*** inline functions ****************************************************************************/

#endif /* MC__PALETTE_H */
