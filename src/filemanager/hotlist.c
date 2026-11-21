/*
   Hotlist -- for the M-Commander

   Copyright (C) 1994-2025
   Free Software Foundation, Inc.
   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

   Written by:
   Radek Doulik, 1994
   Janne Kukonlehto, 1995
   Andrej Borsenkow, 1996
   Norbert Warmuth, 1997
   Andrew Borodin <aborodin@vmail.ru>, 2012-2022
   Ilia Maslakov <il.smind@gmail.com>, 2026

   Janne did the original Hotlist code, Andrej made the groupable
   hotlist; the move hotlist and revamped the file format and made
   it stronger.

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

/** \file hotlist.c
 *  \brief Source: directory hotlist
 */

#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"

#include "lib/tty/tty.h"   // COLS
#include "lib/tty/key.h"   // KEY_M_CTRL
#include "lib/skin.h"      // colors
#include "lib/mcconfig.h"  // Load/save directories hotlist
#include "lib/fileloc.h"
#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "src/setup.h"  // For profile_bname
#include "src/history.h"

#include "command.h"  // cmdline

#include "hotlist.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define UX            3
#define UY            2

#define B_ADD_CURRENT B_USER
#define B_REMOVE      (B_USER + 1)
#define B_NEW_GROUP   (B_USER + 2)
#define B_NEW_ENTRY   (B_USER + 3)
#define B_ENTER_GROUP (B_USER + 4)
#define B_UP_GROUP    (B_USER + 5)
#define B_INSERT      (B_USER + 6)
#define B_APPEND      (B_USER + 7)
#define B_MOVE        (B_USER + 8)
#define B_EDIT        (B_USER + 9)
#define B_SORT        (B_USER + 10)
#define B_MOVE_HERE   (B_USER + 11)

#define TKN_GROUP     0
#define TKN_ENTRY     1
#define TKN_STRING    2
#define TKN_URL       3
#define TKN_ENDGROUP  4
#define TKN_COMMENT   5
#define TKN_EOL       125
#define TKN_EOF       126
#define TKN_UNKNOWN   127

#define SKIP_TO_EOL                                                                                \
    {                                                                                              \
        int _tkn;                                                                                  \
        while ((_tkn = hot_next_token ()) != TKN_EOF && _tkn != TKN_EOL)                           \
            ;                                                                                      \
    }

#define CHECK_TOKEN(_TKN_)                                                                         \
    tkn = hot_next_token ();                                                                       \
    if (tkn != _TKN_)                                                                              \
    {                                                                                              \
        hotlist_state.readonly = TRUE;                                                             \
        hotlist_state.file_error = TRUE;                                                           \
        while (tkn != TKN_EOL && tkn != TKN_EOF)                                                   \
            tkn = hot_next_token ();                                                               \
        break;                                                                                     \
    }

/*** file scope type declarations ****************************************************************/

enum HotListType
{
    HL_TYPE_GROUP,
    HL_TYPE_ENTRY,
    HL_TYPE_COMMENT,
    HL_TYPE_DOTDOT
};

static struct
{
    /*
     * these reflect run time state
     */

    gboolean loaded;      // hotlist is loaded
    gboolean readonly;    // hotlist readonly
    gboolean file_error;  // parse error while reading file
    gboolean running;     /* we are running dlg (and have to
                             update listbox */
    gboolean moving;      // we are in moving hotlist currently
    gboolean modified;    // hotlist was modified
    gboolean to_other;    // open the selected location in the other panel
} hotlist_state;

/* Directory hotlist */
struct hotlist
{
    enum HotListType type;
    char *directory;
    char *label;
    struct hotlist *head;
    struct hotlist *up;
    struct hotlist *next;
};

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static WPanel *our_panel;

static gboolean hotlist_has_dot_dot = TRUE;

static WDialog *hotlist_dlg, *movelist_dlg;
static struct hotlist *moving_item;
static WHLine *info_line;
static WListbox *l_hotlist, *l_movelist;
static WLabel *pname, *pkind;

static struct
{
    int ret_cmd, flags, y, x, len;
    const char *text;
    int type;
    widget_pos_flags_t pos_flags;
} hotlist_but[] = {
    // Move dialog only, the hotlist itself has no buttons.
    // y and x are computed by layout_buttons() in this order, Cancel goes right
    { B_MOVE_HERE, NORMAL_BUTTON, 0, 0, 0, N_ ("&Move"), LIST_MOVELIST,
      WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_CANCEL, NORMAL_BUTTON, 0, 0, 0, N_ ("&Cancel"), LIST_MOVELIST,
      WPOS_KEEP_RIGHT | WPOS_KEEP_BOTTOM },
};

static const size_t hotlist_but_num = G_N_ELEMENTS (hotlist_but);

static struct hotlist *hotlist = NULL;

static struct hotlist *current_group;

static GString *tkn_buf = NULL;

static char *hotlist_file_name;
static FILE *hotlist_file;
static time_t hotlist_file_mtime;

static int list_level = 0;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void init_movelist (struct hotlist *item);
static void done_movelist (void);
static void add_new_group_cmd (void);
static void add_new_entry_cmd (WPanel *panel);
static void remove_from_hotlist (struct hotlist *entry);
static void load_hotlist (void);
static void add_dotdot_to_list (void);
static void edit_entry_cmd (void);
static void sort_group_cmd (void);
static void shift_entry_cmd (int delta);

/* --------------------------------------------------------------------------------------------- */
static inline gboolean
entry_is_listed (const struct hotlist *entry)
{
    return entry->type == HL_TYPE_GROUP || entry->type == HL_TYPE_ENTRY;
}

/* --------------------------------------------------------------------------------------------- */

/* What the selected item is: a group, a directory, a VFS path or a plugin address. */
static char *
entry_kind (const struct hotlist *hlp)
{
    const mc_panel_plugin_t *plugin;
    vfs_path_t *vpath;
    const vfs_path_element_t *element;
    char *kind = NULL;

    if (hlp == NULL)
        return g_strdup ("");
    if (hlp->type == HL_TYPE_DOTDOT)
        return g_strdup (_ ("Parent group"));
    if (hlp->type == HL_TYPE_GROUP)
        return g_strdup (_ ("Group"));

    plugin = panel_plugin_find_by_path (hlp->directory);
    if (plugin != NULL)
        return g_strdup_printf (_ ("Plugin: %s"), plugin->name);

    vpath = vfs_path_from_str (hlp->directory);
    element = vfs_path_get_by_index (vpath, -1);
    if (element != NULL && element->class != NULL && (element->class->flags & VFSF_LOCAL) == 0)
        kind = g_strdup_printf (_ ("VFS: %s"),
                                element->vfs_prefix != NULL ? element->vfs_prefix
                                                            : element->class->name);
    vfs_path_free (vpath, TRUE);

    return kind != NULL ? kind : g_strdup (_ ("Local directory"));
}

/* --------------------------------------------------------------------------------------------- */

/* The current group as "a/b", "" for the top level. */
static char *
group_path (void)
{
    GString *path;
    const struct hotlist *grp;

    path = g_string_new ("");
    for (grp = current_group; grp != hotlist && grp != NULL; grp = grp->up)
    {
        if (path->len != 0)
            g_string_prepend (path, PATH_SEP_STR);
        g_string_prepend (path, grp->label);
    }

    g_string_prepend_c (path, PATH_SEP);

    return g_string_free (path, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* "Hotlist" for the top level, "Hotlist: a/b" inside groups; the Move dialog
   says where the item goes. */
static void
update_title (void)
{
    WDialog *dlg = hotlist_state.moving ? movelist_dlg : hotlist_dlg;
    GString *title;
    char *path;

    path = group_path ();
    title = g_string_new ("");
    if (hotlist_state.moving)
        g_string_printf (title, _ ("Move \"%s\" to %s"), moving_item->label, path);
    else if (current_group == hotlist)
        g_string_assign (title, _ ("Hotlist"));
    else
        g_string_printf (title, "%s: %s", _ ("Hotlist"), path);
    g_free (path);

    // the same form frame_set_title() stores: stripped, a space on each side
    g_strstrip (title->str);
    g_string_set_size (title, strlen (title->str));
    g_string_assign (title, str_trunc (title->str, WIDGET (dlg)->rect.cols - 6));
    g_string_prepend_c (title, ' ');
    g_string_append_c (title, ' ');

    // frame_set_title() repaints the frame over the widgets, so do it only on change
    if (g_strcmp0 (FRAME (dlg->bg)->title, title->str) != 0)
    {
        frame_set_title (FRAME (dlg->bg), title->str);
        widget_draw (WIDGET (dlg));
    }
    g_string_free (title, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
update_path_name (void)
{
    const char *text = "";
    struct hotlist *hlp = NULL;
    WListbox *list = hotlist_state.moving ? l_movelist : l_hotlist;
    Widget *w = WIDGET (list);

    if (!listbox_is_empty (list))
    {
        char *ctext = NULL;

        listbox_get_current (list, &ctext, (void **) &hlp);
        if (hlp == NULL)
            text = ctext;
        else if (hlp->type == HL_TYPE_ENTRY || hlp->type == HL_TYPE_DOTDOT)
            text = hlp->directory;
        else if (hlp->type == HL_TYPE_GROUP)
            text = hlp->label;
    }

    if (hotlist_state.moving)
        update_title ();
    else
    {
        char *kind;

        label_set_text (pname, str_trunc (text, w->rect.cols - 2));
        kind = entry_kind (hlp);
        label_set_text (pkind, str_trunc (kind, w->rect.cols - 2));
        g_free (kind);
        update_title ();
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Put @item into the listbox: a group is marked and drawn in the hot color. */
static void
add_list_item (WListbox *list, listbox_append_t pos, struct hotlist *item)
{
    int idx;

    if (item->type == HL_TYPE_GROUP)
        listbox_add_item_take (list, pos, 0, g_strdup_printf ("%s (%s)", item->label, _ ("group")),
                               item, FALSE);
    else
        listbox_add_item (list, pos, 0, item->label, item, FALSE);

    if (pos == LISTBOX_APPEND_AT_END)
        idx = listbox_get_length (list) - 1;
    else if (pos == LISTBOX_APPEND_AFTER)
        idx = list->current + 1;
    else
        idx = list->current;
    listbox_set_emphasis (list, idx, item->type == HL_TYPE_GROUP);
}

/* --------------------------------------------------------------------------------------------- */

static void
fill_listbox (WListbox *list)
{
    struct hotlist *current;

    for (current = current_group->head; current != NULL; current = current->next)
    {
        // the Move dialog shows where an item can go: groups, not the item itself
        if (hotlist_state.moving && list == l_movelist
            && (current->type == HL_TYPE_ENTRY || current == moving_item))
            continue;
        if (entry_is_listed (current) || current->type == HL_TYPE_DOTDOT)
            add_list_item (list, LISTBOX_APPEND_AT_END, current);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
unlink_entry (struct hotlist *entry)
{
    struct hotlist *current = current_group->head;

    if (current == entry)
        current_group->head = entry->next;
    else
    {
        while (current != NULL && current->next != entry)
            current = current->next;
        if (current != NULL)
            current->next = entry->next;
    }
    entry->next = entry->up = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
link_entry_last (struct hotlist *entry, struct hotlist *group)
{
    entry->up = group;
    entry->next = NULL;

    if (group->head == NULL)
        group->head = entry;
    else
    {
        struct hotlist *p = group->head;

        while (p->next != NULL)
            p = p->next;
        p->next = entry;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
link_entry_after (struct hotlist *entry, struct hotlist *after)
{
    entry->up = current_group;
    entry->next = after->next;
    after->next = entry;
}

/* --------------------------------------------------------------------------------------------- */

static void
link_entry_before (struct hotlist *entry, struct hotlist *before)
{
    entry->up = current_group;
    entry->next = before;

    if (current_group->head == before)
        current_group->head = entry;
    else
    {
        struct hotlist *p = current_group->head;

        while (p->next != before)
            p = p->next;
        p->next = entry;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Rebuild @list from current_group and select @item, or the first one. */
static void
refill_listbox (WListbox *list, const struct hotlist *item)
{
    int pos = 0;

    listbox_remove_list (list);
    fill_listbox (list);
    if (item != NULL)
        pos = MAX (0, listbox_search_data (list, item));
    listbox_set_current (list, pos);
}

/* --------------------------------------------------------------------------------------------- */

/* Show @entry in the dialog: enter its group and select it. */
static void
goto_entry (struct hotlist *entry)
{
    current_group = entry->up;
    refill_listbox (l_hotlist, entry);
}

/* --------------------------------------------------------------------------------------------- */

/* Where the panel is: a directory or a plugin address. NULL, after a message,
   for a plugin that cannot say. Caller frees. */
static char *
panel_location (WPanel *panel)
{
    char *location;

    location = panel_plugin_location (panel);
    if (location != NULL)
        return location;

    if (panel->is_plugin_panel)
    {
        const char *name;

        name = panel->plugin != NULL && panel->plugin->display_name != NULL
            ? panel->plugin->display_name
            : _ ("plugin");
        message (D_ERROR, _ ("Hotlist"), _ ("%s: this view has no address to save"), name);
        return NULL;
    }

    return vfs_path_to_str_flags (panel->cwd_vpath, 0, VPF_STRIP_PASSWORD);
}

/* --------------------------------------------------------------------------------------------- */

/* One form of a location for comparison: plugin addresses as they are, paths
   canonical, without password and without a trailing slash. Caller frees. */
static char *
normalize_location (const char *directory)
{
    vfs_path_t *vpath;
    char *s;
    size_t len;

    if (panel_plugin_find_by_path (directory) != NULL)
        return g_strdup (directory);

    vpath = vfs_path_from_str (directory);
    s = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_PASSWORD);
    vfs_path_free (vpath, TRUE);

    len = strlen (s);
    if (len > 1 && IS_PATH_SEP (s[len - 1]))
        s[len - 1] = '\0';

    return s;
}

/* --------------------------------------------------------------------------------------------- */

/* @directory must come from normalize_location(); @except is not a match */
static struct hotlist *
find_entry_by_directory (struct hotlist *grp, const char *directory, const struct hotlist *except)
{
    struct hotlist *p;

    for (p = grp->head; p != NULL; p = p->next)
    {
        if (p->type == HL_TYPE_GROUP)
        {
            struct hotlist *found;

            found = find_entry_by_directory (p, directory, except);
            if (found != NULL)
                return found;
        }
        else if (p->type == HL_TYPE_ENTRY && p != except)
        {
            char *d;
            gboolean same;

            d = normalize_location (p->directory);
            same = strcmp (d, directory) == 0;
            g_free (d);
            if (same)
                return p;
        }
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* TRUE when @directory is already in the hotlist, @except aside. The user is told where. */
static gboolean
report_duplicate (const char *directory, const struct hotlist *except)
{
    struct hotlist *entry;
    char *norm;

    norm = normalize_location (directory);
    entry = find_entry_by_directory (hotlist, norm, except);
    g_free (norm);
    if (entry == NULL)
        return FALSE;

    // str_trunc() returns a static buffer, so truncate one argument only
    message (D_NORMAL, _ ("Hotlist"), _ ("\"%s\" is already in the hotlist as \"%s\""),
             str_trunc (directory, 50), entry->label);

    if (hotlist_state.running)
        goto_entry (entry);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static int
hotlist_run_cmd (int action)
{
    switch (action)
    {
    case B_MOVE:
    {
        struct hotlist *saved = current_group;
        struct hotlist *item = NULL;
        struct hotlist *moveto_group;
        int ret;

        listbox_get_current (l_hotlist, NULL, (void **) &item);
        if (item == NULL || !entry_is_listed (item))
            return 0;

        init_movelist (item);
        if (listbox_is_empty (l_movelist))
        {
            done_movelist ();
            message (D_NORMAL, _ ("Hotlist"), _ ("There is no group to move \"%s\" to"),
                     item->label);
            return 0;
        }

        ret = dlg_run (movelist_dlg);
        moveto_group = current_group;
        done_movelist ();
        current_group = saved;
        if (ret != B_MOVE_HERE)
            return 0;

        // the same group too: the item goes to its end
        unlink_entry (item);
        link_entry_last (item, moveto_group);
        if (moveto_group == current_group)
            refill_listbox (l_hotlist, item);
        else
            listbox_remove_current (l_hotlist);
        repaint_screen ();
        hotlist_state.modified = TRUE;
        return 0;
    }
    case B_REMOVE:
    {
        struct hotlist *entry = NULL;

        listbox_get_current (l_hotlist, NULL, (void **) &entry);
        remove_from_hotlist (entry);
    }
        return 0;

    case B_NEW_GROUP:
        add_new_group_cmd ();
        return 0;

    case B_ADD_CURRENT:
        add2hotlist_cmd (our_panel);
        return 0;

    case B_NEW_ENTRY:
        add_new_entry_cmd (our_panel);
        return 0;

    case B_EDIT:
        edit_entry_cmd ();
        return 0;

    case B_SORT:
        sort_group_cmd ();
        return 0;

    case B_ENTER:
    case B_ENTER_GROUP:
    {
        WListbox *list;
        void *data;
        struct hotlist *hlp;

        list = hotlist_state.moving ? l_movelist : l_hotlist;
        listbox_get_current (list, NULL, &data);

        if (data == NULL)
            return 1;

        hlp = (struct hotlist *) data;

        if (hlp->type == HL_TYPE_ENTRY)
            return (action == B_ENTER ? 1 : 0);
        if (hlp->type != HL_TYPE_DOTDOT)
        {
            listbox_remove_list (list);
            current_group = hlp;
            fill_listbox (list);
            return 0;
        }
    }
        MC_FALLTHROUGH;  // if list empty - just go up

    case B_UP_GROUP:
    {
        WListbox *list = hotlist_state.moving ? l_movelist : l_hotlist;
        struct hotlist *from = current_group;

        current_group = current_group->up;
        refill_listbox (list, from);  // stay on the group we came from
        return 0;
    }

    default:
        return 1;
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
hotlist_button_callback (WButton *button, int action)
{
    int ret;

    (void) button;
    ret = hotlist_run_cmd (action);
    update_path_name ();
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static inline cb_ret_t
hotlist_handle_key (WDialog *h, int key)
{
    switch (key)
    {
    case KEY_M_CTRL | '\n':
        goto l1;

    case '\n':
    case KEY_ENTER:
        if (hotlist_button_callback (NULL, B_ENTER) != 0)
        {
            h->ret_value = B_ENTER;
            dlg_close (h);
        }
        return MSG_HANDLED;

    case KEY_RIGHT:
        // enter to the group
        return hotlist_button_callback (NULL, B_ENTER_GROUP) == 0 ? MSG_HANDLED : MSG_NOT_HANDLED;

    case KEY_LEFT:
        // leave the group
        return hotlist_button_callback (NULL, B_UP_GROUP) == 0 ? MSG_HANDLED : MSG_NOT_HANDLED;

    case KEY_DC:
        if (hotlist_state.moving)
            return MSG_NOT_HANDLED;
        hotlist_button_callback (NULL, B_REMOVE);
        return MSG_HANDLED;

    case KEY_IC:
    case ALT ('a'):
        if (hotlist_state.moving)
            return MSG_NOT_HANDLED;
        hotlist_button_callback (NULL, B_ADD_CURRENT);
        return MSG_HANDLED;

    case ALT ('r'):
        if (hotlist_state.moving)
            return MSG_NOT_HANDLED;
        hotlist_button_callback (NULL, B_REMOVE);
        return MSG_HANDLED;

    case ALT ('i'):
        if (hotlist_state.moving)
            return MSG_NOT_HANDLED;
        hotlist_button_callback (NULL, B_EDIT);
        return MSG_HANDLED;

    case KEY_F (7):
    case ALT ('g'):
        if (hotlist_state.moving)
            return MSG_NOT_HANDLED;
        hotlist_button_callback (NULL, B_NEW_GROUP);
        return MSG_HANDLED;

    case KEY_F (14):  // Shift-F4
    case ALT ('e'):
        if (hotlist_state.moving)
            return MSG_NOT_HANDLED;
        hotlist_button_callback (NULL, B_NEW_ENTRY);
        return MSG_HANDLED;

    case KEY_F (6):
    case ALT ('m'):
        if (hotlist_state.moving)
        {
            // in the Move dialog F6 is the Move button
            h->ret_value = B_MOVE_HERE;
            dlg_close (h);
        }
        else
            hotlist_button_callback (NULL, B_MOVE);
        return MSG_HANDLED;

    case KEY_F (9):
    case ALT ('t'):
        if (hotlist_state.moving)
            return MSG_NOT_HANDLED;
        hotlist_button_callback (NULL, B_SORT);
        return MSG_HANDLED;

    case KEY_M_CTRL | KEY_UP:
        if (hotlist_state.moving)
            return MSG_NOT_HANDLED;
        shift_entry_cmd (-1);
        return MSG_HANDLED;

    case KEY_M_CTRL | KEY_DOWN:
        if (hotlist_state.moving)
            return MSG_NOT_HANDLED;
        shift_entry_cmd (1);
        return MSG_HANDLED;

    case ALT ('o'):
        // open in the other panel
        if (!hotlist_state.moving)
        {
            struct hotlist *hlp = NULL;

            listbox_get_current (l_hotlist, NULL, (void **) &hlp);
            if (hlp != NULL && hlp->type == HL_TYPE_ENTRY)
            {
                hotlist_state.to_other = TRUE;
                h->ret_value = B_ENTER;
                dlg_close (h);
            }
        }
        return MSG_HANDLED;

    l1:
    case ALT ('\n'):
    case ALT ('\r'):
        if (!hotlist_state.moving)
        {
            void *ldata = NULL;

            listbox_get_current (l_hotlist, NULL, &ldata);

            if (ldata != NULL)
            {
                struct hotlist *hlp = (struct hotlist *) ldata;

                if (hlp->type == HL_TYPE_ENTRY)
                {
                    char *tmp;

                    tmp = g_strconcat ("cd ", hlp->directory, (char *) NULL);
                    input_insert (cmdline, tmp, FALSE);
                    g_free (tmp);
                    h->ret_value = B_CANCEL;
                    dlg_close (h);
                }
            }
        }
        return MSG_HANDLED;  // ignore key

    default:
        return MSG_NOT_HANDLED;
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
hotlist_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_NOTIFY:  // MSG_NOTIFY is fired by the listbox to tell us the item has changed.
        if (parm == CK_Enter)
            return hotlist_handle_key (h, '\n');
        if (parm == CK_Edit && !hotlist_state.moving)
            hotlist_button_callback (NULL, B_EDIT);
        MC_FALLTHROUGH;

    case MSG_INIT:
        update_path_name ();
        return MSG_HANDLED;

    case MSG_UNHANDLED_KEY:
        return hotlist_handle_key (h, parm);

    case MSG_POST_KEY:
        /*
         * The code here has two purposes:
         *
         * (1) Always stay on the hotlist.
         *
         * Activating a button using its hotkey (and even pressing ENTER, as
         * there's a "default button") moves the focus to the button. But we
         * want to stay on the hotlist, to be able to use the usual keys (up,
         * down, etc.). So we do `widget_select (lst)`.
         *
         * (2) Refresh the hotlist.
         *
         * We may have run a command that changed the contents of the list.
         * We therefore need to refresh it. So we do `widget_draw (lst)`.
         */
        {
            Widget *lst;

            lst = WIDGET (h == hotlist_dlg ? l_hotlist : l_movelist);

            /* widget_select() already redraws the widget, but since it's a
             * no-op if the widget is already selected ("focused"), we have
             * to call widget_draw() separately. */
            if (!widget_get_state (lst, WST_FOCUSED))
                widget_select (lst);
            else
                widget_draw (lst);
        }
        return MSG_HANDLED;

    case MSG_RESIZE:
    {
        WRect r = w->rect;

        r.lines = LINES - (h == hotlist_dlg ? 2 : 6);
        r.cols = COLS - 6;

        return dlg_default_callback (w, NULL, MSG_RESIZE, 0, &r);
    }

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static lcback_ret_t
hotlist_listbox_callback (WListbox *list)
{
    WDialog *dlg = DIALOG (WIDGET (list)->owner);

    if (!listbox_is_empty (list))
    {
        void *data = NULL;

        listbox_get_current (list, NULL, &data);

        if (data != NULL)
        {
            struct hotlist *hlp = (struct hotlist *) data;

            if (hlp->type == HL_TYPE_ENTRY)
            {
                dlg->ret_value = B_ENTER;
                dlg_close (dlg);
                return LISTBOX_DONE;
            }
            else
            {
                hotlist_button_callback (NULL, B_ENTER);
                send_message (dlg, NULL, MSG_POST_KEY, '\n', NULL);
                return LISTBOX_CONT;
            }
        }
        else
        {
            dlg->ret_value = B_ENTER;
            dlg_close (dlg);
            return LISTBOX_DONE;
        }
    }

    hotlist_button_callback (NULL, B_UP_GROUP);
    send_message (dlg, NULL, MSG_POST_KEY, 'u', NULL);
    return LISTBOX_CONT;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Expands all button names (once) and recalculates button positions.
 * returns number of columns in the dialog box, which is 10 chars longer
 * than buttonbar.
 *
 * If common width of the window (i.e. in xterm) is less than returned
 * width - sorry :)  (anyway this did not handled in previous version too)
 */

/* Lay the buttons of @list_type out in rows of @width columns, Cancel at the
   right end of the last row. Returns the number of rows. */
static int
layout_buttons (int list_type, int width)
{
    size_t i;
    int row = 0;
    int cur_x = 0;
    size_t cancel = hotlist_but_num;

    static gboolean i18n_flag = FALSE;

    if (!i18n_flag)
    {
        for (i = 0; i < hotlist_but_num; i++)
        {
#ifdef ENABLE_NLS
            hotlist_but[i].text = _ (hotlist_but[i].text);
#endif
            hotlist_but[i].len = str_term_width1 (hotlist_but[i].text) + 3;
            if (hotlist_but[i].flags == DEFPUSH_BUTTON)
                hotlist_but[i].len += 2;
        }

        i18n_flag = TRUE;
    }

    for (i = 0; i < hotlist_but_num; i++)
    {
        if ((hotlist_but[i].type & list_type) == 0)
            continue;

        if (hotlist_but[i].ret_cmd == B_CANCEL)
        {
            cancel = i;
            continue;
        }

        if (cur_x != 0 && cur_x + hotlist_but[i].len > width)
        {
            row++;
            cur_x = 0;
        }

        hotlist_but[i].y = row;
        hotlist_but[i].x = cur_x;
        cur_x += hotlist_but[i].len + 1;
    }

    if (cancel < hotlist_but_num)
    {
        if (cur_x + 1 + hotlist_but[cancel].len > width)
            row++;
        hotlist_but[cancel].y = row;
        hotlist_but[cancel].x = width - hotlist_but[cancel].len;
    }

    return row + 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
add_buttons (WGroup *g, int list_type, int y)
{
    size_t i;

    for (i = 0; i < hotlist_but_num; i++)
        if ((hotlist_but[i].type & list_type) != 0)
            group_add_widget_autopos (g,
                                      button_new (y + hotlist_but[i].y, UX + hotlist_but[i].x,
                                                  hotlist_but[i].ret_cmd, hotlist_but[i].flags,
                                                  hotlist_but[i].text, hotlist_button_callback),
                                      hotlist_but[i].pos_flags, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
init_hotlist (void)
{
    int lines, cols;
    int y;
    WGroup *g;

    do_refresh ();

    lines = LINES - 2;
    cols = COLS - 6;

    hotlist_dlg = dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors,
                              hotlist_callback, NULL, "[Hotlist]", _ ("Hotlist"));
    g = GROUP (hotlist_dlg);

    /* The frame takes one row and one column on each side plus the border.
       Rows inside: the list, an "Info" line, the location of the selected
       item and what it is. Everything is on keys, there are no buttons. */
    y = UY;
    l_hotlist = listbox_new (y, UY, lines - 7, cols - 2 * UY, FALSE, hotlist_listbox_callback);
    l_hotlist->quick_search = TRUE;
    fill_listbox (l_hotlist);
    group_add_widget_autopos (g, l_hotlist, WPOS_KEEP_ALL, NULL);
    y += WIDGET (l_hotlist)->rect.lines;

    info_line = hline_new (y++, -1, -1);
    info_line->text_align = J_LEFT;
    hline_set_textv (info_line, " %s ", _ ("Info"));
    group_add_widget_autopos (g, info_line, WPOS_KEEP_BOTTOM | WPOS_KEEP_HORZ, NULL);

    pname = label_new (y++, UX, NULL);
    group_add_widget_autopos (g, pname, WPOS_KEEP_BOTTOM | WPOS_KEEP_LEFT, NULL);

    pkind = label_new (y, UX, NULL);
    group_add_widget_autopos (g, pkind, WPOS_KEEP_BOTTOM | WPOS_KEEP_LEFT, NULL);

    widget_select (WIDGET (l_hotlist));
}

/* --------------------------------------------------------------------------------------------- */

static void
init_movelist (struct hotlist *item)
{
    int lines, cols;
    int rows;
    int y;
    WGroup *g;

    moving_item = item;
    hotlist_state.moving = TRUE;

    do_refresh ();

    lines = LINES - 6;
    cols = COLS - 10;
    rows = layout_buttons (LIST_MOVELIST, cols - 2 * UX);

    movelist_dlg = dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors,
                               hotlist_callback, NULL, "[Hotlist]", "");
    g = GROUP (movelist_dlg);

    y = UY;
    l_movelist =
        listbox_new (y, UY, lines - 5 - rows, cols - 2 * UY, FALSE, hotlist_listbox_callback);
    l_movelist->quick_search = TRUE;
    fill_listbox (l_movelist);
    group_add_widget_autopos (g, l_movelist, WPOS_KEEP_ALL, NULL);
    y += WIDGET (l_movelist)->rect.lines;

    group_add_widget_autopos (g, hline_new (y++, -1, -1), WPOS_KEEP_BOTTOM | WPOS_KEEP_HORZ, NULL);

    add_buttons (g, LIST_MOVELIST, y);

    widget_select (WIDGET (l_movelist));
}

/* --------------------------------------------------------------------------------------------- */

static void
done_movelist (void)
{
    widget_destroy (WIDGET (movelist_dlg));
    movelist_dlg = NULL;
    l_movelist = NULL;
    moving_item = NULL;
    hotlist_state.moving = FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Destroy the list dialog.
 * Don't confuse with done_hotlist() for the list in memory.
 */

static void
hotlist_done (void)
{
    widget_destroy (WIDGET (hotlist_dlg));
    l_hotlist = NULL;
#if 0
    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
#endif
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static inline char *
find_group_section (struct hotlist *grp)
{
    return g_strconcat (grp->directory, ".Group", (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static struct hotlist *
add2hotlist (char *label, char *directory, enum HotListType type, listbox_append_t pos)
{
    struct hotlist *new;
    struct hotlist *current = NULL;

    /*
     * Hotlist is neither loaded nor loading.
     * Must be called by "Ctrl-x a" before using hotlist.
     */
    if (current_group == NULL)
        load_hotlist ();

    listbox_get_current (l_hotlist, NULL, (void **) &current);

    if (current == NULL)
        pos = LISTBOX_APPEND_AT_END;
    // Make sure '..' stays at the top of the list.
    else if (current->type == HL_TYPE_DOTDOT)
        pos = LISTBOX_APPEND_AFTER;

    new = g_new0 (struct hotlist, 1);

    new->type = type;
    new->label = label;
    new->directory = directory;
    new->up = current_group;

    if (type == HL_TYPE_GROUP)
    {
        current_group = new;
        add_dotdot_to_list ();
        current_group = new->up;
    }

    if (current_group->head == NULL)
    {
        // first element in group
        current_group->head = new;
    }
    else if (pos == LISTBOX_APPEND_AFTER)
    {
        new->next = current->next;
        current->next = new;
    }
    else if (pos == LISTBOX_APPEND_BEFORE && current == current_group->head)
    {
        // should be inserted before first item
        new->next = current;
        current_group->head = new;
    }
    else if (pos == LISTBOX_APPEND_BEFORE)
    {
        struct hotlist *p = current_group->head;

        while (p->next != current)
            p = p->next;

        new->next = current;
        p->next = new;
    }
    else
        link_entry_last (new, current_group);

    if (hotlist_state.running && type != HL_TYPE_COMMENT && type != HL_TYPE_DOTDOT)
    {
        add_list_item (l_hotlist, pos, new);
        listbox_set_current (l_hotlist, l_hotlist->current);
    }

    return new;
}

/* --------------------------------------------------------------------------------------------- */

/* @edit: the dialog changes an existing entry and has OK/Cancel buttons only */
static int
add_new_entry_input (const char *header, const char *text1, const char *text2, const char *help,
                     const char *def_label, const char *def_path, char **r1, char **r2,
                     gboolean edit)
{
    quick_widget_t new_widgets[] = {
        // clang-format off
        QUICK_LABELED_INPUT (text1, input_label_above, def_label, "input-lbl", r1, NULL, FALSE,
                             FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (text2, input_label_above, def_path, "input-lbl", r2, NULL, FALSE,
                             FALSE, INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD),
        QUICK_START_BUTTONS (TRUE, TRUE),
            QUICK_BUTTON (_ ("&Append"), B_APPEND, NULL, NULL),
            QUICK_BUTTON (_ ("&Insert"), B_INSERT, NULL, NULL),
            QUICK_BUTTON (_ ("&Cancel"), B_CANCEL, NULL, NULL),
        QUICK_END,
        // clang-format on
    };

    quick_widget_t edit_widgets[] = {
        // clang-format off
        QUICK_LABELED_INPUT (text1, input_label_above, def_label, "input-lbl", r1, NULL, FALSE,
                             FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (text2, input_label_above, def_path, "input-lbl", r2, NULL, FALSE,
                             FALSE, INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
        // clang-format on
    };

    WRect r = { -1, -1, 0, 64 };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = header,
        .help = help,
        .widgets = edit ? edit_widgets : new_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    return quick_dialog (&qdlg);
}

/* --------------------------------------------------------------------------------------------- */

static void
add_new_entry_cmd (WPanel *panel)
{
    char *def_text;
    char *title = NULL;
    char *url = NULL;
    int ret;

    // Take current location as default value for input fields
    def_text = panel_plugin_location (panel);
    if (def_text == NULL)
        def_text = panel->is_plugin_panel
            ? g_strdup ("")
            : vfs_path_to_str_flags (panel->cwd_vpath, 0, VPF_STRIP_PASSWORD);
    ret = add_new_entry_input (_ ("New hotlist entry"), _ ("Label:"), _ ("Location:"), "[Hotlist]",
                               def_text, def_text, &title, &url, FALSE);
    g_free (def_text);

    if (ret == B_CANCEL || title == NULL || *title == '\0' || url == NULL || *url == '\0'
        || report_duplicate (url, NULL))
    {
        g_free (title);
        g_free (url);
        return;
    }

    if (ret == B_ENTER || ret == B_APPEND)
        add2hotlist (title, url, HL_TYPE_ENTRY, LISTBOX_APPEND_AFTER);
    else
        add2hotlist (title, url, HL_TYPE_ENTRY, LISTBOX_APPEND_BEFORE);

    hotlist_state.modified = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* @def_name is NULL for a new group; a name means the dialog renames a group */
static int
add_new_group_input (const char *header, const char *label, const char *def_name, char **result)
{
    quick_widget_t new_widgets[] = {
        // clang-format off
        QUICK_LABELED_INPUT (label, input_label_above, "", "input", result, NULL,
                             FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_START_BUTTONS (TRUE, TRUE),
            QUICK_BUTTON (_ ("&Append"), B_APPEND, NULL, NULL),
            QUICK_BUTTON (_ ("&Insert"), B_INSERT, NULL, NULL),
            QUICK_BUTTON (_ ("&Cancel"), B_CANCEL, NULL, NULL),
        QUICK_END,
        // clang-format on
    };

    quick_widget_t edit_widgets[] = {
        // clang-format off
        QUICK_LABELED_INPUT (label, input_label_above, def_name, "input", result, NULL,
                             FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
        // clang-format on
    };

    WRect r = { -1, -1, 0, 64 };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = header,
        .help = "[Hotlist]",
        .widgets = def_name != NULL ? edit_widgets : new_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    int ret;

    ret = quick_dialog (&qdlg);

    return (ret != B_CANCEL) ? ret : 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
add_new_group_cmd (void)
{
    char *label;
    int ret;

    ret = add_new_group_input (_ ("New hotlist group"), _ ("Name of new group:"), NULL, &label);
    if (ret == 0 || label == NULL || *label == '\0')
        return;

    if (ret == B_ENTER || ret == B_APPEND)
        add2hotlist (label, 0, HL_TYPE_GROUP, LISTBOX_APPEND_AFTER);
    else
        add2hotlist (label, 0, HL_TYPE_GROUP, LISTBOX_APPEND_BEFORE);

    hotlist_state.modified = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_entry_cmd (void)
{
    struct hotlist *entry = NULL;

    listbox_get_current (l_hotlist, NULL, (void **) &entry);
    if (entry == NULL || !entry_is_listed (entry))
        return;

    if (entry->type == HL_TYPE_GROUP)
    {
        char *label = NULL;

        if (add_new_group_input (_ ("Edit hotlist group"), _ ("Group name:"), entry->label, &label)
                == 0
            || label == NULL || *label == '\0')
        {
            g_free (label);
            return;
        }

        g_free (entry->label);
        entry->label = label;
    }
    else
    {
        char *label = NULL;
        char *url = NULL;
        int ret;

        ret = add_new_entry_input (_ ("Edit hotlist entry"), _ ("Label:"), _ ("Location:"),
                                   "[Hotlist]", entry->label, entry->directory, &label, &url, TRUE);
        if (ret == B_CANCEL || label == NULL || *label == '\0' || url == NULL || *url == '\0')
        {
            g_free (label);
            g_free (url);
            return;
        }

        if (report_duplicate (url, entry))
        {
            g_free (label);
            g_free (url);
            return;
        }

        g_free (entry->label);
        g_free (entry->directory);
        entry->label = label;
        entry->directory = url;
    }

    refill_listbox (l_hotlist, entry);
    hotlist_state.modified = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Move the selected item one listed item up (@delta < 0) or down. */
static void
shift_entry_cmd (int delta)
{
    struct hotlist *entry = NULL;
    struct hotlist *other = NULL;
    struct hotlist *p;

    listbox_get_current (l_hotlist, NULL, (void **) &entry);
    if (entry == NULL || !entry_is_listed (entry))
        return;

    if (delta < 0)
    {
        // the listed item before the entry
        for (p = current_group->head; p != entry; p = p->next)
            if (entry_is_listed (p))
                other = p;
    }
    else
    {
        // the listed item after the entry
        for (p = entry->next; p != NULL && other == NULL; p = p->next)
            if (entry_is_listed (p))
                other = p;
    }

    if (other == NULL || other->type == HL_TYPE_DOTDOT)
        return;

    unlink_entry (entry);
    if (delta < 0)
        link_entry_before (entry, other);
    else
        link_entry_after (entry, other);

    refill_listbox (l_hotlist, entry);
    hotlist_state.modified = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    struct hotlist *entry;
    char *key;
} sort_item_t;

static int
sort_rank (enum HotListType type)
{
    if (type == HL_TYPE_GROUP)
        return 0;
    if (type == HL_TYPE_ENTRY)
        return 1;
    return 2;
}

static int
sort_item_cmp (const void *a, const void *b)
{
    const sort_item_t *ia = (const sort_item_t *) a;
    const sort_item_t *ib = (const sort_item_t *) b;

    // groups first, comments last
    if (ia->entry->type != ib->entry->type)
        return sort_rank (ia->entry->type) - sort_rank (ib->entry->type);

    return str_key_collate (ia->key, ib->key, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* Sort the current group by label: groups first, then entries, comments last. */
static void
sort_group_cmd (void)
{
    struct hotlist *current = NULL;
    struct hotlist *p;
    struct hotlist *dotdot = NULL;
    GArray *items;
    guint i;

    items = g_array_new (FALSE, FALSE, sizeof (sort_item_t));

    for (p = current_group->head; p != NULL; p = p->next)
    {
        sort_item_t item;

        if (p->type == HL_TYPE_DOTDOT)
        {
            dotdot = p;
            continue;
        }

        item.entry = p;
        item.key = str_create_key (p->label, FALSE);
        g_array_append_val (items, item);
    }

    if (items->len < 2
        || query_dialog (_ ("Hotlist"), _ ("Sort this group by label?"), D_NORMAL, 2, _ ("&Yes"),
                         _ ("&No"))
            != 0)
    {
        for (i = 0; i < items->len; i++)
            str_release_key (g_array_index (items, sort_item_t, i).key, FALSE);
        g_array_free (items, TRUE);
        return;
    }

    listbox_get_current (l_hotlist, NULL, (void **) &current);

    g_array_sort (items, sort_item_cmp);

    // ".." goes first wherever it was
    current_group->head = dotdot;
    p = dotdot;
    for (i = 0; i < items->len; i++)
    {
        sort_item_t *item = &g_array_index (items, sort_item_t, i);

        if (p == NULL)
            current_group->head = item->entry;
        else
            p->next = item->entry;
        p = item->entry;
        str_release_key (item->key, FALSE);
    }
    p->next = NULL;

    g_array_free (items, TRUE);

    refill_listbox (l_hotlist, current);
    hotlist_state.modified = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
remove_group (struct hotlist *grp)
{
    struct hotlist *current = grp->head;

    while (current != NULL)
    {
        struct hotlist *next = current->next;

        if (current->type == HL_TYPE_GROUP)
            remove_group (current);

        g_free (current->label);
        g_free (current->directory);
        g_free (current);

        current = next;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
remove_from_hotlist (struct hotlist *entry)
{
    if (entry == NULL)
        return;

    if (entry->type == HL_TYPE_DOTDOT)
        return;

    if (confirm_directory_hotlist_delete)
    {
        char text[BUF_MEDIUM];
        int result;

        if (safe_delete)
            query_set_sel (1);

        g_snprintf (text, sizeof (text), _ ("Are you sure you want to remove entry \"%s\"?"),
                    str_trunc (entry->label, 30));
        result = query_dialog (Q_ ("DialogTitle|Delete"), text, D_ERROR | D_CENTER, 2, _ ("&Yes"),
                               _ ("&No"));
        if (result != 0)
            return;
    }

    if (entry->type == HL_TYPE_GROUP)
    {
        struct hotlist *head = entry->head;

        if (head != NULL && (head->type != HL_TYPE_DOTDOT || head->next != NULL))
        {
            char text[BUF_MEDIUM];
            int result;

            g_snprintf (text, sizeof (text), _ ("Group \"%s\" is not empty.\nRemove it?"),
                        str_trunc (entry->label, 30));
            result = query_dialog (Q_ ("DialogTitle|Delete"), text, D_ERROR | D_CENTER, 2,
                                   _ ("&Yes"), _ ("&No"));
            if (result != 0)
                return;
        }

        remove_group (entry);
    }

    unlink_entry (entry);

    g_free (entry->label);
    g_free (entry->directory);
    g_free (entry);
    // now remove list entry from screen
    listbox_remove_current (l_hotlist);
    hotlist_state.modified = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
load_group (struct hotlist *grp)
{
    gchar **profile_keys, **keys;
    char *group_section;
    struct hotlist *current = 0;

    group_section = find_group_section (grp);

    keys = mc_config_get_keys (mc_global.main_config, group_section, NULL);

    current_group = grp;

    for (profile_keys = keys; *profile_keys != NULL; profile_keys++)
        add2hotlist (mc_config_get_string (mc_global.main_config, group_section, *profile_keys, ""),
                     g_strdup (*profile_keys), HL_TYPE_GROUP, LISTBOX_APPEND_AT_END);

    g_strfreev (keys);

    keys = mc_config_get_keys (mc_global.main_config, grp->directory, NULL);

    for (profile_keys = keys; *profile_keys != NULL; profile_keys++)
        add2hotlist (mc_config_get_string (mc_global.main_config, group_section, *profile_keys, ""),
                     g_strdup (*profile_keys), HL_TYPE_ENTRY, LISTBOX_APPEND_AT_END);

    g_free (group_section);
    g_strfreev (keys);

    for (current = grp->head; current; current = current->next)
        load_group (current);
}

/* --------------------------------------------------------------------------------------------- */

static int
hot_skip_blanks (void)
{
    int c;

    while ((c = getc (hotlist_file)) != EOF && c != '\n' && g_ascii_isspace (c))
        ;
    return c;
}

/* --------------------------------------------------------------------------------------------- */

static int
hot_next_token (void)
{
    int c, ret = 0;
    size_t l;

    if (tkn_buf == NULL)
        tkn_buf = g_string_new ("");
    g_string_set_size (tkn_buf, 0);

again:
    c = hot_skip_blanks ();
    switch (c)
    {
    case EOF:
        ret = TKN_EOF;
        break;
    case '\n':
        ret = TKN_EOL;
        break;
    case '#':
        while ((c = getc (hotlist_file)) != EOF && c != '\n')
            g_string_append_c (tkn_buf, c);
        ret = TKN_COMMENT;
        break;
    case '"':
        while ((c = getc (hotlist_file)) != EOF && c != '"')
        {
            if (c == '\\')
            {
                c = getc (hotlist_file);
                if (c == EOF)
                {
                    g_string_free (tkn_buf, TRUE);
                    return TKN_EOF;
                }
            }
            g_string_append_c (tkn_buf, c == '\n' ? ' ' : c);
        }
        ret = (c == EOF) ? TKN_EOF : TKN_STRING;
        break;
    case '\\':
        c = getc (hotlist_file);
        if (c == EOF)
        {
            g_string_free (tkn_buf, TRUE);
            return TKN_EOF;
        }
        if (c == '\n')
            goto again;

        MC_FALLTHROUGH;  // it is taken as normal character

    default:
        do
        {
            g_string_append_c (tkn_buf, g_ascii_toupper (c));
        }
        while ((c = fgetc (hotlist_file)) != EOF && (g_ascii_isalnum (c) || !isascii (c)));
        if (c != EOF)
            ungetc (c, hotlist_file);
        l = tkn_buf->len;
        if (strncmp (tkn_buf->str, "GROUP", l) == 0)
            ret = TKN_GROUP;
        else if (strncmp (tkn_buf->str, "ENTRY", l) == 0)
            ret = TKN_ENTRY;
        else if (strncmp (tkn_buf->str, "ENDGROUP", l) == 0)
            ret = TKN_ENDGROUP;
        else if (strncmp (tkn_buf->str, "URL", l) == 0)
            ret = TKN_URL;
        else
            ret = TKN_UNKNOWN;
        break;
    }
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static void
hot_load_group (struct hotlist *grp)
{
    int tkn;
    struct hotlist *new_grp;
    char *label, *url;

    current_group = grp;

    while ((tkn = hot_next_token ()) != TKN_ENDGROUP)
        switch (tkn)
        {
        case TKN_GROUP:
            CHECK_TOKEN (TKN_STRING);
            new_grp = add2hotlist (g_strndup (tkn_buf->str, tkn_buf->len), 0, HL_TYPE_GROUP,
                                   LISTBOX_APPEND_AT_END);
            SKIP_TO_EOL;
            hot_load_group (new_grp);
            current_group = grp;
            break;
        case TKN_ENTRY:
        {
            CHECK_TOKEN (TKN_STRING);
            label = g_strndup (tkn_buf->str, tkn_buf->len);
            CHECK_TOKEN (TKN_URL);
            CHECK_TOKEN (TKN_STRING);
            url = tilde_expand (tkn_buf->str);
            add2hotlist (label, url, HL_TYPE_ENTRY, LISTBOX_APPEND_AT_END);
            SKIP_TO_EOL;
        }
        break;
        case TKN_COMMENT:
            label = g_strndup (tkn_buf->str, tkn_buf->len);
            add2hotlist (label, 0, HL_TYPE_COMMENT, LISTBOX_APPEND_AT_END);
            break;
        case TKN_EOF:
            hotlist_state.readonly = TRUE;
            hotlist_state.file_error = TRUE;
            return;
        case TKN_EOL:
            // skip empty lines
            break;
        default:
            hotlist_state.readonly = TRUE;
            hotlist_state.file_error = TRUE;
            SKIP_TO_EOL;
            break;
        }
    SKIP_TO_EOL;
}

/* --------------------------------------------------------------------------------------------- */

static void
hot_load_file (struct hotlist *grp)
{
    int tkn;
    struct hotlist *new_grp;
    char *label, *url;

    current_group = grp;

    while ((tkn = hot_next_token ()) != TKN_EOF)
        switch (tkn)
        {
        case TKN_GROUP:
            CHECK_TOKEN (TKN_STRING);
            new_grp = add2hotlist (g_strndup (tkn_buf->str, tkn_buf->len), 0, HL_TYPE_GROUP,
                                   LISTBOX_APPEND_AT_END);
            SKIP_TO_EOL;
            hot_load_group (new_grp);
            current_group = grp;
            break;
        case TKN_ENTRY:
        {
            CHECK_TOKEN (TKN_STRING);
            label = g_strndup (tkn_buf->str, tkn_buf->len);
            CHECK_TOKEN (TKN_URL);
            CHECK_TOKEN (TKN_STRING);
            url = tilde_expand (tkn_buf->str);
            add2hotlist (label, url, HL_TYPE_ENTRY, LISTBOX_APPEND_AT_END);
            SKIP_TO_EOL;
        }
        break;
        case TKN_COMMENT:
            label = g_strndup (tkn_buf->str, tkn_buf->len);
            add2hotlist (label, 0, HL_TYPE_COMMENT, LISTBOX_APPEND_AT_END);
            break;
        case TKN_EOL:
            // skip empty lines
            break;
        default:
            hotlist_state.readonly = TRUE;
            hotlist_state.file_error = TRUE;
            SKIP_TO_EOL;
            break;
        }
}

/* --------------------------------------------------------------------------------------------- */

static void
clean_up_hotlist_groups (const char *section)
{
    char *grp_section;

    grp_section = g_strconcat (section, ".Group", (char *) NULL);
    if (mc_config_has_group (mc_global.main_config, section))
        mc_config_del_group (mc_global.main_config, section);

    if (mc_config_has_group (mc_global.main_config, grp_section))
    {
        char **profile_keys, **keys;

        keys = mc_config_get_keys (mc_global.main_config, grp_section, NULL);

        for (profile_keys = keys; *profile_keys != NULL; profile_keys++)
            clean_up_hotlist_groups (*profile_keys);

        g_strfreev (keys);
        mc_config_del_group (mc_global.main_config, grp_section);
    }
    g_free (grp_section);
}

/* --------------------------------------------------------------------------------------------- */

static void
load_hotlist (void)
{
    gboolean remove_old_list = FALSE;
    struct stat stat_buf;

    if (hotlist_state.loaded)
    {
        stat (hotlist_file_name, &stat_buf);
        if (hotlist_file_mtime < stat_buf.st_mtime)
            done_hotlist ();
        else
            return;
    }

    if (hotlist_file_name == NULL)
        hotlist_file_name = mc_config_get_full_path (MC_HOTLIST_FILE);

    hotlist = g_new0 (struct hotlist, 1);
    hotlist->type = HL_TYPE_GROUP;
    hotlist->label = g_strdup (_ ("Top level group"));
    hotlist->up = hotlist;
    /*
     * compatibility :-(
     */
    hotlist->directory = g_strdup ("Hotlist");

    hotlist_file = fopen (hotlist_file_name, "r");
    if (hotlist_file == NULL)
    {
        int result;

        load_group (hotlist);
        hotlist_state.loaded = TRUE;
        /*
         * just to be sure we got copy
         */
        hotlist_state.modified = TRUE;
        result = save_hotlist ();
        hotlist_state.modified = FALSE;
        if (result != 0)
            remove_old_list = TRUE;
        else
            message (
                D_ERROR, _ ("Hotlist Load"),
                _ ("MC was unable to write %s file,\nyour old hotlist entries were not deleted"),
                MC_USERCONF_DIR PATH_SEP_STR MC_HOTLIST_FILE);
    }
    else
    {
        hot_load_file (hotlist);
        fclose (hotlist_file);
        hotlist_state.loaded = TRUE;
    }

    if (remove_old_list)
    {
        GError *mcerror = NULL;

        clean_up_hotlist_groups ("Hotlist");
        if (!mc_config_save_file (mc_global.main_config, &mcerror))
            setup_save_config_show_error (mc_global.main_config->ini_path, &mcerror);

        mc_error_message (&mcerror, NULL);
    }

    stat (hotlist_file_name, &stat_buf);
    hotlist_file_mtime = stat_buf.st_mtime;
    current_group = hotlist;
}

/* --------------------------------------------------------------------------------------------- */

static void
hot_save_group (struct hotlist *grp)
{
    struct hotlist *current;
    int i;
    char *s;

#define INDENT(n)                                                                                  \
    do                                                                                             \
    {                                                                                              \
        for (i = 0; i < n; i++)                                                                    \
            putc (' ', hotlist_file);                                                              \
    }                                                                                              \
    while (0)

    for (current = grp->head; current != NULL; current = current->next)
        switch (current->type)
        {
        case HL_TYPE_GROUP:
            INDENT (list_level);
            fputs ("GROUP \"", hotlist_file);
            for (s = current->label; *s != '\0'; s++)
            {
                if (*s == '"' || *s == '\\')
                    putc ('\\', hotlist_file);
                putc (*s, hotlist_file);
            }
            fputs ("\"\n", hotlist_file);
            list_level += 2;
            hot_save_group (current);
            list_level -= 2;
            INDENT (list_level);
            fputs ("ENDGROUP\n", hotlist_file);
            break;
        case HL_TYPE_ENTRY:
            INDENT (list_level);
            fputs ("ENTRY \"", hotlist_file);
            for (s = current->label; *s != '\0'; s++)
            {
                if (*s == '"' || *s == '\\')
                    putc ('\\', hotlist_file);
                putc (*s, hotlist_file);
            }
            fputs ("\" URL \"", hotlist_file);
            for (s = current->directory; *s != '\0'; s++)
            {
                if (*s == '"' || *s == '\\')
                    putc ('\\', hotlist_file);
                putc (*s, hotlist_file);
            }
            fputs ("\"\n", hotlist_file);
            break;
        case HL_TYPE_COMMENT:
            fprintf (hotlist_file, "#%s\n", current->label);
            break;
        case HL_TYPE_DOTDOT:
            // do nothing
            break;
        default:
            break;
        }
}

/* --------------------------------------------------------------------------------------------- */

static void
add_dotdot_to_list (void)
{
    if (current_group != hotlist && hotlist_has_dot_dot)
        add2hotlist (g_strdup (".."), g_strdup (".."), HL_TYPE_DOTDOT, LISTBOX_APPEND_AT_END);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
add2hotlist_cmd (WPanel *panel)
{
    char *lc_prompt;
    const char *cp = _ ("Label for \"%s\":");
    int l;
    char *label_string, *label;

    // extra variable to use it in the button callback
    our_panel = panel;

    if (current_group == NULL)
        load_hotlist ();

    label_string = panel_location (panel);
    if (label_string == NULL)
        return;

    if (report_duplicate (label_string, NULL))
    {
        g_free (label_string);
        return;
    }

    l = str_term_width1 (cp);
    lc_prompt = g_strdup_printf (cp, str_trunc (label_string, COLS - 2 * UX - (l + 8)));
    label = input_dialog (_ ("Add to hotlist"), lc_prompt, MC_HISTORY_HOTLIST_ADD, label_string,
                          INPUT_COMPLETE_NONE);
    g_free (lc_prompt);

    if (label == NULL || *label == '\0')
    {
        g_free (label_string);
        g_free (label);
    }
    else
    {
        add2hotlist (label, label_string, HL_TYPE_ENTRY, LISTBOX_APPEND_AFTER);
        hotlist_state.modified = TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
hotlist_show (WPanel *panel, gboolean *to_other)
{
    char *target = NULL;
    int res;

    hotlist_state.to_other = FALSE;

    // extra variable to use it in the button callback
    our_panel = panel;

    load_hotlist ();

    init_hotlist ();

    // display file info
    tty_setcolor (CORE_SELECTED_COLOR);

    hotlist_state.running = TRUE;
    res = dlg_run (hotlist_dlg);
    hotlist_state.running = FALSE;
    save_hotlist ();

    if (res == B_ENTER)
    {
        char *text = NULL;
        struct hotlist *hlp = NULL;

        listbox_get_current (l_hotlist, &text, (void **) &hlp);
        target = g_strdup (hlp != NULL ? hlp->directory : text);
    }

    *to_other = hotlist_state.to_other;

    hotlist_done ();
    return target;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
save_hotlist (void)
{
    gboolean saved = FALSE;
    struct stat stat_buf;

    if (!hotlist_state.readonly && hotlist_state.modified && hotlist_file_name != NULL)
    {
        mc_util_make_backup_if_possible (hotlist_file_name, ".bak");

        hotlist_file = fopen (hotlist_file_name, "w");
        if (hotlist_file == NULL)
            mc_util_restore_from_backup_if_possible (hotlist_file_name, ".bak");
        else
        {
            hot_save_group (hotlist);
            fclose (hotlist_file);
            stat (hotlist_file_name, &stat_buf);
            hotlist_file_mtime = stat_buf.st_mtime;
            hotlist_state.modified = FALSE;
            saved = TRUE;
        }
    }

    return saved;
}

/* --------------------------------------------------------------------------------------------- */

static void
hotlist_walk (const struct hotlist *grp, hotlist_entry_fn fn, void *data)
{
    const struct hotlist *h;

    for (h = grp->head; h != NULL; h = h->next)
        if (h->type == HL_TYPE_GROUP)
            hotlist_walk (h, fn, data);
        else if (h->type == HL_TYPE_ENTRY)
            fn (h->label, h->directory, grp == hotlist ? "" : grp->label, data);
}

/* --------------------------------------------------------------------------------------------- */

void
hotlist_foreach (hotlist_entry_fn fn, void *data)
{
    // only a hotlist that is there: loading makes the file and may ask about it
    if (!hotlist_state.loaded)
    {
        char *name = mc_config_get_full_path (MC_HOTLIST_FILE);
        const gboolean there = access (name, R_OK) == 0;

        g_free (name);
        if (!there)
            return;
    }
    load_hotlist ();
    if (hotlist != NULL)
        hotlist_walk (hotlist, fn, data);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Unload list from memory.
 * Don't confuse with hotlist_done() for GUI.
 */

void
done_hotlist (void)
{
    if (hotlist != NULL)
    {
        remove_group (hotlist);
        g_free (hotlist->label);
        g_free (hotlist->directory);
        MC_PTR_FREE (hotlist);
    }

    hotlist_state.loaded = FALSE;

    MC_PTR_FREE (hotlist_file_name);
    l_hotlist = NULL;
    current_group = NULL;

    if (tkn_buf != NULL)
    {
        g_string_free (tkn_buf, TRUE);
        tkn_buf = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
