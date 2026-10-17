/*
   Internal file viewer for the M-Commander
   Plugin source-controller plumbing.

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

/** \file source.c
 *  \brief Source: plugin source-controller two-phase swap helpers.
 */

#include <config.h>

#include <fcntl.h>  // O_RDONLY, O_NONBLOCK
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/util.h"  // mc_pipe_t, mc_popen, mc_pclose
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "internal.h"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    enum
    {
        SRC_PIPE,
        SRC_FILE,
        SRC_GENERATOR
    } kind;
    mc_pipe_t *pipe;  // SRC_PIPE
    int fd;           // SRC_FILE
    struct stat st;   // SRC_FILE
    int wakeup[2];    // SRC_GENERATOR
} mcview_source_handle_t;

/*** file scope functions ************************************************************************/

static mcview_source_handle_t *
mcview_try_open_source (const mcview_source_spec_t *spec, char **err_out)
{
    mcview_source_handle_t *h;

    if (err_out != NULL)
        *err_out = NULL;

    if (spec == NULL
        || (spec->generator == NULL && spec->command == NULL && spec->file == NULL
            && (spec->argv == NULL || spec->argc == 0)))
    {
        if (err_out != NULL)
            *err_out = g_strdup (_ ("Source spec must set either command or file."));
        return NULL;
    }

    h = g_new0 (mcview_source_handle_t, 1);

    if (spec->generator != NULL)
    {
        if (pipe (h->wakeup) != 0)
        {
            if (err_out != NULL)
                *err_out = g_strdup (_ ("Cannot open source pipe."));
            g_free (h);
            return NULL;
        }
        for (guint i = 0; i < 2; i++)
        {
            if (fcntl (h->wakeup[i], F_SETFD, FD_CLOEXEC) == -1
                || fcntl (h->wakeup[i], F_SETFL, O_NONBLOCK) == -1)
            {
                close (h->wakeup[0]);
                close (h->wakeup[1]);
                if (err_out != NULL)
                    *err_out = g_strdup (_ ("Cannot open source pipe."));
                g_free (h);
                return NULL;
            }
        }
        h->kind = SRC_GENERATOR;
        return h;
    }

    if (spec->argv != NULL && spec->argc != 0)
    {
        GError *gerr = NULL;
        mc_pipe_t *p = mc_popen_argv ((const char *const *) spec->argv, spec->cwd, TRUE,
                                      spec->separate_stderr, &gerr);

        if (p == NULL)
        {
            if (err_out != NULL)
                *err_out =
                    g_strdup (gerr != NULL ? gerr->message : _ ("Cannot open source process."));
            g_clear_error (&gerr);
            g_free (h);
            return NULL;
        }
        h->kind = SRC_PIPE;
        h->pipe = p;
        return h;
    }
    if (spec->command != NULL)
    {
        GError *gerr = NULL;
        mc_pipe_t *p;

        p = mc_popen (spec->command, TRUE, FALSE, &gerr);
        if (p == NULL)
        {
            if (err_out != NULL)
                *err_out =
                    g_strdup (gerr != NULL ? gerr->message : _ ("Cannot open source command."));
            if (gerr != NULL)
                g_error_free (gerr);
            g_free (h);
            return NULL;
        }
        h->kind = SRC_PIPE;
        h->pipe = p;
        return h;
    }

    {
        vfs_path_t *vpath = vfs_path_from_str (spec->file);
        int fd = mc_open (vpath, O_RDONLY | O_NONBLOCK);

        if (fd == -1)
        {
            vfs_path_free (vpath, TRUE);
            if (err_out != NULL)
                *err_out = g_strdup_printf (_ ("Cannot open %s"), spec->file);
            g_free (h);
            return NULL;
        }
        if (mc_fstat (fd, &h->st) == -1)
        {
            mc_close (fd);
            vfs_path_free (vpath, TRUE);
            if (err_out != NULL)
                *err_out = g_strdup_printf (_ ("Cannot stat %s"), spec->file);
            g_free (h);
            return NULL;
        }
        vfs_path_free (vpath, TRUE);
        h->kind = SRC_FILE;
        h->fd = fd;
        return h;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_install_source (WView *view, mcview_source_handle_t *handle,
                       const mcview_source_spec_t *spec)
{
    gboolean process_source = handle->kind == SRC_PIPE;

    if (spec->raw_file != NULL)
        view->mode_flags.magic = TRUE;
    if (spec->initial_nroff || spec->initial_terminal)
        view->source_display_mode = TRUE;
    if (spec->initial_nroff)
    {
        view->mode_flags.hex = FALSE;
        view->mode_flags.nroff = TRUE;
    }
    /* A source that lays its own text out, such as a diagram wider than the
       screen, asks to be scrolled sideways rather than broken. */
    if (spec->has_wrap)
    {
        view->mode_flags.wrap = spec->wrap;
        view->source_wrap = TRUE;
    }
    if (spec->initial_terminal)
    {
        view->mode_flags.hex = FALSE;
        view->mode_flags.nroff = FALSE;
        view->mode_flags.ansi = FALSE;
        view->mode_flags.structured = FALSE;
        view->mode_flags.terminal = TRUE;
        if (view->vterm == NULL)
            view->vterm = mcview_vterm_new ();
        mcview_vterm_set_keep_history (view->vterm, TRUE);
        (void) mcview_vterm_set_size (view->vterm, MAX (view->data_area.lines, 1),
                                      MAX (view->data_area.cols, 1));
        mcview_vterm_set_dpy_top_row (
            view->vterm, spec->auto_scroll_bottom ? MCVIEW_VTERM_FOLLOW_END : (int) spec->top_row);
    }
    if (handle->kind == SRC_GENERATOR)
    {
        view->source_generation++;
        if (view->source_generation == 0)
            view->source_generation++;
        mcview_set_datasource_generator (view, spec->generator, handle->wakeup);
        g_clear_pointer (&view->command, g_free);
        vfs_path_free (view->filename_vpath, TRUE);
        view->filename_vpath = NULL;
        vfs_path_free (view->workdir_vpath, TRUE);
        view->workdir_vpath = NULL;
    }
    else if (handle->kind == SRC_PIPE)
    {
        view->source_generation++;
        if (view->source_generation == 0)
            view->source_generation++;
        mcview_set_datasource_stdio_pipe (view, handle->pipe);
        mcview_stream_start (view);

        g_free (view->command);
        view->command = g_strdup (spec->command != NULL ? spec->command : spec->argv[0]);
        vfs_path_free (view->filename_vpath, TRUE);
        view->filename_vpath = NULL;
        vfs_path_free (view->workdir_vpath, TRUE);
        view->workdir_vpath = NULL;
    }
    else
    {
        mcview_set_datasource_file (view, handle->fd, &handle->st);

        g_free (view->command);
        view->command = NULL;
        vfs_path_free (view->filename_vpath, TRUE);
        view->filename_vpath = vfs_path_from_str (spec->file);
        vfs_path_free (view->workdir_vpath, TRUE);
        view->workdir_vpath = NULL;
    }

    view->dpy_start = 0;
    view->dpy_paragraph_skip_lines = 0;
    mcview_state_machine_init (&view->dpy_state_top, 0);
    view->dpy_wrap_dirty = FALSE;
    view->force_max = -1;
    view->dpy_text_column = 0;

    /* Refresh the converter for the installed buffer. */
    mcview_set_codeset (view);

    mcview_compute_areas (view);
    mcview_update_bytes_per_line (view);

    g_free (handle); /* handle struct itself is transient; datasource owns the fd/pipe */
    if (process_source || (spec->generator != NULL && !view->growbuf_finished))
        mcview_source_state_notify (view, MCVIEW_SOURCE_STARTED, -1, 0);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_source_state_notify (WView *view, mcview_source_state_t state, int exit_code,
                            int term_signal)
{
    mcview_source_state_event_t event;

    if (view == NULL || view->source_controller == NULL
        || view->source_controller->source_state == NULL || view->source_generation == 0)
        return;

    event.generation = view->source_generation;
    event.state = state;
    event.exit_code = exit_code;
    event.term_signal = term_signal;
    event.output_size = (guint64) mcview_growbuf_filesize (view);
    view->source_controller->source_state (view->source_ctx, &event);
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_source_rebuild_viewport (WView *view)
{
    const mcview_source_controller_t *controller;
    mcview_source_spec_t *draft;
    mcview_source_handle_t *handle;
    char *error = NULL;

    if (view == NULL || !view->mode_flags.magic || view->source_controller == NULL
        || view->source_spec == NULL)
        return;
    controller = view->source_controller;
    if (!controller->rebuild_on_resize || controller->prepare_viewport == NULL)
        return;
    if (view->source_viewport_columns == (guint) MAX (view->data_area.cols, 1)
        && view->source_viewport_lines == (guint) MAX (view->data_area.lines, 1))
        return;
    draft = mcview_source_spec_clone (view->source_spec);
    if (!controller->prepare_viewport (view->source_ctx, draft, MAX (view->data_area.cols, 1),
                                       MAX (view->data_area.lines, 1), &error))
    {
        message (D_ERROR, MSG_ERROR, "%s",
                 error != NULL ? error : _ ("Viewport source prepare failed."));
        g_free (error);
        mcview_source_spec_free (draft);
        return;
    }
    handle = mcview_try_open_source (draft, &error);
    if (handle == NULL)
    {
        message (D_ERROR, MSG_ERROR, "%s",
                 error != NULL ? error : _ ("Cannot open resized source."));
        g_free (error);
        mcview_source_spec_free (draft);
        return;
    }
    mcview_reset_for_source_swap (view);
    mcview_install_source (view, handle, draft);
    mcview_source_spec_free (view->source_spec);
    view->source_spec = draft;
    view->source_viewport_columns = (guint) MAX (view->data_area.cols, 1);
    view->source_viewport_lines = (guint) MAX (view->data_area.lines, 1);
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

static int
mcview_capture_percent (WView *view)
{
    return mcview_calc_percent (view, view->dpy_start);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_restore_percent (WView *view, int percent)
{
    off_t total;
    off_t target;

    if (percent < 0)
        return;
    total = mcview_get_filesize (view);
    if (total <= 0)
        return;
    if (percent >= 100)
        target = total > 0 ? total - 1 : 0;
    else
        target = (off_t) ((double) percent * (double) total / 100.0);
    view->dpy_start = mcview_bol (view, target, 0);
    view->dpy_wrap_dirty = TRUE;
}

/*** public functions ****************************************************************************/

mcview_source_spec_t *
mcview_source_spec_clone (const mcview_source_spec_t *src)
{
    mcview_source_spec_t *dst;

    if (src == NULL)
        return NULL;
    dst = g_new0 (mcview_source_spec_t, 1);
    dst->command = g_strdup (src->command);
    dst->argv = g_strdupv (src->argv);
    dst->argc = src->argc;
    dst->cwd = g_strdup (src->cwd);
    dst->separate_stderr = src->separate_stderr;
    dst->file = g_strdup (src->file);
    dst->title = g_strdup (src->title);
    dst->help_file = g_strdup (src->help_file);
    dst->help_node = g_strdup (src->help_node);
    dst->auto_scroll_bottom = src->auto_scroll_bottom;
    dst->top_row = src->top_row;
    dst->initial_terminal = src->initial_terminal;
    dst->initial_nroff = src->initial_nroff;
    dst->has_wrap = src->has_wrap;
    dst->wrap = src->wrap;
    dst->raw_file = g_strdup (src->raw_file);
    dst->generator = mcview_generator_ref (src->generator);
    return dst;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_source_spec_free (mcview_source_spec_t *s)
{
    if (s == NULL)
        return;
    mcview_generator_unref (s->generator);
    g_free (s->command);
    g_strfreev (s->argv);
    g_free (s->cwd);
    g_free (s->file);
    g_free (s->title);
    g_free (s->help_file);
    g_free (s->help_node);
    g_free (s->raw_file);
    g_free (s);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_source_set_raw (WView *view, gboolean raw)
{
    mcview_source_spec_t *draft;
    mcview_source_handle_t *handle;
    char *error = NULL;

    if (view == NULL || view->source_controller == NULL || view->source_spec == NULL
        || view->source_spec->raw_file == NULL)
        return FALSE;

    if (raw)
    {
        draft = g_new0 (mcview_source_spec_t, 1);
        draft->file = g_strdup (view->source_spec->raw_file);
    }
    else
        draft = mcview_source_spec_clone (view->source_spec);

    handle = mcview_try_open_source (draft, &error);
    if (handle == NULL)
    {
        message (D_ERROR, MSG_ERROR, "%s",
                 error != NULL ? error : _ ("Cannot switch viewer source."));
        g_free (error);
        mcview_source_spec_free (draft);
        return FALSE;
    }

    mcview_reset_for_source_swap (view);
    mcview_install_source (view, handle, draft);
    mcview_source_spec_free (draft);
    if (!raw)
        mcview_source_rebuild_viewport (view);
    view->dirty++;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_source_controller_detach (WView *view)
{
    const mcview_source_controller_t *controller;
    mcview_source_spec_t *spec;
    void *ctx;

    if (view == NULL)
        return;

    /* close() may enter a dialog. Stop production before releasing the runtime
       controller so a nested event loop cannot call the old producer. */
    if (view->datasource == DS_GENERATOR)
    {
        mcview_generator_stop (view);
        if (!view->growbuf_finished)
        {
            view->growbuf_finished = TRUE;
            mcview_source_state_notify (view, MCVIEW_SOURCE_CANCELLED, -1, 0);
        }
    }
    controller = view->source_controller;
    spec = view->source_spec;
    ctx = view->source_ctx;
    view->source_controller = NULL;
    view->source_spec = NULL;
    view->source_ctx = NULL;
    view->source_viewport_columns = 0;
    view->source_viewport_lines = 0;

    if (controller != NULL && controller->free != NULL)
        controller->free (ctx);
    mcview_source_spec_free (spec);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_source_controller_attach (WView *view, mcview_source_spec_t *initial_spec,
                                 const mcview_source_controller_t *controller, void *ctx,
                                 char **err_out)
{
    mcview_source_handle_t *handle = NULL;
    gboolean viewport_source;

    if (err_out != NULL)
        *err_out = NULL;

    viewport_source =
        controller != NULL && controller->rebuild_on_resize && controller->prepare_viewport != NULL;
    if (view == NULL || controller == NULL || controller->open_options == NULL
        || controller->prepare == NULL || controller->commit == NULL || controller->rollback == NULL
        || controller->free == NULL || (initial_spec == NULL && !viewport_source))
    {
        if (err_out != NULL)
            *err_out = g_strdup (_ ("Invalid source controller."));
        goto fail;
    }

    if (initial_spec == NULL)
    {
        mcview_compute_areas (view);
        initial_spec = g_new0 (mcview_source_spec_t, 1);
        if (!controller->prepare_viewport (ctx, initial_spec, MAX (view->data_area.cols, 1),
                                           MAX (view->data_area.lines, 1), err_out))
            goto fail;
    }

    handle = mcview_try_open_source (initial_spec, err_out);
    if (handle == NULL)
        goto fail;

    /* Opening succeeded; replace the current source atomically. */
    mcview_reset_for_source_swap (view);
    mcview_source_controller_detach (view);

    view->source_controller = controller;
    view->source_ctx = ctx;
    view->source_spec = initial_spec;
    mcview_install_source (view, handle, initial_spec);
    if (viewport_source)
    {
        view->source_viewport_columns = (guint) MAX (view->data_area.cols, 1);
        view->source_viewport_lines = (guint) MAX (view->data_area.lines, 1);
    }
    if (initial_spec->auto_scroll_bottom)
        mcview_moveto_bottom (view);
    view->dirty++;
    return TRUE;

fail:
    if (handle != NULL)
        g_free (handle);
    if (controller != NULL && controller->free != NULL)
        controller->free (ctx);
    mcview_source_spec_free (initial_spec);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* Drop datasource-owned state before installing another source.
 * Keep source_spec/controller/ctx and filter snapshot state. */
void
mcview_reset_for_source_swap (WView *view)
{
    mcview_selection_done (view);
    mcview_selection_init (view);
    mcview_close_datasource (view);

    vfs_path_free (view->filename_vpath, TRUE);
    view->filename_vpath = NULL;
    vfs_path_free (view->workdir_vpath, TRUE);
    view->workdir_vpath = NULL;
    g_free (view->command);
    view->command = NULL;

    view->hexedit_lownibble = FALSE;
    view->hexview_in_text = FALSE;
    view->hex_cursor = 0;
    mcview_hexedit_free_change_list (view);

    if (view->search != NULL)
    {
        mc_search_free (view->search);
        view->search = NULL;
    }
    g_free (view->last_search_string);
    view->last_search_string = NULL;
    view->search_start = 0;
    view->search_end = 0;

    if (view->vterm != NULL)
    {
        mcview_vterm_free (view->vterm);
        view->vterm = NULL;
    }

    /* Offset-derived caches are rebuilt lazily for the new buffer. */
    if (view->coord_cache != NULL)
    {
        g_ptr_array_free (view->coord_cache, TRUE);
        view->coord_cache = NULL;
    }

    view->mode_flags.hex = FALSE;
    view->mode_flags.terminal = FALSE;
    view->hexedit_mode = FALSE;

    memset (view->marks, 0, sizeof (view->marks));
    view->marker = 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Generic options/swap flow. The current source stays visible until the
   replacement command/file has been prepared and opened successfully. */
void
mcview_source_options (WView *view)
{
    mcview_source_spec_t *draft;
    const mcview_source_controller_t *c;
    void *ctx;
    char *err = NULL;
    mcview_source_handle_t *handle;
    mcview_filter_snapshot_t snap;
    int percent;

    if (view == NULL || view->source_controller == NULL || view->source_spec == NULL)
        return;
    c = view->source_controller;
    ctx = view->source_ctx;
    if (c->open_options == NULL || c->prepare == NULL || c->commit == NULL || c->rollback == NULL)
        return;

    draft = mcview_source_spec_clone (view->source_spec);
    if (!c->open_options (ctx, draft))
    {
        c->rollback (ctx);
        mcview_source_spec_free (draft);
        return;
    }

    /* A source built for the viewport is built for it again. */
    if (c->rebuild_on_resize && c->prepare_viewport != NULL
            ? !c->prepare_viewport (ctx, draft, MAX (view->data_area.cols, 1),
                                    MAX (view->data_area.lines, 1), &err)
            : !c->prepare (ctx, draft, &err))
    {
        message (D_ERROR, MSG_ERROR, "%s", err != NULL ? err : _ ("Source prepare failed."));
        g_free (err);
        c->rollback (ctx);
        mcview_source_spec_free (draft);
        return;
    }

    handle = mcview_try_open_source (draft, &err);
    if (handle == NULL)
    {
        message (D_ERROR, MSG_ERROR, "%s", err != NULL ? err : _ ("Cannot open new source."));
        g_free (err);
        c->rollback (ctx);
        mcview_source_spec_free (draft);
        return;
    }

    mcview_filter_take_snapshot (view, &snap);
    percent = mcview_capture_percent (view);

    mcview_reset_for_source_swap (view);
    mcview_install_source (view, handle, draft);

    c->commit (ctx);
    mcview_source_spec_free (view->source_spec);
    view->source_spec = draft;

    (void) mcview_filter_restore (view, &snap);
    mcview_filter_snapshot_clear (&snap);

    if (draft->auto_scroll_bottom)
        mcview_moveto_bottom (view);
    else
        mcview_restore_percent (view, percent);

    view->dirty++;

    /* Options dialogs can leave stale frame cells after a source swap. */
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */
