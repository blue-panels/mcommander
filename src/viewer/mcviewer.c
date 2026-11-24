/*
   Internal file viewer for the M-Commander
   Interface functions

   Copyright (C) 1994-2025
   Free Software Foundation, Inc.
   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

   Written by:
   Miguel de Icaza, 1994, 1995, 1998
   Janne Kukonlehto, 1994, 1995
   Jakub Jelinek, 1995
   Joseph M. Hinkle, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Roland Illig <roland.illig@gmx.de>, 2004, 2005
   Slava Zanko <slavazanko@google.com>, 2009, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2009-2022
   Ilia Maslakov <il.smind@gmail.com>, 2009, 2010
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the M-Commander
   a fork of GNU Midnight Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/mcconfig.h"
#include "lib/tty/tty.h"
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"  // load_file_position()
#include "lib/widget.h"
#include "lib/runtime-events.h"

#include "src/filemanager/layout.h"
#include "src/filemanager/filemanager.h"  // the_menubar
#include "src/events_init.h"
#include "src/runtime-host.h"

#include "internal.h"

/*** global variables ****************************************************************************/

mcview_mode_flags_t mcview_global_flags = {
    .wrap = TRUE, .hex = FALSE, .magic = TRUE, .nroff = FALSE, .ansi = FALSE, .highlight = FALSE
};

mcview_mode_flags_t mcview_altered_flags = {
    .wrap = FALSE, .hex = FALSE, .magic = FALSE, .nroff = FALSE, .ansi = FALSE, .highlight = FALSE
};

gboolean mcview_remember_file_position = FALSE;

/* Maxlimit for skipping updates */
int mcview_max_dirt_limit = 10;

/* Scrolling is done in pages or line increments */
gboolean mcview_mouse_move_pages = TRUE;

/* Open supported files (JSON/YAML/XML) in structured (tree) mode right away */
gboolean mcview_structured_auto = FALSE;

/* The file size and the node count the structured view stops at */
int mcview_structured_max_size = MCTREE_DEFAULT_MAX_PARSE_SIZE;
int mcview_structured_max_nodes = MCTREE_DEFAULT_MAX_NODES;

/* One-shot: open the next viewed file in structured mode.
 * A separate variable, not a bit in mcview_global_flags: intermediate viewer
 * instances (mc.ext preprocessing) write the global flags back on close and
 * would eat the request before the real file is loaded. */
gboolean mcview_open_structured_once = FALSE;

/* end of file will be showen from mcview_show_eof */
char *mcview_show_eof = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* The settings of the viewer, and the names they had while they lived in the
 * main section of the configuration file. A file written by an older version is
 * read under the old names once, and written back under the new ones. */
static const struct
{
    const char *name;
    const char *old_name;
    gboolean *value;
} viewer_bool_options[] = {
    { "wrap", "wrap_mode", &mcview_global_flags.wrap },
    { "syntax", "viewer_syntax_highlighting", &mcview_global_flags.highlight },
    { "mouse_move_pages", "mouse_move_pages_viewer", &mcview_mouse_move_pages },
    { "remember_file_position", "mcview_remember_file_position", &mcview_remember_file_position },
    { "structured_auto", "mcview_structured_auto", &mcview_structured_auto },
    { NULL, NULL, NULL },
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mcview_publish_runtime_open (WView *view, const char *source_kind, int start_line)
{
    mc_runtime_event_snapshot_t *snapshot;

    runtime_host_set_current_viewer (view);
    events_publish_runtime_startup ();
    if (!events_runtime_is_started () || !mc_runtime_events_is_initialized ())
        return;

    snapshot = mc_runtime_event_snapshot_new (MC_RUNTIME_EVENT_VIEWER_OPEN);
    snapshot->data.viewer_open.viewer =
        mc_runtime_handle_for_object (MC_RUNTIME_HANDLE_VIEWER, view);
    snapshot->data.viewer_open.path = view->filename_vpath != NULL
        ? vfs_path_to_str_flags (view->filename_vpath, 0, VPF_STRIP_PASSWORD)
        : g_strdup ("");
    snapshot->data.viewer_open.source_kind = g_strdup (source_kind);
    snapshot->data.viewer_open.start_line = (guint) MAX (start_line, 0);

    (void) mc_runtime_event_publish (snapshot, NULL);
    mc_runtime_event_snapshot_free (snapshot);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    WView *view = (WView *) w;
    const WRect *r = &view->data_area;
    gboolean ok = TRUE;

    switch (msg)
    {
    case MSG_MOUSE_DOWN:
        if (mcview_is_in_panel (view))
        {
            if (event->y == WIDGET (w->owner)->rect.y)
            {
                // return MOU_UNHANDLED
                event->result.abort = TRUE;
                // don't draw viewer over menu
                ok = FALSE;
                break;
            }

            if (!widget_get_state (w, WST_FOCUSED))
            {
                // Grab focus
                (void) change_panel ();
            }
        }

        if (mcview_selection_mouse (view, msg, event))
        {
            widget_select (w);
            view->dirty++;
            break;
        }
        MC_FALLTHROUGH;

    case MSG_MOUSE_CLICK:
        if (mcview_selection_mouse (view, msg, event))
        {
            view->dirty++;
            break;
        }

        if (view->mode_flags.structured)
        {
            // click moves the tree cursor; a click on the current row toggles the node
            if (msg == MSG_MOUSE_CLICK && view->struct_tree != NULL)
            {
                int row = event->y - r->y;

                if (row >= 0 && row < r->lines
                    && (guint) (view->struct_tree->top + row)
                        < mctree_view_row_count (view->struct_tree))
                {
                    int idx = view->struct_tree->top + row;

                    if (idx == view->struct_tree->cursor)
                        (void) mctree_view_toggle_current (view->struct_tree);
                    else
                    {
                        view->struct_tree->cursor = idx;
                        mctree_view_move (view->struct_tree, 0);
                    }
                    view->dirty++;
                }
            }
            break;
        }
        if (!view->mode_flags.wrap && !view->mode_flags.terminal)
        {
            // Scrolling left and right
            int x;

            x = event->x + 1;  // FIXME

            if (x < r->cols * 1 / 4)
            {
                mcview_move_left (view, 1);
                event->result.repeat = msg == MSG_MOUSE_DOWN;
            }
            else if (x < r->cols * 3 / 4)
            {
                // ignore the click
                ok = FALSE;
            }
            else
            {
                mcview_move_right (view, 1);
                event->result.repeat = msg == MSG_MOUSE_DOWN;
            }
        }
        else
        {
            // Scrolling up and down
            int y;

            y = event->y + 1;  // FIXME

            if (y < r->y + r->lines * 1 / 3)
            {
                int n = mcview_mouse_move_pages ? r->lines / 2 : 1;

                if (view->mode_flags.terminal && view->vterm != NULL)
                    mcview_vterm_set_dpy_top_row (
                        view->vterm,
                        mcview_vterm_resolve_scrollback_top_row (view->vterm, r->lines) - n);
                else
                    mcview_move_up (view, n);

                event->result.repeat = msg == MSG_MOUSE_DOWN;
            }
            else if (y < r->y + r->lines * 2 / 3)
            {
                // ignore the click
                ok = FALSE;
            }
            else
            {
                int n = mcview_mouse_move_pages ? r->lines / 2 : 1;

                if (view->mode_flags.terminal && view->vterm != NULL)
                    mcview_vterm_set_dpy_top_row (
                        view->vterm,
                        mcview_vterm_resolve_scrollback_top_row (view->vterm, r->lines) + n);
                else
                    mcview_move_down (view, n);

                event->result.repeat = msg == MSG_MOUSE_DOWN;
            }
        }
        break;

    case MSG_MOUSE_DRAG:
        if (mcview_selection_mouse (view, msg, event))
        {
            /* Keep extending a drag that leaves the viewport, as mcterm does. */
            if (event->y < r->y)
                mcview_move_up (view, 1);
            else if (event->y >= r->y + r->lines)
                mcview_move_down (view, 1);
            view->dirty++;
        }
        else
            ok = FALSE;
        break;

    case MSG_MOUSE_UP:
        (void) mcview_selection_mouse (view, msg, event);
        ok = FALSE;
        break;

    case MSG_MOUSE_SCROLL_UP:
        // ignore mouse wheel events in the inactive quick view panel
        if (widget_get_state (w, WST_FOCUSED))
        {
            if (view->mode_flags.structured && view->struct_tree != NULL)
                mctree_view_move (view->struct_tree, -2);
            else if (view->mode_flags.terminal && view->vterm != NULL)
                mcview_vterm_set_dpy_top_row (
                    view->vterm,
                    mcview_vterm_resolve_scrollback_top_row (view->vterm, view->data_area.lines)
                        - 2);
            else
                mcview_move_up (view, 2);
        }
        else
            ok = FALSE;
        break;

    case MSG_MOUSE_SCROLL_DOWN:
        // ignore mouse wheel events in the inactive quick view panel
        if (widget_get_state (w, WST_FOCUSED))
        {
            if (view->mode_flags.structured && view->struct_tree != NULL)
                mctree_view_move (view->struct_tree, 2);
            else if (view->mode_flags.terminal && view->vterm != NULL)
                mcview_vterm_set_dpy_top_row (
                    view->vterm,
                    mcview_vterm_resolve_scrollback_top_row (view->vterm, view->data_area.lines)
                        + 2);
            else
                mcview_move_down (view, 2);
        }
        else
            ok = FALSE;
        break;

    default:
        ok = FALSE;
        break;
    }

    if (ok)
        mcview_update (view);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Read the viewer settings from the [Viewer] section of the configuration file. */

void
mcview_load_options (void)
{
    /* No section of its own means a file written before the viewer got one:
       read the settings where they used to be, under their old names. */
    const gboolean old = !mc_config_has_group (mc_global.main_config, CONFIG_VIEWER_SECTION);
    const char *group = old ? CONFIG_APP_SECTION : CONFIG_VIEWER_SECTION;
    size_t i;

    for (i = 0; viewer_bool_options[i].name != NULL; i++)
        *viewer_bool_options[i].value =
            mc_config_get_bool (mc_global.main_config, group,
                                old ? viewer_bool_options[i].old_name : viewer_bool_options[i].name,
                                *viewer_bool_options[i].value);

    mcview_max_dirt_limit = mc_config_get_int (
        mc_global.main_config, group, old ? "max_dirt_limit" : "dirt_limit", mcview_max_dirt_limit);

    /* Both are new in the [Viewer] section: an older file has nothing to read
       them from, and the defaults of the resolver stand. */
    mcview_structured_max_size =
        mc_config_get_int (mc_global.main_config, CONFIG_VIEWER_SECTION, "structured_max_size",
                           mcview_structured_max_size);
    mcview_structured_max_nodes =
        mc_config_get_int (mc_global.main_config, CONFIG_VIEWER_SECTION, "structured_max_nodes",
                           mcview_structured_max_nodes);
    if (mcview_structured_max_size <= 0)
        mcview_structured_max_size = MCTREE_DEFAULT_MAX_PARSE_SIZE;
    if (mcview_structured_max_nodes <= 0)
        mcview_structured_max_nodes = MCTREE_DEFAULT_MAX_NODES;

    g_free (mcview_show_eof);
    mcview_show_eof =
        mc_config_get_string (mc_global.main_config, group, old ? "mcview_eof" : "eof", "");
}

/* --------------------------------------------------------------------------------------------- */

/** Write the viewer settings, and drop the copies left in the main section. */

void
mcview_save_options (void)
{
    size_t i;

    for (i = 0; viewer_bool_options[i].name != NULL; i++)
    {
        mc_config_set_bool (mc_global.main_config, CONFIG_VIEWER_SECTION,
                            viewer_bool_options[i].name, *viewer_bool_options[i].value);
        mc_config_del_key (mc_global.main_config, CONFIG_APP_SECTION,
                           viewer_bool_options[i].old_name);
    }

    mc_config_set_int (mc_global.main_config, CONFIG_VIEWER_SECTION, "dirt_limit",
                       mcview_max_dirt_limit);
    mc_config_del_key (mc_global.main_config, CONFIG_APP_SECTION, "max_dirt_limit");

    mc_config_set_int (mc_global.main_config, CONFIG_VIEWER_SECTION, "structured_max_size",
                       mcview_structured_max_size);
    mc_config_set_int (mc_global.main_config, CONFIG_VIEWER_SECTION, "structured_max_nodes",
                       mcview_structured_max_nodes);

    mc_config_set_string (mc_global.main_config, CONFIG_VIEWER_SECTION, "eof",
                          mcview_show_eof != NULL ? mcview_show_eof : "");
    mc_config_del_key (mc_global.main_config, CONFIG_APP_SECTION, "mcview_eof");
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_done_options (void)
{
    MC_PTR_FREE (mcview_show_eof);
}

/* --------------------------------------------------------------------------------------------- */

WView *
mcview_new (const WRect *r, gboolean is_panel)
{
    WView *view;
    Widget *w;

    view = g_new0 (WView, 1);
    w = WIDGET (view);

    widget_init (w, r, mcview_callback, mcview_mouse_callback);
    w->options |= WOP_SELECTABLE | WOP_TOP_SELECT;
    w->keymap = viewer_map;

    mcview_clear_mode_flags (&view->mode_flags);
    view->hexedit_mode = FALSE;
    view->hex_keymap = viewer_hex_map;
    view->struct_keymap = viewer_struct_map;
    view->hexview_in_text = FALSE;
    view->locked = FALSE;

    view->dpy_frame_size = is_panel ? 1 : 0;
    view->converter = str_cnv_from_term;

    mcview_init (view);

    {
        const mcview_mode_flags_t altered = mcview_altered_flags;

        if (mcview_global_flags.hex)
            mcview_toggle_hex_mode (view);
        if (mcview_global_flags.nroff)
            mcview_toggle_nroff_mode (view);
        if (mcview_global_flags.wrap)
            mcview_toggle_wrap_mode (view);
        if (mcview_global_flags.magic)
            mcview_toggle_magic_mode (view);
        if (mcview_global_flags.ansi)
            mcview_toggle_ansi_mode (view);
        view->mode_flags.highlight = mcview_global_flags.highlight;

        mcview_altered_flags = altered;
    }

    return view;
}

/* --------------------------------------------------------------------------------------------- */
/** Real view only */

gboolean
mcview_viewer (const char *command, const vfs_path_t *file_vpath, int start_line,
               off_t search_start, off_t search_end)
{
    gboolean succeeded;
    WView *lc_mcview;
    WDialog *view_dlg;
    Widget *vw, *b;
    WGroup *g;
    WRect r;

    // Create dialog and widgets, put them on the dialog
    view_dlg = dlg_create (FALSE, 0, 0, 1, 1, WPOS_FULLSCREEN, FALSE, NULL, mcview_dialog_callback,
                           NULL, "[Internal File Viewer]", NULL);
    view_dlg->help_file = MCVIEW_HELP_FILE;
    vw = WIDGET (view_dlg);
    widget_want_tab (vw, TRUE);

    g = GROUP (view_dlg);

    r = vw->rect;
    r.lines--;
    lc_mcview = mcview_new (&r, FALSE);
    group_add_widget_autopos (g, lc_mcview, WPOS_KEEP_ALL, NULL);

    b = WIDGET (buttonbar_new ());
    group_add_widget_autopos (g, b, b->pos_flags, NULL);

    view_dlg->get_title = mcview_get_title;

    succeeded = mcview_load (lc_mcview, command, vfs_path_as_str (file_vpath), start_line,
                             search_start, search_end);

    if (succeeded)
    {
        mcview_publish_runtime_open (lc_mcview, command != NULL ? "command" : "file", start_line);
        dlg_run (view_dlg);
    }
    else
        dlg_close (view_dlg);

    if (widget_get_state (vw, WST_CLOSED))
        widget_destroy (vw);

    return succeeded;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_viewer_fd (int fd)
{
    WView *lc_mcview;
    WDialog *view_dlg;
    Widget *vw, *b;
    WGroup *g;
    WRect r;

    view_dlg = dlg_create (FALSE, 0, 0, 1, 1, WPOS_FULLSCREEN, FALSE, NULL, mcview_dialog_callback,
                           NULL, "[Internal File Viewer]", NULL);
    view_dlg->help_file = MCVIEW_HELP_FILE;
    vw = WIDGET (view_dlg);
    widget_want_tab (vw, TRUE);

    g = GROUP (view_dlg);

    r = vw->rect;
    r.lines--;
    lc_mcview = mcview_new (&r, FALSE);
    group_add_widget_autopos (g, lc_mcview, WPOS_KEEP_ALL, NULL);

    b = WIDGET (buttonbar_new ());
    group_add_widget_autopos (g, b, b->pos_flags, NULL);

    view_dlg->get_title = mcview_get_title;

    mcview_set_datasource_raw_pipe (lc_mcview, fd);
    mcview_display (lc_mcview);

    mcview_publish_runtime_open (lc_mcview, "pipe", 0);
    dlg_run (view_dlg);

    if (widget_get_state (vw, WST_CLOSED))
        widget_destroy (vw);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_viewer_stream (const char *command)
{
    mc_pipe_t *p;
    GError *error = NULL;
    WView *lc_mcview;
    WDialog *view_dlg;
    Widget *vw, *b;
    WGroup *g;
    WRect r;

    /* stdout only, no stderr pipe (caller should redirect stderr in command) */
    p = mc_popen (command, TRUE, FALSE, &error);
    if (p == NULL)
    {
        message (D_ERROR, MSG_ERROR, "%s", error->message);
        g_error_free (error);
        return FALSE;
    }

    view_dlg = dlg_create (FALSE, 0, 0, 1, 1, WPOS_FULLSCREEN, FALSE, NULL, mcview_dialog_callback,
                           NULL, "[Internal File Viewer]", NULL);
    view_dlg->help_file = MCVIEW_HELP_FILE;
    vw = WIDGET (view_dlg);
    widget_want_tab (vw, TRUE);

    g = GROUP (view_dlg);

    r = vw->rect;
    r.lines--;
    lc_mcview = mcview_new (&r, FALSE);
    group_add_widget_autopos (g, lc_mcview, WPOS_KEEP_ALL, NULL);

    b = WIDGET (buttonbar_new ());
    group_add_widget_autopos (g, b, b->pos_flags, NULL);

    view_dlg->get_title = mcview_get_title;

    mcview_set_datasource_stdio_pipe (lc_mcview, p);
    mcview_stream_start (lc_mcview);

    mcview_publish_runtime_open (lc_mcview, "stream", 0);
    dlg_run (view_dlg);

    if (widget_get_state (vw, WST_CLOSED))
        widget_destroy (vw);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Open the viewer driven by a plugin source controller. This function owns
 * initial_spec and ctx once called. */
gboolean
mcview_viewer_with_controller (mcview_source_spec_t *initial_spec,
                               const mcview_source_controller_t *controller, void *ctx,
                               int start_line)
{
    WView *lc_mcview;
    WDialog *view_dlg;
    Widget *vw, *b;
    WGroup *g;
    WRect r;
    char *attach_error = NULL;

    if (controller == NULL
        || (initial_spec == NULL
            && (!controller->rebuild_on_resize || controller->prepare_viewport == NULL))
        || (initial_spec != NULL && initial_spec->command == NULL && initial_spec->file == NULL
            && (initial_spec->argv == NULL || initial_spec->argc == 0)))
    {
        if (controller != NULL && controller->free != NULL)
            controller->free (ctx);
        mcview_source_spec_free (initial_spec);
        return FALSE;
    }
    if (controller->open_options == NULL || controller->prepare == NULL
        || controller->commit == NULL || controller->rollback == NULL || controller->free == NULL)
    {
        if (controller->free != NULL)
            controller->free (ctx);
        mcview_source_spec_free (initial_spec);
        return FALSE;
    }

    view_dlg = dlg_create (FALSE, 0, 0, 1, 1, WPOS_FULLSCREEN, FALSE, NULL, mcview_dialog_callback,
                           NULL, "[Internal File Viewer]", NULL);
    view_dlg->help_file = MCVIEW_HELP_FILE;
    vw = WIDGET (view_dlg);
    widget_want_tab (vw, TRUE);

    g = GROUP (view_dlg);

    r = vw->rect;
    r.lines--;
    lc_mcview = mcview_new (&r, FALSE);
    group_add_widget_autopos (g, lc_mcview, WPOS_KEEP_ALL, NULL);

    b = WIDGET (buttonbar_new ());
    group_add_widget_autopos (g, b, b->pos_flags, NULL);

    view_dlg->get_title = mcview_get_title;

    if (!mcview_source_controller_attach (lc_mcview, initial_spec, controller, ctx, &attach_error))
    {
        message (D_ERROR, MSG_ERROR, "%s",
                 attach_error != NULL ? attach_error : _ ("Cannot open source."));
        g_free (attach_error);
        widget_destroy (vw);
        return FALSE;
    }

    mcview_publish_runtime_open (
        lc_mcview,
        lc_mcview->source_spec->command != NULL
                || (lc_mcview->source_spec->argv != NULL && lc_mcview->source_spec->argc != 0)
            ? "process"
            : "file",
        start_line);
    dlg_run (view_dlg);

    mcview_source_controller_detach (lc_mcview);

    if (widget_get_state (vw, WST_CLOSED))
        widget_destroy (vw);

    return TRUE;
}

/* {{{ Miscellaneous functions }}} */

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_load (WView *view, const char *command, const char *file, int start_line, off_t search_start,
             off_t search_end)
{
    gboolean retval = FALSE;
    vfs_path_t *vpath = NULL;

    g_assert (view->bytes_per_line != 0);

    /* End controller ownership before loading an unrelated source. */
    if (view->source_controller != NULL)
    {
        mcview_reset_for_source_swap (view);
        mcview_source_controller_detach (view);
    }

    // drop tree state of the previously shown file (quick view reuses the widget)
    mcview_structured_reset (view);
    mcview_selection_clear (view);

    view->filename_vpath = vfs_path_from_str (file);

    // get working dir
    if (file != NULL && file[0] != '\0')
    {
        vfs_path_free (view->workdir_vpath, TRUE);

        if (!g_path_is_absolute (file))
        {
            vfs_path_t *p;

            p = vfs_path_clone (vfs_get_raw_current_dir ());
            view->workdir_vpath = vfs_path_append_new (p, file, (char *) NULL);
            vfs_path_free (p, TRUE);
        }
        else
        {
            // try extract path from filename
            const char *fname;
            char *dir;

            fname = x_basename (file);
            dir = g_strndup (file, (size_t) (fname - file));
            view->workdir_vpath = vfs_path_from_str (dir);
            g_free (dir);
        }
    }

    if (!mcview_is_in_panel (view))
        view->dpy_text_column = 0;

    mcview_set_codeset (view);

    if (command != NULL && (view->mode_flags.magic || file == NULL || file[0] == '\0'))
        retval = mcview_load_command_output (view, command);
    else if (file != NULL && file[0] != '\0')
    {
        int fd;
        char tmp[BUF_MEDIUM];
        struct stat st;

        // Open the file
        vpath = vfs_path_from_str (file);
        fd = mc_open (vpath, O_RDONLY | O_NONBLOCK);
        if (fd == -1)
        {
            mcview_close_datasource (view);
            mcview_show_error (view, _ ("Cannot open\n%s"), file);
            vfs_path_free (view->filename_vpath, TRUE);
            view->filename_vpath = NULL;
            vfs_path_free (view->workdir_vpath, TRUE);
            view->workdir_vpath = NULL;
            goto finish;
        }

        // Make sure we are working with a regular file
        if (mc_fstat (fd, &st) == -1)
        {
            mc_close (fd);
            mcview_close_datasource (view);
            mcview_show_error (view, _ ("Cannot stat\n%s"), file);
            vfs_path_free (view->filename_vpath, TRUE);
            view->filename_vpath = NULL;
            vfs_path_free (view->workdir_vpath, TRUE);
            view->workdir_vpath = NULL;
            goto finish;
        }

        if (!S_ISREG (st.st_mode))
        {
            mc_close (fd);
            mcview_close_datasource (view);
            g_snprintf (tmp, sizeof (tmp), _ ("Cannot view\n%s\nNot a regular file"), file);
            mcview_show_error (view, NULL, tmp);
            vfs_path_free (view->filename_vpath, TRUE);
            view->filename_vpath = NULL;
            vfs_path_free (view->workdir_vpath, TRUE);
            view->workdir_vpath = NULL;
            goto finish;
        }

        if (st.st_size == 0 || mc_lseek (fd, 0, SEEK_SET) == -1)
        {
            // Must be one of those nice files that grow (/proc)
            mcview_set_datasource_vfs_pipe (view, fd);
        }
        else
        {
            if (view->mode_flags.magic)
            {
                int type;

                type = get_compression_type (fd, file);

                if (type != COMPRESSION_NONE)
                {
                    char *tmp_filename;
                    vfs_path_t *vpath1;
                    int fd1;

                    tmp_filename = g_strconcat (file, decompress_extension (type), (char *) NULL);
                    vpath1 = vfs_path_from_str (tmp_filename);
                    g_free (tmp_filename);
                    fd1 = mc_open (vpath1, O_RDONLY | O_NONBLOCK);
                    vfs_path_free (vpath1, TRUE);

                    if (fd1 == -1)
                    {
                        // VFS decompression failed -- display raw file content
                    }
                    else
                    {
                        mc_close (fd);
                        fd = fd1;
                        mc_fstat (fd, &st);
                    }
                }
            }

            mcview_set_datasource_file (view, fd, &st);
        }
        retval = TRUE;
    }

finish:
    view->command = g_strdup (command);
    view->dpy_start = 0;
    view->dpy_paragraph_skip_lines = 0;
    mcview_state_machine_init (&view->dpy_state_top, 0);
    view->dpy_wrap_dirty = FALSE;
    view->force_max = -1;
    view->dpy_text_column = 0;

    mcview_compute_areas (view);
    mcview_update_bytes_per_line (view);

    if (mcview_remember_file_position && view->filename_vpath != NULL && start_line == 0)
    {
        long line, col;
        off_t new_offset, max_offset;

        load_file_position (view->filename_vpath, &line, &col, &new_offset, &view->saved_bookmarks);
        max_offset = mcview_get_filesize (view) - 1;
        if (max_offset < 0)
            new_offset = 0;
        else
            new_offset = MIN (new_offset, max_offset);
        if (!view->mode_flags.hex)
        {
            view->dpy_start = mcview_bol (view, new_offset, 0);
            view->dpy_wrap_dirty = TRUE;
        }
        else
        {
            view->dpy_start = new_offset - new_offset % view->bytes_per_line;
            view->hex_cursor = new_offset;
        }
    }
    else if (start_line > 0)
        mcview_moveto (view, start_line - 1, 0);

    view->search_start = search_start;
    view->search_end = search_end;
    view->hexedit_lownibble = FALSE;
    view->hexview_in_text = FALSE;
    view->change_list = NULL;
    vfs_path_free (vpath, TRUE);

    if (retval && !view->mode_flags.hex && file != NULL && file[0] != '\0')
    {
        /* %view{structured} or the mctree symlink force the tree (with diagnostics);
           the auto option tries it quietly on matching extensions. */
        if (mcview_open_structured_once)
        {
            mcview_open_structured_once = FALSE;
            (void) mcview_structured_try_enter (view, FALSE);
        }
        else if (mcview_structured_auto && mcview_structured_auto_candidate (view))
            (void) mcview_structured_try_enter (view, TRUE);
    }

    mcview_syntax_load (view);

    return retval;
}

/* --------------------------------------------------------------------------------------------- */
