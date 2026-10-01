/*
   File manager for the M-Commander
   The mcterm overlay controller

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

/** \file mcterm_overlay.c
 *  \brief Source: file manager mcterm overlay controller
 */

#include <config.h>

#include "mcterm_overlay.h"

#ifdef ENABLE_MCTERM

#include "lib/global.h"
#include "lib/keybind.h"
#include "lib/shell.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/tty/key.h"
#include "lib/tty/tty.h"
#include "lib/vfs/vfs.h"
#include "lib/widget/dialog-switch.h"
#include "lib/widget/mouse.h"

#include "src/execute.h"
#include "src/keymap.h"
#include "src/mcterm/mcterm.h"
#include "src/mcterm/mcterm_cwd.h"

#include "command.h"
#include "wprompt.h"
#include "filemanager.h"
#include "layout.h"
#include "dir_watch.h"

/*** file scope variables ************************************************************************/

static WMcTerm *mcterm_panel = NULL;
static gboolean mcterm_mode = FALSE;
/* Which set of names the button bar carries: -1 until it carries one at all. */
static int mcterm_bar_kind = -1;
/* A command of ours is running; the panels are to be re-read once it is over. */
static gboolean mcterm_exec_needs_panel_reload = FALSE;
/* The panels stepped aside for the command; they come back when it is over. */
static gboolean mcterm_exec_from_panels = FALSE;
/* The command is over and the panels wait for a key, as "Pause after run" asks. */
static gboolean mcterm_pause_pending = FALSE;
/* The shell was started before the panels settled on which of them is current: at its first
   prompt it is sent to the one that is. */
static gboolean mcterm_initial_sync_pending = FALSE;
/* The user's line, taken off the shell for a command of ours and typed back at the next prompt. */
static char *mcterm_parked_line = NULL;
static int mcterm_parked_left = 0;
// A command of ours is on its way to the shell: the parked line waits for the prompt after it.
static gboolean mcterm_parking = FALSE;

#define MCTERM_INITIAL_PROMPT_TIMEOUT_MS 1000
#define MCTERM_COMMAND_LABEL_WIDTH       14

static gboolean mcterm_overlay_any_panel_visible (void);
static void mcterm_overlay_park_shell_line (void);
static void mcterm_overlay_set_buttonbar (void);

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
mcterm_overlay_live (void)
{
    return (mcterm_panel != NULL && mcterm_is_alive (mcterm_panel));
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcterm_overlay_ready (void)
{
    return (mcterm_overlay_live () && mcterm_shell_at_prompt (mcterm_panel)
            && mcterm_osc7_capable (mcterm_panel));
}

/* --------------------------------------------------------------------------------------------- */

/* At its prompt the shell's line editor is the command line, whichever view is up: what is typed
   goes to it, and what it holds is the line. mc's own input stands in only while the shell has
   no prompt to type on. Read off the shell itself, not remembered, so the panels can come and go
   and nothing here has to be told. */
static gboolean
mcterm_overlay_shell_owns_cmdline (void)
{
    return (command_prompt && mcterm_overlay_ready () && !mcterm_in_alt_screen (mcterm_panel));
}

/* --------------------------------------------------------------------------------------------- */

/* Whether the host keeps the bottom row for itself: the shell's prompt when it is idle, the
   busy line while a command runs, and mc's command line under a full-screen program - which is
   given one row less so its own last row does not land where the command line is. */
static gboolean
mcterm_overlay_owns_bottom_row (void)
{
    return (command_prompt && mcterm_overlay_live () && mcterm_osc7_capable (mcterm_panel));
}

/* --------------------------------------------------------------------------------------------- */

static Widget *
mcterm_overlay_widget (void)
{
    return mcterm_widget (mcterm_panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_overlay_rect (const WRect *mwr, WRect *r)
{
    int start_y = mwr->y + (menubar_visible ? 1 : 0);
    int height = mwr->lines - (menubar_visible ? 1 : 0) - (mc_global.keybar_visible ? 1 : 0)
        - (mcterm_overlay_owns_bottom_row () ? 1 : 0);

    *r = (WRect) { start_y, mwr->x, MAX (height, 1), mwr->cols };
}

/* --------------------------------------------------------------------------------------------- */

/**
 * What the row in front of the command line says while a command is running.
 *
 * The shell has no prompt to show then - it is busy - but the row should not go blank: it says
 * that something is running and where the panel stands, which is where a command would go.
 *
 * @return the text, or NULL when the shell has a prompt of its own to show.
 */

const char *
mcterm_overlay_prompt_text (void)
{
    static char text[MC_MAXPATHLEN + 32];
    char *command;
    const char *cwd;

    if (mcterm_pause_pending)
        return _ ("Press any key to continue...");

    // A shell still on its way to the first prompt runs nothing: mc's own text stays.
    if (!mcterm_overlay_live () || !mcterm_osc7_capable (mcterm_panel)
        || mcterm_shell_at_prompt (mcterm_panel) || mcterm_initial_sync_pending)
        return NULL;

    cwd = (current_panel != NULL) ? vfs_path_as_str (current_panel->cwd_vpath) : "";
    command = mcterm_running_command (mcterm_panel, MCTERM_COMMAND_LABEL_WIDTH);

    g_snprintf (text, sizeof (text), "(%s %s) %s%s ", command != NULL ? command : "background",
                mc_skin_spinner_frame (mcterm_busy_phase (mcterm_panel)), cwd,
                (cwd[0] != '\0' && cwd[strlen (cwd) - 1] != PATH_SEP) ? PATH_SEP_STR : "");
    g_free (command);

    return text;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_overlay_pause_pending (void)
{
    return mcterm_pause_pending;
}

/* --------------------------------------------------------------------------------------------- */

/* The prompt has changed width: lay the row out again and draw it. */
static void
mcterm_overlay_place_prompt (void)
{
    if (!command_prompt)
        return;

    setup_cmdline ();
    widget_draw (WIDGET (the_prompt));
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_overlay_sync_panel_from_shell (void)
{
    char *new_cwd;

    if (!mcterm_mode || current_panel == NULL || !mcterm_overlay_ready ()
        || !vfs_file_is_local (current_panel->cwd_vpath))
        return;

    new_cwd = mcterm_cwd_on_exit (mcterm_panel, vfs_path_as_str (current_panel->cwd_vpath));
    if (new_cwd != NULL)
    {
        vfs_path_t *vp = vfs_path_from_str (new_cwd);

        if (vp != NULL)
        {
            panel_cd (current_panel, vp, cd_exact);
            vfs_path_free (vp, TRUE);
        }
        g_free (new_cwd);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_overlay_sync_shell_to_panel (void)
{
    const char *panel_cwd;

    if (current_panel == NULL || !vfs_file_is_local (current_panel->cwd_vpath)
        || !mcterm_overlay_ready ())
        return;

    panel_cwd = vfs_path_as_str (current_panel->cwd_vpath);

    if (mcterm_cwd_differs (mcterm_panel, panel_cwd))
    {
        char *quoted = g_shell_quote (panel_cwd);
        char *cmd = g_strdup_printf ("cd %s", quoted);

        mcterm_parking = TRUE;
        mcterm_overlay_park_shell_line ();
        mcterm_send_internal_line (mcterm_panel, cmd);
        mcterm_parking = FALSE;
        g_free (cmd);
        g_free (quoted);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcterm_overlay_terminal_focused (void)
{
    WGroup *g = GROUP (filemanager);

    return (g->current != NULL && WIDGET (g->current->data) == mcterm_overlay_widget ());
}

/* --------------------------------------------------------------------------------------------- */

/* What the command line's own keymap makes of the key, as the key the shell's line editor takes
   for the same thing. With the panels up the plain arrows are theirs, so mc's [input] map spells
   "one left" as Alt-Left, say - and the shell must be told "left", not "Alt-Left". A key the map
   has no editing meaning for goes as it is. */
static int
mcterm_overlay_cmdline_shell_key (int parm)
{
    if (cmdline == NULL)
        return parm;

    switch (widget_lookup_key (WIDGET (cmdline), parm))
    {
    case CK_Left:
        return KEY_LEFT;
    case CK_Right:
        return KEY_RIGHT;
    case CK_Home:
        return KEY_HOME;
    case CK_End:
        return KEY_END;
    // The emacs keys, which readline, zle and fish all take; Ctrl-arrows zle does not.
    case CK_WordLeft:
        return KEY_M_ALT | 'b';
    case CK_WordRight:
        return KEY_M_ALT | 'f';
    case CK_BackSpace:
        return KEY_BACKSPACE;
    case CK_Delete:
        return KEY_DC;
    case CK_DeleteToWordBegin:
        return KEY_M_ALT | KEY_BACKSPACE;
    case CK_DeleteToWordEnd:
        return KEY_M_ALT | 'd';
    case CK_DeleteToEnd:
        return XCTRL ('K');
    case CK_HistoryPrev:
        return KEY_UP;
    case CK_HistoryNext:
        return KEY_DOWN;
    case CK_Complete:
        return '\t';
    default:
        return parm;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Whether the shell edits its line at all. sh, dash and ash read it raw from the tty: an arrow
   sent to them is only printed. */
static gboolean
mcterm_overlay_shell_edits_line (void)
{
    if (mc_global.shell == NULL)
        return FALSE;

    switch (mc_global.shell->type)
    {
    case SHELL_BASH:
    case SHELL_ZSH:
    case SHELL_FISH:
    case SHELL_TCSH:
        return TRUE;
    default:
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Hand the shell a key of the command line's. A shell with no line editor gets the text and
   the control keys the tty itself acts on, and nothing it would only print. */
static void
mcterm_overlay_send_cmdline_key (int parm)
{
    const int key = mcterm_overlay_cmdline_shell_key (parm);

    if (!mcterm_overlay_shell_edits_line () && key > 0xFF && (key & ~0x1F) != KEY_M_CTRL
        && key != KEY_BACKSPACE)
        return;

    mcterm_send_key (mcterm_panel, key);
}

/* --------------------------------------------------------------------------------------------- */

/* At its own prompt the command line is the shell's line: almost every key edits, completes or
   recalls it there. Only the few mc always answers to - its menu, its function bar, Escape and
   the Alt shortcuts - are kept back. */
static gboolean
mcterm_overlay_cmdline_takes_key (int parm)
{
    if (mcterm_panel == NULL || !command_prompt || mcterm_overlay_terminal_focused ())
        return FALSE;
    if (mcterm_in_alt_screen (mcterm_panel) || !mcterm_shell_at_prompt (mcterm_panel)
        || !mcterm_osc7_capable (mcterm_panel))
        return FALSE;

    // The keys the shell's line editor moves and edits with.
    switch (parm)
    {
    case KEY_UP:
    case KEY_DOWN:
    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_HOME:
    case KEY_END:
    case KEY_PPAGE:
    case KEY_NPAGE:
    case KEY_BACKSPACE:
    case KEY_DC:
    case KEY_IC:
        return TRUE;
    default:
        break;
    }

    // Whatever else mc's own command line would edit with.
    if (mcterm_overlay_cmdline_shell_key (parm) != parm)
        return TRUE;

    // The control keys readline uses - Ctrl+R to search, Ctrl+U, Ctrl+W, Ctrl+A and the rest. mc
    // tags them with its own control bit; the encoder turns each back into its C0 byte. Ctrl+O is
    // mc's own - it hides the terminal - so it is left for mc to answer.
    if ((parm & KEY_M_CTRL) != 0)
        return (parm != XCTRL ('O') && (parm & ~KEY_M_CTRL) <= 0xFF);

    // Printable text and the raw C0 control keys, but not Escape, which mc answers to.
    return (parm >= 0x01 && parm <= 0xFF && parm != ESC_CHAR);
}

/* --------------------------------------------------------------------------------------------- */

/* Put the cursor where the input goes: the terminal draws its own when it is
   being read, the command line whenever it is the one typed into. */
static void
mcterm_overlay_place_cursor (void)
{
    if (mcterm_overlay_terminal_focused () || mcterm_shell_cursor_in_terminal (mcterm_panel))
        send_message (mcterm_overlay_widget (), NULL, MSG_CURSOR, 0, NULL);
    else if (command_prompt)
        send_message (WIDGET (cmdline), NULL, MSG_CURSOR, 0, NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* Where typing goes when the panels step aside. */
static void
mcterm_overlay_focus_typing (void)
{
    widget_select (command_prompt ? WIDGET (cmdline) : mcterm_overlay_widget ());
}

/* --------------------------------------------------------------------------------------------- */

/* Hand the typing over to the command line, which is where it goes. */
static void
mcterm_overlay_focus_cmdline (void)
{
    if (command_prompt && mcterm_overlay_terminal_focused ())
        widget_select (WIDGET (cmdline));
}

static gboolean
mcterm_overlay_starts_cmdline_input (int parm)
{
    return (command_prompt && parm >= ' ' && parm <= 255);
}

/* --------------------------------------------------------------------------------------------- */

/* Type a line into the shell's line editor, cursor position and all: @left characters of it
   stay to the right of the cursor. */
static void
mcterm_overlay_type_line (const char *text, int left)
{
    const unsigned char *p;

    for (p = (const unsigned char *) text; *p != '\0'; p++)
        if (!mcterm_send_key (mcterm_panel, *p))
            return;

    while (left-- > 0)
        if (!mcterm_send_key (mcterm_panel, KEY_LEFT))
            break;
}

/* --------------------------------------------------------------------------------------------- */

/* A command of ours is about to go to the shell. Whatever the user has typed on its line is
   taken off and kept, to be typed in again once the shell has its prompt back. */
static void
mcterm_overlay_park_shell_line (void)
{
    char *text;

    if (!mcterm_overlay_shell_owns_cmdline ())
        return;

    text = mcterm_shell_line_text (mcterm_panel);
    if (text == NULL)
        return;

    g_free (mcterm_parked_line);
    mcterm_parked_line = text;
    mcterm_parked_left = MAX (str_length (text) - mcterm_shell_line_point (mcterm_panel), 0);
    mcterm_clear_shell_line (mcterm_panel);
}

/* --------------------------------------------------------------------------------------------- */

/* The line parked for a command of ours goes back to the shell's line editor as soon as it has
   a prompt, cursor position and all. */
static void
mcterm_overlay_type_parked_line (void)
{
    if (mcterm_parked_line == NULL || mcterm_parking || !mcterm_overlay_shell_owns_cmdline ())
        return;

    mcterm_overlay_type_line (mcterm_parked_line, mcterm_parked_left);
    g_clear_pointer (&mcterm_parked_line, g_free);
}

/* --------------------------------------------------------------------------------------------- */

/* Text typed into mc's own input while the shell had no prompt, and a line parked for a command
   of ours, go to the shell's line editor as soon as it has a prompt, cursor position and all, so
   the two never hold a line at the same time. */
static void
mcterm_overlay_move_cmdline_to_shell (void)
{
    const char *text;

    if (cmdline == NULL || !mcterm_overlay_shell_owns_cmdline ())
        return;

    mcterm_overlay_type_parked_line ();

    text = input_get_ctext (cmdline);
    if (text == NULL || *text == '\0')
        return;

    mcterm_overlay_type_line (text, MAX (str_length (text) - cmdline->point, 0));
    input_clean (cmdline);
}

/* --------------------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------- */

/* Redraw everything that may cover the terminal after the terminal or its status row was drawn. */
static void
mcterm_overlay_draw_covering_widgets (void)
{
    GList *l;

    mcterm_overlay_draw_visible_panels ();

    if (the_menubar != NULL && the_menubar->is_dropped)
        widget_draw (WIDGET (the_menubar));

    for (l = top_dlg; l != NULL; l = g_list_next (l))
        if (WIDGET (l->data) == WIDGET (filemanager))
            break;
    if (l == NULL)
        return;
    for (l = g_list_previous (l); l != NULL; l = g_list_previous (l))
        widget_draw (WIDGET (l->data));
}

/* --------------------------------------------------------------------------------------------- */

/* Something is running: keep the row in front of the command line moving. */
static void
mcterm_overlay_busy_tick_cb (void *data)
{
    (void) data;

    if (!command_prompt || mcterm_panel == NULL)
        return;

    /* A full-screen program on screen paints its own display and drives its own redraws; the
       busy line is for a command hidden behind the panels. While such a program is shown,
       forcing a redraw and spinning the busy mark over it only fights it and reads as flicker. */
    if (mcterm_in_alt_screen (mcterm_panel)
        && widget_get_state (mcterm_overlay_widget (), WST_VISIBLE))
        return;

    /* In either view the busy line lives on the same bottom row; with the terminal on screen
       the row is kept clear for it, so lay the terminal out to that height first. */
    if (mcterm_mode)
        mcterm_overlay_resize (&CONST_WIDGET (filemanager)->rect);

    mcterm_overlay_place_prompt ();

    /* The terminal resize and the busy line are below panels, menus and dialogs. Restore all
       of them before the refresh, just as the PTY output redraw path does. */
    if (mcterm_mode)
        mcterm_overlay_draw_covering_widgets ();

    tty_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

/* What "Pause after run" asks for, as do_execute() reads it. */
static gboolean
mcterm_overlay_pause_wanted (void)
{
    return pause_after_run == pause_always
        || (pause_after_run == pause_on_dumb_terminals && !mc_global.tty.xterm_flag
            && mc_global.tty.console_flag == '\0');
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_overlay_prompt_ready_cb (void *data)
{
    (void) data;

    if (mcterm_panel == NULL)
        return;

    if (mcterm_exec_needs_panel_reload)
    {
        mcterm_exec_needs_panel_reload = FALSE;
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    }

    /* With the panels up the shell is out of sight, but its prompt is not: it is the row in
       front of the command line, and it has just become the shell's own. */
    /* With the panels up, a line in mc's own input waits there for the next key: it may be in
       the middle of being completed. */
    /* The first prompt: with the panels up the shell goes to the current one. With the
       terminal up it was sent there already, when the terminal was brought up. */
    if (mcterm_initial_sync_pending)
    {
        mcterm_initial_sync_pending = FALSE;
        if (!mcterm_mode)
            mcterm_overlay_sync_shell_to_panel ();
    }

    if (!mcterm_mode)
    {
        // A line parked for a cd of ours comes back; mc's own input keeps what it holds.
        mcterm_overlay_type_parked_line ();
        mcterm_overlay_place_prompt ();
        /* The row was just drawn over; the cursor goes back to the command line, or it is left
           wherever the drawing ended. */
        mcterm_overlay_place_cursor ();
        tty_refresh ();
        return;
    }

    mcterm_overlay_sync_panel_from_shell ();
    mcterm_overlay_resize (&CONST_WIDGET (filemanager)->rect);
    widget_draw (mcterm_overlay_widget ());

    if (mcterm_exec_from_panels)
    {
        mcterm_exec_from_panels = FALSE;
        // Without the command line there is no row for the message: no pause then.
        if (!mcterm_overlay_pause_wanted () || !command_prompt)
        {
            mcterm_overlay_toggle ();
            return;
        }
        mcterm_pause_pending = TRUE;
        mcterm_overlay_place_prompt ();
        tty_refresh ();
        return;
    }

    if (!command_prompt)
        return;

    mcterm_overlay_move_cmdline_to_shell ();
    mcterm_overlay_place_prompt ();
    mcterm_overlay_place_cursor ();
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_overlay_after_redraw_cb (void *data)
{
    (void) data;

    /* An mc started in our terminal asked for the panels and exited. The shell has drawn its
       prompt by now, so this is a good moment to switch back. */
    if (mcterm_mode && show_panels_request_pending ())
    {
        show_panels_request_clear ();
        mcterm_overlay_toggle ();
        return;
    }

    mcterm_overlay_draw_covering_widgets ();
}

/* --------------------------------------------------------------------------------------------- */

static int
mcterm_overlay_mouse_handler (Widget *w, Gpm_Event *event)
{
    Widget *pw;

    if (mcterm_panel != NULL)
        mcterm_set_scroll_allowed (mcterm_panel, !mcterm_overlay_any_panel_visible ());

    pw = get_panel_widget (0);
    if (pw != NULL && widget_get_state (pw, WST_VISIBLE) && mouse_global_in_widget (event, pw))
        return MOU_UNHANDLED;

    pw = get_panel_widget (1);
    if (pw != NULL && widget_get_state (pw, WST_VISIBLE) && mouse_global_in_widget (event, pw))
        return MOU_UNHANDLED;

    return mouse_handle_event (w, event);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_overlay_discard_dead_terminal (void)
{
    if (mcterm_panel != NULL && !mcterm_is_alive (mcterm_panel))
    {
        group_remove_widget (mcterm_overlay_widget ());
        mcterm_free (mcterm_panel);
        mcterm_panel = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcterm_overlay_any_panel_visible (void)
{
    Widget *pw;

    pw = get_panel_widget (0);
    if (pw != NULL && widget_get_state (pw, WST_VISIBLE))
        return TRUE;

    pw = get_panel_widget (1);

    return (pw != NULL && widget_get_state (pw, WST_VISIBLE));
}

/* --------------------------------------------------------------------------------------------- */

/* These act on the file the cursor of the current panel stands on. While that panel is off the
   screen there is no cursor to look at, so the file that would be viewed, copied, moved or
   deleted is anybody's guess: the key goes to the terminal instead and does nothing here. The
   other panel being up is no help - the cursor that counts is still out of sight. */
gboolean
mcterm_overlay_command_needs_panel_cursor (long command)
{
    if (current_panel != NULL && widget_get_state (WIDGET (current_panel), WST_VISIBLE))
        return FALSE;

    switch (command)
    {
    // Its entries reach for the file under the cursor through %f and %d.
    case CK_UserMenu:
    case CK_View:
    case CK_Edit:
    /* The panel keymap names these, so they never come out of a lookup on the file manager and
       are already the shell's while the panels are down. Named here for the day it changes. */
    case CK_ViewRaw:
    case CK_Copy:
    case CK_CopySingle:
    case CK_Move:
    case CK_MoveSingle:
    case CK_Delete:
    case CK_DeleteSingle:
        return TRUE;
    default:
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* What the terminal does to its own output, and what it hands out of it. None of it depends on
   who holds the focus - the mark and the filter belong to the view, not to the typing. */
static gboolean
mcterm_overlay_terminal_owns (long command)
{
    switch (command)
    {
    case CK_Store:
    case CK_MarkAll:
    case CK_FilterWord:
    case CK_FilterToggle:
    case CK_Search:
    case CK_QuickFilter:
    case CK_Clear:
    case CK_ClearAll:
        return TRUE;
    default:
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcterm_overlay_panel_focused (Widget *focused)
{
    Widget *pw;

    pw = get_panel_widget (0);
    if (pw != NULL && pw == focused && widget_get_state (pw, WST_VISIBLE))
        return TRUE;

    pw = get_panel_widget (1);
    return (pw != NULL && pw == focused && widget_get_state (pw, WST_VISIBLE));
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcterm_overlay_create_terminal (void)
{
    WRect r;
    const char *start_dir;

    mcterm_overlay_discard_dead_terminal ();
    if (mcterm_panel != NULL)
        return TRUE;

    mcterm_overlay_rect (&CONST_WIDGET (filemanager)->rect, &r);

    start_dir = (current_panel != NULL && vfs_file_is_local (current_panel->cwd_vpath))
        ? vfs_path_as_str (current_panel->cwd_vpath)
        : NULL;

    mcterm_panel = mcterm_new (&r, start_dir);
    if (mcterm_panel == NULL)
        return FALSE;
    mcterm_initial_sync_pending = TRUE;

    mcterm_set_prompt_callback (mcterm_panel, mcterm_overlay_prompt_ready_cb, NULL);
    mcterm_set_busy_tick_callback (mcterm_panel, mcterm_overlay_busy_tick_cb, NULL);
    mcterm_set_after_redraw_callback (mcterm_panel, mcterm_overlay_after_redraw_cb, NULL);
    mcterm_overlay_widget ()->mouse_handler = mcterm_overlay_mouse_handler;
    group_add_widget (GROUP (filemanager), mcterm_overlay_widget ());
    /* Stays hidden until the overlay is switched on, so a terminal created
       for a command does not paint over the panels. */
    widget_hide (mcterm_overlay_widget ());

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Start the shell the file manager runs on, at the time the file manager itself starts.
 *
 * Nothing waits for the first prompt: a slow rc file must not hold the panels back. Until the
 * shell reports a prompt of its own, the row in front of the command line shows mc's own text.
 */

void
mcterm_overlay_start (void)
{
    if (mcterm_overlay_create_terminal ())
        wprompt_set_terminal (the_prompt, mcterm_panel);
}

/* --------------------------------------------------------------------------------------------- */

/* Ctrl-O steps back out of the terminal the way it stepped in. */
static cb_ret_t
mcterm_overlay_modal_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_UNHANDLED_KEY:
        // Ctrl-O steps back out the way it stepped in.
        if (parm == XCTRL ('O') || parm == KEY_F (10))
        {
            dlg_close (DIALOG (w));
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case MSG_RESIZE:
    {
        // The terminal follows the window: fill it, which tells the shell its new size too.
        WRect r = { 0, 0, LINES, COLS };

        dlg_default_callback (w, NULL, MSG_RESIZE, 0, NULL);
        widget_set_size_rect (mcterm_overlay_widget (), &r);
        widget_draw (w);
        return MSG_HANDLED;
    }

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_overlay_repaint_screen (void)
{
    tty_touch_screen ();
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Show mc's own terminal full screen, for the editor and the viewers to reach with Ctrl-O.
 *
 * Their Ctrl-O borrows the terminal mc already runs, shown full screen. The
 * terminal widget moves into a modal of its own for as long as it is shown, and back into the
 * file manager when it is left. Its callbacks draw the file manager's own row, which is not on
 * screen now, so they are put by until it returns.
 *
 * @return TRUE when the terminal was shown, FALSE when there is none and the caller must fall
 *         back to toggle_terminal ().
 */

gboolean
mcterm_overlay_show_terminal (void)
{
    WDialog *h;
    Widget *tw;
    WRect saved;

    if (!mcterm_overlay_live ())
        return FALSE;

    tw = mcterm_overlay_widget ();
    saved = tw->rect;

    mcterm_set_prompt_callback (mcterm_panel, NULL, NULL);
    mcterm_set_busy_tick_callback (mcterm_panel, NULL, NULL);
    mcterm_set_after_redraw_callback (mcterm_panel, NULL, NULL);

    group_remove_widget (tw);

    h = dlg_create (TRUE, 0, 0, LINES, COLS, WPOS_FULLSCREEN, FALSE, dialog_colors,
                    mcterm_overlay_modal_callback, NULL, NULL, NULL);
    group_add_widget (GROUP (h), tw);
    widget_show (tw);
    {
        WRect r = { 0, 0, LINES, COLS };

        widget_set_size_rect (tw, &r);
    }
    mcterm_set_scroll_allowed (mcterm_panel, TRUE);
    mcterm_scroll_to_end (mcterm_panel);
    // No host command line here, so the terminal draws the shell's prompt row itself.
    mcterm_set_typing_elsewhere (mcterm_panel, FALSE);
    widget_select (tw);

    tty_touch_screen ();
    dlg_run (h);

    mcterm_set_typing_elsewhere (mcterm_panel, TRUE);
    group_remove_widget (tw);
    widget_destroy (WIDGET (h));

    mcterm_set_prompt_callback (mcterm_panel, mcterm_overlay_prompt_ready_cb, NULL);
    mcterm_set_busy_tick_callback (mcterm_panel, mcterm_overlay_busy_tick_cb, NULL);
    mcterm_set_after_redraw_callback (mcterm_panel, mcterm_overlay_after_redraw_cb, NULL);
    group_add_widget (GROUP (filemanager), tw);
    widget_set_size_rect (tw, &saved);
    widget_hide (tw);
    tty_touch_screen ();

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_overlay_active (void)
{
    return mcterm_mode;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_overlay_terminal_alone (void)
{
    return (mcterm_mode && mcterm_panel != NULL && !mcterm_overlay_any_panel_visible ());
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_overlay_toggle (void)
{
    const WRect *mwr = &CONST_WIDGET (filemanager)->rect;

    if (!mcterm_mode)
    {
        WRect r;

        mcterm_overlay_rect (mwr, &r);
        mcterm_overlay_discard_dead_terminal ();

        if (!mcterm_overlay_create_terminal ())
        {
            toggle_terminal ();
            return;
        }

        widget_set_size_rect (mcterm_overlay_widget (), &r);
        widget_show (mcterm_overlay_widget ());
        mcterm_set_after_redraw_callback (mcterm_panel, mcterm_overlay_after_redraw_cb, NULL);
        mcterm_overlay_widget ()->mouse_handler = mcterm_overlay_mouse_handler;
        mcterm_overlay_sync_shell_to_panel ();

        if (command_prompt)
            widget_set_size (WIDGET (cmdline), WIDGET (cmdline)->rect.y, mwr->x, 1, mwr->cols);

        // Drop a request left over from an earlier session of the overlay
        show_panels_request_clear ();

        mcterm_set_scroll_allowed (mcterm_panel, TRUE);
        mcterm_mode = TRUE;
        // Selectable here only: elsewhere a panel would lose its selection to it.
        widget_set_options (WIDGET (cmdline), WOP_SELECTABLE, TRUE);
        mcterm_overlay_move_cmdline_to_shell ();
        // Move the focus away before hiding the panels: hiding the focused panel
        // would select the other one and make it current_panel.
        mcterm_overlay_focus_typing ();

        widget_hide (get_panel_widget (0));
        widget_hide (get_panel_widget (1));
        widget_hide (WIDGET (the_hint));

        mcterm_overlay_set_buttonbar ();
        mcterm_overlay_repaint_screen ();
    }
    else
    {
        if (mcterm_panel != NULL)
        {
            if (!mcterm_is_alive (mcterm_panel))
            {
                group_remove_widget (mcterm_overlay_widget ());
                mcterm_free (mcterm_panel);
                mcterm_panel = NULL;
            }
            else
            {
                widget_hide (mcterm_overlay_widget ());
                mcterm_overlay_sync_panel_from_shell ();
            }
        }

        widget_show (get_panel_widget (0));
        widget_show (get_panel_widget (1));
        widget_set_visibility (WIDGET (the_hint), mc_global.message_visible);
        if (command_prompt)
            widget_show (WIDGET (the_prompt));

        mcterm_mode = FALSE;
        mcterm_pause_pending = FALSE;
        mcterm_exec_from_panels = FALSE;
        widget_set_options (WIDGET (cmdline), WOP_SELECTABLE, FALSE);
        layout_change ();
        widget_select (WIDGET (current_panel));
        // The panels were off screen while the shell was doing things to the files.
        dir_watch_reload_pending ();
        mcterm_overlay_set_buttonbar ();
        mcterm_overlay_repaint_screen ();
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_overlay_destroy (void)
{
    mcterm_panel = NULL;
    mcterm_mode = FALSE;
    mcterm_bar_kind = -1;
    g_clear_pointer (&mcterm_parked_line, g_free);
}

/* --------------------------------------------------------------------------------------------- */

/* Draw one panel slot. Only a view_listing slot is a real WPanel; tree/info/
   quick-view widgets must NOT be cast to WPanel (writing p->active into, e.g.,
   a WTree corrupts its memory and crashes on draw). */
static void
mcterm_overlay_draw_panel_slot (int idx, const WPanel *active_panel)
{
    Widget *pw = get_panel_widget (idx);

    if (pw == NULL || !widget_get_state (pw, WST_VISIBLE))
        return;

    if (get_panel_type (idx) == view_listing)
    {
        WPanel *p = PANEL (pw);
        gboolean saved = p->active;

        p->active = (p == active_panel);
        widget_draw (pw);
        p->active = saved;
    }
    else
        widget_draw (pw);
}

/* The button bar names the keys that are on offer, and with no panel on screen those are mostly
   the terminal's own: F2 to F6 mark the output, cut it down and clear it. Help, Mkdir, the
   pull-down menu and the way out stay the file manager's wherever the keys are typed, and F8 is
   left empty - the reach for Delete is too well worn to give it another meaning here. */
static void
mcterm_overlay_set_buttonbar (void)
{
    /* An alt-screen application has the keys itself, the function keys among them: the bar
       would name commands that are not on offer. */
    const int kind = (mcterm_mode && mcterm_panel != NULL && !mcterm_in_alt_screen (mcterm_panel)
                      && !mcterm_overlay_any_panel_visible ())
        ? 1
        : 0;

    if (the_bar == NULL || mcterm_bar_kind == kind)
        return;

    mcterm_bar_kind = kind;

    if (kind == 0)
        midnight_set_buttonbar (the_bar);
    else
    {
        Widget *fw = WIDGET (filemanager);
        Widget *tw = mcterm_overlay_widget ();

        buttonbar_set_label (the_bar, 1, Q_ ("ButtonBar|Help"), fw->keymap, NULL);
        buttonbar_set_label (the_bar, 2, Q_ ("ButtonBar|Copy"), mcterm_map, tw);
        buttonbar_set_label (the_bar, 3, Q_ ("ButtonBar|Mark"), mcterm_map, tw);
        buttonbar_set_label (the_bar, 4, Q_ ("ButtonBar|Filter"), mcterm_map, tw);
        buttonbar_set_label (the_bar, 5, Q_ ("ButtonBar|UnFilt"), mcterm_map, tw);
        buttonbar_set_label (the_bar, 6, Q_ ("ButtonBar|ClrAll"), mcterm_map, tw);
        buttonbar_set_label (the_bar, 7, Q_ ("ButtonBar|Mkdir"), fw->keymap, NULL);
        buttonbar_clear_label (the_bar, 8, NULL);
        buttonbar_set_label (the_bar, 9, Q_ ("ButtonBar|PullDn"), fw->keymap, NULL);
        buttonbar_set_label (the_bar, 10, Q_ ("ButtonBar|Quit"), fw->keymap, NULL);
    }

    widget_draw (WIDGET (the_bar));
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_overlay_draw_visible_panels (void)
{
    if (mcterm_panel != NULL)
        mcterm_set_scroll_allowed (mcterm_panel, !mcterm_overlay_any_panel_visible ());

    mcterm_overlay_set_buttonbar ();

    WPanel *active_panel;

    if (!mcterm_mode || mcterm_panel == NULL)
        return;

    active_panel = (current_panel != NULL && widget_get_state (WIDGET (current_panel), WST_VISIBLE))
        ? current_panel
        : NULL;

    mcterm_overlay_draw_panel_slot (0, active_panel);
    mcterm_overlay_draw_panel_slot (1, active_panel);
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_overlay_after_filemanager_draw (void)
{
    mcterm_overlay_draw_visible_panels ();

    if (mcterm_mode)
    {
        mcterm_overlay_place_prompt ();
        if (mcterm_overlay_ready ())
            mcterm_overlay_place_cursor ();
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_overlay_resize (const WRect *r)
{
    WRect tr;
    Widget *tw;

    if (!mcterm_mode || mcterm_panel == NULL)
        return;

    mcterm_overlay_rect (r, &tr);
    tw = mcterm_overlay_widget ();
    if (!rects_are_equal (&tw->rect, &tr))
        widget_set_size_rect (tw, &tr);

    if (command_prompt)
    {
        Widget *cw = WIDGET (cmdline);
        WRect cr = { cw->rect.y, r->x, 1, r->cols };

        if (!rects_are_equal (&cw->rect, &cr))
            widget_set_size_rect (cw, &cr);
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_overlay_complete_or_cycle_focus (void)
{
    const char *text;
    WGroup *g_fm;
    Widget *focused;
    Widget *pw0;
    Widget *pw1;
    gboolean p0_vis;
    gboolean p1_vis;
    Widget *next;

    if (!mcterm_mode || !mcterm_overlay_ready ())
        return FALSE;

    text = input_get_ctext (cmdline);
    g_fm = GROUP (filemanager);
    focused = g_fm->current != NULL ? WIDGET (g_fm->current->data) : NULL;

    if (focused == WIDGET (cmdline) || (text != NULL && *text != '\0'))
    {
        /* The shell owns the line it is editing, so it does the completion too: hand it a Tab
           and let its own redraw show the result, whenever it is ready. */
        mcterm_send_key (mcterm_panel, '\t');
        return TRUE;
    }

    pw0 = get_panel_widget (0);
    pw1 = get_panel_widget (1);
    p0_vis = pw0 != NULL && widget_get_state (pw0, WST_VISIBLE);
    p1_vis = pw1 != NULL && widget_get_state (pw1, WST_VISIBLE);

    if (focused == pw0)
        next = p1_vis ? pw1 : mcterm_overlay_widget ();
    else if (focused == pw1)
        next = p0_vis ? pw0 : mcterm_overlay_widget ();
    else if (focused == mcterm_overlay_widget ())
        next = command_prompt ? WIDGET (cmdline) : NULL;
    else
    {
        Widget *cur = current_panel != NULL ? WIDGET (current_panel) : NULL;

        if (cur == pw0)
            next = p1_vis ? pw1 : (p0_vis ? pw0 : mcterm_overlay_widget ());
        else if (cur == pw1)
            next = p0_vis ? pw0 : (p1_vis ? pw1 : mcterm_overlay_widget ());
        else
            next = p0_vis ? pw0 : (p1_vis ? pw1 : NULL);
    }

    if (next != NULL)
    {
        widget_select (next);
        mcterm_overlay_draw_visible_panels ();
        tty_refresh ();
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

cb_ret_t
mcterm_overlay_send_enter_if_cmdline_empty (void)
{
    if (mcterm_mode && mcterm_overlay_ready () && input_is_empty (cmdline))
        return send_message (mcterm_overlay_widget (), NULL, MSG_KEY, '\n', NULL);

    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_overlay_show_panel_if_hidden (int idx)
{
    Widget *pw;

    if (!mcterm_mode || mcterm_panel == NULL)
        return FALSE;

    pw = get_panel_widget (idx);
    if (pw == NULL || widget_get_state (pw, WST_VISIBLE))
        return FALSE;

    widget_show (pw);
    /* The panel that comes up is the one to work on: leaving the current one hidden would put
       the cursor that commands read back out of sight. */
    if (get_panel_type (idx) == view_listing
        && (current_panel == NULL || !widget_get_state (WIDGET (current_panel), WST_VISIBLE)))
        current_panel = PANEL (pw);

    dir_watch_reload_pending ();
    mcterm_overlay_draw_visible_panels ();
    tty_refresh ();

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_overlay_toggle_panel_command (gboolean right_panel_command)
{
    const int panel_idx = right_panel_command ? 1 : 0;

    /* A non-listing panel (quick view, info, tree) is first switched back
       to the file listing; only a listing panel toggles its visibility. */
    if (get_panel_type (panel_idx) != view_listing)
    {
        create_panel (panel_idx, view_listing);

        if (mcterm_mode)
        {
            Widget *pw = get_panel_widget (panel_idx);

            if (pw != NULL)
            {
                widget_show (pw);
                if (current_panel == NULL
                    || !widget_get_state (WIDGET (current_panel), WST_VISIBLE))
                    current_panel = PANEL (pw);
            }

            dir_watch_reload_pending ();
            mcterm_overlay_draw_visible_panels ();
            tty_refresh ();
        }

        return TRUE;
    }

    if (!mcterm_mode)
    {
        mcterm_overlay_toggle ();
        if (mcterm_mode && mcterm_panel != NULL)
        {
            int other_idx = right_panel_command ? 0 : 1;
            Widget *pw = get_panel_widget (other_idx);

            if (pw != NULL)
            {
                widget_show (pw);
                if (current_panel == NULL
                    || !widget_get_state (WIDGET (current_panel), WST_VISIBLE))
                    current_panel = PANEL (pw);
            }

            dir_watch_reload_pending ();
            mcterm_overlay_draw_visible_panels ();
            tty_refresh ();
        }
    }
    else if (mcterm_panel != NULL)
    {
        int idx = right_panel_command ? 1 : 0;
        Widget *pw = get_panel_widget (idx);

        if (pw != NULL)
        {
            if (widget_get_state (pw, WST_VISIBLE))
            {
                widget_hide (pw);
                if (current_panel == PANEL (pw))
                {
                    Widget *other = get_panel_widget (1 - idx);

                    current_panel = (other != NULL && widget_get_state (other, WST_VISIBLE))
                        ? PANEL (other)
                        : PANEL (pw);
                }

                mcterm_overlay_focus_typing ();
                widget_draw (mcterm_overlay_widget ());
            }
            else
            {
                widget_show (pw);
                if (current_panel != NULL
                    && !widget_get_state (WIDGET (current_panel), WST_VISIBLE))
                    current_panel = PANEL (pw);
                dir_watch_reload_pending ();
            }

            mcterm_overlay_draw_visible_panels ();
            tty_refresh ();
        }
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

mcterm_overlay_cmdline_result_t
mcterm_overlay_run_cmdline (const char *cmd, gboolean is_cd, gboolean is_exit)
{
    gboolean from_panels;

    if ((is_cd && !mcterm_mode) || (is_exit && !mcterm_mode))
        return MCTERM_OVERLAY_CMDLINE_NOT_APPLICABLE;

    if (!mcterm_overlay_ready ())
    {
        if (mcterm_mode)
            return MCTERM_OVERLAY_CMDLINE_HANDLED;

        if (!mcterm_overlay_create_terminal ()
            || !mcterm_wait_for_prompt (mcterm_panel, MCTERM_INITIAL_PROMPT_TIMEOUT_MS))
            return MCTERM_OVERLAY_CMDLINE_NOT_APPLICABLE;
    }

    from_panels = !mcterm_mode;
    if (from_panels)
        mcterm_overlay_toggle ();

    /* Bringing the terminal up may send the shell to the panel's directory; its prompt for
       that must not pass for the command's, so the command waits for it. */
    if (!mcterm_overlay_ready ()
        && !mcterm_wait_for_prompt (mcterm_panel, MCTERM_INITIAL_PROMPT_TIMEOUT_MS))
        return MCTERM_OVERLAY_CMDLINE_HANDLED;

    if (!mcterm_overlay_live ())
        return MCTERM_OVERLAY_CMDLINE_NOT_APPLICABLE;

    mcterm_set_command_hint (mcterm_panel, cmd);
    if (mcterm_send_line (mcterm_panel, cmd))
    {
        mcterm_exec_from_panels = from_panels;
        return MCTERM_OVERLAY_CMDLINE_SENT;
    }

    mcterm_set_command_hint (mcterm_panel, NULL);
    return MCTERM_OVERLAY_CMDLINE_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Run a command in the terminal mc already has, if it is in a state to take one.
 *
 * @return TRUE when the command was dealt with - sent, or refused because the shell is busy.
 *         FALSE when this terminal is no place for it and the caller must run it the old way.
 */

/* Whether mc has a terminal running a shell of its own. */
gboolean
mcterm_overlay_has_terminal (void)
{
    return mcterm_overlay_live ();
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_overlay_exec_command (const char *cmd)
{
    Widget *pw;
    gboolean panels_hidden = FALSE;
    int i;

    /* Without the protocol the terminal cannot say when the command has finished, so such a
       shell gets no commands from us: the caller hands the screen over instead. */
    if (!mcterm_overlay_live () || !mcterm_osc7_capable (mcterm_panel))
        return FALSE;

    if (!mcterm_shell_at_prompt (mcterm_panel))
    {
        message (D_ERROR, MSG_ERROR, "%s", _ ("The terminal is already running a command"));
        return TRUE;
    }

    // The command is about to write; show the terminal it writes to.
    if (!mcterm_mode)
    {
        gboolean from_panels;

        mcterm_overlay_toggle ();
        /* Bringing the terminal up may send the shell to the panel's directory; its prompt
           for that must not pass for the command's. */
        from_panels = mcterm_overlay_ready ()
            || mcterm_wait_for_prompt (mcterm_panel, MCTERM_INITIAL_PROMPT_TIMEOUT_MS);
        if (!from_panels)
            return TRUE;
        mcterm_exec_from_panels = TRUE;
    }

    for (i = 0; i < 2; i++)
    {
        pw = get_panel_widget (i);
        if (pw != NULL && widget_get_state (pw, WST_VISIBLE))
        {
            widget_hide (pw);
            panels_hidden = TRUE;
        }
    }

    if (panels_hidden)
    {
        mcterm_overlay_set_buttonbar ();
        widget_draw (mcterm_overlay_widget ());
        tty_refresh ();
    }

    mcterm_set_command_hint (mcterm_panel, cmd);
    if (!mcterm_send_line (mcterm_panel, cmd))
    {
        mcterm_set_command_hint (mcterm_panel, NULL);
        mcterm_exec_from_panels = FALSE;
        return FALSE;
    }

    // Whatever it does to the files, the panels should know about it when it is done.
    mcterm_exec_needs_panel_reload = TRUE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_overlay_panel_exec (const char *cmd)
{
    return mcterm_overlay_exec_command (cmd);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_overlay_cmdline_is_empty (void)
{
    return (
        input_is_empty (cmdline)
        && (!mcterm_overlay_shell_owns_cmdline () || mcterm_shell_line_is_empty (mcterm_panel)));
}

/* --------------------------------------------------------------------------------------------- */

char *
mcterm_overlay_cmdline_text (void)
{
    if (!input_is_empty (cmdline))
        return g_strdup (input_get_ctext (cmdline));

    return mcterm_overlay_shell_owns_cmdline () ? mcterm_shell_line_text (mcterm_panel) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* Completion with the panels up is mc's own, with its list of choices, as it always was: the
   shell would list its choices into a terminal that is not on screen. The line is taken off the
   shell into mc's input, which keeps it, choices and all, until the next key: a second press
   lists the choices, any other key hands the line back to the shell first. */
static void
mcterm_overlay_complete_with_mc (void)
{
    if (input_is_empty (cmdline))
    {
        char *text = mcterm_shell_line_text (mcterm_panel);

        if (text != NULL)
        {
            const int point = mcterm_shell_line_point (mcterm_panel);

            mcterm_clear_shell_line (mcterm_panel);
            input_assign_text (cmdline, text);
            input_set_point (cmdline, point);
            g_free (text);
        }
    }

    input_complete (cmdline);
}

/* --------------------------------------------------------------------------------------------- */

/* The clipboard keys of the command line while the shell owns it. Store chooses the source as
   on mc's own input, with the shell's line as the line. Paste types the clipfile onto the
   shell's line as one line. Cut stays with the shell: mc cannot see what the shell's line
   editor has marked. */
static cb_ret_t
mcterm_overlay_clip_command (long command)
{
    switch (command)
    {
    case CK_Store:
    {
        char *text = mcterm_overlay_cmdline_text ();

        input_store_line_or_files (text);
        g_free (text);
        return MSG_HANDLED;
    }

    case CK_Paste:
    {
        char *text = input_clip_text ();

        if (text != NULL)
        {
            mcterm_overlay_focus_cmdline ();
            mcterm_overlay_move_cmdline_to_shell ();
            if (!mcterm_send_text (mcterm_panel, text))
                message (D_ERROR, MSG_ERROR, "%s", _ ("The shell did not take the whole text"));
            g_free (text);
        }
        return MSG_HANDLED;
    }

    default:
        return MSG_NOT_HANDLED;
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_overlay_paste_is_shells (void)
{
    if (mcterm_panel == NULL)
        return FALSE;

    if (mcterm_mode)
    {
        WGroup *g = GROUP (filemanager);

        // a panel over the terminal takes the keys, and so the paste
        return !mcterm_overlay_panel_focused (g->current != NULL ? WIDGET (g->current->data)
                                                                 : NULL);
    }

    return mcterm_overlay_shell_owns_cmdline ();
}

/* --------------------------------------------------------------------------------------------- */

cb_ret_t
mcterm_overlay_handle_paste (const GString *text)
{
    // the first key ends the pause, as when the paste is typed
    if (mcterm_pause_pending)
        return MSG_NOT_HANDLED;

    if (!mcterm_mode)
    {
        mcterm_overlay_focus_cmdline ();
        mcterm_overlay_move_cmdline_to_shell ();
    }

    return send_message (mcterm_overlay_widget (), NULL, MSG_PASTE, 0, (void *) text);
}

/* --------------------------------------------------------------------------------------------- */

/* With the panels up, a key that would have gone to the command line goes to the shell's line
   editor instead. Called after mc's own keys have been dealt with, so what is left really is the
   command line's. */
cb_ret_t
mcterm_overlay_cmdline_key (int parm)
{
    long command;

    if (mcterm_mode || !mcterm_overlay_shell_owns_cmdline ())
        return MSG_NOT_HANDLED;

    command = widget_lookup_key (WIDGET (cmdline), parm);
    if (command == CK_Complete)
        mcterm_overlay_complete_with_mc ();
    else if (mcterm_overlay_clip_command (command) == MSG_NOT_HANDLED)
    {
        mcterm_overlay_move_cmdline_to_shell ();
        mcterm_overlay_send_cmdline_key (parm);
    }

    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

/* Whether the line is one mc answers to itself with the panels up: cd moves the panel, exit
   leaves mc. Neither is for the shell then. */
static gboolean
mcterm_overlay_line_is_mc_own (const char *text)
{
    while (whitespace (*text))
        text++;

    return ((strncmp (text, "cd", 2) == 0 && (text[2] == '\0' || whitespace (text[2])))
            || strcmp (text, "exit") == 0);
}

/* --------------------------------------------------------------------------------------------- */

/* Enter with the panels up. An empty line is the panel's Enter. cd and exit are taken off the
   shell and given to mc's own input, for the caller to deal with as it always has. Anything else
   the shell runs, with the terminal brought up to show it, the way a command typed into mc's own
   line is shown. */
cb_ret_t
mcterm_overlay_cmdline_enter (void)
{
    char *text;

    if (mcterm_mode || !mcterm_overlay_shell_owns_cmdline ())
        return MSG_NOT_HANDLED;

    text = mcterm_shell_line_text (mcterm_panel);
    if (text == NULL)
        return MSG_NOT_HANDLED;

    if (mcterm_overlay_line_is_mc_own (text))
    {
        mcterm_clear_shell_line (mcterm_panel);
        input_assign_text (cmdline, text);
        g_free (text);
        return MSG_NOT_HANDLED;
    }
    g_free (text);

    mcterm_overlay_toggle ();
    /* Bringing the terminal up may send the shell to the panel's directory, with the typed
       line parked and typed back at the prompt after: Enter waits for that prompt. */
    if (!mcterm_overlay_ready ()
        && !mcterm_wait_for_prompt (mcterm_panel, MCTERM_INITIAL_PROMPT_TIMEOUT_MS))
        return MSG_HANDLED;
    // The shell is busy from here on, not from the mark it sends back.
    if (mcterm_send_line (mcterm_panel, NULL))
    {
        mcterm_exec_needs_panel_reload = TRUE;
        mcterm_exec_from_panels = TRUE;
    }

    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

/* Hand the key to the terminal. A key it cannot type into the shell - a function key, say - is
   stopped here all the same when the file manager would run it on the panel cursor that cannot
   be seen: left unhandled, the dialog looks the key up in its own keymap and runs the command,
   and F8 deletes the file under the hidden cursor. */
static cb_ret_t
mcterm_overlay_key_to_terminal (Widget *w, int parm)
{
    if (send_message (mcterm_overlay_widget (), NULL, MSG_KEY, parm, NULL) == MSG_HANDLED)
        return MSG_HANDLED;

    return mcterm_overlay_command_needs_panel_cursor (widget_lookup_key (w, parm))
        ? MSG_HANDLED
        : MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

cb_ret_t
mcterm_overlay_handle_key (Widget *w, int parm, mcterm_overlay_command_cb_t execute_command,
                           mcterm_overlay_enter_cb_t execute_cmdline_enter, void *data)
{
    long cmd;
    long term_cmd = CK_IgnoreKey;

    if (!mcterm_mode || mcterm_panel == NULL)
        return MSG_NOT_HANDLED;

    if (mcterm_pause_pending)
    {
        mcterm_overlay_toggle ();
        return MSG_HANDLED;
    }

    // An open menu takes every key itself; none of them is the shell's.
    if (the_menubar != NULL && widget_get_state (WIDGET (the_menubar), WST_FOCUSED))
        return MSG_NOT_HANDLED;

    // A panel on screen owns the keys that walk a list.
    mcterm_set_scroll_allowed (mcterm_panel, !mcterm_overlay_any_panel_visible ());
    // Both can be turned off while the terminal is up, so they are told each time.
    mcterm_set_typing_elsewhere (mcterm_panel, command_prompt);

    // What is typed for the terminal to search or filter by takes the keys that edit it.
    if (mcterm_query_key (mcterm_panel, parm) == MSG_HANDLED)
        return MSG_HANDLED;

    {
        WGroup *g = GROUP (filemanager);
        Widget *focused = g->current != NULL ? WIDGET (g->current->data) : NULL;

        if (mcterm_overlay_panel_focused (focused))
        {
            if (parm == '\n' && command_prompt && !input_is_empty (cmdline))
                return execute_cmdline_enter (data);

            cmd = widget_lookup_key (w, parm);
            if (cmd == CK_ChangePanel)
                return execute_command (cmd, data);

            return MSG_NOT_HANDLED;
        }
    }
    // Tab moves focus into the terminal; in the command line it stays completion.
    if (parm == (KEY_M_SHIFT | '\t') && command_prompt && mcterm_overlay_terminal_focused ())
    {
        widget_select (WIDGET (cmdline));
        mcterm_overlay_draw_visible_panels ();
        mcterm_overlay_place_cursor ();
        tty_refresh ();
        return MSG_HANDLED;
    }

    // The command line is the shell's own line: Enter runs it in the shell that owns it.
    if ((parm == '\n' || parm == KEY_ENTER) && command_prompt && !mcterm_overlay_terminal_focused ()
        && !mcterm_in_alt_screen (mcterm_panel) && mcterm_shell_at_prompt (mcterm_panel)
        && mcterm_osc7_capable (mcterm_panel))
    {
        mcterm_send_key (mcterm_panel, '\r');
        // Whatever it does to the files, the panels should know about it when it is done.
        mcterm_exec_needs_panel_reload = TRUE;
        return MSG_HANDLED;
    }

    {
        gboolean in_alt = mcterm_in_alt_screen (mcterm_panel);
        gboolean at_prompt =
            mcterm_shell_at_prompt (mcterm_panel) && mcterm_osc7_capable (mcterm_panel);

        if ((parm == 0x0F || parm == XCTRL ('O')) && !in_alt)
            return MSG_NOT_HANDLED;

        // What the terminal itself acts on: the view, the cursor, and the mark. Only while it has
        // the focus - at the command line those same keys move and edit the shell's own line.
        term_cmd = mcterm_key_command (mcterm_panel, parm);

        /* The clipboard keys of the command line: paste, and copy when the terminal has no mark
           of its own for the same key to take. */
        if (mcterm_overlay_shell_owns_cmdline ())
        {
            const long clip_cmd = widget_lookup_key (WIDGET (cmdline), parm);

            if (clip_cmd == CK_Paste
                || (clip_cmd == CK_Store && !mcterm_mark_active (mcterm_panel)))
                return mcterm_overlay_clip_command (clip_cmd);
        }

        /* Clearing the output, marking it and cutting it down are the terminal's whoever is
           typing: the line stays put, and the view is not dragged back to the end first. */
        if (!in_alt && !mcterm_overlay_any_panel_visible ()
            && mcterm_overlay_terminal_owns (term_cmd))
            return mcterm_overlay_key_to_terminal (w, parm);

        if (!in_alt && !mcterm_overlay_any_panel_visible () && term_cmd != CK_IgnoreKey
            && mcterm_overlay_terminal_focused ())
        {
            cb_ret_t r = send_message (mcterm_overlay_widget (), NULL, MSG_KEY, parm, NULL);

            if (r == MSG_HANDLED)
                return r;
        }
        else if (!in_alt)
            mcterm_scroll_to_end (mcterm_panel);

        if (!in_alt && at_prompt && input_is_empty (cmdline))
        {
            cmd = widget_lookup_key (w, parm);
            if (cmd == CK_ChangePanel)
                return execute_command (cmd, data);
        }

        if (in_alt || !at_prompt)
            return mcterm_overlay_key_to_terminal (w, parm);
    }

    // At the shell's prompt the command line is its own: hand it the key to edit and recall with.
    if (mcterm_overlay_cmdline_takes_key (parm))
    {
        mcterm_overlay_send_cmdline_key (parm);
        return MSG_HANDLED;
    }

    /* A new file goes to the directory of the panel, which the shell follows, and needs no
       cursor to stand on. The panel keymap names the key, and with no panel on screen the
       panel is not asked. */
    if (!mcterm_overlay_any_panel_visible () && current_panel != NULL
        && keybind_lookup_keymap_command (panel_map, parm) == CK_EditNew)
        return send_message (current_panel, NULL, MSG_ACTION, CK_EditNew, NULL);

    /* With no panel on screen the terminal's keymap comes first: what it names is the
       terminal's, and the file manager's key of the same name never gets to run. */
    if (term_cmd != CK_IgnoreKey && !mcterm_overlay_any_panel_visible ())
        return mcterm_overlay_key_to_terminal (w, parm);

    cmd = widget_lookup_key (w, parm);
    if (cmd != CK_IgnoreKey && !mcterm_overlay_command_needs_panel_cursor (cmd))
        return execute_command (cmd, data);

    if (mcterm_overlay_starts_cmdline_input (parm))
    {
        mcterm_overlay_focus_cmdline ();
        mcterm_send_key (mcterm_panel, parm);
        return MSG_HANDLED;
    }

    return mcterm_overlay_key_to_terminal (w, parm);
}

#else /* !ENABLE_MCTERM */

gboolean
mcterm_overlay_pause_pending (void)
{
    return FALSE;
}

#include "src/execute.h"

void
mcterm_overlay_start (void)
{
}

gboolean
mcterm_overlay_show_terminal (void)
{
    return FALSE;
}

const char *
mcterm_overlay_prompt_text (void)
{
    return NULL;
}

void
mcterm_overlay_sync_shell_to_panel (void)
{
}

gboolean
mcterm_overlay_has_terminal (void)
{
    return FALSE;
}

gboolean
mcterm_overlay_exec_command (const char *cmd)
{
    (void) cmd;
    return FALSE;
}

gboolean
mcterm_overlay_active (void)
{
    return FALSE;
}

gboolean
mcterm_overlay_terminal_alone (void)
{
    return FALSE;
}

void
mcterm_overlay_toggle (void)
{
    toggle_terminal ();
}

void
mcterm_overlay_destroy (void)
{
}

void
mcterm_overlay_draw_visible_panels (void)
{
}

void
mcterm_overlay_after_filemanager_draw (void)
{
}

void
mcterm_overlay_resize (const WRect *r)
{
    (void) r;
}

gboolean
mcterm_overlay_complete_or_cycle_focus (void)
{
    return FALSE;
}

cb_ret_t
mcterm_overlay_send_enter_if_cmdline_empty (void)
{
    return MSG_NOT_HANDLED;
}

gboolean
mcterm_overlay_show_panel_if_hidden (int idx)
{
    (void) idx;
    return FALSE;
}

gboolean
mcterm_overlay_command_needs_panel_cursor (long command)
{
    (void) command;
    return FALSE;
}

gboolean
mcterm_overlay_toggle_panel_command (gboolean right_panel_command)
{
    (void) right_panel_command;
    return FALSE;
}

mcterm_overlay_cmdline_result_t
mcterm_overlay_run_cmdline (const char *cmd, gboolean is_cd, gboolean is_exit)
{
    (void) cmd;
    (void) is_cd;
    (void) is_exit;
    return MCTERM_OVERLAY_CMDLINE_NOT_APPLICABLE;
}

gboolean
mcterm_overlay_panel_exec (const char *cmd)
{
    (void) cmd;
    return FALSE;
}

gboolean
mcterm_overlay_cmdline_is_empty (void)
{
    return input_is_empty (cmdline);
}

char *
mcterm_overlay_cmdline_text (void)
{
    return input_is_empty (cmdline) ? NULL : g_strdup (input_get_ctext (cmdline));
}

cb_ret_t
mcterm_overlay_cmdline_key (int parm)
{
    (void) parm;
    return MSG_NOT_HANDLED;
}

gboolean
mcterm_overlay_paste_is_shells (void)
{
    return FALSE;
}

cb_ret_t
mcterm_overlay_handle_paste (const GString *text)
{
    (void) text;
    return MSG_NOT_HANDLED;
}

cb_ret_t
mcterm_overlay_cmdline_enter (void)
{
    return MSG_NOT_HANDLED;
}

cb_ret_t
mcterm_overlay_handle_key (Widget *w, int parm, mcterm_overlay_command_cb_t execute_command,
                           mcterm_overlay_enter_cb_t execute_cmdline_enter, void *data)
{
    (void) w;
    (void) parm;
    (void) execute_command;
    (void) execute_cmdline_enter;
    (void) data;
    return MSG_NOT_HANDLED;
}

#endif /* ENABLE_MCTERM */
