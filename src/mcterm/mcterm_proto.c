/*
   Midnight Commander - mcterm shell protocol marks.

   Reads the semantic prompt marks (OSC 133) the shell sends around its prompt and its
   commands, so that the host knows whether the shell is waiting for input or busy.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"

#include "mcterm_proto.h"

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

static mcterm_mark_t
osc133_mark_of (char letter)
{
    switch (letter)
    {
    case 'A':
        return MCTERM_MARK_PROMPT_START;
    case 'B':
        return MCTERM_MARK_PROMPT_END;
    case 'C':
        return MCTERM_MARK_COMMAND_START;
    case 'D':
        return MCTERM_MARK_COMMAND_DONE;
    case 'R':
        return MCTERM_MARK_RPROMPT_START;
    case 'S':
        return MCTERM_MARK_RPROMPT_END;
    default:
        return MCTERM_MARK_NONE;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Whether one ';'-separated field carries the token we expect. */
static gboolean
osc133_field_has_token (const char *field, size_t len, const char *token)
{
    static const size_t key_len = sizeof (MCTERM_MARK_TOKEN_KEY) - 1;

    if (len <= key_len || strncmp (field, MCTERM_MARK_TOKEN_KEY, key_len) != 0)
        return FALSE;

    return (strlen (token) == len - key_len
            && strncmp (field + key_len, token, len - key_len) == 0);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Read a semantic prompt mark from an OSC 133 payload, as vterm kept it: "133;D;0;mc=<token>".
 *
 * With a token expected, a payload that does not carry ours is none of our business: it is the
 * output of some command, or the shell integration of a session on the far side of ssh.
 *
 * @param raw the payload, including the leading "133;"
 * @param token the session token, or NULL to take any mark
 * @param out where the mark lands; untouched when the payload is refused
 * @return TRUE when the payload is a mark of ours
 */

gboolean
mcterm_osc133_parse (const char *raw, const char *token, mcterm_osc133_t *out)
{
    mcterm_mark_t mark;
    gboolean token_seen = FALSE;
    int exit_code = -1;
    const char *p;
    int field_no;

    if (raw == NULL || strncmp (raw, "133;", 4) != 0)
        return FALSE;

    p = raw + 4;

    mark = osc133_mark_of (*p);
    if (mark == MCTERM_MARK_NONE)
        return FALSE;

    p++;
    if (*p != '\0' && *p != ';')
        return FALSE;

    /* The fields after the letter: the exit code of a "done" mark comes first, and the token
       is named, so that either may be absent without the other losing its place. */
    for (field_no = 0; *p == ';'; field_no++)
    {
        const char *field = ++p;
        size_t len;

        while (*p != '\0' && *p != ';')
            p++;
        len = (size_t) (p - field);

        if (token != NULL && osc133_field_has_token (field, len, token))
            token_seen = TRUE;
        else if (field_no == 0 && mark == MCTERM_MARK_COMMAND_DONE && len > 0)
        {
            char *end;
            long code;

            code = strtol (field, &end, 10);
            if (end == field + len && code >= 0 && code <= 255)
                exit_code = (int) code;
        }
    }

    if (token != NULL && !token_seen)
        return FALSE;

    out->mark = mark;
    out->exit_code = exit_code;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
