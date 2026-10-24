/** \file mcterm_screen.h
 *  \brief Header: a program in a terminal of its own, as a screen beside the panels
 */

#ifndef MC__MCTERM_SCREEN_H
#define MC__MCTERM_SCREEN_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

typedef struct mcterm_screen_t mcterm_screen_t;

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

#ifdef ENABLE_MCTERM

/* Start @argv in a terminal of its own, on the whole screen. The program has every key but the
   one of PluginConsole in the panel keymap, Alt-F3, which goes back to the panels. The screen is
   shown by mcterm_screen_show() and joins the screen list then; it stays until the program is gone,
   after an exit with code 0 at once, or as soon as it is in front again, on the next key otherwise,
   and @on_close is called as it goes.
   NULL when @argv[0] is not found or the program could not be started. */
mcterm_screen_t *mcterm_screen_new (const char *title, const char *dir, const char *const *argv,
                                    const char *const *env, void (*on_close) (void *data),
                                    void *data);

/* Bring the screen to the front; it returns when the user goes to another screen or closes
   this one. FALSE when the program had been quit while the screen was not in front: the
   screen is gone then, @on_close called, and @s no more. */
gboolean mcterm_screen_show (mcterm_screen_t *s);

/* The screen stays, but @on_close is no longer called. */
void mcterm_screen_forget (mcterm_screen_t *s);

#endif /* ENABLE_MCTERM */

/*** inline functions ****************************************************************************/

#endif /* MC__MCTERM_SCREEN_H */
