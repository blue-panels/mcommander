/** \file vterm.h
 *  \brief Header: terminal emulator state and events
 */

#ifndef MC__VIEWER_VTERM_H
#define MC__VIEWER_VTERM_H

#include "lib/global.h"

#include "ansi.h"
#include "terminal_buffer.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define MCVIEW_VTERM_MAX_PARAMS 16

/*** enums ***************************************************************************************/

typedef enum
{
    VTERM_CHAR,
    VTERM_SGR,
    VTERM_CR,
    VTERM_LF,
    VTERM_CURSOR_ABS,
    VTERM_CURSOR_FWD,
    VTERM_CURSOR_BACK,
    VTERM_CURSOR_UP,
    VTERM_CURSOR_DOWN,
    VTERM_ERASE_EOL,
    VTERM_ERASE_BOL,
    VTERM_ERASE_LINE,
    VTERM_ERASE_SCREEN,
    VTERM_ERASE_TO_EOS, /* ESC[J  or ESC[0J: cursor to end of screen */
    VTERM_ERASE_TO_BOS, /* ESC[1J: start of screen to cursor */
    VTERM_ALT_SCREEN_ENTER,
    VTERM_ALT_SCREEN_EXIT,
    VTERM_SET_SCROLL_REGION, /* param1=top_row (0-based), param2=bottom_row (0-based) */
    VTERM_CURSOR_ROW_ABS,    /* param1=target_row (0-based), col unchanged */
    VTERM_ERASE_CHARS,       /* param1=count; erase from cursor_col, cursor stays */
    VTERM_DCH,               /* param1=count; delete chars at cursor, shift left, fill right */
    VTERM_ICH,               /* param1=count; insert blanks at cursor, shift right, drop at edge */
    VTERM_RI,                /* ESC M: reverse index -- scroll region down, cursor up */
    VTERM_REPLY,             /* the program asked what the terminal is; ev.reply is the answer */
    VTERM_CONSUMED,
} vterm_result_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    vterm_result_t type;
    int param1;
    int param2;
    gunichar ch;
    const char *reply;  // VTERM_REPLY: static string to write back to the program
    mcview_ansi_state_t ansi;
} vterm_event_t;

typedef struct mcview_vterm_struct mcview_vterm_t;

/* A sixel picture the program drew: where it sits on the screen, in cells, and
   the DCS as it arrived, to be written to the real terminal as is. */
typedef struct
{
    int row; /* top-left cell, screen coordinates */
    int col;
    int rows; /* cells covered */
    int cols;
    int width; /* pixels */
    int height;
    GBytes *data; /* ESC P ... q ... ESC \ */
} mcview_vterm_image_t;

/*** declarations of public functions ************************************************************/

mcview_vterm_t *mcview_vterm_new (void);
void mcview_vterm_free (mcview_vterm_t *vt);

vterm_event_t mcview_vterm_feed (mcview_vterm_t *vt, unsigned char byte);

void mcview_vterm_apply_event (mcview_vterm_t *vt, const vterm_event_t *ev);

int mcview_vterm_cursor_row (const mcview_vterm_t *vt);
int mcview_vterm_cursor_col (const mcview_vterm_t *vt);
gboolean mcview_vterm_in_alt_screen (const mcview_vterm_t *vt);
gboolean mcview_vterm_app_cursor_keys (const mcview_vterm_t *vt);
mcview_terminal_buffer_t *mcview_vterm_buf (mcview_vterm_t *vt);
off_t mcview_vterm_replay_offset (const mcview_vterm_t *vt);
void mcview_vterm_set_replay_offset (mcview_vterm_t *vt, off_t offset);

#define MCVIEW_VTERM_FOLLOW_END (-1)

void mcview_vterm_set_keep_history (mcview_vterm_t *vt, gboolean keep);
int mcview_vterm_history_len (const mcview_vterm_t *vt);
const GArray *mcview_vterm_history_row (const mcview_vterm_t *vt, int index);
gboolean mcview_vterm_history_row_wrapped (const mcview_vterm_t *vt, int index);
// DECAWM, off unless asked for: a character past the last column starts a new row.
void mcview_vterm_set_autowrap (mcview_vterm_t *vt, gboolean autowrap);
/* Where a position of the old width went in the last reflow: rows are absolute,
   as mcview_vterm_scrolled_rows() counts them. FALSE when it is not known. */
gboolean mcview_vterm_reflow_map (const mcview_vterm_t *vt, gint64 abs_row, int col,
                                  gint64 *new_abs_row, int *new_col);
gint64 mcview_vterm_scrolled_rows (const mcview_vterm_t *vt);

int mcview_vterm_dpy_top_row (const mcview_vterm_t *vt);
void mcview_vterm_set_dpy_top_row (mcview_vterm_t *vt, int row);
int mcview_vterm_resolve_top_row (const mcview_vterm_t *vt, int data_lines);
int mcview_vterm_resolve_scrollback_top_row (const mcview_vterm_t *vt, int data_lines);
mcview_terminal_buffer_t *mcview_vterm_compose_scrollback (const mcview_vterm_t *vt, int top_row,
                                                           int rows);
void mcview_vterm_reset (mcview_vterm_t *vt);
// A page of newlines: the screen goes into the history, the last @keep rows stay at the bottom.
void mcview_vterm_page_up (mcview_vterm_t *vt, int keep);
/* Drop the history. What is on the screen stays, and so does the count of the rows
   that ever left it: what is pointed at by number is pointed at still. */
void mcview_vterm_clear_history (mcview_vterm_t *vt);
const char *mcview_vterm_osc7_raw (const mcview_vterm_t *vt);
guint mcview_vterm_osc7_generation (const mcview_vterm_t *vt);
/* The last semantic prompt mark (OSC 133) as it arrived, and a counter of them. */
const char *mcview_vterm_osc133_raw (const mcview_vterm_t *vt);
guint mcview_vterm_osc133_generation (const mcview_vterm_t *vt);
/* The text a program put on the clipboard with OSC 52 (decoded, not NUL-terminated by len), and how
   many times it did; the clipboard is only ever set, never read. */
const char *mcview_vterm_osc52_text (const mcview_vterm_t *vt, gsize *len);
guint mcview_vterm_osc52_generation (const mcview_vterm_t *vt);

/* Sixel pictures. The cell size is what turns pixels into rows and columns;
   whoever knows the terminal sets it. The generation moves whenever the list
   changes, so the host can tell when the pictures need painting again. */
void mcview_vterm_set_cell_size (mcview_vterm_t *vt, int width, int height);
/* Whether to tell the program the terminal draws sixel: in the Device
   Attributes, and in the pixel sizes it asks for (XTWINOPS 14, 16, 18). */
void mcview_vterm_set_sixel (mcview_vterm_t *vt, gboolean sixel);
guint mcview_vterm_images_len (const mcview_vterm_t *vt);
const mcview_vterm_image_t *mcview_vterm_image (const mcview_vterm_t *vt, guint index);
guint mcview_vterm_images_generation (const mcview_vterm_t *vt);

/* Update terminal size; returns TRUE on change. */
gboolean mcview_vterm_set_size (mcview_vterm_t *vt, int rows, int cols);

void mcview_vterm_restore_sync_snapshot (mcview_vterm_t *vt, mcview_terminal_buffer_t *snap_buf,
                                         int snap_cursor_row);

/* Render a terminal buffer region to the TUI screen, in the colors of the skin
   section of whoever is drawing it. */
void mcview_render_terminal_canvas (const mcview_terminal_buffer_t *buf, int top_row, int screen_y,
                                    int screen_x, int rows, int cols,
                                    const mcview_canvas_colors_t *colors);
/* Allocate the colors of the 16-color palette a canvas may be drawn in, before it is drawn. */
void mcview_preload_canvas_colors (const mcview_canvas_colors_t *colors);
/* The section of the viewer itself, which is what mcview draws with. */
void mcview_canvas_colors_viewer (mcview_canvas_colors_t *colors);

/*** inline functions ****************************************************************************/

#endif /* MC__VIEWER_VTERM_H */