/*
   Terminal widget mcterm for the M-Commander
   A program in a terminal of its own, as a screen beside the panels

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

/** \file mcterm_screen.c
 *  \brief Source: a program in a terminal of its own, as a screen beside the panels
 *
 *  The program, an editor say, has the whole screen and every key but that of PluginConsole in
 *  the panel keymap, Alt-F3, which goes back to the panels. When it ends with code 0 its
 *  screen goes with it; otherwise the bottom row says how it ended, what it said last stays to
 *  be read, and the next key closes the screen.
 */

#include <config.h>

#include <time.h>  // nanosleep

#ifdef ENABLE_MCTERM

#include "lib/global.h"
#include "lib/keybind.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/tty/key.h"
#include "lib/tty/tty.h"
#include "lib/widget.h"
#include "lib/widget/dialog-switch.h"

#include "src/keymap.h"  // panel_map

#include "mcterm.h"
#include "mcterm_screen.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* How long a program that closed its terminal is given to be reaped, for its exit code */
#define SCREEN_EXIT_WAIT_MSEC 1000

/*** file scope type declarations ****************************************************************/

struct mcterm_screen_t
{
    WDialog *dlg;
    WMcTerm *term;
    char *title;
    char *program;  // the name the exit is reported with
    gboolean exited;
    gboolean settled;      // the exit code is read, or will not be
    gint64 reap_deadline;  // until when it is waited for
    int exit_code;         // -1 when not known
    void (*on_close) (void *data);
    void *data;
};

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static dlg_colors_t screen_colors;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* How the program ended, on the bottom row over its last words. */
static void
screen_draw_exit (const mcterm_screen_t *s)
{
    const WRect *r = &CONST_WIDGET (s->dlg)->rect;
    char *text;

    if (!s->settled || r->lines < 1 || r->cols < 1)
        return;

    if (s->exit_code >= 0)
        text = g_strdup_printf (_ ("%s has exited with code %d. Press any key to close"),
                                s->program, s->exit_code);
    else
        text = g_strdup_printf (_ ("%s has exited. Press any key to close"), s->program);

    tty_setcolor (MCTERM_NORMAL_COLOR);
    tty_gotoyx (r->y + r->lines - 1, r->x);
    tty_print_string (str_fit_to_term (text, r->cols, J_LEFT_FIT));
    g_free (text);
}

/* --------------------------------------------------------------------------------------------- */

/* The exit code is read once the program is reaped, in the idle time of the screen, which
   goes on taking keys and drawing meanwhile. A clean exit was a quit by the user: the screen
   goes with the program. A screen that is not in front is dealt with when it is in front again,
   picked from the screen list or by mcterm_screen_show (). */
static void
screen_settle (mcterm_screen_t *s)
{
    Widget *w = WIDGET (s->dlg);

    if (!s->settled)
    {
        if (!mcterm_exit_status (s->term, &s->exit_code)
            && g_get_monotonic_time () < s->reap_deadline)
        {
            // the idle loop of the screen comes back here at once: not too often
            const struct timespec ts = { 0, 10 * 1000 * 1000 };

            if (widget_get_state (w, WST_ACTIVE))
                nanosleep (&ts, NULL);
            widget_idle (w, TRUE);
            return;
        }
        s->settled = TRUE;
    }

    if (!widget_get_state (w, WST_ACTIVE))
    {
        widget_idle (w, TRUE);
        return;
    }

    widget_idle (w, FALSE);
    if (s->exit_code == 0)
        dlg_close (s->dlg);
    else
    {
        screen_draw_exit (s);
        tty_refresh ();
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
screen_program_exited (void *data)
{
    mcterm_screen_t *s = (mcterm_screen_t *) data;

    s->exited = TRUE;
    s->reap_deadline = g_get_monotonic_time () + (gint64) SCREEN_EXIT_WAIT_MSEC * 1000;
    screen_settle (s);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
screen_key (mcterm_screen_t *s, int key)
{
    // the key that opened the screen from the panels goes back to them
    if (panel_map != NULL && keybind_lookup_keymap_command (panel_map, key) == CK_PluginConsole)
    {
        if (filemanager != NULL)
            dialog_switch_to (filemanager);
        return MSG_HANDLED;
    }

    // the program has gone: its last words can still be read, any other key closes
    if (s->exited)
    {
        dlg_close (s->dlg);
        return MSG_HANDLED;
    }
    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
screen_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);
    mcterm_screen_t *s = (mcterm_screen_t *) h->data.p;

    switch (msg)
    {
    case MSG_DRAW:
        // below what the program wrote stood the panels: the whole screen is the program's
        tty_setcolor (MCTERM_NORMAL_COLOR);
        tty_fill_region (w->rect.y, w->rect.x, w->rect.lines, w->rect.cols, ' ');
        dlg_default_callback (w, sender, msg, parm, data);
        if (s != NULL)
            screen_draw_exit (s);
        return MSG_HANDLED;

    case MSG_KEY:
        return s != NULL ? screen_key (s, parm) : MSG_NOT_HANDLED;

    case MSG_IDLE:
        if (s != NULL && s->exited)
            screen_settle (s);
        else
            widget_idle (w, FALSE);
        return MSG_HANDLED;

    case MSG_VALIDATE:
        // mc quits: the screen goes, and the program with it
        if (mc_global.midnight_shutdown)
            dlg_close (h);
        return MSG_HANDLED;

    case MSG_DESTROY:
        if (s != NULL)
        {
            h->data.p = NULL;
            if (s->on_close != NULL)
                s->on_close (s->data);
            g_free (s->title);
            g_free (s->program);
            g_free (s);
        }
        return dlg_default_callback (w, sender, msg, parm, data);

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
screen_get_title (const WDialog *h, const ssize_t width)
{
    const mcterm_screen_t *s = (const mcterm_screen_t *) h->data.p;

    return g_strdup (str_fit_to_term (s != NULL ? s->title : "", (int) width, J_LEFT_FIT));
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mcterm_screen_t *
mcterm_screen_new (const char *title, const char *dir, const char *const *argv,
                   const char *const *env, void (*on_close) (void *data), void *data)
{
    mcterm_screen_t *s;
    WDialog *dlg;
    Widget *dw;
    WMcTerm *term;
    int i;

    for (i = 0; i < DLG_COLOR_COUNT; i++)
        screen_colors[i] = MCTERM_NORMAL_COLOR;

    dlg = dlg_create (FALSE, 0, 0, 1, 1, WPOS_FULLSCREEN, TRUE, screen_colors, screen_callback,
                      NULL, NULL, NULL);
    dw = WIDGET (dlg);
    // Tab is the program's
    widget_want_tab (dw, TRUE);

    term = mcterm_new_program (&dw->rect, dir, argv, env);
    if (term == NULL)
    {
        widget_destroy (dw);
        return NULL;
    }

    s = g_new0 (mcterm_screen_t, 1);
    s->dlg = dlg;
    s->term = term;
    s->exit_code = -1;
    s->title = g_strdup (title != NULL ? title : argv[0]);
    s->program = g_path_get_basename (argv[0]);
    s->on_close = on_close;
    s->data = data;
    dlg->data.p = s;
    dlg->get_title = screen_get_title;

    group_add_widget_autopos (GROUP (dlg), mcterm_widget (term), WPOS_KEEP_ALL, NULL);
    mcterm_set_shell_exit_callback (term, screen_program_exited, s);

    return s;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_screen_show (mcterm_screen_t *s)
{
    Widget *w;

    if (s == NULL)
        return FALSE;

    w = WIDGET (s->dlg);

    // the program was quit while the screen was not in front: the screen goes now
    if (s->exited && !s->settled && mcterm_exit_status (s->term, &s->exit_code))
        s->settled = TRUE;
    if (s->settled && s->exit_code == 0)
    {
        if (!widget_get_state (w, WST_CONSTRUCT))
            dialog_switch_remove (s->dlg);
        widget_destroy (w);
        return FALSE;
    }

    if (!widget_get_state (w, WST_CONSTRUCT))
    {
        dialog_switch_to (s->dlg);
        return TRUE;
    }

    // the first time: run here, the way the editor and the viewer run
    dlg_run (s->dlg);
    if (widget_get_state (w, WST_CLOSED))
        widget_destroy (w);
    // the screen covered both panels and the menu, not only the panel that opened it
    repaint_screen ();
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_screen_forget (mcterm_screen_t *s)
{
    if (s != NULL)
        s->on_close = NULL;
}

/* --------------------------------------------------------------------------------------------- */

#endif /* ENABLE_MCTERM */
