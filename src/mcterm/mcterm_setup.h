/** \file mcterm_setup.h
 *  \brief Header: shell integration setup strings written into the pty
 */

#ifndef MC__MCTERM_SETUP_H
#define MC__MCTERM_SETUP_H

#include "lib/shell.h"  // shell_type_t

/*** typedefs(not structures) and defined constants **********************************************/

/* Stands in a setup string where the per-session token goes. */
#define MCTERM_TOKEN_PLACEHOLDER "@MCTOKEN@"

/*** structures declarations (and typedefs of structures)*****************************************/

/* What it takes to start a shell with the integration already in it: the startup files of
   zsh, or the -C option of fish. */
typedef struct mcterm_shell_rc mcterm_shell_rc_t;

/*** declarations of public functions ************************************************************/

const char *mcterm_shell_setup (shell_type_t shell_type);
/* A copy of @setup with every placeholder replaced by @token. */
char *mcterm_setup_with_token (const char *setup, const char *token);
/* How to start a shell of type @shell_type with the integration in it, @token in every mark
   it sends back; NULL for a shell that is typed into instead, or when the files of bash or zsh
   cannot be written. */
mcterm_shell_rc_t *mcterm_shell_rc_new (shell_type_t shell_type, const char *token);
/* In the forked child, before the exec: the environment the shell must start with. */
void mcterm_shell_rc_child_env (const mcterm_shell_rc_t *rc);
/* Put what the shell's argv needs after argv[0] into @argv; the number of arguments added. */
guint mcterm_shell_rc_args (const mcterm_shell_rc_t *rc, const char **argv, guint argv_size);
/* Take the files away again; the shell has no use for them once it has started. */
void mcterm_shell_rc_free (mcterm_shell_rc_t *rc);

#endif
