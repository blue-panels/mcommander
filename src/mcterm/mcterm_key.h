/** \file mcterm_key.h
 *  \brief Header: keys of the embedded terminal, encoded for its child
 */

#ifndef MC__MCTERM_KEY_H
#define MC__MCTERM_KEY_H

#include "lib/global.h"
#include "lib/mcconfig.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** declarations of public functions ************************************************************/

void mcterm_key_table_init (const char *global_config_path, mc_config_t *cfg);
size_t mcterm_encode_key_xterm (int key, unsigned char *buf, size_t bufsz, gboolean app_cursor);
/* The key as a far2l terminal reports it: a press and a release, APC f2l:<base64> BEL each, of the
   virtual key, character and Windows control key state. 0 when the key has no such form (a byte of
   a character out of ASCII, a key of no virtual key), and the caller sends it as xterm would. */
size_t mcterm_encode_key_far2l (int key, unsigned char *buf, size_t bufsz);

/*** inline functions ****************************************************************************/

#endif /* MC__MCTERM_KEY_H */
