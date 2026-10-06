/*
   User menu for the M-Commander
   The menu that edits itself: the list, the dialogs, the files of a level

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
#define UM_DIALOG_LINES  (UM_COMMAND_LINES + 15)

/*** file scope type declarations ****************************************************************/

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
    UM_ACTION_IMPORT,
    UM_ACTION_SHOW_ALL
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

// the files the user did not want converted, not to be asked about again
static GHashTable *um_not_converted = NULL;

/**
 * A menu file without the format line: written by an older version, or by
 * hand.  The menu reads only its own format, so it offers to convert the file
 * first; declined, the file is left alone and its entries are not shown.
 */
static gboolean
um_level_convert (const char *file)
{
    GError *error = NULL;
    char *text;
    int answer;

    if (um_not_converted == NULL)
        um_not_converted = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    else if (g_hash_table_contains (um_not_converted, file))
        return FALSE;

    text = g_strdup_printf (_ ("The menu file\n%s\nhas no first line \"# mc menu format 2\": "
                               "an older version\nof mc wrote it, or it was written by hand.\n\n"
                               "Convert it to the format? The file as it is now\n"
                               "is kept as %s.old."),
                            file, file);
    answer = query_dialog (_ ("User menu"), text, D_NORMAL, 2, _ ("&Convert"), _ ("&Skip"));
    g_free (text);

    if (answer != 0)
    {
        g_hash_table_add (um_not_converted, g_strdup (file));
        return FALSE;
    }

    if (!user_menu_ini_convert_file (file, &error))
    {
        message (D_ERROR, MSG_ERROR, _ ("Cannot convert the menu file\n%s\n\n%s"), file,
                 error->message);
        g_error_free (error);
        g_hash_table_add (um_not_converted, g_strdup (file));
        return FALSE;
    }

    return TRUE;
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

    if (user_menu_ini_needs_conversion (file) && !um_level_convert (file))
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
    char *cond[UM_COND_COUNT];
    char *dcond[UM_COND_COUNT];
    gboolean is_default;
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

static void um_form_buttons (WGroup *group, int y, int width, gboolean with_editor);

// the types of on=, in the order of the boxes of the Conditions dialog
static const char *const um_on_names[] = {
    "file", "dir", "link", "fifo", "socket", "broken", "char", "block",
};
#define UM_ON_COUNT G_N_ELEMENTS (um_on_names)

// what the Conditions button edits, and where it shows the result
static um_draft_t *um_cond_draft = NULL;
static WLabel *um_cond_summary = NULL;
static int um_cond_summary_width = 0;

/* --------------------------------------------------------------------------------------------- */

static gboolean
um_str_eq (const char *a, const char *b)
{
    if (a == NULL || b == NULL)
        return a == b;
    return strcmp (a, b) == 0;
}

/* --------------------------------------------------------------------------------------------- */

/** The conditions the way the file has them, one key after another. */
static char *
um_cond_text (char *const *cond, char *const *dcond, gboolean is_default)
{
    GString *out;
    int k;

    out = g_string_new ("");

    for (k = 0; k < UM_COND_COUNT; k++)
        if (cond[k] != NULL)
        {
            if (out->len != 0)
                g_string_append (out, ", ");
            g_string_append_printf (out, "%s=%s", user_menu_ini_cond_keys[k], cond[k]);
        }

    if (is_default)
        g_string_append (out, out->len != 0 ? ", default" : "default");

    for (k = 0; k < UM_COND_COUNT; k++)
        if (dcond[k] != NULL)
        {
            if (out->len != 0)
                g_string_append (out, ", ");
            g_string_append_printf (out, "%s=%s", user_menu_ini_default_keys[k], dcond[k]);
        }

    if (out->len == 0)
        g_string_append (out, _ ("always"));

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
um_cond_any (char *const *cond)
{
    int k;

    for (k = UM_COND_PATH; k <= UM_COND_PANEL_LAST; k++)
        if (cond[k] != NULL)
            return TRUE;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/** The boxes of the types: "!dir" checks every box but the directory. */
static void
um_on_to_boxes (const char *on, gboolean *boxes)
{
    gboolean listed[UM_ON_COUNT] = { FALSE };
    gboolean negated[UM_ON_COUNT] = { FALSE };
    gboolean any_negated = FALSE;
    gchar **items;
    guint i, t;

    for (t = 0; t < UM_ON_COUNT; t++)
        boxes[t] = FALSE;

    if (on == NULL)
        return;

    items = g_strsplit (on, ";", -1);
    for (i = 0; items[i] != NULL; i++)
    {
        const char *item = g_strstrip (items[i]);
        const gboolean negate = *item == '!';

        if (negate)
            item++;

        for (t = 0; t < UM_ON_COUNT; t++)
            if (strcmp (item, um_on_names[t]) == 0)
            {
                if (negate)
                {
                    negated[t] = TRUE;
                    any_negated = TRUE;
                }
                else
                    listed[t] = TRUE;
            }
    }
    g_strfreev (items);

    for (t = 0; t < UM_ON_COUNT; t++)
        boxes[t] = listed[t] || (any_negated && !negated[t]);
}

/* --------------------------------------------------------------------------------------------- */

/** No box checked, or all of them, is any type: no key. */
static char *
um_boxes_to_on (const gboolean *boxes)
{
    GString *out;
    guint t, checked = 0;

    out = g_string_new ("");

    for (t = 0; t < UM_ON_COUNT; t++)
        if (boxes[t])
        {
            if (out->len != 0)
                g_string_append_c (out, ';');
            g_string_append (out, um_on_names[t]);
            checked++;
        }

    if (checked == 0 || checked == UM_ON_COUNT)
    {
        g_string_free (out, TRUE);
        return NULL;
    }

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/** The text of an input, NULL where it is empty. */
static char *
um_input_value (WInput *in)
{
    char *text;

    text = input_get_text (in);
    g_strstrip (text);
    if (*text == '\0')
    {
        g_free (text);
        return NULL;
    }

    return text;
}

/* --------------------------------------------------------------------------------------------- */

/** A value goes back only where the user changed it: what the dialog cannot say stays. */
static void
um_cond_take (char **value, char *was, char *now)
{
    if (um_str_eq (was, now))
        g_free (now);
    else
    {
        g_free (*value);
        *value = now;
    }
    g_free (was);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean um_conditions_edit (um_draft_t *draft, gboolean for_default);

// the choice of the entry the menu opens on, while the Conditions dialog is up
static WRadio *um_default_radio = NULL;

/* --------------------------------------------------------------------------------------------- */

/** The conditions of default.*: once they are set, "When" is the choice. */
static int
um_default_conditions_button (WButton *button, int action)
{
    (void) button;
    (void) action;

    if (um_cond_draft != NULL && um_conditions_edit (um_cond_draft, TRUE))
    {
        um_default_radio->sel = um_default_radio->pos = 2;
        widget_draw (WIDGET (um_default_radio));
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Where the entry is shown, or, for_default, where the menu opens on it.  The
 * dialog looks at one panel; conditions that look at both are left to the
 * file.
 */
static gboolean
um_conditions_edit (um_draft_t *draft, gboolean for_default)
{
    static const char *panel_names[2];
    static const char *default_names[3];
    const int width = 64;
    const int col = 19;
    char **set = for_default ? draft->dcond : draft->cond;
    char **own = set, **other = set + UM_COND_OTHER;
    char *dcond_was[UM_COND_COUNT];
    WRadio *dflt = NULL;
    char **cond;
    gboolean boxes[UM_ON_COUNT], boxes_now[UM_ON_COUNT];
    WCheck *checks[UM_ON_COUNT];
    WDialog *dlg;
    WGroup *g;
    WInput *path, *needs;
    WCheck *exec, *marked, *regex;
    WRadio *panel;
    gboolean was_exec, was_marked, was_regex;
    int was_panel;
    int path_k;
    int y = 2;
    guint t;
    gboolean ok;

    if (um_cond_any (own) && um_cond_any (other))
    {
        message (D_NORMAL, _ ("Conditions"),
                 _ ("These conditions look at both panels, and the dialog shows one.\n"
                    "Change them in the file: the Editor button opens it."));
        return FALSE;
    }

    was_panel = um_cond_any (other) ? 1 : 0;
    cond = was_panel == 1 ? other : own;

    // the field holds either masks or a regular expression
    was_regex = cond[UM_COND_PATH_RE] != NULL;
    if (was_regex && cond[UM_COND_PATH] != NULL)
    {
        message (D_NORMAL, _ ("Conditions"),
                 _ ("These conditions have masks and a regular expression together,\n"
                    "and the dialog shows one of them. Change them in the file: the\n"
                    "Editor button opens it."));
        return FALSE;
    }
    path_k = was_regex ? UM_COND_PATH_RE : UM_COND_PATH;

    um_on_to_boxes (cond[UM_COND_ON], boxes);
    was_exec = um_str_eq (cond[UM_COND_EXEC], "true");
    was_marked = um_str_eq (cond[UM_COND_MARKED], "true");

    panel_names[0] = _ ("C&urrent panel");
    panel_names[1] = _ ("Ot&her panel");
    default_names[0] = _ ("N&ever");
    default_names[1] = _ ("&Always");
    default_names[2] = _ ("&When:");

    dlg = dlg_create (TRUE, 0, 0, for_default ? 18 : 22, width, WPOS_CENTER, FALSE, dialog_colors,
                      NULL, NULL, "[Edit Menu File]",
                      for_default ? _ ("Where the menu opens on it") : _ ("Conditions"));
    g = GROUP (dlg);

    group_add_widget (g, label_new (y, 3, _ ("Path mask:")));
    path = input_new (y++, col, input_colors, width - col - 3,
                      cond[path_k] != NULL ? cond[path_k] : "", "usermenu-path",
                      INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD);
    group_add_widget (g, path);

    regex = check_new (y++, col, was_regex, _ ("Re&gular expression"));
    group_add_widget (g, regex);
    y++;
    group_add_widget (g, label_new (y++, 3, _ ("Show on (none checked = any):")));
    {
        const char *labels[UM_ON_COUNT] = {
            _ ("File"),   _ ("Directory"),   _ ("Link"),        _ ("Fifo"),
            _ ("Socket"), _ ("Broken link"), _ ("Char device"), _ ("Block device"),
        };

        for (t = 0; t < UM_ON_COUNT; t++)
        {
            checks[t] = check_new (y + (int) t / 3, 5 + ((int) t % 3) * 19, boxes[t], labels[t]);
            group_add_widget (g, checks[t]);
        }
        y += (UM_ON_COUNT + 2) / 3;
    }

    y++;
    exec = check_new (y++, 3, was_exec, _ ("E&xecutable only"));
    group_add_widget (g, exec);
    marked = check_new (y++, 3, was_marked, _ ("Only when files are &marked"));
    group_add_widget (g, marked);

    group_add_widget (g, label_new (y, 3, _ ("Needs program:")));
    needs = input_new (y++, col, input_colors, width - col - 3,
                       set[UM_COND_NEEDS] != NULL ? set[UM_COND_NEEDS] : "", "usermenu-needs",
                       INPUT_COMPLETE_COMMANDS);
    group_add_widget (g, needs);

    y++;
    panel = radio_new (y, 3, 2, panel_names);
    panel->sel = panel->pos = was_panel;
    group_add_widget (g, panel);

    if (!for_default)
    {
        int k;
        WButton *when;

        // kept to put back on Cancel: the button below edits them in place
        for (k = 0; k < UM_COND_COUNT; k++)
            dcond_was[k] = g_strdup (draft->dcond[k]);

        group_add_widget (g, label_new (y, 26, _ ("The menu opens on it:")));
        dflt = radio_new (y + 1, 28, 3, default_names);
        dflt->sel = dflt->pos = draft->is_default ? 1
            : um_cond_any (draft->dcond) || um_cond_any (draft->dcond + UM_COND_OTHER)
                || draft->dcond[UM_COND_NEEDS] != NULL
            ? 2
            : 0;
        group_add_widget (g, dflt);
        um_default_radio = dflt;
        when = button_new (y + 3, 40, B_USER + 2, NORMAL_BUTTON, _ ("Con&ditions"),
                           um_default_conditions_button);
        group_add_widget (g, when);
        y += 2;
    }
    y += 2;

    group_add_widget (g, hline_new (y++, -1, -1));
    um_form_buttons (g, y, width, FALSE);

    widget_select (WIDGET (path));

    ok = dlg_run (dlg) == B_ENTER;

    if (ok)
    {
        char *value[UM_COND_PANEL_LAST + 1] = { NULL };
        const int now_panel = panel->sel;
        char **to = now_panel == 1 ? other : own;
        int k;

        for (k = UM_COND_PATH; k <= UM_COND_PANEL_LAST; k++)
            value[k] = g_strdup (cond[k]);

        um_cond_take (&value[path_k], g_strdup (cond[path_k]), um_input_value (path));

        // the same text, now under the key of the other kind
        if (regex->state != was_regex)
        {
            const int kind = regex->state ? UM_COND_PATH_RE : UM_COND_PATH;

            value[kind] = value[path_k];
            value[path_k] = NULL;
        }

        for (t = 0; t < UM_ON_COUNT; t++)
            boxes_now[t] = checks[t]->state;
        if (memcmp (boxes, boxes_now, sizeof (boxes)) != 0)
        {
            g_free (value[UM_COND_ON]);
            value[UM_COND_ON] = um_boxes_to_on (boxes_now);
        }

        if (exec->state != was_exec)
        {
            g_free (value[UM_COND_EXEC]);
            value[UM_COND_EXEC] = exec->state ? g_strdup ("true") : NULL;
        }
        if (marked->state != was_marked)
        {
            g_free (value[UM_COND_MARKED]);
            value[UM_COND_MARKED] = marked->state ? g_strdup ("true") : NULL;
        }

        um_cond_take (&set[UM_COND_NEEDS], g_strdup (set[UM_COND_NEEDS]), um_input_value (needs));

        if (dflt != NULL)
        {
            // Never and Always leave no conditions of their own behind
            draft->is_default = dflt->sel == 1;
            if (dflt->sel != 2)
                for (k = 0; k < UM_COND_COUNT; k++)
                {
                    g_free (draft->dcond[k]);
                    draft->dcond[k] = NULL;
                }
        }

        // the values go to the keys of the panel chosen, whichever they came from
        for (k = UM_COND_PATH; k <= UM_COND_PANEL_LAST; k++)
        {
            g_free (own[k]);
            own[k] = NULL;
            g_free (other[k]);
            other[k] = NULL;
            to[k] = value[k];
        }
    }

    if (dflt != NULL)
    {
        int k;

        for (k = 0; k < UM_COND_COUNT; k++)
            if (ok)
                g_free (dcond_was[k]);
            else
            {
                g_free (draft->dcond[k]);
                draft->dcond[k] = dcond_was[k];
            }
        um_default_radio = NULL;
    }

    widget_destroy (WIDGET (dlg));

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static void
um_cond_summary_update (void)
{
    char *text;

    text = um_cond_text (um_cond_draft->cond, um_cond_draft->dcond, um_cond_draft->is_default);
    label_set_text (um_cond_summary, str_fit_to_term (text, um_cond_summary_width, J_LEFT_FIT));
    g_free (text);
}

/* --------------------------------------------------------------------------------------------- */

static int
um_conditions_button (WButton *button, int action)
{
    (void) button;
    (void) action;

    if (um_cond_draft != NULL && um_conditions_edit (um_cond_draft, FALSE))
        um_cond_summary_update ();

    return 0;  // the entry dialog stays
}

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
 * The dialog of one entry: a hotkey, a label and the commands, what to do with
 * the output, and where the entry is shown.  A submenu has no commands.
 */
static void
um_form_build (um_form_t *form, um_draft_t *draft, gboolean is_submenu, gboolean is_new)
{
    const int width = 64;
    const int inner = width - 6;
    const int lines = is_submenu ? 11 : UM_DIALOG_LINES;
    WButton *conditions;
    int x;
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

    group_add_widget (group, label_new (y, 3, _ ("Show when:")));
    conditions =
        button_new (y, 0, B_USER + 1, NORMAL_BUTTON, _ ("Con&ditions"), um_conditions_button);
    x = width - 3 - button_get_width (conditions);
    WIDGET (conditions)->rect.x = x;
    um_cond_draft = draft;
    um_cond_summary_width = x - 15;
    um_cond_summary = label_new (y, 14, "");
    group_add_widget (group, um_cond_summary);
    um_cond_summary_update ();
    group_add_widget (group, conditions);
    y++;

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
    int k;

    draft.label = g_strdup (entry->label);
    draft.command = g_strdup (entry->command);
    draft.hotkey = entry->hotkey;
    draft.view = entry->view;
    draft.silent = entry->silent;
    for (k = 0; k < UM_COND_COUNT; k++)
    {
        draft.cond[k] = g_strdup (entry->cond[k]);
        draft.dcond[k] = g_strdup (entry->dcond[k]);
    }
    draft.is_default = entry->is_default;

    while (TRUE)
    {
        um_form_t form;
        int result;

        um_form_build (&form, &draft, entry->is_submenu, is_new);
        result = dlg_run (form.dlg);
        um_form_read (&form, &draft);
        widget_destroy (WIDGET (form.dlg));
        um_command_area = NULL;
        um_cond_draft = NULL;
        um_cond_summary = NULL;

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
        for (k = 0; k < UM_COND_COUNT; k++)
        {
            g_free (entry->cond[k]);
            entry->cond[k] = draft.cond[k];
            g_free (entry->dcond[k]);
            entry->dcond[k] = draft.dcond[k];
        }
        entry->is_default = draft.is_default;
    }
    else
    {
        g_free (draft.label);
        g_free (draft.command);
        for (k = 0; k < UM_COND_COUNT; k++)
        {
            g_free (draft.cond[k]);
            g_free (draft.dcond[k]);
        }
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
        case ALT ('a'):
            action = UM_ACTION_SHOW_ALL;
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
        // shown only because Alt-A asked for all of them
        if (!user_menu_ini_entry_visible (entry))
            listbox_set_dimmed (listbox->list, (int) i, TRUE);
        g_free (text);
        g_free (label);
    }

    if (current >= 0 && current < (int) entries->len)
        listbox_set_current (listbox->list, current);

    um_action = UM_ACTION_RUN;
    selected = listbox_run (listbox);

    return selected;
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
    guint not_converted = 0;
    gboolean ok = FALSE;

    entries = g_ptr_array_new_with_free_func ((GDestroyNotify) user_menu_entry_free);
    added = user_menu_ini_import_file (entries, old_menu, MENU_LEVEL_USER, NULL);

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
            const char *c;

            // one comment line for each condition that did not become keys
            for (c = entry->comment; c != NULL && (c = strchr (c, '\n')) != NULL; c++)
                not_converted++;

            user_menu_ini_label_fix (all, entry);
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
        if (not_converted == 0)
            message (D_NORMAL, _ ("User menu"),
                     _ ("Taken into\n%s\n\nEntries: %u. The file they came from is left where "
                        "it is."),
                     ini, added);
        else
            message (D_NORMAL, _ ("User menu"),
                     _ ("Taken into\n%s\n\nEntries: %u. The file they came from is left where "
                        "it is.\nConditions not converted: %u, kept as comments above their "
                        "entries."),
                     ini, added, not_converted);
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

/**
 * The entries shown at one level, in the order of the file, where their
 * conditions hold.  Borrowed pointers.
 */
static GPtrArray *
um_view (GPtrArray *entries, const char *parent, gboolean show_all, guint *hidden)
{
    GPtrArray *view;
    guint i;

    view = g_ptr_array_new ();
    *hidden = 0;

    for (i = 0; i < entries->len; i++)
    {
        user_menu_entry_t *entry = g_ptr_array_index (entries, i);

        if (!um_parent_eq (entry, parent))
            continue;

        if (user_menu_ini_entry_visible (entry))
            g_ptr_array_add (view, entry);
        else
        {
            (*hidden)++;
            if (show_all)
                g_ptr_array_add (view, entry);
        }
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
    gboolean pick_default = TRUE;          // a menu just opened starts on its default entry
    gboolean show_all = FALSE;             // Alt-A: the entries whose conditions do not hold, too
    const user_menu_entry_t *keep = NULL;  // the entry the cursor stays on after Alt-A
    guint hidden;
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
                user_menu_ini_label_fix (entries, new_entry);
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

        view = um_view (entries, parent, show_all, &hidden);

        if (keep != NULL)
        {
            guint i;

            for (i = 0; i < view->len; i++)
                if (g_ptr_array_index (view, i) == keep)
                    current = (int) i;
            keep = NULL;
        }

        if (pick_default)
        {
            guint i;

            pick_default = FALSE;
            for (i = 0; i < view->len; i++)
                if (user_menu_ini_entry_default (g_ptr_array_index (view, i)))
                {
                    current = (int) i;
                    break;
                }
        }

        // the title names the submenus we are inside, and what is hidden here
        {
            GString *t;
            guint i;

            t = g_string_new (_ ("User menu"));
            for (i = 0; i < path->len; i++)
            {
                g_string_append (t, " / ");
                g_string_append (t, (const char *) g_ptr_array_index (path, i));
            }
            if (show_all)
                g_string_append_printf (t, " %s", _ ("(all entries)"));
            else if (hidden != 0)
            {
                g_string_append_c (t, ' ');
                g_string_append_printf (t, _ ("(%u hidden, Alt-A)"), hidden);
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
        if (entry == NULL && um_action != UM_ACTION_ADD && um_action != UM_ACTION_IMPORT
            && um_action != UM_ACTION_SHOW_ALL)
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
                pick_default = TRUE;
            }
            else if (!user_menu_ini_entry_visible (entry))
                message (D_ERROR, _ ("User menu"), "%s",
                         _ ("The conditions of this entry do not hold here,\n"
                            "so it is not run. F4 edits them."));
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

                user_menu_ini_label_fix (entries, new_entry);
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
                    user_menu_ini_label_fix (entries, entry);
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

        case UM_ACTION_SHOW_ALL:
            show_all = !show_all;
            keep = entry;
            break;

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
