/** \file mcterm_key.h
 *  \brief Header: keys of the embedded terminal, encoded for its child
 */

#ifndef MC__MCTERM_KEY_H
#define MC__MCTERM_KEY_H

#include "lib/global.h"
#include "lib/mcconfig.h"
#include "lib/tty/key.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** declarations of public functions ************************************************************/

void mcterm_key_table_init (const char *global_config_path, mc_config_t *cfg);
size_t mcterm_encode_key_xterm (int key, unsigned char *buf, size_t bufsz, gboolean app_cursor);
/* As mcterm_encode_key_xterm (), or by the kitty keyboard protocol when the child asked for it */
size_t mcterm_encode_key (int key, guint kitty_flags, unsigned char *buf, size_t bufsz,
                          gboolean app_cursor);
/* A key as the terminal sent it, re-encoded by the kitty flags of the child; 0 when the child
   gets nothing for it (a release it did not ask for, say) */
size_t mcterm_encode_kitty_event (const tty_key_event_t *ev, guint flags, unsigned char *buf,
                                  size_t bufsz, gboolean app_cursor);
/* A character key by the kitty protocol, 0 when the flags leave it plain text */
size_t mcterm_encode_kitty_codepoint (gunichar cp, int mods, guint flags, unsigned char *buf,
                                      size_t bufsz);

/*** inline functions ****************************************************************************/

#endif /* MC__MCTERM_KEY_H */
