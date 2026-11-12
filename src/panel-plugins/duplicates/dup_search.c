/*
   Panel plugin duplicates for the M-Commander
   The progress dialog of a search for duplicates

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

/** \file dup_search.c
 *  \brief Source: the progress dialog of a search for duplicates
 */

#include <config.h>

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/tty/key.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "dup_search.h"

/*** file scope macro definitions ****************************************************************/

#define DLG_WIDTH    72
#define WAIT_MS      50

#define B_SHOW_FOUND (B_USER + 1)

/*** file scope type declarations ****************************************************************/

typedef struct
{
    WDialog *dlg;
    WLabel *walking;
    WLabel *counts;
    WLabel *hashing;
    WGauge *gauge;
    WLabel *current;
    WLabel *found;
    int gauge_cols;
    int pressed;  // the action of a button, or 0
} progress_t;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
button_callback (WButton *button, int action)
{
    (void) button;
    (void) action;
    return 0;  // the dialog stays; the loop reads ret_value
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
progress_dlg_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    if (msg == MSG_ACTION && parm == CK_Cancel)
    {
        DIALOG (w)->ret_value = B_CANCEL;
        return MSG_HANDLED;
    }

    return dlg_default_callback (w, sender, msg, parm, data);
}

/* --------------------------------------------------------------------------------------------- */

static void
progress_create (progress_t *p)
{
    WGroup *g;
    WButton *show, *cancel;
    int y = 2;
    const int x = 3;
    const int width = DLG_WIDTH - 2 * x;
    int bx;

    memset (p, 0, sizeof (*p));
    p->gauge_cols = width - 7;

    p->dlg = dlg_create (TRUE, 0, 0, 14, DLG_WIDTH, WPOS_CENTER, FALSE, dialog_colors,
                         progress_dlg_callback, NULL, "[Find duplicates]", _ ("Find duplicates"));
    g = GROUP (p->dlg);

    p->walking = label_new (y++, x, "");
    group_add_widget (g, p->walking);
    p->counts = label_new (y++, x, "");
    group_add_widget (g, p->counts);
    y++;
    p->hashing = label_new (y++, x, "");
    group_add_widget (g, p->hashing);
    p->gauge = gauge_new (y++, x, width, FALSE, 100, 0);
    group_add_widget (g, p->gauge);
    p->current = label_new (y++, x, "");
    group_add_widget (g, p->current);
    y++;
    p->found = label_new (y++, x, "");
    group_add_widget (g, p->found);
    group_add_widget (g, hline_new (y++, -1, -1));

    show = button_new (y, 0, B_SHOW_FOUND, DEFPUSH_BUTTON, _ ("&Show found"), button_callback);
    cancel = button_new (y, 0, B_CANCEL, NORMAL_BUTTON, _ ("&Cancel"), button_callback);
    bx = (DLG_WIDTH - button_get_width (show) - button_get_width (cancel) - 1) / 2;
    WIDGET (show)->rect.x = bx;
    WIDGET (cancel)->rect.x = bx + button_get_width (show) + 1;
    group_add_widget (g, show);
    group_add_widget (g, cancel);
    widget_select (WIDGET (show));

    p->dlg->help_file = dup_help_file ();
    dlg_init (p->dlg);
}

/* --------------------------------------------------------------------------------------------- */

static void
set_path (WLabel *label, const char *prefix, const char *path)
{
    char *text;

    text = g_strconcat (prefix, path != NULL ? path : "", (char *) NULL);
    label_set_text (label, str_trunc (text, DLG_WIDTH - 6));
    g_free (text);
}

/* --------------------------------------------------------------------------------------------- */

static char *
format_size (guint64 bytes)
{
    return g_format_size_full (bytes, G_FORMAT_SIZE_IEC_UNITS);
}

/* --------------------------------------------------------------------------------------------- */

/* Reads the events that came; a button that ends the dialog is left in p->pressed */
static void
progress_events (progress_t *p)
{
    Gpm_Event event;
    int c;

    event.x = -1;
    while ((c = tty_get_event (&event, FALSE, FALSE)) != EV_NONE)
    {
        p->dlg->ret_value = 0;
        dlg_process_event (p->dlg, c, &event);
        if (p->dlg->ret_value == B_CANCEL || p->dlg->ret_value == B_SHOW_FOUND)
        {
            p->pressed = p->dlg->ret_value;
            return;
        }
        event.x = -1;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
progress_show (progress_t *p, dup_session_t *s)
{
    char *dir, *size;
    guint files, taken;
    guint64 bytes;

    dup_session_walk_state (s, &dir, &files, &taken, &bytes);
    set_path (p->walking, _ ("Walking: "), dir);
    size = format_size (bytes);
    label_set_textv (p->counts, _ ("Files: %u   taken: %u (%s)"), files, taken, size);
    g_free (size);
    g_free (dir);

    if (s->state == DUP_SESSION_HASHING)
    {
        const duplicates_progress_t *pr = &s->progress;
        const guint groups = dup_session_groups (s)->len;
        char *done, *total, *freed;
        int value;

        done = format_size (pr->bytes_done);
        total = format_size (pr->bytes_total);
        label_set_textv (p->hashing, _ ("Hashing %u / %u files, %s / %s"), pr->files_done,
                         pr->files_total, done, total);

        /* by the files: a few large files at the end would hold a share of bytes at 0 */
        value = pr->files_total == 0
            ? p->gauge_cols
            : (int) ((guint64) p->gauge_cols * MIN (pr->files_done, pr->files_total)
                     / pr->files_total);
        gauge_set_value (p->gauge, p->gauge_cols, value);
        gauge_show (p->gauge, TRUE);
        set_path (p->current, "", pr->current);

        freed = format_size (dup_session_reclaimable (s));
        label_set_textv (p->found,
                         ngettext ("Found so far: %u group, %s can be freed",
                                   "Found so far: %u groups, %s can be freed", groups),
                         groups, freed);
        g_free (freed);
        g_free (total);
        g_free (done);
    }

    mc_refresh ();
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
dup_drain (int notify_fd)
{
    char buf[256];
    ssize_t n;

    do
        n = read (notify_fd, buf, sizeof (buf));
    while (n > 0 || (n == -1 && errno == EINTR));
}

/* --------------------------------------------------------------------------------------------- */

dup_progress_result_t
dup_progress_run (dup_session_t *s, int notify_fd)
{
    progress_t p;

    progress_create (&p);

    while (p.pressed == 0 && dup_session_running (s))
    {
        struct pollfd pfd = { notify_fd, POLLIN, 0 };

        (void) poll (&pfd, 1, WAIT_MS);
        dup_drain (notify_fd);
        dup_session_update (s);
        progress_show (&p, s);
        progress_events (&p);
    }

    dlg_run_done (p.dlg);
    widget_destroy (WIDGET (p.dlg));

    if (p.pressed == B_CANCEL)
        return DUP_PROGRESS_CANCEL;
    return p.pressed == B_SHOW_FOUND ? DUP_PROGRESS_SHOW : DUP_PROGRESS_DONE;
}

/* --------------------------------------------------------------------------------------------- */
