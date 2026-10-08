/** \file mcterm_overlay.h
 *  \brief Header: file manager mcterm overlay controller
 */

#ifndef MC__MCTERM_OVERLAY_H
#define MC__MCTERM_OVERLAY_H

#include "lib/widget.h"

/*** typedefs(not structures) and defined constants **********************************************/

typedef cb_ret_t (*mcterm_overlay_command_cb_t) (long command, void *data);
typedef cb_ret_t (*mcterm_overlay_enter_cb_t) (void *data);

typedef enum
{
    MCTERM_OVERLAY_CMDLINE_NOT_APPLICABLE,
    MCTERM_OVERLAY_CMDLINE_HANDLED,
    MCTERM_OVERLAY_CMDLINE_SENT
} mcterm_overlay_cmdline_result_t;

/*** declarations of public functions ************************************************************/

void mcterm_overlay_start (void);
gboolean mcterm_overlay_active (void);
/* Whether the terminal has the screen to itself: it is up and no panel is over it. */
gboolean mcterm_overlay_terminal_alone (void);
void mcterm_overlay_toggle (void);
/* Show mc's terminal full screen for the editor and viewers; FALSE when there is none to show. */
gboolean mcterm_overlay_show_terminal (void);
void mcterm_overlay_destroy (void);

void mcterm_overlay_draw_visible_panels (void);
void mcterm_overlay_after_filemanager_draw (void);
void mcterm_overlay_resize (const WRect *r);

gboolean mcterm_overlay_complete_or_cycle_focus (void);
cb_ret_t mcterm_overlay_send_enter_if_cmdline_empty (void);
gboolean mcterm_overlay_show_panel_if_hidden (int idx);
/* Whether @command works on the file the panel's cursor stands on and so must not run while no
   panel is on screen. */
gboolean mcterm_overlay_command_needs_panel_cursor (long command);
gboolean mcterm_overlay_toggle_panel_command (gboolean right_panel_command);

mcterm_overlay_cmdline_result_t mcterm_overlay_run_cmdline (const char *cmd, gboolean is_cd,
                                                            gboolean is_exit);
gboolean mcterm_overlay_panel_exec (const char *cmd);
/* What the prompt row says while a command runs, or NULL when the shell has its own. */
const char *mcterm_overlay_prompt_text (void);
// The command is over and the panels wait for a key: "Pause after run".
gboolean mcterm_overlay_pause_pending (void);
/* Send the shell after the current panel, when it is free to go. */
void mcterm_overlay_sync_shell_to_panel (void);
/* Whether mc has a terminal with a shell in it. */
gboolean mcterm_overlay_has_terminal (void);
/* Run a command in the terminal mc has; FALSE when the caller must run it the old way. */
gboolean mcterm_overlay_exec_command (const char *cmd);

/* The command line as the user sees it: mc's own input and the shell's line together. */
gboolean mcterm_overlay_cmdline_is_empty (void);
/* What is on it, wherever it is held; NULL when nothing. Caller frees. */
char *mcterm_overlay_cmdline_text (void);
/* The name of what runs in the terminal, for the window title; NULL at the prompt.
   Caller frees. */
char *mcterm_overlay_title_command (void);
/* Panel view hooks for keys that are the command line's, called after mc's own keys. */
cb_ret_t mcterm_overlay_cmdline_key (int parm);
cb_ret_t mcterm_overlay_cmdline_enter (void);
/* Whether a paste is for the shell: the terminal has the focus, or the shell owns the command
   line. MSG_NOT_HANDLED from the handler means the paste is to be typed key by key. */
gboolean mcterm_overlay_paste_is_shells (void);
cb_ret_t mcterm_overlay_handle_paste (const GString *text);

cb_ret_t mcterm_overlay_handle_key (Widget *w, int parm,
                                    mcterm_overlay_command_cb_t execute_command,
                                    mcterm_overlay_enter_cb_t execute_cmdline_enter, void *data);

/*** inline functions ****************************************************************************/

#endif
