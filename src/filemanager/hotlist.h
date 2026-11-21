/** \file hotlist.h
 *  \brief Header: directory hotlist
 */

#ifndef MC__HOTLIST_H
#define MC__HOTLIST_H

/*** typedefs(not structures) and defined constants **********************************************/

#include "panel.h"

/* A directory of the hotlist: its label, its path and the group it is in ("" at the top) */
typedef void (*hotlist_entry_fn) (const char *label, const char *directory, const char *group,
                                  void *data);

/*** enums ***************************************************************************************/

typedef enum
{
    LIST_HOTLIST = 0x01,
    LIST_MOVELIST = 0x02
} hotlist_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void add2hotlist_cmd (WPanel *panel);
char *hotlist_show (WPanel *panel, gboolean *to_other);
gboolean save_hotlist (void);
void done_hotlist (void);
void hotlist_foreach (hotlist_entry_fn fn, void *data);

/*** inline functions ****************************************************************************/
#endif
