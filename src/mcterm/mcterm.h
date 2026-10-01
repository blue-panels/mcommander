/** \file mcterm.h
 *  \brief Header: the terminal widget that runs the shell
 */

#ifndef MC__MCTERM_H
#define MC__MCTERM_H

#include "lib/global.h"
#include "lib/widget.h"
#include "lib/keybind.h"
#include "lib/tty/tty.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* What the session token is introduced by, in every OSC 7 our own shell sends. */
#define MCTERM_OSC7_TOKEN_PREFIX "?mc="

/*** enums ***************************************************************************************/

/* Which way a search of the output runs from the cursor. */
typedef enum
{
    MCTERM_SEARCH_DOWN = 0,  // down the output and round to the oldest row, as a panel searches
    MCTERM_SEARCH_UP         // up the output and round to the newest row, as less searches
} mcterm_search_dir_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct WMcTerm WMcTerm;

/*** global variables defined in .c file *********************************************************/

extern mcterm_search_dir_t mcterm_search_direction;
extern gboolean mcterm_clipboard_write;

/*** declarations of public functions ************************************************************/

#ifdef ENABLE_MCTERM

/* The settings of the terminal, in the [Terminal] section of the ini file. */
void mcterm_load_options (void);
void mcterm_save_options (void);

WMcTerm *mcterm_new (const WRect *r, const char *start_dir);
void mcterm_free (WMcTerm *t);
gboolean mcterm_is_alive (const WMcTerm *t);
gboolean mcterm_in_alt_screen (const WMcTerm *t);
void mcterm_scroll_to_end (WMcTerm *t);
void mcterm_set_scroll_allowed (WMcTerm *t, gboolean allowed);
Widget *mcterm_widget (WMcTerm *t);
gboolean mcterm_send_line (WMcTerm *t, const char *line);
gboolean mcterm_send_internal_line (WMcTerm *t, const char *line);
/* Remember a command submitted by the host and describe the process currently running it. */
void mcterm_set_command_hint (WMcTerm *t, const char *command);
char *mcterm_running_command (const WMcTerm *t, int max_width);
gboolean mcterm_shell_at_prompt (const WMcTerm *t);
/* Whether the shell's cursor is on a row the terminal draws rather than on the host's row: on
   an earlier row of a wrapped line. */
gboolean mcterm_shell_cursor_in_terminal (const WMcTerm *t);
/* Whether the shell's own line editor holds nothing typed yet: TRUE at a fresh prompt, FALSE once
   a command has been entered on it. Also TRUE when there is no prompt to speak of. */
gboolean mcterm_shell_line_is_empty (WMcTerm *t);
/* What the shell's line editor holds, read off the screen; NULL when nothing. Caller frees. */
char *mcterm_shell_line_text (WMcTerm *t);
/* How many characters of the line stand before the cursor; 0 when there is no line. */
int mcterm_shell_line_point (WMcTerm *t);
/* Tell the shell to drop its line; it reads as empty until the shell says something. */
void mcterm_clear_shell_line (WMcTerm *t);
gboolean mcterm_wait_for_prompt (WMcTerm *t, int timeout_msec);
const char *mcterm_osc7_raw (const WMcTerm *t);
/* The session token our own shell puts in every OSC 7, NULL when there is none. */
const char *mcterm_osc7_token (const WMcTerm *t);
/* The exit code the shell reported for the last command, -1 when it reported none. */
int mcterm_last_exit_code (const WMcTerm *t);
/* Advances while a command runs, so that the host can show that something is going on. */
guint mcterm_busy_phase (const WMcTerm *t);
void mcterm_set_busy_tick_callback (WMcTerm *t, void (*cb) (void *), void *data);
void mcterm_set_prompt_callback (WMcTerm *t, void (*cb) (void *), void *data);
void mcterm_set_after_redraw_callback (WMcTerm *t, void (*cb) (void *), void *data);
gboolean mcterm_osc7_capable (const WMcTerm *t);
int mcterm_cursor_col (const WMcTerm *t);
/* At a prompt the widget leaves the shell's row to the host: draw it with this. */
void mcterm_draw_prompt_row (const WMcTerm *t, int screen_y, const char *skin_section, int color);
void mcterm_preload_prompt_colors (const char *skin_section, int color);
gboolean mcterm_send_tab_complete (WMcTerm *t, const char *text);
/* Hand one key to the shell, for its own line editor to act on. */
gboolean mcterm_send_key (WMcTerm *t, int key);
long mcterm_key_command (const WMcTerm *t, int key);
/* While a search or a filter is typed, the key that edits or ends it. MSG_NOT_HANDLED for any
   other: the typing may have ended, and the key goes where it would have gone. */
cb_ret_t mcterm_query_key (WMcTerm *t, int key);
/* Whether the host types on a command line of its own. Without one the plain
   arrows are left to the shell, there being nowhere else for typing to go. */
void mcterm_set_typing_elsewhere (WMcTerm *t, gboolean elsewhere);
/* Whether some of the output is marked, for Store to take. */
gboolean mcterm_mark_active (const WMcTerm *t);
/* Type @text into the shell. FALSE when the shell is gone or took none of it for a second;
   what it took by then stays on its line. */
gboolean mcterm_send_text (WMcTerm *t, const char *text);

/* Called while the master has output to read; FALSE when the fd is gone. */
typedef gboolean (*mcterm_pty_drain_fn) (int fd, void *data);
/* Write @len bytes to the pty master @fd without blocking on it, calling @drain whenever it
   has output to read. FALSE when the fd is gone or took no byte for @stall_usec; a prefix of
   the text may have gone through by then. */
gboolean mcterm_pty_send (int fd, const char *text, size_t len, gint64 stall_usec,
                          mcterm_pty_drain_fn drain, void *data);

#else /* !ENABLE_MCTERM */

static inline WMcTerm *
mcterm_new (const WRect *r, const char *start_dir)
{
    (void) r;
    (void) start_dir;
    return NULL;
}
static inline void
mcterm_free (WMcTerm *t)
{
    (void) t;
}
static inline gboolean
mcterm_is_alive (const WMcTerm *t)
{
    (void) t;
    return FALSE;
}
static inline gboolean
mcterm_in_alt_screen (const WMcTerm *t)
{
    (void) t;
    return FALSE;
}
static inline Widget *
mcterm_widget (WMcTerm *t)
{
    (void) t;
    return NULL;
}
static inline gboolean
mcterm_send_line (WMcTerm *t, const char *line)
{
    (void) t;
    (void) line;
    return FALSE;
}
static inline gboolean
mcterm_send_internal_line (WMcTerm *t, const char *line)
{
    (void) t;
    (void) line;
    return FALSE;
}
static inline void
mcterm_set_command_hint (WMcTerm *t, const char *command)
{
    (void) t;
    (void) command;
}
static inline char *
mcterm_running_command (const WMcTerm *t, int max_width)
{
    (void) t;
    (void) max_width;
    return NULL;
}
static inline gboolean
mcterm_shell_at_prompt (const WMcTerm *t)
{
    (void) t;
    return FALSE;
}
static inline gboolean
mcterm_shell_cursor_in_terminal (const WMcTerm *t)
{
    (void) t;
    return FALSE;
}
static inline gboolean
mcterm_shell_line_is_empty (WMcTerm *t)
{
    (void) t;
    return TRUE;
}
static inline char *
mcterm_shell_line_text (WMcTerm *t)
{
    (void) t;
    return NULL;
}
static inline int
mcterm_shell_line_point (WMcTerm *t)
{
    (void) t;
    return 0;
}
static inline void
mcterm_clear_shell_line (WMcTerm *t)
{
    (void) t;
}
static inline gboolean
mcterm_wait_for_prompt (WMcTerm *t, int timeout_msec)
{
    (void) t;
    (void) timeout_msec;
    return FALSE;
}
static inline const char *
mcterm_osc7_raw (const WMcTerm *t)
{
    (void) t;
    return NULL;
}
static inline const char *
mcterm_osc7_token (const WMcTerm *t)
{
    (void) t;
    return NULL;
}
static inline int
mcterm_last_exit_code (const WMcTerm *t)
{
    (void) t;
    return -1;
}
static inline guint
mcterm_busy_phase (const WMcTerm *t)
{
    (void) t;
    return 0;
}
static inline void
mcterm_set_busy_tick_callback (WMcTerm *t, void (*cb) (void *), void *data)
{
    (void) t;
    (void) cb;
    (void) data;
}
static inline void
mcterm_set_prompt_callback (WMcTerm *t, void (*cb) (void *), void *data)
{
    (void) t;
    (void) cb;
    (void) data;
}
static inline void
mcterm_set_after_redraw_callback (WMcTerm *t, void (*cb) (void *), void *data)
{
    (void) t;
    (void) cb;
    (void) data;
}
static inline gboolean
mcterm_osc7_capable (const WMcTerm *t)
{
    (void) t;
    return FALSE;
}
static inline int
mcterm_cursor_col (const WMcTerm *t)
{
    (void) t;
    return -1;
}
static inline void
mcterm_draw_prompt_row (const WMcTerm *t, int screen_y, const char *skin_section, int color)
{
    (void) t;
    (void) screen_y;
    (void) skin_section;
    (void) color;
}
static inline void
mcterm_preload_prompt_colors (const char *skin_section, int color)
{
    (void) skin_section;
    (void) color;
}
static inline gboolean
mcterm_send_tab_complete (WMcTerm *t, const char *text)
{
    (void) t;
    (void) text;
    return FALSE;
}
static inline gboolean
mcterm_send_key (WMcTerm *t, int key)
{
    (void) t;
    (void) key;
    return FALSE;
}
static inline long
mcterm_key_command (const WMcTerm *t, int key)
{
    (void) t;
    (void) key;
    return CK_IgnoreKey;
}
static inline cb_ret_t
mcterm_query_key (WMcTerm *t, int key)
{
    (void) t;
    (void) key;
    return MSG_NOT_HANDLED;
}
static inline void
mcterm_set_typing_elsewhere (WMcTerm *t, gboolean elsewhere)
{
    (void) t;
    (void) elsewhere;
}
static inline gboolean
mcterm_mark_active (const WMcTerm *t)
{
    (void) t;
    return FALSE;
}
static inline gboolean
mcterm_send_text (WMcTerm *t, const char *text)
{
    (void) t;
    (void) text;
    return FALSE;
}

#endif /* ENABLE_MCTERM */

/*** inline functions ****************************************************************************/

#endif /* MC__MCTERM_H */
