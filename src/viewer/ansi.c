/*
   Internal file viewer for the M-Commander
   ANSI escape sequence parser

   Copyright (C) 2026
   Ilia Maslakov il.smind@gmail.com

   This file is part of M-Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see https://www.gnu.org/licenses/.
 */

/** \file ansi.c
 *  \brief Source: ANSI SGR escape sequence parser for mcview
 *
 *  Parses ANSI escape sequences (ESC[...m) from byte stream and extracts
 *  foreground/background color, bold, and underline attributes.
 *
 *  State machine:  NORMAL --ESC--> ESCAPE --[--> CSI --digit/;--> CSI
 *                                                    --m--> apply SGR, back to NORMAL
 *                                                    --letter--> consume, back to NORMAL
 *                  ESCAPE --] P _ ^ X--> STRING --BEL or ESC \--> back to NORMAL
 *                  ESCAPE --other--> consume, back to NORMAL
 *
 *  A string (OSC, DCS, APC, PM, SOS) is consumed whole; of them only OSC 8 means anything here,
 *  it turns the link attribute on and off.  A newline ends an unterminated string and is shown,
 *  so that a stray ESC ] does not hide the rest of the file.
 */

#include <config.h>

#include "lib/global.h"

#include "ansi.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define ESC_CHAR '\033'

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

static int mcview_ansi_color_cube_index (int v);
static int mcview_ansi_rgb_to_256 (int r, int g, int b);
static void mcview_ansi_csi_finalize_param (mcview_ansi_state_t *state);
static void mcview_ansi_apply_sgr (mcview_ansi_state_t *state);
static void mcview_ansi_apply_one_sgr_param (mcview_ansi_state_t *state, int idx);

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/**
 * Map an RGB component (0-255) to the nearest 6x6x6 cube index (0-5).
 * Cube component values: 0, 95, 135, 175, 215, 255.
 * Thresholds are midpoints between adjacent values.
 */
static int
mcview_ansi_color_cube_index (int v)
{
    if (v < 48)
        return 0;
    if (v < 115)
        return 1;
    if (v < 155)
        return 2;
    if (v < 195)
        return 3;
    if (v < 235)
        return 4;
    return 5;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Convert 24-bit RGB to the nearest xterm 256-color palette index.
 * Uses the 6x6x6 color cube (indices 16-231).
 */
static int
mcview_ansi_rgb_to_256 (int r, int g, int b)
{
    r = CLAMP (r, 0, 255);
    g = CLAMP (g, 0, 255);
    b = CLAMP (b, 0, 255);

    return 16 + 36 * mcview_ansi_color_cube_index (r) + 6 * mcview_ansi_color_cube_index (g)
        + mcview_ansi_color_cube_index (b);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Push the current parameter (if any) into params[] array.
 */
static void
mcview_ansi_csi_finalize_param (mcview_ansi_state_t *state)
{
    if (state->param_count < MCVIEW_ANSI_MAX_PARAMS)
    {
        state->params[state->param_count] = state->current_param;
        state->is_colon_sep[state->param_count] = state->next_is_colon;
        state->param_count++;
    }

    state->current_param = 0;
    state->has_current_param = FALSE;
    state->next_is_colon = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Apply one SGR parameter at index idx.
 * Handles extended color sequences (38;5;N and 48;5;N) by consuming
 * additional parameters from the params[] array.
 */
static void
mcview_ansi_apply_one_sgr_param (mcview_ansi_state_t *state, int idx)
{
    int code;

    if (idx >= state->param_count)
        return;

    code = state->params[idx];

    if (code == 0)
    {
        // reset all
        state->fg = MCVIEW_ANSI_COLOR_DEFAULT;
        state->bg = MCVIEW_ANSI_COLOR_DEFAULT;
        state->bold = FALSE;
        state->italic = FALSE;
        state->underline = FALSE;
        state->blink = FALSE;
        state->reverse = FALSE;
        state->conceal = FALSE;
    }
    else if (code == 1)
        state->bold = TRUE;
    else if (code == 3)
        state->italic = TRUE;
    else if (code == 4)
        // 4:0 is no underline; 4:1 to 4:5 are its styles, all drawn as one
        state->underline = !(idx + 1 < state->param_count && state->is_colon_sep[idx + 1]
                             && state->params[idx + 1] == 0);
    else if (code == 5 || code == 6)
        state->blink = TRUE;
    else if (code == 7)
        state->reverse = TRUE;
    else if (code == 8)
        state->conceal = TRUE;
    else if (code == 21)
        // double underline - map to regular underline (ncurses/slang limitation)
        state->underline = TRUE;
    else if (code == 22)
        state->bold = FALSE;
    else if (code == 23)
        state->italic = FALSE;
    else if (code == 24)
        state->underline = FALSE;
    else if (code == 25)
        state->blink = FALSE;
    else if (code == 27)
        state->reverse = FALSE;
    else if (code == 28)
        state->conceal = FALSE;
    else if (code >= 30 && code <= 37)
        state->fg = code - 30;
    else if (code == 38)
    {
        if (idx + 2 < state->param_count && state->params[idx + 1] == 5)
            // extended foreground 256-color: 38;5;N or 38:5:N
            state->fg = state->params[idx + 2];
        else if (idx + 4 < state->param_count && state->params[idx + 1] == 2)
        {
            // truecolor foreground -> approximate to 256-color
            if (idx + 5 < state->param_count && state->is_colon_sep[idx + 1])
                // de jure colon notation: 38:2:CS:R:G:B - skip color space
                state->fg = mcview_ansi_rgb_to_256 (state->params[idx + 3], state->params[idx + 4],
                                                    state->params[idx + 5]);
            else
                // de facto semicolon notation: 38;2;R;G;B
                state->fg = mcview_ansi_rgb_to_256 (state->params[idx + 2], state->params[idx + 3],
                                                    state->params[idx + 4]);
        }
    }
    else if (code == 39)
        state->fg = MCVIEW_ANSI_COLOR_DEFAULT;
    else if (code >= 40 && code <= 47)
        state->bg = code - 40;
    else if (code == 48)
    {
        if (idx + 2 < state->param_count && state->params[idx + 1] == 5)
            // extended background 256-color: 48;5;N or 48:5:N
            state->bg = state->params[idx + 2];
        else if (idx + 4 < state->param_count && state->params[idx + 1] == 2)
        {
            // truecolor background -> approximate to 256-color
            if (idx + 5 < state->param_count && state->is_colon_sep[idx + 1])
                // de jure colon notation: 48:2:CS:R:G:B - skip color space
                state->bg = mcview_ansi_rgb_to_256 (state->params[idx + 3], state->params[idx + 4],
                                                    state->params[idx + 5]);
            else
                // de facto semicolon notation: 48;2;R;G;B
                state->bg = mcview_ansi_rgb_to_256 (state->params[idx + 2], state->params[idx + 3],
                                                    state->params[idx + 4]);
        }
    }
    else if (code == 49)
        state->bg = MCVIEW_ANSI_COLOR_DEFAULT;
    else if (code >= 90 && code <= 97)
        state->fg = code - 90 + 8;
    else if (code >= 100 && code <= 107)
        state->bg = code - 100 + 8;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Apply all accumulated SGR parameters to the color state.
 * Called when 'm' terminates a CSI sequence.
 */
static void
mcview_ansi_apply_sgr (mcview_ansi_state_t *state)
{
    int i;

    // ESC[m with no params is equivalent to ESC[0m (reset)
    if (state->param_count == 0)
    {
        state->fg = MCVIEW_ANSI_COLOR_DEFAULT;
        state->bg = MCVIEW_ANSI_COLOR_DEFAULT;
        state->bold = FALSE;
        state->italic = FALSE;
        state->underline = FALSE;
        state->blink = FALSE;
        state->reverse = FALSE;
        state->conceal = FALSE;
        return;
    }

    for (i = 0; i < state->param_count; i++)
    {
        int code;

        code = state->params[i];

        // skip colon-separated sub-parameters (they belong to the preceding parameter)
        if (state->is_colon_sep[i])
            continue;

        // skip sub-parameters consumed by extended color sequences
        if (code == 38 || code == 48)
        {
            mcview_ansi_apply_one_sgr_param (state, i);

            if (i + 1 < state->param_count && state->is_colon_sep[i + 1])
            {
                // colon notation: skip all colon-separated sub-params
                while (i + 1 < state->param_count && state->is_colon_sep[i + 1])
                    i++;
            }
            else if (i + 2 < state->param_count && state->params[i + 1] == 5)
                i += 2;  // 256-color: 38;5;N - skip 2
            else if (i + 4 < state->param_count && state->params[i + 1] == 2)
                i += 4;  // truecolor: 38;2;R;G;B - skip 4
        }
        else
            mcview_ansi_apply_one_sgr_param (state, i);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Take one byte of an OSC: the number, then for OSC 8 the params and the URI.
 */
static void
mcview_ansi_osc_byte (mcview_ansi_state_t *state, int ch)
{
    switch (state->osc_field)
    {
    case 0:
        if (ch == ';')
            state->osc_field = 1;
        else if (ch >= '0' && ch <= '9' && state->osc_code >= 0 && state->osc_code <= 65535)
            state->osc_code = state->osc_code * 10 + (ch - '0');
        else
            state->osc_code = -1;
        break;
    case 1:
        if (ch == ';')
            state->osc_field = 2;
        break;
    default:
        state->osc_uri = TRUE;
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The string ended with ST or BEL: an OSC 8 with a URI starts a link, one without ends it.
 */
static void
mcview_ansi_finish_string (mcview_ansi_state_t *state)
{
    state->in_string = FALSE;
    state->string_esc = FALSE;

    if (state->in_osc && state->osc_code == 8 && state->osc_field > 0)
        state->link = state->osc_uri;
}

/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

void
mcview_ansi_state_init (mcview_ansi_state_t *state)
{
    state->fg = MCVIEW_ANSI_COLOR_DEFAULT;
    state->bg = MCVIEW_ANSI_COLOR_DEFAULT;
    state->bold = FALSE;
    state->italic = FALSE;
    state->underline = FALSE;
    state->blink = FALSE;
    state->reverse = FALSE;
    state->conceal = FALSE;
    state->link = FALSE;
    state->in_escape = FALSE;
    state->in_csi = FALSE;
    state->in_string = FALSE;
    state->string_esc = FALSE;
    state->csi_private = FALSE;
    state->param_count = 0;
    state->current_param = 0;
    state->has_current_param = FALSE;
    state->next_is_colon = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

mcview_ansi_result_t
mcview_ansi_parse_char (mcview_ansi_state_t *state, int ch)
{
    // State: inside a string, up to BEL or ST (ESC \)
    if (state->in_string)
    {
        if (ch == '\n')
        {
            state->in_string = FALSE;
            return ANSI_RESULT_CHAR;
        }

        if (state->string_esc)
        {
            state->string_esc = FALSE;
            if (ch == '\\')
            {
                mcview_ansi_finish_string (state);
                return ANSI_RESULT_CONSUMED;
            }
            // any other escape cancels the string and starts on its own
            state->in_string = FALSE;
            state->in_escape = TRUE;
            return mcview_ansi_parse_char (state, ch);
        }

        if (ch == 0x07)
            mcview_ansi_finish_string (state);
        else if (ch == ESC_CHAR)
            state->string_esc = TRUE;
        else if (ch == 0x18 || ch == 0x1A)
            state->in_string = FALSE;  // CAN and SUB cancel the string
        else if (state->in_osc)
            mcview_ansi_osc_byte (state, ch);

        return ANSI_RESULT_CONSUMED;
    }

    // State: just saw ESC, waiting for '['
    if (state->in_escape)
    {
        state->in_escape = FALSE;

        if (ch == '[')
        {
            // enter CSI mode
            state->in_csi = TRUE;
            state->csi_private = FALSE;
            state->param_count = 0;
            state->current_param = 0;
            state->has_current_param = FALSE;
            return ANSI_RESULT_CONSUMED;
        }

        if (ch == ']' || ch == 'P' || ch == '_' || ch == '^' || ch == 'X')
        {
            // OSC, DCS, APC, PM, SOS
            state->in_string = TRUE;
            state->string_esc = FALSE;
            state->in_osc = ch == ']';
            state->osc_code = 0;
            state->osc_field = 0;
            state->osc_uri = FALSE;
            return ANSI_RESULT_CONSUMED;
        }

        // ESC followed by anything else: consume the char (e.g., ESC c = RIS)
        return ANSI_RESULT_CONSUMED;
    }

    // State: inside CSI sequence (ESC[...)
    if (state->in_csi)
    {
        if (ch >= '0' && ch <= '9')
        {
            // accumulate digit into current parameter (clamped to prevent overflow)
            if (state->current_param <= 65535)
                state->current_param = state->current_param * 10 + (ch - '0');
            state->has_current_param = TRUE;
            return ANSI_RESULT_CONSUMED;
        }

        if (ch == ';' || ch == ':')
        {
            // parameter separator (; is standard, : is sub-parameter separator)
            mcview_ansi_csi_finalize_param (state);
            if (ch == ':')
                state->next_is_colon = TRUE;
            return ANSI_RESULT_CONSUMED;
        }

        // any letter (0x40-0x7E) terminates CSI
        if (ch >= 0x40 && ch <= 0x7E)
        {
            mcview_ansi_csi_finalize_param (state);
            state->in_csi = FALSE;

            // CSI > 4 ; 2 m (modifyOtherKeys) and the like are not SGR
            if (ch == 'm' && !state->csi_private)
                mcview_ansi_apply_sgr (state);

            // non-'m' terminators: consume without applying to colors
            return ANSI_RESULT_CONSUMED;
        }

        // private markers < = > ? and intermediate bytes: consume
        state->csi_private = TRUE;
        return ANSI_RESULT_CONSUMED;
    }

    // Normal state: check for ESC
    if (ch == ESC_CHAR)
    {
        state->in_escape = TRUE;
        return ANSI_RESULT_CONSUMED;
    }

    // regular displayable character
    return ANSI_RESULT_CHAR;
}

/* --------------------------------------------------------------------------------------------- */
