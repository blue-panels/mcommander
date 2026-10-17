/*
   Internal file viewer for the M-Commander
   Common finctions (used from some other mcviewer functions)

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

#include <string.h>  // memset()
#include <sys/types.h>

#include "lib/global.h"
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"  // save_file_position()
#include "lib/widget.h"
#include "lib/charsets.h"

#include "src/selcodepage.h"
#include "src/util.h"  // file_error_message()

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_magic_mode (WView *view)
{
    char *filename, *command;
    dir_list *dir;
    int *dir_idx;
    gboolean old_magic;

    if (view->filter_active)
        return;

    mcview_altered_flags.magic = TRUE;
    old_magic = view->mode_flags.magic;
    view->mode_flags.magic = !view->mode_flags.magic;

    if (view->source_controller != NULL && view->source_spec != NULL)
    {
        if (view->source_spec->raw_file == NULL
            || !mcview_source_set_raw (view, !view->mode_flags.magic))
            view->mode_flags.magic = old_magic;
        view->dpy_bbar_dirty = TRUE;
        view->dirty++;
        return;
    }

    // reinit view
    filename = g_strdup (vfs_path_as_str (view->filename_vpath));
    command = g_strdup (view->command);
    dir = view->dir;
    dir_idx = view->dir_idx;
    view->dir = NULL;
    view->dir_idx = NULL;
    mcview_done (view);
    mcview_init (view);
    mcview_load (view, command, filename, 0, 0, 0);
    view->dir = dir;
    view->dir_idx = dir_idx;
    g_free (filename);
    g_free (command);

    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_wrap_mode (WView *view)
{
    if (view->filter_active)
        return;

    mcview_selection_clear (view);
    view->mode_flags.wrap = !view->mode_flags.wrap;
    mcview_altered_flags.wrap = TRUE;
    view->dpy_wrap_dirty = TRUE;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_nroff_mode (WView *view)
{
    mcview_selection_clear (view);
    view->mode_flags.nroff = !view->mode_flags.nroff;
    mcview_altered_flags.nroff = TRUE;
    view->dpy_wrap_dirty = TRUE;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_syntax_mode (WView *view)
{
    if (view->mode_flags.hex || view->mode_flags.terminal)
        return;

    mcview_selection_clear (view);
    view->mode_flags.highlight = !view->mode_flags.highlight;
    mcview_altered_flags.highlight = TRUE;
    if (view->mode_flags.highlight && view->mode_flags.ansi)
    {
        // the two color the same bytes; the one just asked for wins
        view->mode_flags.ansi = FALSE;
        mcview_altered_flags.ansi = TRUE;
    }
    mcview_syntax_load (view);
    view->dpy_wrap_dirty = TRUE;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_ansi_mode (WView *view)
{
    if (view->mode_flags.terminal)
        return; /* the ansi flag is irrelevant while terminal mode is active */

    mcview_selection_clear (view);
    view->mode_flags.ansi = !view->mode_flags.ansi;
    mcview_altered_flags.ansi = TRUE;
    // and the syntax steps aside for it, or comes back when it is switched off
    mcview_syntax_load (view);
    view->dpy_wrap_dirty = TRUE;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_cycle_display_mode (WView *view)
{
    if (view->mode_flags.hex)
        return;

    mcview_selection_clear (view);
    if (view->mode_flags.terminal)
    {
        view->mode_flags.terminal = FALSE;
        mcview_vterm_free (view->vterm);
        view->vterm = NULL;
        view->mode_flags.ansi = FALSE;
        mcview_altered_flags.ansi = TRUE;
        view->dpy_wrap_dirty = TRUE;
    }
    else if (view->mode_flags.ansi)
    {
        if (view->filter_active)
            mcview_filter_deactivate (view);
        view->mode_flags.terminal = TRUE;
        view->vterm = mcview_vterm_new ();
        mcview_vterm_set_keep_history (view->vterm, TRUE);
    }
    else
    {
        view->mode_flags.ansi = TRUE;
        mcview_altered_flags.ansi = TRUE;
        view->dpy_wrap_dirty = TRUE;
    }

    // the syntax steps aside for the escapes, or comes back when they are shown again
    mcview_syntax_load (view);
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_hex_mode (WView *view)
{
    mcview_selection_clear (view);

    /* Filter, terminal and structured mode are text-mode only: deactivate before entering hex. */
    if (!view->mode_flags.hex)
    {
        if (view->mode_flags.structured)
            mcview_structured_leave (view);
        if (view->filter_active)
            mcview_filter_deactivate (view);
        if (view->mode_flags.terminal)
        {
            view->mode_flags.terminal = FALSE;
            mcview_vterm_free (view->vterm);
            view->vterm = NULL;
        }
    }

    view->mode_flags.hex = !view->mode_flags.hex;

    if (view->mode_flags.hex)
    {
        view->hex_cursor = view->dpy_start;
        view->dpy_start = mcview_offset_rounddown (view->dpy_start, view->bytes_per_line);
        widget_want_cursor (WIDGET (view), TRUE);
    }
    else
    {
        view->dpy_start = mcview_bol (view, view->hex_cursor, 0);
        view->hex_cursor = view->dpy_start;
        widget_want_cursor (WIDGET (view), FALSE);
        /* The terminal of a source that asked for one went with the hex:
           without it the escape sequences of the source are shown as text.
           The raw file behind F8 is text and stays text. */
        if (view->source_spec != NULL && view->source_spec->initial_terminal
            && (view->source_spec->raw_file == NULL || view->mode_flags.magic))
        {
            view->mode_flags.terminal = TRUE;
            if (view->vterm == NULL)
                view->vterm = mcview_vterm_new ();
            mcview_vterm_set_keep_history (view->vterm, TRUE);
            if (!view->source_spec->auto_scroll_bottom)
                mcview_vterm_set_dpy_top_row (view->vterm, 0);
        }
    }
    mcview_altered_flags.hex = TRUE;
    view->dpy_paragraph_skip_lines = 0;
    view->dpy_wrap_dirty = TRUE;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_init (WView *view)
{
    size_t i;

    view->filename_vpath = NULL;
    view->workdir_vpath = NULL;
    view->command = NULL;
    view->search_nroff_seq = NULL;
    view->source_display_mode = FALSE;
    view->source_wrap = FALSE;
    view->generator = NULL;
    view->generator_wakeup[0] = view->generator_wakeup[1] = -1;

    mcview_set_datasource_none (view);

    view->growbuf_in_use = FALSE;
    // leave the other growbuf fields uninitialized

    view->hexedit_lownibble = FALSE;
    view->locked = FALSE;
    view->coord_cache = NULL;
    view->lcache.by_bol = NULL;
    view->lcache.by_next = NULL;

    view->dpy_start = 0;
    view->dpy_paragraph_skip_lines = 0;
    mcview_state_machine_init (&view->dpy_state_top, 0);
    view->dpy_wrap_dirty = FALSE;
    view->force_max = -1;
    view->dpy_text_column = 0;
    view->dpy_end = 0;
    view->hex_cursor = 0;
    view->cursor_col = 0;
    view->cursor_row = 0;
    mcview_selection_init (view);
    view->change_list = NULL;

    // {status,ruler,data}_area are left uninitialized

    view->dirty = 0;
    view->dpy_bbar_dirty = TRUE;
    view->bytes_per_line = 1;

    view->search_start = 0;
    view->search_end = 0;

    view->marker = 0;
    for (i = 0; i < G_N_ELEMENTS (view->marks); i++)
        view->marks[i] = 0;

    view->update_steps = 0;
    view->update_activate = 0;

    view->saved_bookmarks = NULL;

    view->vterm = NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_remove_tmp_preview (WView *view)
{
    if (view != NULL && view->tmp_preview != NULL)
    {
        mc_unlink (view->tmp_preview);
        vfs_path_free (view->tmp_preview, TRUE);
        view->tmp_preview = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_tmp_preview (WView *view, const char *path)
{
    mcview_remove_tmp_preview (view);

    if (view != NULL && path != NULL)
        view->tmp_preview = vfs_path_from_str (path);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_done (WView *view)
{
    mcview_syntax_unload (view);

    // Save current file position
    if (mcview_remember_file_position && view->filename_vpath != NULL)
    {
        save_file_position (view->filename_vpath, -1, 0,
                            view->mode_flags.hex ? view->hex_cursor : view->dpy_start,
                            view->saved_bookmarks);
        view->saved_bookmarks = NULL;
    }

    /* A mode the user switched by hand becomes the one the next file opens in;
       the rest stay as the settings have them. A mode that came with the source,
       such as nroff for rendered Markdown, belongs to that source alone. */
    if (mcview_altered_flags.wrap)
        mcview_global_flags.wrap = view->mode_flags.wrap;
    if (mcview_altered_flags.hex)
        mcview_global_flags.hex = view->mode_flags.hex;
    if (mcview_altered_flags.magic)
        mcview_global_flags.magic = view->mode_flags.magic;
    if (mcview_altered_flags.ansi)
        mcview_global_flags.ansi = view->mode_flags.ansi;
    if (mcview_altered_flags.highlight)
        mcview_global_flags.highlight = view->mode_flags.highlight;
    if (mcview_altered_flags.nroff && !view->source_display_mode)
        mcview_global_flags.nroff = view->mode_flags.nroff;
    /* and goes with it: Quick view loads the next file into the same view, which would keep
       nroff, where the colors of the syntax are not drawn */
    if (view->source_display_mode)
        view->mode_flags.nroff = mcview_global_flags.nroff;
    if (view->source_wrap)
        view->mode_flags.wrap = mcview_global_flags.wrap;
    mcview_global_flags.structured = FALSE;

    mcview_structured_reset (view);
    mcview_selection_done (view);

    // Free memory used by the viewer
    // view->widget needs no destructor
    vfs_path_free (view->filename_vpath, TRUE);
    view->filename_vpath = NULL;
    vfs_path_free (view->workdir_vpath, TRUE);
    view->workdir_vpath = NULL;
    MC_PTR_FREE (view->command);

    mcview_close_datasource (view);
    mcview_source_controller_detach (view);
    // the growing buffer is freed with the datasource

    if (view->coord_cache != NULL)
    {
        g_ptr_array_free (view->coord_cache, TRUE);
        view->coord_cache = NULL;
    }

    if (view->converter == INVALID_CONV)
        view->converter = str_cnv_from_term;

    if (view->converter != str_cnv_from_term)
    {
        str_close_conv (view->converter);
        view->converter = str_cnv_from_term;
    }

    mcview_search_deinit (view);
    view->search = NULL;
    view->last_search_string = NULL;

    mcview_filter_deactivate (view);

    if (view->vterm != NULL)
    {
        mcview_vterm_free (view->vterm);
        view->vterm = NULL;
    }
    view->mode_flags.terminal = FALSE;

    mcview_hexedit_free_change_list (view);

    if (mc_global.mc_run_mode == MC_RUN_VIEWER && view->dir != NULL)
    {
        // mcviewer is the owner of file list
        dir_list_free_list (view->dir);
        g_free (view->dir);
        g_free (view->dir_idx);
    }

    view->dir = NULL;
}

/* --------------------------------------------------------------------------------------------- */

MC_MOCKABLE void
mcview_set_codeset (WView *view)
{
    const char *cp_id = NULL;

    view->utf8 = TRUE;
    cp_id = get_codepage_id (mc_global.source_codepage >= 0 ? mc_global.source_codepage
                                                            : mc_global.display_codepage);
    if (cp_id != NULL)
    {
        GIConv conv;
        conv = str_crt_conv_from (cp_id);
        if (conv != INVALID_CONV)
        {
            if (view->converter != str_cnv_from_term)
                str_close_conv (view->converter);
            view->converter = conv;
        }
        view->utf8 = (gboolean) str_isutf8 (cp_id);
        view->dpy_wrap_dirty = TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_select_encoding (WView *view)
{
    if (do_select_codepage ())
    {
        mcview_selection_clear (view);
        mcview_set_codeset (view);
    }
}

/* --------------------------------------------------------------------------------------------- */

MC_MOCKABLE void
mcview_show_error (WView *view, const char *format, const char *filename)
{
    if (mcview_is_in_panel (view))
        mcview_set_datasource_string (view, filename);
    else if (format != NULL)
        file_error_message (format, filename);
    else
        message (D_ERROR, MSG_ERROR, "%s", filename);
}

/* --------------------------------------------------------------------------------------------- */
/** returns index of the first char in the line
 * it is constant for all line characters
 */

off_t
mcview_bol (WView *view, off_t current, off_t limit)
{
    int c;

    if (current <= 0)
        return 0;

    const off_t filesize = mcview_get_filesize (view);

    if (current > filesize)
        return filesize;

    if (!mcview_get_byte (view, current, &c))
        return current;

    if (c == '\n')
    {
        if (!mcview_get_byte (view, current - 1, &c))
            return current;

        if (c == '\r')
            current--;
    }

    for (; current > 0 && current > limit; current--)
    {
        if (!mcview_get_byte (view, current - 1, &c))
            break;
        if (c == '\r' || c == '\n')
            break;
    }

    return current;
}

/* --------------------------------------------------------------------------------------------- */
/** returns index of last char on line + width EOL
 * mcview_eol of the current line == mcview_bol next line
 */

off_t
mcview_eol (WView *view, off_t current)
{
    int c;
    int prev_ch = 0;

    if (current < 0)
        return 0;

    for (; mcview_get_byte (view, current, &c); current++)
    {
        if (c == '\n')
        {
            current++;
            break;
        }
        if (prev_ch == '\r')
            break;

        prev_ch = c;
    }

    return current;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcview_get_title (const WDialog *h, const ssize_t width)
{
    const WView *view;
    const char *modified;
    const char *file_label;
    const char *view_filename;

    view = (const WView *) widget_find_by_type (CONST_WIDGET (h), mcview_callback);
    modified = view->hexedit_mode && (view->change_list != NULL) ? "(*) " : "    ";
    view_filename = vfs_path_as_str (view->filename_vpath);

    const ssize_t width1 = width - 4;

    /* Source-controller title takes precedence; falls back to filename/command. */
    if (view->source_spec != NULL && view->source_spec->title != NULL
        && view->source_spec->title[0] != '\0')
        file_label = view->source_spec->title;
    else
        file_label = view_filename != NULL ? view_filename
            : view->command != NULL        ? view->command
                                           : "";
    file_label = str_term_trim (file_label, width1 - str_term_width1 (_ ("View: ")));

    return g_strconcat (_ ("View: "), modified, file_label, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_calc_percent (WView *view, off_t p)
{
    off_t filesize;
    int percent;

    if (view->status_area.cols < 1 || (view->status_area.x + view->status_area.cols) < 4)
        return (-1);
    if (mcview_may_still_grow (view))
        return (-1);

    filesize = mcview_get_filesize (view);
    if (view->mode_flags.hex && filesize > 0)
    {
        // p can't be beyond the last char, only over that. Compensate for this.
        filesize--;
    }

    if (filesize == 0 || p >= filesize)
        percent = 100;
    else if (p > (INT_MAX / 100))
        percent = p / (filesize / 100);
    else
        percent = p * 100 / filesize;

    return percent;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_clear_mode_flags (mcview_mode_flags_t *flags)
{
    memset (flags, 0, sizeof (*flags));
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_global_flags_save (mcview_mode_flags_t *saved)
{
    *saved = mcview_global_flags;

    mcview_altered_flags.hex = FALSE;
    mcview_altered_flags.nroff = FALSE;
    mcview_altered_flags.ansi = FALSE;
    mcview_altered_flags.highlight = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_global_flags_restore (const mcview_mode_flags_t *saved)
{
    if (!mcview_altered_flags.hex)
        mcview_global_flags.hex = saved->hex;
    if (!mcview_altered_flags.nroff)
        mcview_global_flags.nroff = saved->nroff;
    if (!mcview_altered_flags.ansi)
        mcview_global_flags.ansi = saved->ansi;
    if (!mcview_altered_flags.highlight)
        mcview_global_flags.highlight = saved->highlight;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_load_text (WView *view, const char *text)
{
    mcview_load (view, NULL, "", 0, 0, 0);
    mcview_close_datasource (view);
    mcview_show_error (view, NULL, text);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_embedded (WView *view, gboolean embedded)
{
    view->embedded = embedded;
    mcview_compute_areas (view);
}

/* --------------------------------------------------------------------------------------------- */
