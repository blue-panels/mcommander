/*
   The user menu that edits itself.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file usermenu_ini.c
 *  \brief Source: the user menu that edits itself
 *
 *  The menu of mc.menu is a file written by hand, in a language of conditions
 *  of its own.  This is the other menu: entries live in a key file, one group
 *  each, and the list edits them.  Both are read; a key file, where there is
 *  one, is what the list shows.
 */

#include <config.h>

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/mcconfig.h"
#include "lib/strutil.h"
#include "lib/tty/key.h"
#include "lib/tty/tty.h"
#include "lib/util.h"
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "src/filemanager/cmd.h"
#include "src/history.h"
#include "src/setup.h"
#include "src/util.h"

#include "usermenu.h"
#include "usermenu_ini.h"

/*** file scope macro definitions ****************************************************************/

#define MENU_INI_LOCAL ".mc6menu"
#define MENU_INI_USER  "menu.ini"

// the field of commands, and the dialog around it
#define UM_COMMAND_LINES 8
#define UM_DIALOG_LINES  (UM_COMMAND_LINES + 14)

#define UM_KEY_HOTKEY    "hotkey"
#define UM_KEY_COMMAND   "command"
#define UM_KEY_VIEW      "view"
#define UM_KEY_SILENT    "silent"
#define UM_KEY_SUBMENU   "submenu"
#define UM_KEY_PARENT    "parent"

/*** file scope type declarations ****************************************************************/

typedef enum
{
    MENU_LEVEL_LOCAL = 0,
    MENU_LEVEL_USER,
    MENU_LEVEL_COUNT
} menu_level_t;

typedef enum
{
    UM_EDIT_CANCEL = 0,
    UM_EDIT_OK,
    UM_EDIT_FILE
} um_edit_t;

typedef enum
{
    UM_ACTION_NONE = 0,
    UM_ACTION_RUN,
    UM_ACTION_ADD,
    UM_ACTION_EDIT,
    UM_ACTION_DELETE,
    UM_ACTION_UP,
    UM_ACTION_DOWN,
    UM_ACTION_FILE,
    UM_ACTION_IMPORT
} um_action_t;

/*** file scope variables ************************************************************************/

static um_action_t um_action = UM_ACTION_NONE;

// the field the dialog hands Enter to, while it is up
static WTextArea *um_command_area = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
um_file_has_content (const char *file)
{
    struct stat st;

    return stat (file, &st) == 0 && S_ISREG (st.st_mode) && st.st_size > 0;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * A menu of the directory runs commands, so it is taken only from a file that
 * belongs to this user or to root and that nobody else can write: the same
 * condition the menu file written by hand has always been read under.
 */
static gboolean
um_file_is_safe (const char *file)
{
    struct stat st;

    if (stat (file, &st) != 0)
        return FALSE;

    if ((st.st_uid == 0 || st.st_uid == geteuid ()) && (st.st_mode & (S_IWGRP | S_IWOTH)) == 0)
        return TRUE;

    if (verbose)
        message (D_NORMAL, _ ("Warning -- ignoring file"),
                 _ ("File %s is not owned by root or you or is world writable.\n"
                    "Using it may compromise your security"),
                 file);

    return FALSE;
}

/**
 * The file a level is kept in: one of the directory, one of the user.
 */
static char *
um_level_file (menu_level_t level)
{
    switch (level)
    {
    case MENU_LEVEL_LOCAL:
        return g_strdup (MENU_INI_LOCAL);

    case MENU_LEVEL_USER:
    default:
        // Not mc_config_get_full_path(): that one knows a fixed list of names.
        return g_build_filename (mc_config_get_path (), MENU_INI_USER, (char *) NULL);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
user_menu_entry_free (user_menu_entry_t *entry)
{
    if (entry == NULL)
        return;

    g_free (entry->label);
    g_free (entry->command);
    g_free (entry->parent);
    g_free (entry);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * A command of several lines goes into a single line of input, so the newlines
 * are written the way a shell writes them.  A backslash stands for itself only
 * when it is doubled, or the two directions would not meet.
 */
char *
user_menu_ini_escape (const char *command)
{
    GString *out;
    const char *p;

    out = g_string_sized_new (strlen (command) + 8);

    for (p = command; *p != '\0'; p++)
        switch (*p)
        {
        case '\n':
            g_string_append (out, "\\n");
            break;
        case '\\':
            g_string_append (out, "\\\\");
            break;
        default:
            g_string_append_c (out, *p);
            break;
        }

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

char *
user_menu_ini_unescape (const char *text)
{
    GString *out;
    const char *p;

    out = g_string_sized_new (strlen (text) + 1);

    for (p = text; *p != '\0'; p++)
    {
        if (*p != '\\')
        {
            g_string_append_c (out, *p);
            continue;
        }

        switch (*(p + 1))
        {
        case 'n':
            g_string_append_c (out, '\n');
            p++;
            break;
        case '\\':
            g_string_append_c (out, '\\');
            p++;
            break;
        default:
            g_string_append_c (out, *p);
            break;
        }
    }

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read one file.  The groups are taken in the order the file has them, which is
 * the order of the list.  A file that is not there is an empty menu; a file
 * that cannot be read gives no entries and FALSE.
 */
gboolean
user_menu_ini_load_file (GPtrArray *entries, const char *file, int level, GError **error)
{
    GKeyFile *keys;
    gchar **groups;
    gsize i, count = 0;

    if (!exist_file (file))
        return TRUE;

    keys = g_key_file_new ();
    if (!g_key_file_load_from_file (keys, file, G_KEY_FILE_KEEP_COMMENTS, error))
    {
        g_key_file_free (keys);
        return FALSE;
    }

    groups = g_key_file_get_groups (keys, &count);

    for (i = 0; i < count; i++)
    {
        user_menu_entry_t *entry;
        char *hotkey;

        entry = g_new0 (user_menu_entry_t, 1);
        entry->label = g_strdup (groups[i]);
        entry->command = g_key_file_get_string (keys, groups[i], UM_KEY_COMMAND, NULL);
        if (entry->command == NULL)
            entry->command = g_strdup ("");
        entry->view = g_key_file_get_boolean (keys, groups[i], UM_KEY_VIEW, NULL);
        entry->silent = g_key_file_get_boolean (keys, groups[i], UM_KEY_SILENT, NULL);
        entry->is_submenu = g_key_file_get_boolean (keys, groups[i], UM_KEY_SUBMENU, NULL);
        entry->parent = g_key_file_get_string (keys, groups[i], UM_KEY_PARENT, NULL);
        entry->level = level;

        hotkey = g_key_file_get_string (keys, groups[i], UM_KEY_HOTKEY, NULL);
        if (hotkey != NULL)
        {
            entry->hotkey = hotkey[0];
            g_free (hotkey);
        }

        g_ptr_array_add (entries, entry);
    }

    g_strfreev (groups);
    g_key_file_free (keys);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/** Open the file of a level in the editor. */
static void
um_level_edit_file (menu_level_t level)
{
    char *file;
    vfs_path_t *vpath;

    file = um_level_file (level);
    vpath = vfs_path_from_str (file);
    edit_file_at_line (vpath, TRUE, 1);
    vfs_path_free (vpath, TRUE);
    g_free (file);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * A file that cannot be read is not an empty menu: say what is wrong with it
 * and offer to fix it, until it is read or the user goes on without it.
 */
static void
um_level_load (GPtrArray *entries, menu_level_t level)
{
    char *file;
    GError *error = NULL;

    file = um_level_file (level);

    // a file of the directory is anybody's; the one of the user is his own
    if (level == MENU_LEVEL_LOCAL && !um_file_is_safe (file))
    {
        g_free (file);
        return;
    }

    while (!user_menu_ini_load_file (entries, file, level, &error))
    {
        char *text;
        int answer;

        text = g_strdup_printf (_ ("Cannot read the menu file\n%s\n\n%s\n\n"
                                   "Its entries are not shown, and the menu does not "
                                   "write into it\nuntil it is fixed."),
                                file, error->message);
        g_clear_error (&error);
        answer = query_dialog (MSG_ERROR, text, D_ERROR, 2, _ ("&Edit the file"), _ ("&Skip"));
        g_free (text);

        if (answer != 0)
            break;

        um_level_edit_file (level);
    }

    g_free (file);
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
um_entries_load (void)
{
    GPtrArray *entries;
    menu_level_t level;

    entries = g_ptr_array_new_with_free_func ((GDestroyNotify) user_menu_entry_free);

    for (level = MENU_LEVEL_LOCAL; level < MENU_LEVEL_COUNT; level++)
        um_level_load (entries, level);

    return entries;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Write the entries of one level into a file.  The file is built anew, in the
 * order of the list, and the keys of a group that is still there are carried
 * over: what a later version writes into a group is not lost by an older one
 * that edits it.  A file that is there but cannot be read is not written over:
 * the entries in it were never read, and would be lost.
 */
gboolean
user_menu_ini_save_file (const char *file, GPtrArray *entries, int level, GError **error)
{
    GKeyFile *old, *keys;
    guint i;
    gboolean ok;
    GError *load_error = NULL;

    old = g_key_file_new ();
    if (!g_key_file_load_from_file (old, file, G_KEY_FILE_KEEP_COMMENTS, &load_error)
        && !g_error_matches (load_error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
        g_propagate_error (error, load_error);
        g_key_file_free (old);
        return FALSE;
    }
    g_clear_error (&load_error);

    keys = g_key_file_new ();

    for (i = 0; i < entries->len; i++)
    {
        user_menu_entry_t *entry = g_ptr_array_index (entries, i);
        gchar **old_keys;
        gsize k, count = 0;
        char hotkey[2] = { '\0', '\0' };

        if (entry->level != level)
            continue;

        old_keys = g_key_file_get_keys (old, entry->label, &count, NULL);
        for (k = 0; k < count; k++)
        {
            char *value;

            value = g_key_file_get_value (old, entry->label, old_keys[k], NULL);
            if (value != NULL)
            {
                g_key_file_set_value (keys, entry->label, old_keys[k], value);
                g_free (value);
            }
        }
        g_strfreev (old_keys);

        hotkey[0] = entry->hotkey;
        g_key_file_set_string (keys, entry->label, UM_KEY_HOTKEY, hotkey);

        if (entry->parent != NULL && *entry->parent != '\0')
            g_key_file_set_string (keys, entry->label, UM_KEY_PARENT, entry->parent);
        else
            g_key_file_remove_key (keys, entry->label, UM_KEY_PARENT, NULL);

        if (entry->is_submenu)
        {
            g_key_file_set_boolean (keys, entry->label, UM_KEY_SUBMENU, TRUE);
            g_key_file_remove_key (keys, entry->label, UM_KEY_COMMAND, NULL);
            g_key_file_remove_key (keys, entry->label, UM_KEY_VIEW, NULL);
            g_key_file_remove_key (keys, entry->label, UM_KEY_SILENT, NULL);
        }
        else
        {
            g_key_file_set_string (keys, entry->label, UM_KEY_COMMAND, entry->command);
            g_key_file_set_boolean (keys, entry->label, UM_KEY_VIEW, entry->view);
            g_key_file_set_boolean (keys, entry->label, UM_KEY_SILENT, entry->silent);
        }
    }

    ok = g_key_file_save_to_file (keys, file, error);

    g_key_file_free (keys);
    g_key_file_free (old);

    return ok;
}

/**
 * Whether the file of a level can be written.  A menu.ini put there by an
 * administrator is somebody else's file, and saying so beforehand is better
 * than a write that fails.
 */
static gboolean
um_level_writable (menu_level_t level)
{
    char *file;
    gboolean ok;

    file = um_level_file (level);

    if (exist_file (file))
        ok = access (file, W_OK) == 0;
    else
    {
        char *dir;

        dir = g_path_get_dirname (file);
        ok = access (dir, W_OK) == 0;
        g_free (dir);
    }

    g_free (file);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Refuse an entry that comes from a file this user cannot write, and say which
 * file it is.
 */
static gboolean
um_entry_is_mine (const user_menu_entry_t *entry)
{
    char *file;

    if (um_level_writable (entry->level))
        return TRUE;

    file = um_level_file (entry->level);
    message (D_ERROR, MSG_ERROR,
             _ ("This entry comes from\n%s\n\nwhich you cannot write. Copy it into a menu of "
                "your own\nwith Ins, or ask whoever owns that file."),
             file);
    g_free (file);

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
um_level_save (GPtrArray *entries, menu_level_t level)
{
    char *file;
    gboolean ok;
    GError *error = NULL;

    file = um_level_file (level);
    ok = user_menu_ini_save_file (file, entries, level, &error);
    if (!ok)
    {
        message (D_ERROR, MSG_ERROR, _ ("Cannot write file\n%s\n\n%s"), file, error->message);
        g_error_free (error);
    }
    g_free (file);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/** On a failed write the list is read again: it shows what the file holds. */
static gboolean
um_entries_save (GPtrArray **entries, menu_level_t level)
{
    if (um_level_save (*entries, level))
        return TRUE;

    g_ptr_array_free (*entries, TRUE);
    *entries = um_entries_load ();

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Enter belongs to the field of commands while it has the focus: a line of a
 * script ends there, not the dialog.  The hotkey pass, where the default button
 * would take it, comes after this.
 */
static cb_ret_t
um_dialog_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    if (msg == MSG_KEY && (parm == '\n' || parm == '\r' || parm == KEY_ENTER)
        && um_command_area != NULL && widget_get_state (WIDGET (um_command_area), WST_FOCUSED))
        return send_message (um_command_area, NULL, MSG_KEY, parm, NULL);

    return dlg_default_callback (w, sender, msg, parm, data);
}

/* --------------------------------------------------------------------------------------------- */

/** What the dialog of an entry edits: a copy of the entry, given back only on OK. */
typedef struct
{
    char *label;
    char *command;
    char hotkey;
    gboolean view;
    gboolean silent;
} um_draft_t;

/** The widgets of that dialog, to read them back once it is closed. */
typedef struct
{
    WDialog *dlg;
    WInput *hotkey;
    WInput *label;
    WTextArea *commands;  // NULL for a submenu, which has none
    WCheck *view;
    WCheck *silent;
} um_form_t;

/* --------------------------------------------------------------------------------------------- */

/** The row of buttons, centred by their own widths. */
static void
um_form_buttons (WGroup *group, int y, int width, gboolean with_editor)
{
    WButton *buttons[3];
    int count = 0, i, total = 0, x;

    buttons[count++] = button_new (y, 0, B_ENTER, DEFPUSH_BUTTON, _ ("&OK"), NULL);
    if (with_editor)
        buttons[count++] = button_new (y, 0, B_USER, NORMAL_BUTTON, _ ("&Editor"), NULL);
    buttons[count++] = button_new (y, 0, B_CANCEL, NORMAL_BUTTON, _ ("&Cancel"), NULL);

    for (i = 0; i < count; i++)
        total += button_get_width (buttons[i]);
    total += 2 * (count - 1);

    // added back to front: the last one added is the first the Tab reaches,
    // and the walk should go OK, Editor, Cancel
    x = (width - total) / 2;
    for (i = count - 1; i >= 0; i--)
    {
        int bx = x;
        int k;

        for (k = 0; k < i; k++)
            bx += button_get_width (buttons[k]) + 2;
        WIDGET (buttons[i])->rect.x = bx;
        group_add_widget (group, buttons[i]);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The dialog of one entry: a hotkey, a label and the commands, and what to do
 * with the output.  No masks and no conditions: what the dialog cannot say,
 * the file still can, and the Editor button opens it.  A submenu has only the
 * hotkey and the label.
 */
static void
um_form_build (um_form_t *form, const um_draft_t *draft, gboolean is_submenu, gboolean is_new)
{
    const int width = 64;
    const int inner = width - 6;
    const int lines = is_submenu ? 10 : UM_DIALOG_LINES;
    const char *title;
    WGroup *group;
    char hotkey_text[2] = { draft->hotkey, '\0' };
    int y = 2;

    title = is_submenu ? (is_new ? _ ("Add a submenu") : _ ("Edit the submenu"))
        : is_new       ? _ ("Add a user menu entry")
                       : _ ("Edit the user menu entry");

    form->dlg = dlg_create (TRUE, 0, 0, lines, width, WPOS_CENTER, FALSE, dialog_colors,
                            um_dialog_callback, NULL, "[Edit Menu File]", title);
    group = GROUP (form->dlg);

    group_add_widget (group, label_new (y++, 3, _ ("Hotkey:")));
    form->hotkey =
        input_new (y++, 3, input_colors, 5, hotkey_text, "usermenu-hotkey", INPUT_COMPLETE_NONE);
    group_add_widget (group, form->hotkey);

    group_add_widget (group, label_new (y++, 3, _ ("Label:")));
    form->label = input_new (y++, 3, input_colors, inner, draft->label, "usermenu-label",
                             INPUT_COMPLETE_NONE);
    group_add_widget (group, form->label);

    form->commands = NULL;
    form->view = NULL;
    form->silent = NULL;

    if (!is_submenu)
    {
        group_add_widget (group, label_new (y++, 3, _ ("Commands:")));
        form->commands = textarea_new (y, 3, UM_COMMAND_LINES, inner, draft->command);
        um_command_area = form->commands;
        group_add_widget (group, form->commands);
        y += UM_COMMAND_LINES;

        group_add_widget (group, hline_new (y++, -1, -1));

        form->view = check_new (y++, 3, draft->view, _ ("Show the output in the &viewer"));
        group_add_widget (group, form->view);
        form->silent = check_new (y++, 3, draft->silent, _ ("Run without the &shell of the panel"));
        group_add_widget (group, form->silent);
    }

    group_add_widget (group, hline_new (y++, -1, -1));
    um_form_buttons (group, y, width, !is_submenu);

    widget_select (WIDGET (form->label));
}

/* --------------------------------------------------------------------------------------------- */

/** What was typed, kept whichever button ended the dialog. */
static void
um_form_read (const um_form_t *form, um_draft_t *draft)
{
    char *text;

    text = input_get_text (form->hotkey);
    draft->hotkey = text[0];
    g_free (text);

    g_free (draft->label);
    draft->label = input_get_text (form->label);

    if (form->commands != NULL)
    {
        g_free (draft->command);
        draft->command = textarea_get_text (form->commands);
        draft->view = form->view->state;
        draft->silent = form->silent->state;
    }
}

/* --------------------------------------------------------------------------------------------- */

static um_edit_t
um_entry_edit (user_menu_entry_t *entry, gboolean is_new)
{
    um_draft_t draft;
    um_edit_t result_kind = UM_EDIT_CANCEL;

    draft.label = g_strdup (entry->label);
    draft.command = g_strdup (entry->command);
    draft.hotkey = entry->hotkey;
    draft.view = entry->view;
    draft.silent = entry->silent;

    while (TRUE)
    {
        um_form_t form;
        int result;

        um_form_build (&form, &draft, entry->is_submenu, is_new);
        result = dlg_run (form.dlg);
        um_form_read (&form, &draft);
        widget_destroy (WIDGET (form.dlg));
        um_command_area = NULL;

        if (result == B_CANCEL)
            break;

        if (result == B_USER)
        {
            // the file the entry lives in, not the commands of this one
            result_kind = UM_EDIT_FILE;
            break;
        }

        if (*draft.label == '\0')
        {
            message (D_ERROR, MSG_ERROR, "%s", _ ("The entry needs a label"));
            continue;
        }

        result_kind = UM_EDIT_OK;
        break;
    }

    if (result_kind == UM_EDIT_OK)
    {
        g_free (entry->label);
        entry->label = draft.label;
        g_free (entry->command);
        entry->command = draft.command;
        entry->hotkey = draft.hotkey;
        entry->view = draft.view;
        entry->silent = draft.silent;
    }
    else
    {
        g_free (draft.label);
        g_free (draft.command);
    }

    return result_kind;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The label as it is shown: the substitutions of the menu are put in, so that
 * "print %f" stands in the list with the name of the file under the cursor.
 * The label in the file keeps the macro, and %{...} is left alone - a list is
 * no place to ask the user anything.
 */
static char *
um_label_expand (const char *label)
{
    GString *out;
    const char *p;

    if (strchr (label, '%') == NULL)
        return g_strdup (label);

    out = g_string_sized_new (strlen (label) + 16);

    for (p = label; *p != '\0'; p++)
    {
        char *value;

        if (*p != '%' || *(p + 1) == '\0')
        {
            g_string_append_c (out, *p);
            continue;
        }

        p++;

        if (*p == '{')
        {
            // a prompt: shown as it is written
            g_string_append (out, "%{");
            while (*(p + 1) != '\0' && *(p + 1) != '}')
                g_string_append_c (out, *++p);
            if (*(p + 1) == '}')
                g_string_append_c (out, *++p);
            continue;
        }

        value = expand_format (NULL, *p, FALSE);
        if (value == NULL || *value == '\0')
        {
            // nothing to put there: the macro stands as it was written
            g_string_append_c (out, '%');
            g_string_append_c (out, *p);
        }
        else
            g_string_append (out, value);

        g_free (value);
    }

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The text execute_menu_command() reads: the first line is the title it skips,
 * and the command follows it, indented, the way the file of the old menu has
 * it.  The engine of the substitutions is thereby the same one.
 */
static char *
um_entry_script (const user_menu_entry_t *entry)
{
    GString *out;
    const char *p;

    out = g_string_new (entry->label);
    g_string_append_c (out, '\n');

    if (entry->view)
        g_string_append (out, "\t%view\n");

    g_string_append_c (out, '\t');
    for (p = entry->command; *p != '\0'; p++)
    {
        g_string_append_c (out, *p);
        if (*p == '\n')
            g_string_append_c (out, '\t');
    }
    g_string_append_c (out, '\n');

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
um_list_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    if (msg == MSG_KEY)
    {
        WDialog *h = DIALOG (w);
        um_action_t action = UM_ACTION_NONE;

        switch (parm)
        {
        case KEY_IC:
            action = UM_ACTION_ADD;
            break;
        case KEY_F (4):
            action = UM_ACTION_EDIT;
            break;
        case KEY_F (14):  // Shift-F4
            action = UM_ACTION_FILE;
            break;
        case KEY_F (5):
            action = UM_ACTION_IMPORT;
            break;
        case KEY_DC:
            action = UM_ACTION_DELETE;
            break;
        case ALT ('u'):
        case KEY_M_CTRL | KEY_UP:
            action = UM_ACTION_UP;
            break;
        case ALT ('d'):
        case KEY_M_CTRL | KEY_DOWN:
            action = UM_ACTION_DOWN;
            break;
        default:
            break;
        }

        if (action != UM_ACTION_NONE)
        {
            um_action = action;
            h->ret_value = B_ENTER;
            dlg_close (h);
            return MSG_HANDLED;
        }

        /* Not one of ours: the dialog goes on to its own handling of the key,
           where Enter closes the list.  Handing it back to the default callback
           would deal it out to the widgets a second time, the list would count
           it as handled, and Enter would do nothing at all. */
        return MSG_NOT_HANDLED;
    }

    return dlg_default_callback (w, sender, msg, parm, data);
}

/* --------------------------------------------------------------------------------------------- */

static int
um_list_run (GPtrArray *entries, int current, const char *title)
{
    Listbox *listbox;
    guint i;
    int width = 0;
    int selected;

    for (i = 0; i < entries->len; i++)
    {
        user_menu_entry_t *entry = g_ptr_array_index (entries, i);
        char *label;

        // the width of what is shown, macros put in
        label = um_label_expand (entry->label);
        width = MAX (width, str_term_width1 (label));
        g_free (label);
    }

    // room for the hotkey column the entries are drawn with
    width = MAX (width + 9, 40);
    width = MIN (width, COLS - 6);

    listbox = listbox_window_new (MAX (1, MIN ((int) entries->len, LINES - 10)), width, title,
                                  "[Edit Menu File]");
    WIDGET (listbox->dlg)->callback = um_list_callback;

    for (i = 0; i < entries->len; i++)
    {
        user_menu_entry_t *entry = g_ptr_array_index (entries, i);
        char *text;
        char *label;

        // The key stands in a column of its own, as the entries of mc.menu do;
        // a submenu carries a trailing slash, the way a directory does.
        label = um_label_expand (entry->label);
        text = g_strdup_printf ("%c  %s%s", entry->hotkey != '\0' ? entry->hotkey : ' ', label,
                                entry->is_submenu ? "/" : "");
        LISTBOX_APPEND_TEXT (listbox, (unsigned char) entry->hotkey, text, entry, FALSE);
        g_free (text);
        g_free (label);
    }

    if (current >= 0 && current < (int) entries->len)
        listbox_set_current (listbox->list, current);

    um_action = UM_ACTION_RUN;
    selected = listbox_run (listbox);

    return selected;
}

/**
 * The label is the name of the group the entry is kept in, and that decides
 * what a label may hold: no brackets, which end a group name, and nothing that
 * breaks a line.  Two entries cannot share a label either, or the second would
 * take the place of the first, so a repeat is numbered.
 */
static void
um_label_fix (GPtrArray *entries, user_menu_entry_t *entry)
{
    char *base;
    char *p;
    guint n = 1;

    if (entry->label == NULL)
        entry->label = g_strdup ("");

    for (p = entry->label; *p != '\0'; p++)
        switch (*p)
        {
        case '[':
            *p = '(';
            break;
        case ']':
            *p = ')';
            break;
        case '\n':
        case '\r':
        case '\t':
            *p = ' ';
            break;
        default:
            break;
        }

    base = g_strdup (entry->label);

    while (TRUE)
    {
        guint i;
        gboolean taken = FALSE;

        for (i = 0; i < entries->len && !taken; i++)
        {
            user_menu_entry_t *other = g_ptr_array_index (entries, i);

            taken = other != entry && strcmp (other->label, entry->label) == 0;
        }

        if (!taken)
            break;

        g_free (entry->label);
        entry->label = g_strdup_printf ("%s (%u)", base, ++n);
    }

    g_free (base);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read a menu file written by hand into entries.  A line that does not start
 * with a space is the title of an entry, its first character the hotkey; the
 * lines under it, indented, are the commands.  Conditions and comments are
 * dropped: the dialog has no place for them, and the file they came from stays
 * where it is.
 */
guint
user_menu_ini_import_file (GPtrArray *entries, const char *file, int level)
{
    char *data = NULL;
    gchar **lines;
    guint i, added = 0;
    user_menu_entry_t *entry = NULL;
    GString *command = NULL;

    if (!g_file_get_contents (file, &data, NULL, NULL))
        return 0;

    lines = g_strsplit (data, "\n", -1);

    for (i = 0; lines[i] != NULL; i++)
    {
        const char *line = lines[i];

        if (*line == ' ' || *line == '\t')
        {
            // a command of the entry above, with its indentation dropped
            if (entry != NULL)
            {
                while (*line == ' ' || *line == '\t')
                    line++;
                if (command->len != 0)
                    g_string_append_c (command, '\n');
                g_string_append (command, line);
            }
            continue;
        }

        // whatever was collected belongs to the entry that is ending here
        if (entry != NULL)
        {
            entry->command = g_string_free (command, FALSE);
            command = NULL;

            // An entry with no commands is a line that only looked like one.
            if (*entry->command == '\0')
                user_menu_entry_free (entry);
            else
            {
                um_label_fix (entries, entry);
                g_ptr_array_add (entries, entry);
                added++;
            }

            entry = NULL;
        }

        // a comment, a condition, or the directive at the top of the file
        if (*line == '\0' || *line == '#' || *line == '+' || *line == '='
            || strncmp (line, "shell_patterns", 14) == 0)
            continue;

        entry = g_new0 (user_menu_entry_t, 1);
        entry->hotkey = *line;
        entry->level = level;

        line++;
        while (*line == ' ' || *line == '\t')
            line++;
        entry->label = g_strchomp (g_strdup (*line != '\0' ? line : lines[i]));
        command = g_string_new ("");
    }

    if (entry != NULL)
    {
        entry->command = g_string_free (command, FALSE);

        if (*entry->command == '\0')
            user_menu_entry_free (entry);
        else
        {
            um_label_fix (entries, entry);
            g_ptr_array_add (entries, entry);
            added++;
        }
    }
    else if (command != NULL)
        g_string_free (command, TRUE);

    g_strfreev (lines);
    g_free (data);

    return added;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

/**
 * A menu file written by hand, if there is one to import: his own first - the
 * one of the directory, then the one of the user - and then the one of the
 * installation, which is what somebody sees who never wrote a menu himself.
 *
 * An empty file is nothing to keep: "Edit menu file" leaves one behind when it
 * is asked for a menu that does not exist.
 */
static char *
um_old_menu (gboolean *is_own)
{
    char *file;

    if (is_own != NULL)
        *is_own = TRUE;

    if (um_file_has_content (MC_LOCAL_MENU) && um_file_is_safe (MC_LOCAL_MENU))
        return g_strdup (MC_LOCAL_MENU);

    if (um_file_has_content (MC_LOCAL_MENU_LEGACY) && um_file_is_safe (MC_LOCAL_MENU_LEGACY))
        return g_strdup (MC_LOCAL_MENU_LEGACY);

    file = mc_config_get_full_path (MC_USERMENU_FILE);
    if (file != NULL && um_file_has_content (file))
        return file;
    g_free (file);

    if (is_own != NULL)
        *is_own = FALSE;

    // What an installation of mc, or an older one of mc6, has left behind.
    {
        static const char *const names[] = { MC_GLOBAL_MENU, MENU_INI_USER };
        const char *const dirs[] = { mc_global.sysconfig_dir, mc_global.share_data_dir };
        gsize i, k;

        for (i = 0; i < G_N_ELEMENTS (dirs); i++)
            for (k = 0; k < G_N_ELEMENTS (names); k++)
            {
                file = g_build_filename (dirs[i], names[k], (char *) NULL);
                if (um_file_has_content (file))
                    return file;
                g_free (file);
            }
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Which of the entries of a menu file written by hand to take.  Space marks the
 * one under the cursor, Ins marks it and steps down, '*' turns every mark over,
 * Enter takes the marked ones and Esc takes none.
 */
static int um_pick_key = 0;

static cb_ret_t
um_pick_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    if (msg == MSG_KEY)
    {
        if (parm == ' ' || parm == '*' || parm == KEY_IC)
        {
            WDialog *h = DIALOG (w);

            um_pick_key = parm;
            h->ret_value = B_ENTER;
            dlg_close (h);
            return MSG_HANDLED;
        }

        return MSG_NOT_HANDLED;
    }

    return dlg_default_callback (w, sender, msg, parm, data);
}

/* --------------------------------------------------------------------------------------------- */

static guint
um_import_pick (GPtrArray *entries, gboolean *marked)
{
    int current = 0;
    guint i, count = 0;

    while (TRUE)
    {
        Listbox *listbox;
        int width = 0;
        int selected;

        for (i = 0; i < entries->len; i++)
        {
            user_menu_entry_t *entry = g_ptr_array_index (entries, i);

            width = MAX (width, str_term_width1 (entry->label));
        }
        width = MIN (MAX (width + 8, 46), COLS - 6);

        listbox = listbox_window_new (MIN ((int) entries->len, LINES - 12), width,
                                      _ ("Import: space marks, Enter takes"), "[Edit Menu File]");
        WIDGET (listbox->dlg)->callback = um_pick_callback;

        for (i = 0; i < entries->len; i++)
        {
            user_menu_entry_t *entry = g_ptr_array_index (entries, i);
            char *text;

            text = g_strdup_printf ("%s %s", marked[i] ? "[x]" : "[ ]", entry->label);
            LISTBOX_APPEND_TEXT (listbox, 0, text, entry, FALSE);
            g_free (text);
        }

        listbox_set_current (listbox->list, current);

        um_pick_key = 0;
        selected = listbox_run (listbox);

        if (selected < 0)
            return 0;  // Esc: nothing is taken

        current = selected;

        if (um_pick_key == 0)
            break;  // Enter: what is marked is what goes in

        if (um_pick_key == '*')
            for (i = 0; i < entries->len; i++)
                marked[i] = !marked[i];
        else
        {
            marked[current] = !marked[current];
            if (um_pick_key == KEY_IC && current + 1 < (int) entries->len)
                current++;
        }
    }

    for (i = 0; i < entries->len; i++)
        if (marked[i])
            count++;

    return count;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Take what is marked out of a menu file written by hand into the menu of the
 * user.  Returns whether anything was taken.
 */
static gboolean
um_import (const char *old_menu)
{
    GPtrArray *entries;
    guint added;
    gboolean ok = FALSE;

    entries = g_ptr_array_new_with_free_func ((GDestroyNotify) user_menu_entry_free);
    added = user_menu_ini_import_file (entries, old_menu, MENU_LEVEL_USER);

    if (added != 0)
    {
        gboolean *marked;
        guint i;

        // the entries to take are the ones the user marks
        marked = g_new0 (gboolean, added);
        added = um_import_pick (entries, marked);

        for (i = entries->len; i > 0; i--)
            if (!marked[i - 1])
                g_ptr_array_remove_index (entries, i - 1);

        g_free (marked);
    }

    if (added != 0)
    {
        GPtrArray *all;
        char *file;
        guint i;

        /* The file is written from the entries it is given, so the ones already
           in it have to be there too: what is imported is added to the menu, it
           does not become the menu. */
        all = g_ptr_array_new_with_free_func ((GDestroyNotify) user_menu_entry_free);
        file = um_level_file (MENU_LEVEL_USER);
        // a file that cannot be read stays as it is: um_level_save() refuses it
        (void) user_menu_ini_load_file (all, file, MENU_LEVEL_USER, NULL);
        g_free (file);

        for (i = 0; i < entries->len; i++)
        {
            user_menu_entry_t *entry = g_ptr_array_index (entries, i);

            um_label_fix (all, entry);
            g_ptr_array_add (all, entry);
        }

        // the entries moved over; the array they came from must not free them
        g_ptr_array_set_free_func (entries, NULL);
        g_ptr_array_free (entries, TRUE);
        entries = all;
    }

    if (added == 0)
        message (D_ERROR, MSG_ERROR, _ ("Nothing was taken from\n%s"), old_menu);
    else if (um_level_save (entries, MENU_LEVEL_USER))
    {
        char *ini;

        ini = um_level_file (MENU_LEVEL_USER);
        message (D_NORMAL, _ ("User menu"),
                 _ ("Taken into\n%s\n\nEntries: %u. The file they came from is left where it "
                    "is,\nand conditions and masks were dropped."),
                 ini, added);
        g_free (ini);
        ok = TRUE;
    }

    g_ptr_array_free (entries, TRUE);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/** A NULL parent and an empty one are the top level, and the same thing. */
static gboolean
um_parent_eq (const user_menu_entry_t *entry, const char *parent)
{
    const char *own;

    own = (entry->parent != NULL && *entry->parent != '\0') ? entry->parent : NULL;

    if (own == NULL || parent == NULL)
        return own == parent;

    return strcmp (own, parent) == 0;
}

/* --------------------------------------------------------------------------------------------- */

/** The entries shown at one level, in the order of the file. Borrowed pointers. */
static GPtrArray *
um_view (GPtrArray *entries, const char *parent)
{
    GPtrArray *view;
    guint i;

    view = g_ptr_array_new ();

    for (i = 0; i < entries->len; i++)
    {
        user_menu_entry_t *entry = g_ptr_array_index (entries, i);

        if (um_parent_eq (entry, parent))
            g_ptr_array_add (view, entry);
    }

    return view;
}

/* --------------------------------------------------------------------------------------------- */

static int
um_index_of (GPtrArray *entries, const user_menu_entry_t *entry)
{
    guint i;

    for (i = 0; i < entries->len; i++)
        if (g_ptr_array_index (entries, i) == entry)
            return (int) i;

    return -1;
}

/* --------------------------------------------------------------------------------------------- */

/** The submenu entry a level belongs to, so a new child can take its level. */
static user_menu_entry_t *
um_find_by_label (GPtrArray *entries, const char *label)
{
    guint i;

    if (label == NULL)
        return NULL;

    for (i = 0; i < entries->len; i++)
    {
        user_menu_entry_t *entry = g_ptr_array_index (entries, i);

        if (strcmp (entry->label, label) == 0)
            return entry;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/** Remove an entry, and everything under it when it is a submenu. */
static void
um_remove_subtree (GPtrArray *entries, user_menu_entry_t *entry)
{
    if (entry->is_submenu)
    {
        guint i = 0;

        while (i < entries->len)
        {
            user_menu_entry_t *child = g_ptr_array_index (entries, i);

            if (um_parent_eq (child, entry->label))
            {
                um_remove_subtree (entries, child);
                i = 0;  // the array shifted under us; scan it again
            }
            else
                i++;
        }
    }

    {
        int idx = um_index_of (entries, entry);

        if (idx >= 0)
            g_ptr_array_remove_index (entries, idx);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Import asked for from the list, or from the empty menu: the file written by
 * hand that mc knows of, or one the user names himself.
 */
static gboolean
um_import_dialog (void)
{
    char *old_menu;
    char *file;
    gboolean ok;

    old_menu = um_old_menu (NULL);

    file = input_dialog (_ ("Import a menu file"), _ ("The file to take the entries from"),
                         MC_HISTORY_FM_MENU_IMPORT, old_menu != NULL ? old_menu : "",
                         INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD);
    g_free (old_menu);

    if (file == NULL || *file == '\0')
    {
        g_free (file);
        return FALSE;
    }

    if (!exist_file (file))
    {
        file_error_message (_ ("Cannot open file\n%s"), file);
        g_free (file);
        return FALSE;
    }

    ok = um_import (file);
    g_free (file);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Which of the two menus F2 opens.
 *
 * A key file of the user, or one of the directory, is the menu: that is what he
 * edited last.  Where neither exists but he has a menu file of his own written
 * by hand, mc offers to import it, once in a session; declined, that file stays
 * the menu.  With nothing of his own he gets the empty menu, which asks for its
 * first entry: the installation ships none.
 */
/**
 * Whether the user keeps a menu of the new kind, without asking him anything.
 * user_menu_ini_preferred() offers an import on the way, which a caller that
 * only wants to know cannot use.
 */
gboolean
user_menu_ini_own_exists (void)
{
    menu_level_t level;

    for (level = MENU_LEVEL_LOCAL; level <= MENU_LEVEL_USER; level++)
    {
        char *file;
        gboolean found;

        file = um_level_file (level);
        found = exist_file (file) && (level != MENU_LEVEL_LOCAL || um_file_is_safe (file));
        g_free (file);

        if (found)
            return TRUE;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

char *
user_menu_ini_path (gboolean local)
{
    return um_level_file (local ? MENU_LEVEL_LOCAL : MENU_LEVEL_USER);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
user_menu_ini_preferred (void)
{
    static gboolean asked = FALSE;
    menu_level_t level;
    char *old_menu;
    gboolean is_own = FALSE;
    char *file;
    gboolean found;

    for (level = MENU_LEVEL_LOCAL; level <= MENU_LEVEL_USER; level++)
    {
        file = um_level_file (level);
        found = exist_file (file) && (level != MENU_LEVEL_LOCAL || um_file_is_safe (file));
        g_free (file);

        if (found)
            return TRUE;
    }

    old_menu = um_old_menu (&is_own);
    if (old_menu == NULL)
        return TRUE;  // nothing of the old kind either: the empty menu it is

    if (!asked)
    {
        asked = TRUE;

        char *text;
        int answer;

        // query_dialog takes the text as it is: the name goes in beforehand
        text = g_strdup_printf (_ ("The menu is a file written by hand:\n%s\n\n"
                                   "Import entries of it into the menu that edits itself?"),
                                old_menu);
        answer = query_dialog (_ ("User menu"), text, D_NORMAL, 2, _ ("&Import"),
                               is_own ? _ ("&Keep the file") : _ ("&Skip"));
        g_free (text);

        if (answer == 0 && um_import (old_menu))
        {
            g_free (old_menu);
            return TRUE;
        }
    }

    g_free (old_menu);

    /* Declined, or nothing taken: his own file stays the menu, as it was.  A
       file of the installation is not his, and the menu that edits itself is
       where his own entries go. */
    return !is_own;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
user_menu_ini_cmd (void)
{
    GPtrArray *entries;
    GPtrArray *path;  // the labels of the submenus we are inside
    GArray *saved;    // the row each of those was left on
    int current = 0;
    gboolean done = FALSE;
    gboolean res = FALSE;

    entries = um_entries_load ();
    path = g_ptr_array_new_with_free_func (g_free);
    saved = g_array_new (FALSE, FALSE, sizeof (int));

    while (!done)
    {
        const char *parent;
        GPtrArray *view;
        char *title;
        int selected;
        user_menu_entry_t *entry = NULL;

        parent = path->len != 0 ? g_ptr_array_index (path, path->len - 1) : NULL;

        // the empty menu: offer to add or to import
        if (entries->len == 0)
        {
            user_menu_entry_t *new_entry;
            int answer;

            answer = query_dialog (_ ("User menu"),
                                   _ ("The menu is empty.\n\n"
                                      "Add an entry, or take entries from a menu file written "
                                      "by hand?"),
                                   D_NORMAL, 3, _ ("&Add an entry"), _ ("&Import"), _ ("&Cancel"));

            if (answer == 1)
            {
                if (um_import_dialog ())
                {
                    g_ptr_array_free (entries, TRUE);
                    entries = um_entries_load ();
                }
                continue;
            }

            if (answer != 0)
                break;

            new_entry = g_new0 (user_menu_entry_t, 1);
            new_entry->label = g_strdup ("");
            new_entry->command = g_strdup ("");
            new_entry->level = MENU_LEVEL_USER;

            switch (um_entry_edit (new_entry, TRUE))
            {
            case UM_EDIT_OK:
                um_label_fix (entries, new_entry);
                g_ptr_array_add (entries, new_entry);
                um_entries_save (&entries, MENU_LEVEL_USER);
                break;

            case UM_EDIT_FILE:
                user_menu_entry_free (new_entry);
                um_level_edit_file (MENU_LEVEL_USER);
                g_ptr_array_free (entries, TRUE);
                entries = um_entries_load ();
                break;

            default:
                user_menu_entry_free (new_entry);
                done = TRUE;
                break;
            }

            continue;
        }

        view = um_view (entries, parent);

        // the title names the submenus we are inside
        if (parent == NULL)
            title = g_strdup (_ ("User menu"));
        else
        {
            GString *t;
            guint i;

            t = g_string_new (_ ("User menu"));
            for (i = 0; i < path->len; i++)
            {
                g_string_append (t, " / ");
                g_string_append (t, (const char *) g_ptr_array_index (path, i));
            }
            title = g_string_free (t, FALSE);
        }

        if (current >= (int) view->len)
            current = (int) view->len - 1;
        if (current < 0)
            current = 0;

        selected = um_list_run (view, current, title);
        g_free (title);

        if (selected >= 0 && selected < (int) view->len)
        {
            current = selected;
            entry = g_ptr_array_index (view, selected);
        }

        // Esc, or nothing to choose from: up a level, or out of the menu at the top
        if (entry == NULL && um_action != UM_ACTION_ADD && um_action != UM_ACTION_IMPORT)
        {
            g_ptr_array_free (view, TRUE);

            if (path->len == 0)
                break;

            g_ptr_array_remove_index (path, path->len - 1);
            current = g_array_index (saved, int, saved->len - 1);
            g_array_remove_index (saved, saved->len - 1);
            continue;
        }

        switch (um_action)
        {
        case UM_ACTION_RUN:
            if (entry->is_submenu)
            {
                // step into it, remembering the row to come back to
                g_array_append_val (saved, current);
                g_ptr_array_add (path, g_strdup (entry->label));
                current = 0;
            }
            else
            {
                char *script;

                script = um_entry_script (entry);
                user_menu_execute (NULL, script, !entry->silent);
                g_free (script);
                res = TRUE;
                done = TRUE;
            }
            break;

        case UM_ACTION_ADD:
        {
            user_menu_entry_t *new_entry;
            menu_level_t level;
            int answer;

            // the file a new entry goes into: the one of its neighbour, or of
            // the submenu it is added to, or the user's own
            if (entry != NULL)
                level = entry->level;
            else
            {
                user_menu_entry_t *box = um_find_by_label (entries, parent);

                level = box != NULL ? box->level : MENU_LEVEL_USER;
            }
            if (!um_level_writable (level))
                level = MENU_LEVEL_USER;

            answer = query_dialog (
                _ ("User menu"), _ ("Add a command, or a submenu to hold other entries?"), D_NORMAL,
                3, _ ("Add a &command"), _ ("Add a &submenu"), _ ("&Cancel"));
            if (answer != 0 && answer != 1)
                break;

            new_entry = g_new0 (user_menu_entry_t, 1);
            new_entry->label = g_strdup ("");
            new_entry->command = g_strdup ("");
            new_entry->is_submenu = answer == 1;
            new_entry->parent = parent != NULL ? g_strdup (parent) : NULL;
            new_entry->level = level;

            switch (um_entry_edit (new_entry, TRUE))
            {
            case UM_EDIT_OK:
            {
                int at;

                um_label_fix (entries, new_entry);
                at = entry != NULL ? um_index_of (entries, entry) + 1 : (int) entries->len;
                g_ptr_array_insert (entries, at, new_entry);
                if (um_entries_save (&entries, new_entry->level) && entry != NULL)
                    current++;
                break;
            }

            case UM_EDIT_FILE:
            {
                menu_level_t lvl = new_entry->level;

                user_menu_entry_free (new_entry);
                um_level_edit_file (lvl);
                g_ptr_array_free (entries, TRUE);
                entries = um_entries_load ();
                break;
            }

            default:
                user_menu_entry_free (new_entry);
                break;
            }
            break;
        }

        case UM_ACTION_EDIT:
            if (um_entry_is_mine (entry))
                switch (um_entry_edit (entry, FALSE))
                {
                case UM_EDIT_OK:
                    um_label_fix (entries, entry);
                    um_entries_save (&entries, entry->level);
                    break;

                case UM_EDIT_FILE:
                    um_level_edit_file (entry->level);
                    g_ptr_array_free (entries, TRUE);
                    entries = um_entries_load ();
                    break;

                default:
                    break;
                }
            break;

        case UM_ACTION_DELETE:
            if (um_entry_is_mine (entry)
                && query_dialog (_ ("User menu"),
                                 entry->is_submenu ? _ ("Delete this submenu and everything in it?")
                                                   : _ ("Delete this entry?"),
                                 D_ERROR, 2, _ ("&Yes"), _ ("&No"))
                    == 0)
            {
                menu_level_t level = entry->level;

                um_remove_subtree (entries, entry);
                um_entries_save (&entries, level);
            }
            break;

        case UM_ACTION_UP:
        case UM_ACTION_DOWN:
        {
            int other;

            // the neighbour at this level; the two swap places in the file
            other = um_action == UM_ACTION_UP ? selected - 1 : selected + 1;
            if (um_entry_is_mine (entry) && other >= 0 && other < (int) view->len)
            {
                user_menu_entry_t *neighbour = g_ptr_array_index (view, other);

                if (neighbour->level == entry->level)
                {
                    int a = um_index_of (entries, entry);
                    int b = um_index_of (entries, neighbour);

                    g_ptr_array_index (entries, a) = neighbour;
                    g_ptr_array_index (entries, b) = entry;
                    if (um_entries_save (&entries, entry->level))
                        current = other;
                }
            }
            break;
        }

        case UM_ACTION_IMPORT:
            if (um_import_dialog ())
            {
                g_ptr_array_free (entries, TRUE);
                entries = um_entries_load ();
                current = 0;
            }
            break;

        case UM_ACTION_FILE:
            um_level_edit_file (entry->level);
            g_ptr_array_free (entries, TRUE);
            entries = um_entries_load ();
            break;

        default:
            break;
        }

        g_ptr_array_free (view, TRUE);
    }

    g_array_free (saved, TRUE);
    g_ptr_array_free (path, TRUE);
    g_ptr_array_free (entries, TRUE);
    do_refresh ();

    return res;
}

/* --------------------------------------------------------------------------------------------- */
