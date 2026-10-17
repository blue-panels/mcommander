/*
   Session plugin for the M-Commander
   mc on a pty of its own, that a new terminal can take over

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

/** \file session.c
 *  \brief Source: the session runtime plugin
 *
 *  In the mc the user starts, the early phase execs the helper, which is the client in that
 *  terminal and starts a holder and a new mc on a pty of the holder (see session_helper.c). In
 *  that mc the plugin hears from the holder that a new terminal has come, tells it the directory
 *  of the panel, and has the dialog of the sessions in the Command menu.
 *
 *  It is off until it is turned on in Manage Plugins.
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <gmodule.h>

#include "lib/global.h"
#include "lib/extension-runtime.h"
#include "lib/tty/tty.h"
#include "lib/widget.h"

#include "session_proto.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define B_SWITCH (B_USER + 1)
#define B_NEW    (B_USER + 2)
#define B_END    (B_USER + 3)
#define B_DETACH (B_USER + 4)

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

G_MODULE_EXPORT const mc_runtime_plugin_descriptor_v1_t *mc_runtime_plugin_register_v1 (void);

/*** file scope variables ************************************************************************/

static const mc_runtime_host_api_v1_t *host = NULL;
static mc_runtime_plugin_context_t *plugin_context = NULL;

/* the socket to the holder, in the mc of a session */
static int control_fd = -1;
static session_reader_t control_reader;
/* starts every mark this mc writes, see SESSION_MARK_HEAD */
static char *mark_token = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* The program to start again in the session: this one, where it can be found */

static char *
own_program (const char *argv0)
{
    char *self;

    self = g_file_read_link ("/proc/self/exe", NULL);
    if (self != NULL)
        return self;
    if (argv0 != NULL && strchr (argv0, '/') != NULL)
        return g_strdup (argv0);
    return g_find_program_in_path (argv0 != NULL ? argv0 : "mcommander");
}

/* --------------------------------------------------------------------------------------------- */

static void
session_pre_tty (mc_runtime_plugin_context_t *context, const mc_runtime_pre_tty_info_t *info)
{
    const char *term = g_getenv ("TERM");
    const char *helper;
    char *program;
    GPtrArray *argv;
    int i;

    (void) context;

    if (info->struct_size < sizeof (*info) || g_getenv (SESSION_ENV_ID) != NULL
        || g_strcmp0 (info->run_mode, "full") != 0 || info->nested || info->argc < 1)
        return;

    // the Linux console is not hung up, and the pty would hide it from cons.saver
    if (!isatty (STDIN_FILENO) || !isatty (STDOUT_FILENO) || term == NULL || term[0] == '\0'
        || strncmp (term, "linux", 5) == 0)
        return;

    helper = g_getenv ("MC_SESSION_HELPER");
    if (helper == NULL || helper[0] == '\0')
        helper = MC_SESSION_HELPER;

    program = own_program (info->argv[0]);
    if (program == NULL)
        return;

    argv = g_ptr_array_new ();
    g_ptr_array_add (argv, (gpointer) helper);
    g_ptr_array_add (argv, (gpointer) "client");
    g_ptr_array_add (argv, program);
    for (i = 0; i < info->argc; i++)
        g_ptr_array_add (argv, info->argv[i]);
    g_ptr_array_add (argv, NULL);

    execv (helper, (char **) argv->pdata);

    // no helper: this mc goes on without a session
    g_ptr_array_free (argv, TRUE);
    g_free (program);
}

/* --------------------------------------------------------------------------------------------- */

static void
write_mark (mc_runtime_plugin_context_t *context, gpointer user_data)
{
    const char *mark = (const char *) user_data;
    gsize len = strlen (mark);

    (void) context;

    while (len > 0)
    {
        ssize_t n;

        n = write (STDOUT_FILENO, mark, len);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return;
        }
        mark += n;
        len -= (gsize) n;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The environment of the new terminal, for the programs mc starts from now on; the ones running
   keep theirs */

static void
take_environment (const char *text)
{
    char **lines;
    int i;

    lines = g_strsplit (text, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        char *eq = strchr (lines[i], '=');
        int k;

        if (eq != NULL)
            *eq = '\0';
        for (k = 0; session_env_names[k] != NULL; k++)
            if (strcmp (lines[i], session_env_names[k]) == 0)
                break;
        if (session_env_names[k] == NULL)
            continue;
        if (eq != NULL)
            g_setenv (lines[i], eq + 1, TRUE);
        else
            g_unsetenv (lines[i]);
    }
    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */

/* The mark that tells the holder where the output for this terminal ends, and what next */

static char *
session_mark (const char *what, const char *id)
{
    return g_strconcat (SESSION_MARK_HEAD, mark_token, ";", what, id != NULL ? id : "",
                        SESSION_MARK_TAIL, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static char *
session_row (const session_info_t *s, gboolean self, const char *term)
{
    const char *state;
    char started[32];
    time_t t = (time_t) s->started;
    time_t now = time (NULL);
    struct tm when, today;
    GString *row;

    if (self)
        state = _ ("this one");
    else if (!s->attached)
        state = _ ("detached");
    else if (s->lost)
        state = _ ("looks lost");
    else
        state = _ ("attached");

    localtime_r (&t, &when);
    localtime_r (&now, &today);
    if (when.tm_year == today.tm_year && when.tm_yday == today.tm_yday)
        strftime (started, sizeof (started), "%H:%M", &when);
    else
        strftime (started, sizeof (started), "%b %d", &when);

    row = g_string_new (NULL);
    g_string_printf (row, "%-12s %-6s %s", state, started, s->cwd[0] != '\0' ? s->cwd : "?");
    if (strcmp (s->term, term) != 0)
        g_string_append_printf (row, "  (TERM=%s)", s->term);

    return g_string_free (row, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* A session that someone works in is taken or ended only when the user says so */

static gboolean
in_use_confirmed (const session_info_t *s, const char *question)
{
    return !s->attached || s->lost
        || query_dialog (_ ("Sessions"), question, D_NORMAL, 2, _ ("&Yes"), _ ("&No")) == 0;
}

/* --------------------------------------------------------------------------------------------- */

/* The sessions in a dialog; the mark to let the terminal go with, or NULL to stay */

static char *
sessions_dialog (void)
{
    const char *self_id = g_getenv (SESSION_ENV_ID);
    const char *term = g_getenv ("TERM");
    char *dir;
    char *mark = NULL;
    gboolean done = FALSE;

    if (term == NULL)
        term = "";

    dir = session_dir ();
    if (dir == NULL)
        return NULL;

    while (!done)
    {
        static const struct
        {
            int id;
            const char *label;
            button_flags_t flags;
        } buttons[] = {
            { B_SWITCH, N_ ("&Switch"), DEFPUSH_BUTTON },
            { B_NEW, N_ ("&New"), NORMAL_BUTTON },
            { B_END, N_ ("&End"), NORMAL_BUTTON },
            { B_DETACH, N_ ("&Detach"), NORMAL_BUTTON },
            { B_CANCEL, N_ ("&Cancel"), NORMAL_BUTTON },
        };
        WButton *button[G_N_ELEMENTS (buttons)];
        GPtrArray *list;
        WDialog *dlg;
        WGroup *g;
        WListbox *box;
        const session_info_t *chosen = NULL;
        int cols, lines, list_lines, width, x, y, ret, current = -1;
        guint i;

        list = session_list (dir);

        cols = MIN (76, COLS - 4);
        list_lines = MAX (3, MIN ((int) list->len, LINES - 12));
        // the margin, frame, header, list, line, buttons, frame, the margin
        lines = list_lines + 7;

        dlg = dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors, NULL, NULL,
                          "[Sessions]", _ ("Sessions"));
        g = GROUP (dlg);

        y = 2;
        group_add_widget (g, label_new (y++, 4, _ ("State        Start  Directory")));
        box = listbox_new (y, 3, list_lines, cols - 6, FALSE, NULL);
        for (i = 0; i < list->len; i++)
        {
            const session_info_t *s = (const session_info_t *) g_ptr_array_index (list, i);
            const gboolean self = g_strcmp0 (s->id, self_id) == 0;

            listbox_add_item_take (box, LISTBOX_APPEND_AT_END, 0, session_row (s, self, term),
                                   (void *) s, FALSE);
            if (current < 0 && !self)
                current = (int) i;
        }
        group_add_widget (g, box);
        y += list_lines;

        group_add_widget (g, hline_new (y++, -1, -1));
        width = 0;
        for (i = 0; i < G_N_ELEMENTS (buttons); i++)
        {
            button[i] =
                button_new (y, 0, buttons[i].id, buttons[i].flags, _ (buttons[i].label), NULL);
            width += WIDGET (button[i])->rect.cols + (i > 0 ? 1 : 0);
        }
        x = MAX (1, (cols - width) / 2);
        for (i = 0; i < G_N_ELEMENTS (buttons); i++)
        {
            WIDGET (button[i])->rect.x = x;
            x += WIDGET (button[i])->rect.cols + 1;
            group_add_widget (g, button[i]);
        }

        if (current >= 0)
            listbox_set_current (box, current);
        widget_select (WIDGET (box));

        ret = dlg_run (dlg);
        listbox_get_current (box, NULL, (void **) &chosen);
        widget_destroy (WIDGET (dlg));

        done = TRUE;
        if (ret == B_NEW)
            mark = session_mark (SESSION_MARK_NEW, NULL);
        else if (ret == B_DETACH)
            mark = session_mark (SESSION_MARK_DETACH, NULL);
        else if (ret == B_SWITCH && chosen != NULL && g_strcmp0 (chosen->id, self_id) != 0)
        {
            if (strcmp (chosen->term, term) != 0)
            {
                message (D_ERROR, _ ("Sessions"), "%s",
                         _ ("That session was started in another kind of terminal."));
                done = FALSE;
            }
            else if (in_use_confirmed (chosen,
                                       _ ("That session is in use in another terminal. "
                                          "Take it?")))
                mark = session_mark (SESSION_MARK_SWITCH, chosen->id);
            else
                done = FALSE;
        }
        else if (ret == B_END && chosen != NULL)
        {
            done = FALSE;
            if (g_strcmp0 (chosen->id, self_id) == 0)
                message (D_NORMAL, _ ("Sessions"), "%s",
                         _ ("This is the session you are in: F10 ends it."));
            else if (in_use_confirmed (chosen,
                                       _ ("That session is in use in another terminal. "
                                          "End it?")))
            {
                char *answer;

                answer = session_end (chosen->path);
                if (strcmp (answer, "busy") == 0)
                    message (D_ERROR, _ ("Sessions"), "%s",
                             _ ("That session has an editor, a viewer or a dialog open: "
                                "switch to it and end it there."));
                else if (strcmp (answer, "ok") != 0)
                    message (D_ERROR, _ ("Sessions"), _ ("The session did not end: %s"), answer);
                g_free (answer);
            }
        }

        g_ptr_array_free (list, TRUE);
    }

    g_free (dir);
    return mark;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
control_ready (mc_runtime_plugin_context_t *context, int fd, gpointer user_data)
{
    GString *data;
    gboolean alive;
    gboolean redraw = FALSE;
    char type;

    (void) user_data;

    alive = session_reader_fill (&control_reader, fd);
    data = g_string_new (NULL);
    while (session_reader_next (&control_reader, &type, data))
        if (type == SF_ATTACHED)
        {
            // another terminal: what it can do is asked, and the screen is drawn on it
            take_environment (data->str);
            (void) host->tty_suspend (context, NULL, NULL, TRUE, NULL);
            redraw = TRUE;
        }
        else if (type == SF_END)
        {
            // someone at the list of sessions ends this one: the way F10 does, or not at all
            const char *error = "not_supported";
            const char *answer = "ok";

            // the asker has given up: mc that was busy all that time would end unseen
            if (data->len > 0 && (long) time (NULL) > atol (data->str))
                answer = "late";
            else if ((host->capability_flags & MC_RUNTIME_HOST_CAP_QUIT) == 0
                     || !host->ui_quit (context, &error))
                answer = error != NULL ? error : "error";
            else
                redraw = TRUE;  // back to the loop of the dialog, which sees that mc ends
            (void) session_frame_write (fd, SF_END, answer, strlen (answer));
        }
    g_string_free (data, TRUE);

    if (!alive || control_reader.bad)
    {
        host->fd_unwatch (context, fd);
        close (fd);
        control_fd = -1;
        session_reader_free (&control_reader);
    }

    return redraw;
}

/* --------------------------------------------------------------------------------------------- */

static void
send_cwd (const char *path)
{
    if (control_fd >= 0 && path != NULL)
        (void) session_frame_write (control_fd, SF_CWD, path, strlen (path));
}

/* --------------------------------------------------------------------------------------------- */

static mc_runtime_event_result_t
on_startup (gpointer runtime_context, const mc_runtime_event_snapshot_t *snapshot,
            gpointer user_data)
{
    char *cwd;

    (void) runtime_context;
    (void) snapshot;
    (void) user_data;

    cwd = g_get_current_dir ();
    send_cwd (cwd);
    g_free (cwd);

    return MC_RUNTIME_EVENT_PASS;
}

/* --------------------------------------------------------------------------------------------- */

static mc_runtime_event_result_t
on_panel_chdir (gpointer runtime_context, const mc_runtime_event_snapshot_t *snapshot,
                gpointer user_data)
{
    (void) runtime_context;
    (void) user_data;

    send_cwd (snapshot->data.panel_chdir.new_path);

    return MC_RUNTIME_EVENT_PASS;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
session_init (const mc_runtime_host_api_v1_t *host_api, mc_runtime_plugin_context_t *context,
              GError **mcerror)
{
    const char *fd_text;
    const guint64 needed = MC_RUNTIME_HOST_CAP_FD_WATCH | MC_RUNTIME_HOST_CAP_TTY_SUSPEND;

    host = host_api;
    plugin_context = context;

    fd_text = g_getenv (SESSION_ENV_FD);
    if (fd_text == NULL || g_getenv (SESSION_ENV_TOKEN) == NULL
        || (host_api->capability_flags & needed) != needed)
        return TRUE;

    // not for the programs mc starts: one could pass for mc
    mark_token = g_strdup (g_getenv (SESSION_ENV_TOKEN));
    g_unsetenv (SESSION_ENV_TOKEN);
    control_fd = atoi (fd_text);
    g_unsetenv (SESSION_ENV_FD);
    if (control_fd <= STDERR_FILENO || fcntl (control_fd, F_SETFD, FD_CLOEXEC) != 0)
    {
        control_fd = -1;
        return TRUE;
    }

    session_reader_init (&control_reader);
    if (!host_api->fd_watch (context, control_fd, control_ready, NULL, NULL))
    {
        close (control_fd);
        control_fd = -1;
        return TRUE;
    }

    (void) host_api->subscribe (context, MC_RUNTIME_EVENT_STARTUP, 0, on_startup, NULL, NULL,
                                mcerror);
    (void) host_api->subscribe (context, MC_RUNTIME_EVENT_PANEL_CHDIR, 0, on_panel_chdir, NULL,
                                NULL, mcerror);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
session_shutdown (mc_runtime_plugin_context_t *context)
{
    (void) context;

    if (control_fd >= 0)
    {
        close (control_fd);
        control_fd = -1;
        session_reader_free (&control_reader);
    }
    g_clear_pointer (&mark_token, g_free);
}

/* --------------------------------------------------------------------------------------------- */

static void
session_enumerate_packages (mc_runtime_plugin_context_t *context,
                            mc_runtime_package_callback_t callback, gpointer user_data)
{
    (void) context;
    (void) callback;
    (void) user_data;
}

/* --------------------------------------------------------------------------------------------- */

static void
session_enumerate_actions (mc_runtime_plugin_context_t *context, const char *workspace,
                           mc_runtime_action_callback_t callback, gpointer user_data)
{
    (void) context;
    (void) workspace;
    (void) callback;
    (void) user_data;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
session_invoke_action (mc_runtime_plugin_context_t *context, const char *workspace,
                       const char *action_id, const char **error)
{
    char *mark;
    gboolean ok;

    if (control_fd < 0 || g_strcmp0 (workspace, "mc") != 0)
        return FALSE;

    if (g_strcmp0 (action_id, "sessions") == 0)
        mark = sessions_dialog ();
    else if (g_strcmp0 (action_id, "detach") == 0)
        mark = session_mark (SESSION_MARK_DETACH, NULL);
    else
        return FALSE;

    if (mark == NULL)
        return TRUE;

    // the terminal is left as the shell had it, the holder is told where it ends, and from then on
    // what mc draws goes nowhere until a terminal is attached again
    ok = host->tty_suspend (context, write_mark, mark, FALSE, error);
    g_free (mark);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static void
session_enumerate_menu_actions (mc_runtime_plugin_context_t *context, const char *workspace,
                                mc_runtime_menu_action_callback_t callback, gpointer user_data)
{
    (void) context;

    if (control_fd < 0 || g_strcmp0 (workspace, "mc") != 0)
        return;

    callback ("sessions", "Command", _ ("Sessions..."), NULL, 0, user_data);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

G_MODULE_EXPORT const mc_runtime_plugin_descriptor_v1_t *
mc_runtime_plugin_register_v1 (void)
{
    static const mc_runtime_plugin_descriptor_v1_t descriptor = {
        .abi_version = MC_RUNTIME_PLUGIN_ABI_VERSION,
        .struct_size = sizeof (mc_runtime_plugin_descriptor_v1_t),
        .capability_flags = MC_RUNTIME_PLUGIN_CAP_OFF_BY_DEFAULT,
        .runtime_name = "session",
        .required_host_capabilities = MC_RUNTIME_HOST_CAP_EVENTS | MC_RUNTIME_HOST_CAP_CONTEXT_DATA,
        .init = session_init,
        .shutdown = session_shutdown,
        .enumerate_packages = session_enumerate_packages,
        .enumerate_actions = session_enumerate_actions,
        .invoke_action = session_invoke_action,
        .enumerate_menu_actions = session_enumerate_menu_actions,
        .display_name = "Sessions",
        .pre_tty = session_pre_tty,
    };

    return &descriptor;
}

/* --------------------------------------------------------------------------------------------- */
