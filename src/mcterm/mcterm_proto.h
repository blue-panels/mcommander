/** \file mcterm_proto.h
 *  \brief Header: mcterm shell protocol marks
 */

#ifndef MC__MCTERM_PROTO_H
#define MC__MCTERM_PROTO_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* The key our session token is passed under, in every mark our own shell sends. */
#define MCTERM_MARK_TOKEN_KEY "mc="

/*** enums ***************************************************************************************/

/* The semantic prompt marks of OSC 133, as the shells that speak it emit them. */
typedef enum
{
    MCTERM_MARK_NONE = 0,
    MCTERM_MARK_PROMPT_START,   // 133;A - the prompt begins here
    MCTERM_MARK_PROMPT_END,     // 133;B - the prompt ends, what follows is typed by the user
    MCTERM_MARK_COMMAND_START,  // 133;C - the command is under way
    MCTERM_MARK_COMMAND_DONE,   // 133;D - it finished, with an exit code
    MCTERM_MARK_RPROMPT_START,  // 133;R - ours: the right prompt begins here
    MCTERM_MARK_RPROMPT_END,    // 133;S - ours: the right prompt ends here
} mcterm_mark_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    mcterm_mark_t mark;
    /* Meaningful for MCTERM_MARK_COMMAND_DONE, -1 when the shell sent no code. */
    int exit_code;
} mcterm_osc133_t;

/*** declarations of public functions ************************************************************/

gboolean mcterm_osc133_parse (const char *raw, const char *token, mcterm_osc133_t *out);

/*** inline functions ****************************************************************************/

#endif /* MC__MCTERM_PROTO_H */
