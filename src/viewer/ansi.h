/** \file ansi.h
 *  \brief Header: ANSI SGR escape sequence parser for mcview
 */

#ifndef MC__VIEWER_ANSI_H
#define MC__VIEWER_ANSI_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/** Maximum number of CSI parameters we track */
#define MCVIEW_ANSI_MAX_PARAMS 16

/** Default (unset) color value */
#define MCVIEW_ANSI_COLOR_DEFAULT (-1)

/*** enums ***************************************************************************************/

/** Result of feeding one byte to the ANSI parser */
typedef enum
{
    ANSI_RESULT_CHAR,    /**< regular character - render with current color */
    ANSI_RESULT_CONSUMED /**< escape sequence byte - skip, don't render */
} mcview_ansi_result_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/* Which skin paints a canvas; -1 where it has no colour of its own. */
typedef struct
{
    const char *section;
    int normal;
    int bold;
    int underline;
    int bold_underline;
} mcview_canvas_colors_t;

/** ANSI SGR parser state */
typedef struct
{
    /* --- public color state (read by renderer) --- */
    int fg; /**< foreground: 0-7 base, 8-15 bright, -1 default */
    int bg; /**< background: 0-7 base, 8-15 bright, -1 default */
    gboolean bold;
    gboolean dim; /**< faint: drawn in a darker color */
    gboolean italic;
    gboolean underline;
    gboolean blink;
    gboolean reverse;
    gboolean conceal; /**< the text is not shown */
    gboolean link;    /**< inside the text of an OSC 8 hyperlink */

    /* --- internal parser state --- */
    gboolean in_escape;   /**< seen ESC, waiting for '[' */
    gboolean in_csi;      /**< inside CSI sequence (ESC[...) */
    gboolean in_string;   /**< inside OSC, DCS, APC, PM or SOS, up to ST or BEL */
    gboolean string_esc;  /**< seen ESC inside the string, ST if a backslash follows */
    gboolean in_osc;      /**< the string is an OSC */
    int osc_code;         /**< the OSC number, -1 if it is not a number */
    int osc_field;        /**< fields of the OSC begun: 0 the number, 1 params, 2 URI */
    gboolean osc_uri;     /**< the URI of an OSC 8 is not empty */
    gboolean csi_private; /**< CSI carries a private marker or intermediate; not an SGR */
    int params[MCVIEW_ANSI_MAX_PARAMS];
    gboolean is_colon_sep[MCVIEW_ANSI_MAX_PARAMS]; /**< TRUE if preceded by ':' */
    int param_count;
    int current_param;          /**< parameter being accumulated */
    gboolean has_current_param; /**< whether current_param has digits */
    gboolean next_is_colon;     /**< next param preceded by ':' */
} mcview_ansi_state_t;

/*** declarations of public functions ************************************************************/

/** Initialize parser state to defaults (no color, not in escape) */
void mcview_ansi_state_init (mcview_ansi_state_t *state);

/** Feed one byte to the parser.
 *  Returns ANSI_RESULT_CHAR if the byte is a displayable character.
 *  Returns ANSI_RESULT_CONSUMED if the byte is part of an escape sequence. */
mcview_ansi_result_t mcview_ansi_parse_char (mcview_ansi_state_t *state, int ch);

/** The color index that draws @color faint (SGR 2), for a 256- or a 16-color terminal. */
int mcview_ansi_dim_color (int color, gboolean use_256);

/*** inline functions ****************************************************************************/

#endif /* MC__VIEWER_ANSI_H */
