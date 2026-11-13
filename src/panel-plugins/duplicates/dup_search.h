/** \file dup_search.h
 *  \brief Header: the progress dialog of a search for duplicates
 */

#ifndef MC__PANEL_PLUGIN_DUP_SEARCH_H
#define MC__PANEL_PLUGIN_DUP_SEARCH_H

#include "lib/global.h"

#include "dup_session.h"

/*** typedefs(not structures) and defined constants **********************************************/

typedef enum
{
    DUP_PROGRESS_DONE = 0,  // the search is over, or was stopped
    DUP_PROGRESS_SHOW,      // "Show found": the search goes on, the panel opens
    DUP_PROGRESS_CANCEL     // nothing to show
} dup_progress_result_t;

/*** declarations of public functions ************************************************************/

/* Shows the progress of @s until it ends or a button is pressed.  @notify_fd: the read end of
   the pipe the threads of @s write to. */
dup_progress_result_t dup_progress_run (dup_session_t *s, int notify_fd);

/* Takes the bytes of the pipe the threads write to */
void dup_drain (int notify_fd);

/*** inline functions ****************************************************************************/

#endif
