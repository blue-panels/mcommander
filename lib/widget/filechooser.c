/*
   Widgets for the M-Commander
   WFileChooser: the dialog that picks a file or a directory

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

/** \file filechooser.c
 *  \brief Source: WFileChooser, the dialog that picks a file or a directory
 */

#include <config.h>

#include <string.h>
#include <sys/stat.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/timefmt.h" /* file_date() */
#include "lib/tty/key.h"
#include "lib/util.h" /* size_trunc_len(), tilde_expand() */
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "lib/widget/filechooser.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define FC_SIZE_WIDTH     7  /* the size column of a panel */
#define FC_SIZE_WIDTH_MAX 12 /* a longer translation of "UP--DIR" is cut */

/*** file scope type declarations ****************************************************************/

typedef struct
{
    WDialog *dlg;
    WInput *path;
    WTable *list;
    WButton *ok, *cancel;

    file_chooser_mode_t mode;
    gboolean show_hidden;
    gboolean new_ok;

    vfs_path_t *dir;       /* the directory listed */
    GPtrArray *entries;    /* file_chooser_entry_t, ".." first when there is a parent */
    gboolean search_case;  /* the quick search of the list tells the case */
    WHLine *search_line;   /* the line under the path shows the text of the filter */
    char cell[BUF_MEDIUM]; /* the text of the cell asked last */
    char *result;
} WFileChooser;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
fc_entry_free (gpointer p)
{
    file_chooser_entry_t *e = (file_chooser_entry_t *) p;

    g_free (e->name);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

static gint
fc_entry_compare (gconstpointer a, gconstpointer b)
{
    const file_chooser_entry_t *x = *(const file_chooser_entry_t *const *) a;
    const file_chooser_entry_t *y = *(const file_chooser_entry_t *const *) b;
    int r;

    // as a panel sorts by name: directories first, then the names that start with a dot
    if (x->is_dir != y->is_dir)
        return x->is_dir ? -1 : 1;
    if ((x->name[0] == '.') != (y->name[0] == '.'))
        return x->name[0] == '.' ? -1 : 1;
    r = str_key_collate (x->sort_key, y->sort_key, FALSE);
    return r != 0 ? r : strcmp (x->name, y->name);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
fc_is_root (const vfs_path_t *dir)
{
    const char *s = vfs_path_as_str (dir);

    return s == NULL || strcmp (s, PATH_SEP_STR) == 0;
}

/* --------------------------------------------------------------------------------------------- */

static const file_chooser_entry_t *
fc_entry (const WFileChooser *fc, int row)
{
    if (fc->entries == NULL || row < 0 || (guint) row >= fc->entries->len)
        return NULL;
    return (const file_chooser_entry_t *) g_ptr_array_index (fc->entries, row);
}

/* --------------------------------------------------------------------------------------------- */

/* The parent of @dir, through the VFS so an archive or a remote path stays one; the name of
   @dir in it into @name, NULL at the top */
static vfs_path_t *
fc_parent (const vfs_path_t *dir, char **name)
{
    int i;

    // at the root of an archive the name is that of the archive, in the element before
    *name = NULL;
    for (i = vfs_path_elements_count (dir) - 1; i >= 0 && *name == NULL; i--)
    {
        const char *base = x_basename (vfs_path_get_by_index (dir, i)->path);

        if (base != NULL && *base != '\0')
            *name = g_strdup (base);
    }
    return vfs_path_append_new (dir, "..", (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* The path the line @row stands for: a directory ends with a slash.  When directories are
   chosen, ".." stands for the directory listed, so OK on it takes that one. */
static char *
fc_row_path (const WFileChooser *fc, int row)
{
    const file_chooser_entry_t *e = fc_entry (fc, row);
    vfs_path_t *vpath;
    char *s;

    if (e == NULL)
        return g_strdup (vfs_path_as_str (fc->dir));

    if (fc->mode == FILE_CHOOSER_DIR && strcmp (e->name, "..") == 0)
        vpath = vfs_path_clone (fc->dir);
    else
        vpath = vfs_path_append_new (fc->dir, e->name, (char *) NULL);
    // the path line goes into the history: no password of a VFS URL there
    s = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_PASSWORD);
    vfs_path_free (vpath, TRUE);
    if (e->is_dir && !g_str_has_suffix (s, PATH_SEP_STR))
    {
        char *t = g_strconcat (s, PATH_SEP_STR, (char *) NULL);

        g_free (s);
        s = t;
    }
    return s;
}

/* --------------------------------------------------------------------------------------------- */

/* The line of the cursor goes into the path line */
static void
fc_show_current (WFileChooser *fc)
{
    char *s;

    s = fc_row_path (fc, table_get_current (fc->list));
    input_assign_text (fc->path, s);
    g_free (s);
}

/* --------------------------------------------------------------------------------------------- */

/* List @dir and put the cursor on @select, a name in it, or on the first line */
static gboolean
fc_load (WFileChooser *fc, const vfs_path_t *dir, const char *select)
{
    GPtrArray *entries;
    int row = 0;
    guint i;

    // the file the dialog starts on, or the directory it came up from, shows though hidden
    entries = file_chooser_read_dir (dir, fc->show_hidden, fc->mode == FILE_CHOOSER_DIR, select);
    if (entries == NULL)
    {
        message (D_ERROR, MSG_ERROR, _ ("Cannot read directory contents\n%s"),
                 vfs_path_as_str (dir));
        return FALSE;
    }

    if (!fc_is_root (dir))
    {
        file_chooser_entry_t *up = g_new0 (file_chooser_entry_t, 1);

        up->name = g_strdup ("..");
        up->is_dir = TRUE;
        g_ptr_array_insert (entries, 0, up);
    }

    if (fc->entries != NULL)
        g_ptr_array_free (fc->entries, TRUE);
    fc->entries = entries;
    if (fc->dir != dir)
    {
        vfs_path_free (fc->dir, TRUE);
        fc->dir = vfs_path_clone (dir);
    }

    if (select != NULL)
        for (i = 0; i < entries->len; i++)
            if (strcmp (((file_chooser_entry_t *) g_ptr_array_index (entries, i))->name, select)
                == 0)
            {
                row = (int) i;
                break;
            }

    table_quick_search_stop (fc->list);
    table_set_current (fc->list, row);
    fc_show_current (fc);
    widget_draw (WIDGET (fc->list));
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Go into the directory of line @row, or up to the parent for ".." */
static void
fc_enter_dir (WFileChooser *fc, const file_chooser_entry_t *e)
{
    vfs_path_t *target;
    char *from = NULL;

    // the cursor lands on the directory we came out of
    if (strcmp (e->name, "..") == 0)
        target = fc_parent (fc->dir, &from);
    else
        target = vfs_path_append_new (fc->dir, e->name, (char *) NULL);

    (void) fc_load (fc, target, from);
    vfs_path_free (target, TRUE);
    g_free (from);
}

/* --------------------------------------------------------------------------------------------- */

/* The path line as a path: ~ expanded, relative to the directory listed */
static vfs_path_t *
fc_typed_path (const WFileChooser *fc)
{
    const char *text = input_get_ctext (fc->path);
    char *expanded;
    vfs_path_t *vpath;

    if (text == NULL || *text == '\0')
        return NULL;

    expanded = tilde_expand (text);
    if (g_path_is_absolute (expanded) || strstr (expanded, VFS_PATH_URL_DELIMITER) != NULL)
        vpath = vfs_path_from_str (expanded);
    else
        vpath = vfs_path_append_new (fc->dir, expanded, (char *) NULL);
    g_free (expanded);
    return vpath;
}

/* --------------------------------------------------------------------------------------------- */

/* Take the path line: a directory is listed, unless directories are what is chosen; a file
   is the answer.  TRUE when the dialog may close. */
static gboolean
fc_accept (WFileChooser *fc)
{
    vfs_path_t *vpath;
    struct stat st;
    gboolean exists, ok = FALSE;

    vpath = fc_typed_path (fc);
    if (vpath == NULL)
        return FALSE;

    exists = mc_stat (vpath, &st) == 0;

    /* a directory is listed, unless directories are chosen: then it is the answer */
    if (exists && S_ISDIR (st.st_mode) && fc->mode != FILE_CHOOSER_DIR)
    {
        (void) fc_load (fc, vpath, NULL);
        vfs_path_free (vpath, TRUE);
        return FALSE;
    }

    switch (fc->mode)
    {
    case FILE_CHOOSER_OPEN:
        if (exists || fc->new_ok)
            ok = TRUE;
        else
            message (D_ERROR, MSG_ERROR, _ ("Cannot stat\n%s"), vfs_path_as_str (vpath));
        break;
    case FILE_CHOOSER_SAVE:
        ok = !exists
            || query_dialog (_ ("Save"), _ ("The file exists. Overwrite it?"), D_NORMAL, 2,
                             _ ("&Yes"), _ ("&No"))
                == 0;
        break;
    default:
        if (!exists || !S_ISDIR (st.st_mode))
            message (D_ERROR, MSG_ERROR, _ ("%s\nis not a directory"), vfs_path_as_str (vpath));
        else
            ok = TRUE;
        break;
    }

    if (ok)
    {
        g_free (fc->result);
        fc->result = vfs_path_to_str_flags (vpath, 0, VPF_NONE);
    }
    vfs_path_free (vpath, TRUE);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/* table data source */

/* The name as the quick search finds it, without the slash of a directory */
static const char *
fc_search_text (void *data, int row)
{
    const file_chooser_entry_t *e = fc_entry ((const WFileChooser *) data, row);

    return e != NULL ? e->name : "";
}

/* --------------------------------------------------------------------------------------------- */

/* The fuzzy filter keeps ".." always, then ranks the directories, then the files */
static int
fc_search_group (void *data, int row)
{
    const file_chooser_entry_t *e = fc_entry ((const WFileChooser *) data, row);

    if (e == NULL || !e->is_dir)
        return 2;
    return strcmp (e->name, "..") == 0 ? -1 : 1;
}

/* --------------------------------------------------------------------------------------------- */

/* Directories in the hot color of the dialog, as the groups of the hotlist */
static int
fc_cell_color (void *data, int row, int col)
{
    const file_chooser_entry_t *e = fc_entry ((const WFileChooser *) data, row);

    (void) col;
    return e != NULL && e->is_dir ? dialog_colors[DLG_COLOR_HOT_NORMAL] : -1;
}

/* --------------------------------------------------------------------------------------------- */

static int
fc_get_nrows (const void *data)
{
    const WFileChooser *fc = (const WFileChooser *) data;

    return fc->entries == NULL ? 0 : (int) fc->entries->len;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
fc_get_text (const void *data, int row, int col)
{
    WFileChooser *fc = (WFileChooser *) data;
    const file_chooser_entry_t *e = fc_entry (fc, row);

    if (e == NULL)
        return "";

    switch (col)
    {
    case 0:
        if (e->is_dir && strcmp (e->name, "..") != 0)
            g_snprintf (fc->cell, sizeof (fc->cell), "%s%s", PATH_SEP_STR, e->name);
        else
            g_strlcpy (fc->cell, e->name, sizeof (fc->cell));
        break;
    case 1:
        if (strcmp (e->name, "..") == 0)
            g_strlcpy (fc->cell, _ ("UP--DIR"), sizeof (fc->cell));
        else if (e->is_dir)
            g_strlcpy (fc->cell, _ ("DIR"), sizeof (fc->cell));
        else
            size_trunc_len (fc->cell, FC_SIZE_WIDTH, e->size, 0, FALSE);
        break;
    default:
        if (strcmp (e->name, "..") == 0)
            fc->cell[0] = '\0';
        else
            g_strlcpy (fc->cell, file_date (e->mtime), sizeof (fc->cell));
        break;
    }
    return fc->cell;
}

/* --------------------------------------------------------------------------------------------- */
/* dialog */

static int
fc_ok_button (WButton *button, int action)
{
    WFileChooser *fc = (WFileChooser *) DIALOG (WIDGET (button)->owner)->data.p;

    (void) action;
    return fc_accept (fc) ? 1 : 0;
}

/* --------------------------------------------------------------------------------------------- */

/* The size of the dialog for the screen as it is now */
static void
fc_size (int *lines, int *cols)
{
    *lines = MAX (12, LINES - 6);
    *cols = MIN (MAX (44, COLS - 10), 90);
    *cols = MIN (*cols, COLS - 2);
}

/* --------------------------------------------------------------------------------------------- */

/* OK and Cancel side by side in the middle of the bottom line */
static void
fc_place_buttons (WFileChooser *fc)
{
    const WRect *d = &CONST_WIDGET (fc->dlg)->rect;
    WRect r;
    int x;

    x = d->x + (d->cols - button_get_width (fc->ok) - button_get_width (fc->cancel) - 1) / 2;
    r = WIDGET (fc->ok)->rect;
    r.x = x;
    widget_set_size_rect (WIDGET (fc->ok), &r);
    r = WIDGET (fc->cancel)->rect;
    r.x = x + button_get_width (fc->ok) + 1;
    widget_set_size_rect (WIDGET (fc->cancel), &r);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
fc_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);
    WFileChooser *fc = (WFileChooser *) h->data.p;

    if (fc == NULL)
        return dlg_default_callback (w, sender, msg, parm, data);

    switch (msg)
    {
    case MSG_NOTIFY:
        if (sender == WIDGET (fc->list))
        {
            const file_chooser_entry_t *e = fc_entry (fc, table_get_current (fc->list));

            if (parm != CK_Enter)
                fc_show_current (fc);
            else if (e != NULL && e->is_dir)
                fc_enter_dir (fc, e);
            else if (fc_accept (fc))
            {
                h->ret_value = B_ENTER;
                dlg_close (h);
            }
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case MSG_RESIZE:
    {
        WRect r = w->rect;
        cb_ret_t ret;

        fc_size (&r.lines, &r.cols);
        ret = dlg_default_callback (w, NULL, MSG_RESIZE, 0, &r);
        fc_place_buttons (fc);
        return ret;
    }

    case MSG_KEY:
        // as in the panels: Ctrl-PgUp goes up, Ctrl-PgDn into the directory under the cursor
        if (parm == (KEY_M_CTRL | KEY_PPAGE))
        {
            if (!fc_is_root (fc->dir))
            {
                const file_chooser_entry_t up = { .name = (char *) "..", .is_dir = TRUE };

                fc_enter_dir (fc, &up);
            }
            return MSG_HANDLED;
        }
        if (parm == (KEY_M_CTRL | KEY_NPAGE))
        {
            const file_chooser_entry_t *e = fc_entry (fc, table_get_current (fc->list));

            if (e != NULL && e->is_dir)
                fc_enter_dir (fc, e);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

GPtrArray *
file_chooser_read_dir (const vfs_path_t *dir, gboolean show_hidden, gboolean dirs_only,
                       const char *keep)
{
    DIR *d;
    struct vfs_dirent *de;
    GPtrArray *entries;
    guint i;

    d = mc_opendir (dir);
    if (d == NULL)
        return NULL;

    entries = g_ptr_array_new_with_free_func (fc_entry_free);
    while ((de = mc_readdir (d)) != NULL)
    {
        file_chooser_entry_t *e;
        vfs_path_t *vpath;
        struct stat st;

        if (strcmp (de->d_name, ".") == 0 || strcmp (de->d_name, "..") == 0)
            continue;
        if (!show_hidden && de->d_name[0] == '.' && g_strcmp0 (de->d_name, keep) != 0)
            continue;

        vpath = vfs_path_append_new (dir, de->d_name, (char *) NULL);
        // a link to nothing shows itself, as in a panel
        if (mc_stat (vpath, &st) != 0 && mc_lstat (vpath, &st) != 0)
            memset (&st, 0, sizeof (st));
        vfs_path_free (vpath, TRUE);

        if (dirs_only && !S_ISDIR (st.st_mode))
            continue;

        e = g_new0 (file_chooser_entry_t, 1);
        e->name = g_strdup (de->d_name);
        e->is_dir = S_ISDIR (st.st_mode);
        e->size = (uintmax_t) st.st_size;
        e->mtime = st.st_mtime;
        g_ptr_array_add (entries, e);
    }
    mc_closedir (d);

    // the names compare by the locale, case aside
    for (i = 0; i < entries->len; i++)
    {
        file_chooser_entry_t *e = g_ptr_array_index (entries, i);

        e->sort_key = str_create_key_for_filename (e->name, FALSE);
    }
    g_ptr_array_sort (entries, fc_entry_compare);
    for (i = 0; i < entries->len; i++)
    {
        file_chooser_entry_t *e = g_ptr_array_index (entries, i);

        str_release_key (e->sort_key, FALSE);
        e->sort_key = NULL;
    }
    return entries;
}

/* --------------------------------------------------------------------------------------------- */

char *
file_chooser_dialog (const char *title, const char *start, const char *history_name,
                     file_chooser_mode_t mode, file_chooser_flags_t flags)
{
    table_column_def_t col_defs[] = {
        { 0, J_LEFT_FIT, TABLE_COL_TEXT },
        { FC_SIZE_WIDTH, J_RIGHT, TABLE_COL_TEXT },
        { 0, J_LEFT, TABLE_COL_TEXT },
    };
    const char *titles[3];
    WFileChooser fc;
    table_datasource_t ds;
    vfs_path_t *start_vpath = NULL;
    char *select = NULL;
    int lines, cols;
    struct stat st;

    memset (&fc, 0, sizeof (fc));
    fc.mode = mode;
    fc.show_hidden = (flags & FILE_CHOOSER_HIDDEN) != 0;
    fc.search_case = (flags & FILE_CHOOSER_SEARCH_CASE) != 0;
    fc.new_ok = (flags & FILE_CHOOSER_NEW_OK) != 0;

    /* start in the directory of @start, on the file it names */
    if (start != NULL && *start != '\0')
    {
        char *expanded = tilde_expand (start);

        start_vpath = vfs_path_from_str (expanded);
        g_free (expanded);
        if (mc_stat (start_vpath, &st) != 0 || !S_ISDIR (st.st_mode))
        {
            vfs_path_t *parent = fc_parent (start_vpath, &select);

            vfs_path_free (start_vpath, TRUE);
            start_vpath = parent;
        }
    }
    if (start_vpath == NULL)
        start_vpath = vfs_path_clone (vfs_get_raw_current_dir ());

    fc_size (&lines, &cols);

    // the size column takes the words of the directories; the date one the longest date of the
    // locale and a space before the scrollbar
    col_defs[1].width = MAX (str_term_width1 (_ ("UP--DIR")), str_term_width1 (_ ("DIR")));
    col_defs[1].width = CLAMP (col_defs[1].width, FC_SIZE_WIDTH, FC_SIZE_WIDTH_MAX);
    col_defs[2].width = (int) i18n_checktimelength () + 1;

    fc.dlg = dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, TRUE, dialog_colors, fc_callback,
                         NULL, NULL, title);
    fc.dlg->data.p = &fc;

    fc.path = input_new (1, 2, input_colors, cols - 4, "", history_name,
                         INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD);
    group_add_widget_autopos (GROUP (fc.dlg), fc.path, WPOS_KEEP_TOP | WPOS_KEEP_HORZ, NULL);
    fc.search_line = hline_new (2, -1, -1);
    fc.search_line->text_align = J_LEFT;
    group_add_widget_autopos (GROUP (fc.dlg), fc.search_line, WPOS_KEEP_TOP | WPOS_KEEP_HORZ, NULL);

    fc.list = table_new (3, 2, lines - 6, cols - 2, (int) G_N_ELEMENTS (col_defs), col_defs);
    titles[0] = _ ("Name");
    titles[1] = _ ("Size");
    titles[2] = _ ("Modified");
    table_set_header (fc.list, titles);
    table_set_column_sizing (fc.list, 0, 10, TRUE);
    fc.list->scrollbar = TRUE;
    fc.list->scrollbar_on_frame = TRUE;
    ds.get_nrows = fc_get_nrows;
    ds.get_text = fc_get_text;
    ds.get_checked = NULL;
    ds.set_checked = NULL;
    ds.cycle_choice = NULL;
    ds.data = &fc;
    table_set_datasource (fc.list, ds);
    table_set_cell_color (fc.list, fc_cell_color);
    // letters narrow the list to the names that have them in their order, Ctrl-S searches
    table_set_quick_search (fc.list, 0, TRUE, fc.search_case);
    table_set_quick_search_text (fc.list, fc_search_text);
    table_set_quick_filter_fuzzy (fc.list, TRUE, fc_search_group);
    table_set_quick_search_label (fc.list, fc.search_line);
    table_layout (fc.list);
    group_add_widget_autopos (GROUP (fc.dlg), fc.list, WPOS_KEEP_ALL, NULL);

    group_add_widget_autopos (GROUP (fc.dlg), hline_new (lines - 3, -1, -1),
                              WPOS_KEEP_BOTTOM | WPOS_KEEP_HORZ, NULL);

    fc.ok = button_new (lines - 2, 0, B_ENTER, DEFPUSH_BUTTON, _ ("&OK"), fc_ok_button);
    fc.cancel = button_new (lines - 2, 0, B_CANCEL, NORMAL_BUTTON, _ ("&Cancel"), NULL);
    group_add_widget_autopos (GROUP (fc.dlg), fc.ok, WPOS_KEEP_BOTTOM | WPOS_KEEP_LEFT, NULL);
    group_add_widget_autopos (GROUP (fc.dlg), fc.cancel, WPOS_KEEP_BOTTOM | WPOS_KEEP_LEFT, NULL);
    fc_place_buttons (&fc);

    if (!fc_load (&fc, start_vpath, select))
        (void) fc_load (&fc, vfs_get_raw_current_dir (), NULL);
    widget_select (WIDGET (fc.list));

    if (dlg_run (fc.dlg) != B_ENTER)
        MC_PTR_FREE (fc.result);
    widget_destroy (WIDGET (fc.dlg));

    if (fc.entries != NULL)
        g_ptr_array_free (fc.entries, TRUE);
    vfs_path_free (fc.dir, TRUE);
    vfs_path_free (start_vpath, TRUE);
    g_free (select);
    return fc.result;
}

/* --------------------------------------------------------------------------------------------- */
