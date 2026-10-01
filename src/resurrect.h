/** \file resurrect.h
 *  \brief Header: M-Commander that survives the loss of its terminal
 */

#ifndef MC__RESURRECT_H
#define MC__RESURRECT_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/* The "immortal" option of [Midnight-Commander]; --mortal turns it off for one run */
extern gboolean resurrect_immortal;

/*** declarations of public functions ************************************************************/

/* Offer the waiting M-Commanders of the user, if there are any, and make this one able to wait
   for a terminal when its own is gone. May not return: then the terminal went to another one. */
void resurrect_start (void);

/* Tell the terminal this one was started in, if it is not the first, how mc has finished. */
void resurrect_finish (int exit_code);

/*** inline functions ****************************************************************************/

#endif
