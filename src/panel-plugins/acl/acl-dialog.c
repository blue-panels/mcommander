/*
   Panel plugin acl for the M-Commander
   The access control list dialog, the entry dialog and the user chooser

   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/tty/key.h"
#include "lib/strutil.h"
#include "lib/widget.h"

#include "acl-dialog.h"

/*** file scope macro definitions ****************************************************************/

#define ACL_B_ADD            (B_USER + 1)
#define ACL_B_EDIT           (B_USER + 2)
#define ACL_B_DELETE         (B_USER + 3)
#define ACL_B_SET_ALL        (B_USER + 10)
#define ACL_B_CHANGES_TO_ALL (B_USER + 11)
#define ACL_B_CHOOSE         (B_USER + 20)
#define ACL_B_MASK           (B_USER + 21)

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *name;
    unsigned int id;
    char *comment;
    gboolean system;
    gboolean marked;
} acl_account_t;

/*** file scope variables ************************************************************************/

/* The one dialog open at a time, for the widget callbacks. */
static acl_dlg_state_t *acl_st = NULL;
static aclm_type_t acl_type = ACLM_ACCESS;
static WTable *acl_table = NULL;
static WLabel *acl_hint = NULL;
static WButton *acl_mask_btn = NULL;
static WRadio *acl_radio = NULL;
static gboolean acl_back_to_table = FALSE;
/* entries marked with Ins, by acl_mark_key() */
static GHashTable *acl_marks = NULL;

/* The entry dialog, for the Choose button. */
static unsigned long acl_entry_radio_id;
static unsigned long acl_entry_input_id;

/* The chooser. */
static GPtrArray *acl_accounts = NULL;
static WListbox *acl_chooser_list = NULL;
static WCheck *acl_chooser_system = NULL;

/*** file scope functions ************************************************************************/

static aclm_list_t *
acl_cur_list (void)
{
    return acl_st->lists[acl_type];
}

/* --------------------------------------------------------------------------------------------- */

static const char *
acl_kind_label (aclm_kind_t kind)
{
    switch (kind)
    {
    case ACLM_OWNER:
        return _ ("owner");
    case ACLM_USER:
        return _ ("user");
    case ACLM_GROUP_OBJ:
    case ACLM_GROUP:
        return _ ("group");
    case ACLM_MASK:
        return _ ("mask");
    case ACLM_OTHER:
    default:
        return _ ("other");
    }
}

/* --------------------------------------------------------------------------------------------- */

static unsigned int
acl_account_min (const char *key)
{
    FILE *f;
    char line[256];
    unsigned int min = 1000;

    f = fopen ("/etc/login.defs", "r");
    if (f == NULL)
        return min;

    while (fgets (line, sizeof (line), f) != NULL)
    {
        char *p = line;
        size_t len = strlen (key);

        while (*p == ' ' || *p == '\t')
            p++;
        if (strncmp (p, key, len) == 0 && (p[len] == ' ' || p[len] == '\t'))
        {
            unsigned long v = strtoul (p + len, NULL, 10);

            if (v > 0)
                min = (unsigned int) v;
            break;
        }
    }

    fclose (f);
    return min;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
acl_is_system (gboolean group, unsigned int id)
{
    static unsigned int uid_min = 0, gid_min = 0;

    if (uid_min == 0)
    {
        uid_min = acl_account_min ("UID_MIN");
        gid_min = acl_account_min ("GID_MIN");
    }

    return id < (group ? gid_min : uid_min) || id == 65534;
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_account_free (gpointer p)
{
    acl_account_t *a = (acl_account_t *) p;

    g_free (a->name);
    g_free (a->comment);
    g_free (a);
}

/* --------------------------------------------------------------------------------------------- */

static int
acl_account_compare (gconstpointer a, gconstpointer b)
{
    const acl_account_t *x = *(const acl_account_t *const *) a;
    const acl_account_t *y = *(const acl_account_t *const *) b;

    return strcmp (x->name, y->name);
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
acl_accounts_load (gboolean group)
{
    GPtrArray *all;

    all = g_ptr_array_new_with_free_func (acl_account_free);

    if (group)
    {
        struct group *gr;

        setgrent ();
        while ((gr = getgrent ()) != NULL)
        {
            acl_account_t *a = g_new0 (acl_account_t, 1);

            a->name = g_strdup (gr->gr_name);
            a->id = (unsigned int) gr->gr_gid;
            a->comment = g_strdup ("");
            a->system = acl_is_system (TRUE, a->id);
            g_ptr_array_add (all, a);
        }
        endgrent ();
    }
    else
    {
        struct passwd *pw;

        setpwent ();
        while ((pw = getpwent ()) != NULL)
        {
            acl_account_t *a = g_new0 (acl_account_t, 1);
            char *comma;

            a->name = g_strdup (pw->pw_name);
            a->id = (unsigned int) pw->pw_uid;
            a->comment = g_strdup (pw->pw_gecos != NULL ? pw->pw_gecos : "");
            comma = strchr (a->comment, ',');
            if (comma != NULL)
                *comma = '\0';
            a->system = acl_is_system (FALSE, a->id);
            g_ptr_array_add (all, a);
        }
        endpwent ();
    }

    g_ptr_array_sort (all, acl_account_compare);
    return all;
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_chooser_fill (const char *keep)
{
    int pos = 0, n = 0;
    guint i;

    listbox_remove_list (acl_chooser_list);

    for (i = 0; i < acl_accounts->len; i++)
    {
        acl_account_t *a = g_ptr_array_index (acl_accounts, i);
        char *text;

        if (a->system && !acl_chooser_system->state)
            continue;

        text =
            g_strdup_printf ("%c%-16s %7u  %s", a->marked ? '*' : ' ', a->name, a->id, a->comment);
        listbox_add_item (acl_chooser_list, LISTBOX_APPEND_AT_END, 0, text, a, FALSE);
        if (a->marked)
            listbox_set_emphasis (acl_chooser_list, n, TRUE);
        g_free (text);

        if (keep != NULL && strcmp (keep, a->name) == 0)
            pos = n;
        n++;
    }

    listbox_set_current (acl_chooser_list, pos);
}

/* --------------------------------------------------------------------------------------------- */

/* Ins marks the account under the cursor and moves down, as in the panels. */
static void
acl_chooser_toggle (void)
{
    int pos = acl_chooser_list->current;
    WLEntry *entry = listbox_get_nth_entry (acl_chooser_list, pos);
    acl_account_t *cur;

    if (entry == NULL || entry->data == NULL)
        return;
    cur = entry->data;

    cur->marked = !cur->marked;
    entry->text[0] = cur->marked ? '*' : ' ';
    listbox_set_emphasis (acl_chooser_list, pos, cur->marked);
    listbox_set_current (acl_chooser_list,
                         MIN (pos + 1, listbox_get_length (acl_chooser_list) - 1));
    widget_draw (WIDGET (acl_chooser_list));
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
acl_chooser_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    if (msg == MSG_KEY && parm == KEY_IC && GROUP (w)->current != NULL
        && GROUP (w)->current->data == (void *) acl_chooser_list)
    {
        acl_chooser_toggle ();
        return MSG_HANDLED;
    }

    if (msg == MSG_NOTIFY && sender == WIDGET (acl_chooser_system))
    {
        acl_account_t *cur = NULL;

        listbox_get_current (acl_chooser_list, NULL, (void **) &cur);
        acl_chooser_fill (cur != NULL ? cur->name : NULL);
        /* Restore the frame when the filtered list no longer needs a scrollbar. */
        widget_draw (w);
        return MSG_HANDLED;
    }

    return dlg_default_callback (w, sender, msg, parm, data);
}

/* --------------------------------------------------------------------------------------------- */

/* The names typed in @text, split at commas and blanks. */
gchar **
acl_split_names (const char *text)
{
    gchar **parts;
    GPtrArray *names;
    int i;

    names = g_ptr_array_new ();
    parts = g_strsplit_set (text != NULL ? text : "", ", \t", -1);
    for (i = 0; parts[i] != NULL; i++)
        if (parts[i][0] != '\0')
            g_ptr_array_add (names, g_strdup (parts[i]));
    g_strfreev (parts);
    g_ptr_array_add (names, NULL);

    return (gchar **) g_ptr_array_free (names, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Names picked from the system accounts, joined with ", ", or NULL.  The
 * names already typed come in marked; with nothing marked on OK the one under
 * the cursor is taken.
 */
char *
acl_choose (gboolean group, const char *current)
{
    WDialog *dlg;
    WGroup *g;
    int cols, lines, list_lines, y;
    gboolean show_system = FALSE;
    char *result = NULL;
    gchar **typed;
    guint i;
    int k;

    acl_accounts = acl_accounts_load (group);

    typed = acl_split_names (current);
    for (k = 0; typed[k] != NULL; k++)
        for (i = 0; i < acl_accounts->len; i++)
        {
            acl_account_t *a = g_ptr_array_index (acl_accounts, i);

            if (strcmp (a->name, typed[k]) == 0)
            {
                a->marked = TRUE;
                show_system = show_system || a->system;
            }
        }

    cols = MIN (60, COLS - 4);
    list_lines = MAX (5, MIN (15, LINES - 12));
    /* frame, check box, blank, list, line, buttons, frame */
    lines = list_lines + 8;

    dlg = dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors,
                      acl_chooser_callback, NULL, "[Access control list]",
                      group ? _ ("Choose group") : _ ("Choose user"));
    dlg->help_file = acl_help_file ();
    g = GROUP (dlg);

    y = 2;
    acl_chooser_system = check_new (y++, 3, show_system, _ ("Show &system accounts"));
    group_add_widget (g, acl_chooser_system);

    /* The listbox draws its scrollbar one column beyond its width. */
    acl_chooser_list = listbox_new (++y, 3, list_lines, cols - 5, FALSE, NULL);
    acl_chooser_list->quick_search = TRUE;
    group_add_widget (g, acl_chooser_list);
    y += list_lines;

    group_add_widget (g, hline_new (y++, -1, -1));
    group_add_widget (g, button_new (y, cols / 2 - 12, B_ENTER, DEFPUSH_BUTTON, _ ("&OK"), NULL));
    group_add_widget (g,
                      button_new (y, cols / 2 + 2, B_CANCEL, NORMAL_BUTTON, _ ("&Cancel"), NULL));

    acl_chooser_fill (typed[0]);
    g_strfreev (typed);
    widget_select (WIDGET (acl_chooser_list));

    if (dlg_run (dlg) == B_ENTER)
    {
        GString *names = g_string_new ("");
        acl_account_t *a = NULL;

        for (i = 0; i < acl_accounts->len; i++)
        {
            acl_account_t *m = g_ptr_array_index (acl_accounts, i);

            if (m->marked)
                g_string_append_printf (names, "%s%s", names->len != 0 ? ", " : "", m->name);
        }

        if (names->len == 0)
        {
            listbox_get_current (acl_chooser_list, NULL, (void **) &a);
            if (a != NULL)
                g_string_append (names, a->name);
        }

        if (names->len != 0)
            result = g_string_free (names, FALSE);
        else
            g_string_free (names, TRUE);
    }

    widget_destroy (WIDGET (dlg));
    g_ptr_array_free (acl_accounts, TRUE);
    acl_accounts = NULL;
    acl_chooser_list = NULL;
    acl_chooser_system = NULL;

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
acl_entry_choose_cb (WButton *button, int action)
{
    Widget *owner = WIDGET (WIDGET (button)->owner);
    Widget *radio, *input;
    char *name;

    (void) action;

    radio = widget_find_by_id (owner, acl_entry_radio_id);
    input = widget_find_by_id (owner, acl_entry_input_id);
    if (radio == NULL || input == NULL)
        return 0;

    name = acl_choose (RADIO (radio)->sel == 1, input_get_ctext (INPUT (input)));
    if (name != NULL)
    {
        input_assign_text (INPUT (input), name);
        g_free (name);
    }

    widget_select (input);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_perm_checks (unsigned int perm, gboolean *r, gboolean *w, gboolean *x)
{
    *r = (perm & ACL_PERM_READ) != 0;
    *w = (perm & ACL_PERM_WRITE) != 0;
    *x = (perm & ACL_PERM_EXECUTE) != 0;
}

/* --------------------------------------------------------------------------------------------- */

static unsigned int
acl_perm_from_checks (gboolean r, gboolean w, gboolean x)
{
    return (r ? ACL_PERM_READ : 0) | (w ? ACL_PERM_WRITE : 0) | (x ? ACL_PERM_EXECUTE : 0);
}

/* --------------------------------------------------------------------------------------------- */

/* The three permission boxes alone, for owner, group of the file, other and mask. */
static gboolean
acl_perm_dialog (const char *title, unsigned int *perm)
{
    gboolean r, w, x;
    int ret;

    acl_perm_checks (*perm, &r, &w, &x);

    {
        quick_widget_t quick_widgets[] = {
            // clang-format off
            QUICK_CHECKBOX (N_ ("&Read"), &r, NULL),
            QUICK_CHECKBOX (N_ ("&Write"), &w, NULL),
            QUICK_CHECKBOX (N_ ("E&xecute"), &x, NULL),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END,
            // clang-format on
        };

        WRect r_ = { -1, -1, 0, 44 };

        quick_dialog_t qdlg = {
            .rect = r_,
            .title = title,
            .help = "[Access control list]",
            .help_file = acl_help_file (),
            .widgets = quick_widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        ret = quick_dialog (&qdlg);
    }

    if (ret != B_ENTER)
        return FALSE;

    *perm = acl_perm_from_checks (r, w, x);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Add named entries, or change one.  Several names, separated with commas,
 * are taken only when adding: the first goes into @entry, the ids of the rest
 * into @more_ids.  On OK the names are checked; an unknown one keeps the
 * dialog open with what was typed.
 */
static gboolean
acl_entry_dialog (const char *title, aclm_entry_t *entry, gboolean is_new, GArray *more_ids)
{
    int type = entry->kind == ACLM_GROUP ? 1 : 0;
    char *name = NULL;
    gboolean r, w, x;
    const char *types[] = { N_ ("&User"), N_ ("&Group") };

    acl_perm_checks (entry->perm, &r, &w, &x);
    if (!is_new && aclm_is_named (entry->kind))
        name = acl_name_of (entry->kind, entry->id);
    if (name != NULL && name[0] == '#')
    {
        char *digits = g_strdup (name + 1);

        g_free (name);
        name = digits;
    }

    while (TRUE)
    {
        char *typed = NULL;
        int ret;
        gchar **names;
        const char *unknown = NULL;
        GArray *ids;
        int k;

        {
            quick_widget_t quick_widgets[] = {
                // clang-format off
                QUICK_RADIO (2, types, &type, &acl_entry_radio_id),
                QUICK_SEPARATOR (FALSE),
                QUICK_LABELED_INPUT (N_ ("Names:"), input_label_above,
                                     name != NULL ? name : "", "mc.acl.name", &typed,
                                     &acl_entry_input_id, FALSE, FALSE, INPUT_COMPLETE_NONE),
                QUICK_BUTTON (N_ ("C&hoose..."), ACL_B_CHOOSE, acl_entry_choose_cb, NULL),
                QUICK_SEPARATOR (FALSE),
                QUICK_CHECKBOX (N_ ("&Read"), &r, NULL),
                QUICK_CHECKBOX (N_ ("&Write"), &w, NULL),
                QUICK_CHECKBOX (N_ ("E&xecute"), &x, NULL),
                QUICK_BUTTONS_OK_CANCEL,
                QUICK_END,
                // clang-format on
            };

            WRect r_ = { -1, -1, 0, 64 };

            quick_dialog_t qdlg = {
                .rect = r_,
                .title = title,
                .help = "[Access control list]",
                .help_file = acl_help_file (),
                .widgets = quick_widgets,
                .callback = NULL,
                .mouse_callback = NULL,
            };

            ret = quick_dialog (&qdlg);
        }

        if (ret != B_ENTER)
        {
            g_free (typed);
            g_free (name);
            return FALSE;
        }

        g_free (name);
        name = typed;

        names = acl_split_names (name);
        ids = g_array_new (FALSE, FALSE, sizeof (unsigned int));

        for (k = 0; names[k] != NULL && unknown == NULL; k++)
        {
            unsigned int id;
            guint j;
            gboolean seen = FALSE;

            if (!acl_id_of (type == 1, names[k], &id))
            {
                unknown = names[k];
                break;
            }
            for (j = 0; j < ids->len; j++)
                seen = seen || g_array_index (ids, unsigned int, j) == id;
            if (!seen)
                g_array_append_val (ids, id);
        }

        if (unknown != NULL || ids->len == 0)
            message (D_ERROR, MSG_ERROR,
                     type == 1 ? _ ("There is no group \"%s\"") : _ ("There is no user \"%s\""),
                     unknown != NULL ? unknown : "");
        else if (!is_new && ids->len > 1)
            message (D_ERROR, MSG_ERROR, "%s",
                     _ ("An entry is changed one at a time: give one name."));
        else
        {
            guint j;

            entry->kind = type == 1 ? ACLM_GROUP : ACLM_USER;
            entry->id = g_array_index (ids, unsigned int, 0);
            entry->perm = acl_perm_from_checks (r, w, x);
            for (j = 1; more_ids != NULL && j < ids->len; j++)
                g_array_append_val (more_ids, g_array_index (ids, unsigned int, j));

            g_array_free (ids, TRUE);
            g_strfreev (names);
            g_free (name);
            return TRUE;
        }

        g_array_free (ids, TRUE);
        g_strfreev (names);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The rows of the table: every entry but the mask, which the header shows. */
static int
acl_row_count (void)
{
    aclm_list_t *list = acl_cur_list ();
    int n = 0;
    guint i;

    for (i = 0; i < list->entries->len; i++)
        if (g_array_index (list->entries, aclm_entry_t, i).kind != ACLM_MASK)
            n++;
    return n;
}

/* --------------------------------------------------------------------------------------------- */

static aclm_entry_t *
acl_row_entry (int row)
{
    static const aclm_kind_t base[] = { ACLM_OWNER, ACLM_GROUP_OBJ, ACLM_OTHER };
    aclm_list_t *list = acl_cur_list ();
    guint i;

    /* The file's own entries precede the named users and groups. */
    for (i = 0; i < G_N_ELEMENTS (base); i++)
    {
        aclm_entry_t *e = aclm_list_find (list, base[i], 0);

        if (e != NULL && row-- == 0)
            return e;
    }

    for (i = 0; i < list->entries->len; i++)
    {
        aclm_entry_t *e = &g_array_index (list->entries, aclm_entry_t, i);

        if (!aclm_is_named (e->kind))
            continue;
        if (row-- == 0)
            return e;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static unsigned int
acl_col_bit (int col)
{
    return col == 2 ? ACL_PERM_READ : col == 3 ? ACL_PERM_WRITE : col == 4 ? ACL_PERM_EXECUTE : 0;
}

/* --------------------------------------------------------------------------------------------- */

static char *
acl_mark_key (const aclm_entry_t *e)
{
    return g_strdup_printf ("%d:%d:%u", (int) acl_type, (int) e->kind, e->id);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
acl_is_marked (const aclm_entry_t *e)
{
    char *key = acl_mark_key (e);
    gboolean ret;

    ret = g_hash_table_contains (acl_marks, key);
    g_free (key);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
acl_marked_count (void)
{
    int row, n = 0, rows = acl_row_count ();

    for (row = 0; row < rows; row++)
        if (acl_is_marked (acl_row_entry (row)))
            n++;
    return n;
}

/* --------------------------------------------------------------------------------------------- */

/* A box switched on a marked row is switched the same way on every marked row. */
static void
acl_set_bit (aclm_entry_t *cur, unsigned int bit, gboolean val)
{
    int row, rows = acl_row_count ();

    for (row = 0; row < rows; row++)
    {
        aclm_entry_t *e = acl_row_entry (row);

        if (e == cur || (acl_is_marked (cur) && acl_is_marked (e)))
        {
            if (val)
                e->perm |= bit;
            else
                e->perm &= ~bit;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
acl_ds_nrows (const void *data)
{
    (void) data;
    return acl_row_count ();
}

/* --------------------------------------------------------------------------------------------- */

static const char *
acl_ds_text (const void *data, int row, int col)
{
    static char buf[256];
    aclm_entry_t *e = acl_row_entry (row);
    char *name;

    (void) data;
    buf[0] = '\0';
    if (e == NULL)
        return buf;

    switch (col)
    {
    case 0:
        g_snprintf (buf, sizeof (buf), "%c%s", acl_is_marked (e) ? '*' : ' ',
                    acl_kind_label (e->kind));
        break;
    case 1:
        /* The owner and the group of the file are named on the access list only;
           on the default list they stand for whoever creates the new file. */
        if (e->kind == ACLM_OWNER && acl_type == ACLM_ACCESS)
            name = acl_name_of (ACLM_OWNER, (unsigned int) acl_st->owner);
        else if (e->kind == ACLM_GROUP_OBJ && acl_type == ACLM_ACCESS)
            name = acl_name_of (ACLM_GROUP_OBJ, (unsigned int) acl_st->group);
        else if (aclm_is_named (e->kind))
            name = acl_name_of (e->kind, e->id);
        else
            name = g_strdup ("");
        g_strlcpy (buf, name, sizeof (buf));
        g_free (name);
        break;
    case 5:
    {
        unsigned int eff = aclm_effective (acl_cur_list (), e);
        char effs[4];

        aclm_perm_to_str (eff, effs);
        g_snprintf (buf, sizeof (buf), "%s%s", effs, eff != e->perm ? _ ("  masked") : "");
        break;
    }
    default:
        break;
    }

    return buf;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
acl_ds_checked (const void *data, int row, int col)
{
    aclm_entry_t *e = acl_row_entry (row);

    (void) data;
    return e != NULL && (e->perm & acl_col_bit (col)) != 0;
}

/* --------------------------------------------------------------------------------------------- */

static void acl_changed (void);

static void
acl_ds_set_checked (void *data, int row, int col, gboolean val)
{
    aclm_entry_t *e = acl_row_entry (row);
    unsigned int bit = acl_col_bit (col);

    (void) data;
    if (e == NULL || bit == 0 || acl_st->read_only)
        return;

    acl_set_bit (e, bit, val);
    acl_changed ();
}

/* --------------------------------------------------------------------------------------------- */

/* A right the mask takes away shows in the hot color. */
static int
acl_ds_cell_color (void *data, int row, int col)
{
    aclm_entry_t *e = acl_row_entry (row);
    unsigned int eff;

    (void) data;
    if (e == NULL)
        return -1;

    eff = aclm_effective (acl_cur_list (), e);
    if (eff == e->perm)
        return -1;
    if (col == 5
        || (acl_col_bit (col) != 0 && (e->perm & acl_col_bit (col)) != 0
            && (eff & acl_col_bit (col)) == 0))
        return dialog_colors[DLG_COLOR_HOT_NORMAL];
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_update_hint (void)
{
    const char *text = "";

    if (acl_st->read_only)
        text = _ ("Read only: you are not the owner.");
    else if (acl_type == ACLM_DEFAULT && aclm_list_is_empty (acl_cur_list ()))
        text = _ ("None: new files get the mode from umask.");

    label_set_text (acl_hint, text);
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_update_mask_button (void)
{
    const aclm_entry_t *mask = aclm_list_find (acl_cur_list (), ACLM_MASK, 0);
    char perm[4];
    char *text;

    if (mask != NULL)
    {
        aclm_perm_to_str (mask->perm, perm);
        text = g_strdup_printf ("%s %s", _ ("Mas&k"), perm);
    }
    else
        text = g_strdup_printf ("%s %s", _ ("Mas&k"), "---");
    button_set_text (acl_mask_btn, text);
    g_free (text);
}

/* --------------------------------------------------------------------------------------------- */

/* Refreshing the dialog must not change an existing mask. */
static void
acl_refresh (void)
{
    int n;

    n = acl_row_count ();
    if (n > 0)
        table_set_current (acl_table, MIN (MAX (acl_table->current, 0), n - 1));

    acl_update_mask_button ();
    acl_update_hint ();
    widget_draw (WIDGET (WIDGET (acl_table)->owner));
}

/* --------------------------------------------------------------------------------------------- */

/* After an edit, update the mask according to the selected policy. */
static void
acl_changed (void)
{
    aclm_list_t *list = acl_cur_list ();

    if (!acl_st->read_only && !aclm_list_is_empty (list))
    {
        if (acl_st->recalc_mask)
            aclm_calc_mask (list);
        else
            aclm_drop_unused_mask (list);
    }
    acl_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_select_entry (aclm_kind_t kind, unsigned int id)
{
    int row, n = acl_row_count ();

    for (row = 0; row < n; row++)
    {
        const aclm_entry_t *e = acl_row_entry (row);

        if (e->kind == kind && e->id == id)
        {
            table_set_current (acl_table, row);
            break;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static aclm_entry_t *
acl_current_entry (void)
{
    return acl_row_entry (acl_table->current);
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_do_add (void)
{
    aclm_entry_t e = { ACLM_USER, 0, ACL_PERM_READ };
    aclm_list_t *list = acl_cur_list ();
    GArray *more = g_array_new (FALSE, FALSE, sizeof (unsigned int));
    guint j;

    if (!acl_entry_dialog (_ ("Add entry"), &e, TRUE, more))
    {
        g_array_free (more, TRUE);
        return;
    }

    if (acl_type == ACLM_DEFAULT && aclm_list_is_empty (list))
        aclm_list_base_from (list, acl_st->lists[ACLM_ACCESS]);

    aclm_list_set (list, e.kind, e.id, e.perm);
    for (j = 0; j < more->len; j++)
        aclm_list_set (list, e.kind, g_array_index (more, unsigned int, j), e.perm);
    g_array_free (more, TRUE);
    acl_changed ();
    acl_select_entry (e.kind, e.id);
    widget_draw (WIDGET (acl_table));
}

/* --------------------------------------------------------------------------------------------- */

static const char *
acl_base_title (aclm_kind_t kind)
{
    switch (kind)
    {
    case ACLM_OWNER:
        return _ ("Owner");
    case ACLM_GROUP_OBJ:
        return _ ("Group");
    case ACLM_MASK:
        return _ ("Mask");
    case ACLM_OTHER:
    default:
        return _ ("Other");
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_do_edit (void)
{
    aclm_entry_t *cur = acl_current_entry ();
    aclm_entry_t e;

    if (cur == NULL)
        return;
    e = *cur;

    if (!aclm_is_named (e.kind))
    {
        if (!acl_perm_dialog (acl_base_title (e.kind), &e.perm))
            return;
        aclm_list_set (acl_cur_list (), e.kind, e.id, e.perm);
    }
    else
    {
        aclm_kind_t old_kind = e.kind;
        unsigned int old_id = e.id;

        if (!acl_entry_dialog (_ ("Edit entry"), &e, FALSE, NULL))
            return;
        if (e.kind != old_kind || e.id != old_id)
            aclm_list_remove (acl_cur_list (), old_kind, old_id);
        aclm_list_set (acl_cur_list (), e.kind, e.id, e.perm);
    }

    acl_changed ();
    acl_select_entry (e.kind, e.id);
    widget_draw (WIDGET (acl_table));
}

/* --------------------------------------------------------------------------------------------- */

/* Remove the marked named entries from the current list. */
static void
acl_do_delete_marked (void)
{
    aclm_list_t *list = acl_cur_list ();
    GArray *gone = g_array_new (FALSE, FALSE, sizeof (aclm_entry_t));
    int row, rows = acl_row_count ();
    guint i;

    for (row = 0; row < rows; row++)
    {
        aclm_entry_t *e = acl_row_entry (row);

        if (acl_is_marked (e) && aclm_is_named (e->kind))
            g_array_append_val (gone, *e);
    }

    if (gone->len == 0)
        message (D_ERROR, MSG_ERROR, "%s",
                 _ ("The owner, the group of the file and other cannot be removed."));
    else if (gone->len == 1
             || query_dialog (_ ("Delete"), _ ("Remove the marked users and groups?"), D_NORMAL, 2,
                              _ ("&Yes"), _ ("&No"))
                 == 0)
    {
        for (i = 0; i < gone->len; i++)
        {
            aclm_entry_t *e = &g_array_index (gone, aclm_entry_t, i);
            char *key = acl_mark_key (e);

            g_hash_table_remove (acl_marks, key);
            g_free (key);
            aclm_list_remove (list, e->kind, e->id);
        }
        acl_changed ();
    }

    g_array_free (gone, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_do_delete (void)
{
    aclm_list_t *list = acl_cur_list ();
    aclm_entry_t *cur = acl_current_entry ();

    if (cur == NULL)
        return;

    if (acl_marked_count () > 0)
    {
        acl_do_delete_marked ();
        return;
    }

    if (aclm_is_named (cur->kind))
        aclm_list_remove (list, cur->kind, cur->id);
    else if (cur->kind == ACLM_MASK)
    {
        if (aclm_has_named (list))
        {
            message (D_ERROR, MSG_ERROR, "%s",
                     _ ("Remove the user and group entries before removing the mask."));
            return;
        }
        aclm_list_remove (list, ACLM_MASK, 0);
    }
    else if (acl_type == ACLM_ACCESS)
    {
        message (D_ERROR, MSG_ERROR, "%s",
                 _ ("The owner, the group of the file and other cannot be removed."));
        return;
    }
    else if (aclm_has_named (list))
    {
        message (D_ERROR, MSG_ERROR, "%s",
                 _ ("Remove the user and group entries of the default ACL first."));
        return;
    }
    else
    {
        if (query_dialog (_ ("Default ACL"), _ ("Remove the whole default ACL?"), D_NORMAL, 2,
                          _ ("&Yes"), _ ("&No"))
            != 0)
            return;
        g_array_set_size (list->entries, 0);
    }

    acl_changed ();
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_do_toggle (unsigned int bit)
{
    aclm_entry_t *cur = acl_current_entry ();

    if (cur == NULL)
        return;

    acl_set_bit (cur, bit, (cur->perm & bit) == 0);
    acl_changed ();
}

/* --------------------------------------------------------------------------------------------- */

/* Ins marks the entry under the cursor and moves down, as in the panels. */
static void
acl_do_mark (void)
{
    aclm_entry_t *cur = acl_current_entry ();
    char *key;

    if (cur == NULL)
        return;

    key = acl_mark_key (cur);
    if (!g_hash_table_remove (acl_marks, key))
        g_hash_table_add (acl_marks, key);
    else
        g_free (key);

    if (acl_table->current < acl_row_count () - 1)
        table_set_current (acl_table, acl_table->current + 1);
    widget_draw (WIDGET (acl_table));
}

/* --------------------------------------------------------------------------------------------- */

/* The mask: the most that the users, the groups and the group of the file get. */
static void
acl_do_mask (void)
{
    aclm_list_t *list = acl_cur_list ();
    aclm_entry_t *mask = aclm_list_find (list, ACLM_MASK, 0);
    unsigned int perm;
    gboolean r, w, x, recalc = acl_st->recalc_mask;

    if (mask == NULL)
    {
        message (D_NORMAL, _ ("Mask"), "%s",
                 _ ("There is no mask yet: it comes with the first user or group entry."));
        return;
    }

    perm = mask->perm;
    r = (perm & ACL_PERM_READ) != 0;
    w = (perm & ACL_PERM_WRITE) != 0;
    x = (perm & ACL_PERM_EXECUTE) != 0;

    {
        quick_widget_t quick_widgets[] = {
            // clang-format off
            QUICK_LABEL (N_ ("The most that users and groups get:"), NULL),
            QUICK_CHECKBOX (N_ ("&Read"), &r, NULL),
            QUICK_CHECKBOX (N_ ("&Write"), &w, NULL),
            QUICK_CHECKBOX (N_ ("E&xecute"), &x, NULL),
            QUICK_SEPARATOR (TRUE),
            QUICK_CHECKBOX (N_ ("&Set it from the entries"), &recalc, NULL),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END,
            // clang-format on
        };
        WRect rr = { -1, -1, 0, 48 };
        quick_dialog_t qdlg = {
            .rect = rr,
            .title = _ ("Mask"),
            .help = "[Access control list]",
            .help_file = acl_help_file (),
            .widgets = quick_widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        if (quick_dialog (&qdlg) != B_ENTER)
            return;
    }

    /* boxes changed by hand mean a mask set by hand */
    if (r != ((perm & ACL_PERM_READ) != 0) || w != ((perm & ACL_PERM_WRITE) != 0)
        || x != ((perm & ACL_PERM_EXECUTE) != 0))
        recalc = FALSE;

    acl_st->recalc_mask = recalc;
    if (!recalc)
    {
        perm = (r ? ACL_PERM_READ : 0) | (w ? ACL_PERM_WRITE : 0) | (x ? ACL_PERM_EXECUTE : 0);
        aclm_list_set (list, ACLM_MASK, 0, perm);
    }
    acl_changed ();
}

/* --------------------------------------------------------------------------------------------- */

static int
acl_list_button_cb (WButton *button, int action)
{
    (void) button;

    switch (action)
    {
    case ACL_B_ADD:
        acl_do_add ();
        break;
    case ACL_B_EDIT:
        acl_do_edit ();
        break;
    case ACL_B_DELETE:
        acl_do_delete ();
        break;
    case ACL_B_MASK:
        acl_do_mask ();
        break;
    default:
        break;
    }

    /* a hotkey selects its button after this; the table takes the focus back
       once the key is done with */
    widget_select (WIDGET (acl_table));
    acl_back_to_table = TRUE;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
acl_dialog_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_POST_KEY:
        if (acl_back_to_table)
        {
            acl_back_to_table = FALSE;
            widget_select (WIDGET (acl_table));
        }
        return MSG_HANDLED;

    case MSG_KEY:
        if (acl_st->read_only)
            return MSG_NOT_HANDLED;

        /* The dialog has no input line, so these keys mean the table wherever
           the focus is. */
        switch (parm)
        {
        case KEY_IC:
            acl_do_mark ();
            return MSG_HANDLED;
        case KEY_F (4):
            acl_do_edit ();
            return MSG_HANDLED;
        case KEY_DC:
        case KEY_F (8):
            acl_do_delete ();
            return MSG_HANDLED;
        default:
            break;
        }

        /* the letters only in the table: elsewhere they press buttons */
        if (GROUP (w)->current != NULL && GROUP (w)->current->data == (void *) acl_table)
        {
            switch (parm)
            {
            case 'r':
                acl_do_toggle (ACL_PERM_READ);
                return MSG_HANDLED;
            case 'w':
                acl_do_toggle (ACL_PERM_WRITE);
                return MSG_HANDLED;
            case 'x':
                acl_do_toggle (ACL_PERM_EXECUTE);
                return MSG_HANDLED;
            default:
                break;
            }
        }
        return MSG_NOT_HANDLED;

    case MSG_NOTIFY:
        if (sender == WIDGET (acl_table))
        {
            /* the table asks its owner what Enter means */
            if (parm == CK_Enter && !acl_st->read_only)
            {
                acl_do_edit ();
                return MSG_HANDLED;
            }
            return MSG_NOT_HANDLED;
        }
        if (acl_radio != NULL && sender == WIDGET (acl_radio))
        {
            acl_type = acl_radio->sel == 1 ? ACLM_DEFAULT : ACLM_ACCESS;
            acl_table->current = 0;
            acl_table->top = 0;
            acl_refresh ();
            /* back to the table once the key is done with: the hotkey that
               switched the list selects the radio buttons after this */
            acl_back_to_table = TRUE;
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
acl_buttons_row (WGroup *g, int y, int cols, WButton **b, int n)
{
    int total = 0, x, i;

    for (i = 0; i < n; i++)
        total += button_get_width (b[i]) + (i > 0 ? 1 : 0);

    x = MAX (2, (cols - total) / 2);
    for (i = 0; i < n; i++)
    {
        WIDGET (b[i])->rect.x = x;
        WIDGET (b[i])->rect.y = y;
        group_add_widget (g, b[i]);
        x += button_get_width (b[i]) + 1;
    }

    return x;
}

/*** public functions ****************************************************************************/

acl_dlg_result_t
acl_dialog_run (acl_dlg_state_t *st)
{
    static table_column_def_t col_defs[] = {
        { 7, J_LEFT, TABLE_COL_TEXT },  { 0, J_LEFT, TABLE_COL_TEXT },
        { 4, J_LEFT, TABLE_COL_CHECK }, { 5, J_LEFT, TABLE_COL_CHECK },
        { 4, J_LEFT, TABLE_COL_CHECK }, { 10, J_LEFT, TABLE_COL_TEXT },
    };
    const char *titles[7];
    char *type_title;
    WDialog *dlg;
    WGroup *g;
    int cols, lines, table_lines, y, x, ret, n;
    char mode[10];
    char *text;
    WButton *row[4];
    acl_dlg_result_t result;
    table_datasource_t ds;

    acl_st = st;
    acl_marks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    acl_type = ACLM_ACCESS;
    acl_radio = NULL;

    cols = MIN (80, COLS - 4);
    n = (int) st->lists[ACLM_ACCESS]->entries->len;
    if (st->is_dir && st->lists[ACLM_DEFAULT] != NULL)
        n = MAX (n, (int) st->lists[ACLM_DEFAULT]->entries->len);
    /* the header row and a spare row for what is added */
    table_lines = MAX (5, MIN (n + 2, LINES - 20));
    /* file, path, owner, [list x2], line, table, line, buttons, line, buttons,
       and the frame around them */
    lines = table_lines + (st->is_dir ? 14 : 12);

    dlg = dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors,
                      acl_dialog_callback, NULL, "[Access control list]",
                      st->read_only ? _ ("Access control list (read only)")
                                    : _ ("Access control list"));
    dlg->help_file = acl_help_file ();
    g = GROUP (dlg);

    /* the header: file and mode, the path, owner and group */
    y = 2;
    aclm_perm_to_str ((st->mode >> 6) & 7, mode);
    aclm_perm_to_str ((st->mode >> 3) & 7, mode + 3);
    aclm_perm_to_str (st->mode & 7, mode + 6);

    acl_mask_btn = button_new (y, 0, ACL_B_MASK, NARROW_BUTTON, "Mask ---", acl_list_button_cb);
    WIDGET (acl_mask_btn)->rect.x = cols - 4 - button_get_width (acl_mask_btn);
    text = g_strdup_printf ("%s %s", _ ("Mode"), mode);
    group_add_widget (
        g, label_new (y, WIDGET (acl_mask_btn)->rect.x - 2 - str_term_width1 (text), text));
    g_free (text);
    group_add_widget (g, acl_mask_btn);
    if (st->read_only)
        widget_disable (WIDGET (acl_mask_btn), TRUE);
    if (st->count > 1)
        text = g_strdup_printf ("%s  (%d of %d)", _ ("File"), st->index, st->count);
    else
        text = g_strdup (_ ("File"));
    group_add_widget (g, label_new (y++, 3, text));
    g_free (text);

    group_add_widget (g,
                      label_new (y++, 3, str_fit_to_term (st->shown_path, cols - 6, J_LEFT_FIT)));

    {
        char *owner = acl_name_of (ACLM_OWNER, (unsigned int) st->owner);
        char *grp = acl_name_of (ACLM_GROUP_OBJ, (unsigned int) st->group);

        text = g_strdup_printf ("%s %s", _ ("Owner"), owner);
        group_add_widget (g, label_new (y, 3, text));
        g_free (text);
        text = g_strdup_printf ("%s %s", _ ("Group"), grp);
        group_add_widget (g, label_new (y++, cols / 2, text));
        g_free (text);
        g_free (owner);
        g_free (grp);
    }

    if (st->is_dir)
    {
        static const char *types[] = { N_ ("This fol&der"), N_ ("&New files inside") };
        static gboolean translated = FALSE;

        if (!translated)
        {
            types[0] = _ (types[0]);
            types[1] = _ (types[1]);
            translated = TRUE;
        }

        group_add_widget (g, label_new (y, 3, _ ("List")));
        acl_radio = radio_new (y, 9, 2, types);
        group_add_widget (g, acl_radio);
        y += 2;
    }

    group_add_widget (g, hline_new (y++, -1, -1));

    /* the table */
    acl_table = table_new (y, 3, table_lines, cols - 4, (int) G_N_ELEMENTS (col_defs), col_defs);
    /* the rows have the Ins mark before the type */
    type_title = g_strconcat (" ", _ ("Type"), (char *) NULL);
    titles[0] = type_title;
    titles[1] = _ ("Name");
    titles[2] = _ ("Read");
    titles[3] = _ ("Write");
    titles[4] = _ ("Exec");
    titles[5] = _ ("Effective");
    titles[6] = NULL;
    table_set_header (acl_table, titles);
    g_free (type_title);
    table_set_column_sizing (acl_table, 1, 16, TRUE);
    ds.get_nrows = acl_ds_nrows;
    ds.get_text = acl_ds_text;
    ds.get_checked = acl_ds_checked;
    ds.set_checked = acl_ds_set_checked;
    ds.data = NULL;
    ds.cycle_choice = NULL;
    table_set_datasource (acl_table, ds);
    table_set_cell_color (acl_table, acl_ds_cell_color);
    table_set_check_cursor (acl_table, TRUE);
    acl_table->scrollbar = TRUE;
    acl_table->scrollbar_on_frame = TRUE;
    group_add_widget (g, acl_table);
    table_layout (acl_table);
    y += table_lines;

    group_add_widget (g, hline_new (y++, -1, -1));

    /* what is done to the entries */
    x = 3;
    row[0] = button_new (y, x, ACL_B_ADD, NORMAL_BUTTON, _ ("&Add"), acl_list_button_cb);
    row[1] = button_new (y, x, ACL_B_EDIT, NORMAL_BUTTON, _ ("Ed&it"), acl_list_button_cb);
    row[2] = button_new (y, x, ACL_B_DELETE, NORMAL_BUTTON, _ ("De&lete"), acl_list_button_cb);
    {
        int i;

        for (i = 0; i < 3; i++)
        {
            WIDGET (row[i])->rect.x = x;
            group_add_widget (g, row[i]);
            if (st->read_only)
                widget_disable (WIDGET (row[i]), TRUE);
            x += button_get_width (row[i]) + 2;
        }
    }
    acl_hint = label_new (y++, x + 1, "");
    group_add_widget (g, acl_hint);

    group_add_widget (g, hline_new (y++, -1, -1));

    n = 0;
    row[n++] = button_new (y, 0, B_ENTER, DEFPUSH_BUTTON, _ ("&Set"), NULL);
    if (st->count > 1 && st->index < st->count)
    {
        row[n++] = button_new (y, 0, ACL_B_SET_ALL, NORMAL_BUTTON, _ ("Set a&ll"), NULL);
        row[n++] =
            button_new (y, 0, ACL_B_CHANGES_TO_ALL, NORMAL_BUTTON, _ ("C&hanges to all"), NULL);
    }
    row[n++] = button_new (y, 0, B_CANCEL, NORMAL_BUTTON, _ ("&Cancel"), NULL);
    if (st->read_only)
    {
        int i;

        for (i = 0; i < n - 1; i++)
            widget_disable (WIDGET (row[i]), TRUE);
    }
    (void) acl_buttons_row (g, y, cols, row, n);

    acl_refresh ();
    widget_select (WIDGET (acl_table));

    ret = dlg_run (dlg);

    switch (ret)
    {
    case B_ENTER:
        result = ACL_DLG_SET;
        break;
    case ACL_B_SET_ALL:
        result = ACL_DLG_SET_ALL;
        break;
    case ACL_B_CHANGES_TO_ALL:
        result = ACL_DLG_CHANGES_TO_ALL;
        break;
    default:
        result = ACL_DLG_CANCEL;
        break;
    }

    widget_destroy (WIDGET (dlg));
    g_hash_table_destroy (acl_marks);
    acl_marks = NULL;
    acl_st = NULL;
    acl_table = NULL;
    acl_hint = NULL;
    acl_mask_btn = NULL;
    acl_radio = NULL;

    return result;
}
