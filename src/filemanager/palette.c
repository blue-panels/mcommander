/*
   File manager for the M-Commander
   The command palette: every menu command and hotlist directory in one fuzzy list

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

/** \file palette.c
 *  \brief Source: file manager command palette
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"
#include "lib/event.h"
#include "lib/keybind.h"
#include "lib/mcconfig.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/tty/key.h"  // KEY_*, XCTRL ()
#include "lib/tty/tty.h"  // LINES, COLS
#include "lib/util.h"     // list_append_unique()
#include "lib/widget.h"

#include "hotlist.h"
#include "palette.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define PALETTE_HISTORY       "palette"
#define PALETTE_HISTORY_SHOWN 10
/* the frame, the filter line, the line under it, the header and one row */
#define PALETTE_MIN_LINES 8
#define PALETTE_MIN_COLS  20

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *name;
    char *where;
    char *key;
    char *search; /* the text the filter looks at */
    char *id;     /* the key of the row in the history */
    long command;
    const menu_t *menu;
    char *directory; /* a hotlist row, else NULL */
} palette_row_t;

typedef struct
{
    GPtrArray *rows;
    const menu_t *skip;
    WInput *filter;
    WTable *list;
    char *filter_text; /* the text the list is filtered by */
    gboolean found;    /* the filter left a row */
    int chosen;
    int where_width, key_width; /* the widest texts of these columns */
    GList *history;             /* the ids of the rows run, the newest last */
    gboolean history_changed;
} palette_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
palette_row_free (gpointer data)
{
    palette_row_t *r = (palette_row_t *) data;

    g_free (r->name);
    g_free (r->where);
    g_free (r->key);
    g_free (r->search);
    g_free (r->id);
    g_free (r->directory);
    g_free (r);
}

/* --------------------------------------------------------------------------------------------- */

static palette_row_t *
palette_row (const palette_t *p, int row)
{
    if (row < 0 || (guint) row >= p->rows->len)
        return NULL;
    return (palette_row_t *) g_ptr_array_index (p->rows, row);
}

/* --------------------------------------------------------------------------------------------- */

static void
palette_add_menu_entry (const menu_t *menu, const char *menu_name, const char *name,
                        const char *shortcut, long command, void *data)
{
    palette_t *p = (palette_t *) data;
    const char *action, *desc;
    palette_row_t *r;

    if (menu == p->skip || command == CK_CommandPalette)
        return;

    action = keybind_lookup_actionname (command);
    desc = keybind_lookup_actiondesc (command);

    r = g_new0 (palette_row_t, 1);
    r->name = g_strdup (name);
    r->where = g_strdup (menu_name);
    r->key = g_strdup (shortcut != NULL ? shortcut : "");
    // the English description finds a command whatever the language of the menus
    r->search = desc != NULL ? g_strconcat (name, " ", desc, (char *) NULL) : g_strdup (name);
    r->id = action != NULL ? g_strconcat ("c:", action, (char *) NULL)
                           : g_strconcat ("m:", menu_name, "/", name, (char *) NULL);
    r->command = command;
    r->menu = menu;
    g_ptr_array_add (p->rows, r);
}

/* --------------------------------------------------------------------------------------------- */

static void
palette_add_hotlist_entry (const char *label, const char *directory, const char *group, void *data)
{
    palette_t *p = (palette_t *) data;
    palette_row_t *r;

    r = g_new0 (palette_row_t, 1);
    r->name = g_strdup (label);
    r->where = *group == '\0' ? g_strdup (_ ("Hotlist"))
                              : g_strdup_printf ("%s: %s", _ ("Hotlist"), group);
    r->key = g_strdup ("");
    r->search = g_strconcat (label, " ", directory, (char *) NULL);
    r->id = g_strconcat ("d:", directory, (char *) NULL);
    r->command = CK_IgnoreKey;
    r->directory = g_strdup (directory);
    g_ptr_array_add (p->rows, r);
}

/* --------------------------------------------------------------------------------------------- */

static void
palette_recent_first (palette_t *p, GList *history)
{
    guint top = 0;
    int shown = 0;
    GList *h;

    for (h = history; h != NULL && shown < PALETTE_HISTORY_SHOWN; h = g_list_previous (h))
    {
        guint i;

        for (i = top; i < p->rows->len; i++)
        {
            palette_row_t *r = (palette_row_t *) g_ptr_array_index (p->rows, i);

            if (strcmp (r->id, (const char *) h->data) == 0)
            {
                g_ptr_array_steal_index (p->rows, i);
                g_ptr_array_insert (p->rows, (gint) top, r);
                top++;
                shown++;
                break;
            }
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
palette_save_history (const gchar *event_group_name, const gchar *event_name, gpointer init_data,
                      gpointer data)
{
    const palette_t *p = (const palette_t *) init_data;
    const ev_history_load_save_t *ev = (const ev_history_load_save_t *) data;

    (void) event_group_name;
    (void) event_name;

    if (p->history_changed)
        mc_config_history_save (ev->cfg, PALETTE_HISTORY, p->history);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static int
palette_get_nrows (const void *data)
{
    return (int) ((const palette_t *) data)->rows->len;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
palette_get_text (const void *data, int row, int col)
{
    const palette_row_t *r = palette_row ((const palette_t *) data, row);

    if (r == NULL)
        return "";
    switch (col)
    {
    case 0:
        return r->name;
    case 1:
        return r->where;
    default:
        return r->key;
    }
}

/* --------------------------------------------------------------------------------------------- */

static const char *
palette_search_text (void *data, int row)
{
    const palette_row_t *r = palette_row ((const palette_t *) data, row);

    return r != NULL ? r->search : "";
}

/* --------------------------------------------------------------------------------------------- */

/* The hotlist in the hot color of the dialog, as in the hotlist itself */
static int
palette_cell_color (void *data, int row, int col)
{
    const palette_row_t *r = palette_row ((const palette_t *) data, row);

    (void) col;
    return r != NULL && r->directory != NULL ? dialog_colors[DLG_COLOR_HOT_NORMAL] : -1;
}

/* --------------------------------------------------------------------------------------------- */

static void
palette_size (int *lines, int *cols)
{
    *lines = MIN (MAX (12, LINES - 6), LINES - 2);
    *lines = MAX (*lines, PALETTE_MIN_LINES);
    *cols = MIN (MAX (44, COLS - 10), 90);
    *cols = MIN (*cols, COLS - 2);
    *cols = MAX (*cols, PALETTE_MIN_COLS);
}

/* --------------------------------------------------------------------------------------------- */

static void
palette_refilter (palette_t *p)
{
    const char *text = input_get_ctext (p->filter);

    if (g_strcmp0 (text, p->filter_text) == 0)
        return;
    g_free (p->filter_text);
    p->filter_text = g_strdup (text);
    p->found = table_set_filter_text (p->list, text);
}

/* --------------------------------------------------------------------------------------------- */

static void
palette_choose (WDialog *h, palette_t *p)
{
    palette_refilter (p);
    if (!p->found)
        return;
    p->chosen = table_get_current (p->list);
    if (palette_row (p, p->chosen) != NULL)
    {
        h->ret_value = B_ENTER;
        dlg_close (h);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
palette_is_move_key (int key)
{
    return key == KEY_UP || key == KEY_DOWN || key == KEY_PPAGE || key == KEY_NPAGE
        || key == XCTRL ('p') || key == XCTRL ('n');
}

/* --------------------------------------------------------------------------------------------- */

static void
palette_column_widths (palette_t *p, int cols)
{
    table_set_column_sizing (p->list, 1, MIN (p->where_width, cols / 4), FALSE);
    table_set_column_sizing (p->list, 2, MIN (p->key_width, cols / 5), FALSE);
    table_layout (p->list);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
palette_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);
    palette_t *p = (palette_t *) h->data.p;

    if (p == NULL)
        return dlg_default_callback (w, sender, msg, parm, data);

    switch (msg)
    {
    case MSG_KEY:
        if (palette_is_move_key (parm))
        {
            if (parm == XCTRL ('p'))
                parm = KEY_UP;
            else if (parm == XCTRL ('n'))
                parm = KEY_DOWN;
            send_message (p->list, NULL, MSG_KEY, parm, NULL);
            return MSG_HANDLED;
        }
        if (parm == '\n' || parm == KEY_ENTER)
        {
            palette_choose (h, p);
            return MSG_HANDLED;
        }
        if (!widget_get_state (WIDGET (p->filter), WST_FOCUSED)
            && (parm == KEY_BACKSPACE || (parm >= ' ' && parm <= 255)))
        {
            widget_select (WIDGET (p->filter));
            send_message (p->filter, NULL, MSG_KEY, parm, NULL);
            return MSG_HANDLED;
        }
        if (parm == ESC_CHAR && p->filter_text != NULL && *p->filter_text != '\0')
        {
            input_assign_text (p->filter, "");
            palette_refilter (p);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case MSG_POST_KEY:
        palette_refilter (p);
        return MSG_HANDLED;

    case MSG_PASTE:
    {
        cb_ret_t ret;

        widget_select (WIDGET (p->filter));
        ret = dlg_default_callback (w, sender, msg, parm, data);
        palette_refilter (p);
        return ret;
    }

    case MSG_NOTIFY:
        if (sender == WIDGET (p->list) && parm == CK_Enter)
        {
            palette_choose (h, p);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case MSG_RESIZE:
    {
        WRect r = w->rect;

        cb_ret_t ret;

        palette_size (&r.lines, &r.cols);
        ret = dlg_default_callback (w, NULL, MSG_RESIZE, 0, &r);
        palette_column_widths (p, r.cols);
        return ret;
    }

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
command_palette_run (const WMenuBar *menubar, const menu_t *skip, palette_choice_t *choice)
{
    table_column_def_t col_defs[] = {
        { 0, J_LEFT_FIT, TABLE_COL_TEXT },
        { 0, J_LEFT_FIT, TABLE_COL_TEXT },
        { 0, J_LEFT_FIT, TABLE_COL_TEXT },
    };
    const char *titles[3];
    palette_t p;
    table_datasource_t ds;
    WDialog *dlg;
    int lines, cols;
    guint i;
    gboolean ok = FALSE;

    choice->command = CK_IgnoreKey;
    choice->menu = NULL;
    choice->directory = NULL;

    memset (&p, 0, sizeof (p));
    p.rows = g_ptr_array_new_with_free_func (palette_row_free);
    p.skip = skip;
    p.found = TRUE;
    p.chosen = -1;
    p.filter_text = g_strdup ("");
    p.where_width = 1;
    p.key_width = 1;

    menubar_foreach_entry (menubar, palette_add_menu_entry, &p);
    hotlist_foreach (palette_add_hotlist_entry, &p);
    p.history = mc_config_history_get (PALETTE_HISTORY);
    palette_recent_first (&p, p.history);

    for (i = 0; i < p.rows->len; i++)
    {
        const palette_row_t *r = (const palette_row_t *) g_ptr_array_index (p.rows, i);

        p.where_width = MAX (p.where_width, str_term_width1 (r->where));
        p.key_width = MAX (p.key_width, str_term_width1 (r->key));
    }

    palette_size (&lines, &cols);

    dlg = dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors, palette_callback,
                      NULL, "[Command palette]", _ ("Command palette"));
    dlg->data.p = &p;
    mc_event_add (dlg->event_group, MCEVENT_HISTORY_SAVE, palette_save_history, &p, NULL);

    p.filter = input_new (2, 3, input_colors, cols - 6, "", NULL, INPUT_COMPLETE_NONE);
    group_add_widget_autopos (GROUP (dlg), p.filter, WPOS_KEEP_TOP | WPOS_KEEP_HORZ, NULL);
    group_add_widget_autopos (GROUP (dlg), hline_new (3, -1, -1), WPOS_KEEP_TOP | WPOS_KEEP_HORZ,
                              NULL);

    p.list = table_new (4, 3, lines - 6, cols - 4, (int) G_N_ELEMENTS (col_defs), col_defs);
    titles[0] = _ ("Command");
    titles[1] = _ ("Menu");
    titles[2] = _ ("Key");
    table_set_header (p.list, titles);
    table_set_column_sizing (p.list, 0, 10, TRUE);
    p.list->scrollbar = TRUE;
    p.list->scrollbar_on_frame = TRUE;
    ds.get_nrows = palette_get_nrows;
    ds.get_text = palette_get_text;
    ds.get_checked = NULL;
    ds.set_checked = NULL;
    ds.cycle_choice = NULL;
    ds.data = &p;
    table_set_datasource (p.list, ds);
    table_set_cell_color (p.list, palette_cell_color);
    table_set_quick_search_text (p.list, palette_search_text);
    table_set_quick_filter_fuzzy (p.list, FALSE, NULL);
    palette_column_widths (&p, cols);
    group_add_widget_autopos (GROUP (dlg), p.list, WPOS_KEEP_ALL, NULL);
    widget_select (WIDGET (p.filter));

    if (dlg_run (dlg) == B_ENTER)
    {
        const palette_row_t *r = palette_row (&p, p.chosen);

        if (r != NULL)
        {
            choice->command = r->command;
            choice->menu = r->menu;
            choice->directory = g_strdup (r->directory);
            p.history = list_append_unique (g_list_first (p.history), g_strdup (r->id));
            p.history_changed = TRUE;
            ok = TRUE;
        }
    }
    widget_destroy (WIDGET (dlg));

    g_list_free_full (g_list_first (p.history), g_free);
    g_free (p.filter_text);
    g_ptr_array_free (p.rows, TRUE);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */
