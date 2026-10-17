/** \file runtime_menu.h
 *  \brief Header: menu entries of runtime plugins in the file manager
 */

#ifndef MC__FILEMANAGER_RUNTIME_MENU_H
#define MC__FILEMANAGER_RUNTIME_MENU_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* The workspace the runtime plugins publish file manager actions for */
#define RUNTIME_MENU_WORKSPACE "mc"

/*** declarations of public functions ************************************************************/

void runtime_menu_reload (void);
/* Add the entries of the menu named @menu_path ("File", "Command", ...) to a list that is built
   with g_list_prepend() and reversed at the end; a separator goes before them. */
GList *runtime_menu_prepend_entries (GList *entries, const char *menu_path);
/* Run the action of a menu entry; FALSE when @command is not one of them. */
gboolean runtime_menu_execute (long command);

/*** inline functions ****************************************************************************/

#endif
