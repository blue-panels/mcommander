/*
   Panel plugin acl for the M-Commander
   The dialog for Windows access control lists on cifs and NTFS mounts

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

#include <string.h>

#include "lib/global.h"
#include "lib/tty/key.h"
#include "lib/strutil.h"
#include "lib/widget.h"

#include "acl-dialog.h"
#include "acl-nt.h"

/*** file scope macro definitions ****************************************************************/

#define NT_B_ADD      (B_USER + 1)
#define NT_B_EDIT     (B_USER + 2)
#define NT_B_DELETE   (B_USER + 3)
#define NT_B_CHOOSE   (B_USER + 20)
#define NT_B_ADVANCED (B_USER + 21)

#define NT_WHO_COLUMN 30

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* "Applies to" of a directory and the inheritance flags it stands for */
static const guint8 nt_scope_flags[] = {
    0,
    ACL_NT_OBJECT_INHERIT | ACL_NT_CONTAINER_INHERIT,
    ACL_NT_CONTAINER_INHERIT,
    ACL_NT_OBJECT_INHERIT,
    ACL_NT_OBJECT_INHERIT | ACL_NT_CONTAINER_INHERIT | ACL_NT_INHERIT_ONLY,
    ACL_NT_CONTAINER_INHERIT | ACL_NT_INHERIT_ONLY,
    ACL_NT_OBJECT_INHERIT | ACL_NT_INHERIT_ONLY,
};
static const char *nt_scope_names[] = { N_ ("This folder only"),
                                        N_ ("This folder, subfolders and files"),
                                        N_ ("This folder and subfolders"),
                                        N_ ("This folder and files"),
                                        N_ ("Subfolders and files only"),
                                        N_ ("Subfolders only"),
                                        N_ ("Files only") };

/* the one dialog open at a time */
static acl_nt_sd_t *nt_sd = NULL;
static const char *nt_attr = NULL;
static gboolean nt_is_dir = FALSE;
static WListbox *nt_list = NULL;

/* the entry dialog, for its buttons */
static guint32 nt_edit_mask;
static unsigned long nt_rights_id;
static unsigned long nt_account_id;
static unsigned long nt_name_id;

/*** file scope functions ************************************************************************/

static int
nt_scope_index (guint8 flags)
{
    guint8 f = flags & (ACL_NT_OBJECT_INHERIT | ACL_NT_CONTAINER_INHERIT | ACL_NT_INHERIT_ONLY);
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (nt_scope_flags); i++)
        if (nt_scope_flags[i] == f)
            return (int) i;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
nt_refresh (void)
{
    int keep = nt_list->current;
    guint i;

    listbox_remove_list (nt_list);

    for (i = 0; i < nt_sd->aces->len; i++)
    {
        const acl_nt_ace_t *ace = g_ptr_array_index (nt_sd->aces, i);
        const char *preset = acl_nt_rights_preset (ace->mask);
        char *who = acl_nt_sid_name (ace->sid);
        char *text;

        text =
            g_strdup_printf ("%-6s %s %s%s%s",
                             ace->type == ACL_NT_ACE_ALLOWED      ? _ ("allow")
                                 : ace->type == ACL_NT_ACE_DENIED ? _ ("deny")
                                                                  : _ ("other"),
                             str_fit_to_term (who, NT_WHO_COLUMN, J_LEFT_FIT),
                             preset != NULL ? _ (preset) : _ ("Special"),
                             (ace->flags & ACL_NT_INHERIT_ONLY) != 0 ? _ ("  (inside only)") : "",
                             (ace->flags & ACL_NT_INHERITED) != 0 ? _ ("  (inherited)") : "");
        listbox_add_item (nt_list, LISTBOX_APPEND_AT_END, 0, text, NULL, FALSE);
        if (ace->type == ACL_NT_ACE_DENIED)
            listbox_set_emphasis (nt_list, (int) i, TRUE);
        g_free (text);
        g_free (who);
    }

    if (nt_sd->aces->len != 0)
        listbox_set_current (nt_list, MIN (MAX (keep, 0), (int) nt_sd->aces->len - 1));
    widget_draw (WIDGET (WIDGET (nt_list)->owner));
}

/* --------------------------------------------------------------------------------------------- */

static acl_nt_ace_t *
nt_current (void)
{
    if (nt_list->current < 0 || (guint) nt_list->current >= nt_sd->aces->len)
        return NULL;
    return g_ptr_array_index (nt_sd->aces, nt_list->current);
}

/* --------------------------------------------------------------------------------------------- */

/* What an entry grants, one right a line; message() centers every line, so all
   lines are made one width to keep the block left aligned. */
static void
nt_show_entry (void)
{
    const acl_nt_ace_t *ace = nt_current ();
    GString *text;
    GPtrArray *rights;
    gchar **lines;
    GString *out;
    char *who;
    int width = 0, k;
    guint i;

    if (ace == NULL)
        return;

    who = acl_nt_sid_name (ace->sid);
    text = g_string_new (NULL);
    g_string_append_printf (text, "%s %s\n%s %s\n%s %s\n", _ ("Who:"), who, _ ("SID:"),
                            ace->sid != NULL ? ace->sid : "-", _ ("Type:"),
                            ace->type == ACL_NT_ACE_ALLOWED      ? _ ("Allow")
                                : ace->type == ACL_NT_ACE_DENIED ? _ ("Deny")
                                                                 : _ ("Other"));
    if (nt_is_dir)
        g_string_append_printf (text, "%s %s\n", _ ("Applies to:"),
                                _ (acl_nt_applies_to (ace->flags)));
    g_string_append_printf (text, "%s %s\n%s 0x%08x\n\n", _ ("Inherited:"),
                            (ace->flags & ACL_NT_INHERITED) != 0 ? _ ("yes") : _ ("no"),
                            _ ("Mask:"), (unsigned int) ace->mask);

    rights = acl_nt_rights_list (ace->mask, nt_is_dir);
    for (i = 0; i < rights->len; i++)
        g_string_append_printf (text, "  %s\n", _ ((const char *) g_ptr_array_index (rights, i)));
    if (rights->len == 0)
        g_string_append_printf (text, "  %s\n", _ ("no rights"));
    g_ptr_array_free (rights, TRUE);

    lines = g_strsplit (text->str, "\n", -1);
    out = g_string_new (NULL);
    for (k = 0; lines[k] != NULL; k++)
        width = MAX (width, str_term_width1 (lines[k]));
    for (k = 0; lines[k] != NULL; k++)
    {
        if (lines[k + 1] == NULL && lines[k][0] == '\0')
            break;
        g_string_append_printf (out, "%s%*s%s", lines[k], width - str_term_width1 (lines[k]), "",
                                lines[k + 1] != NULL ? "\n" : "");
    }
    message (D_NORMAL, _ ("Entry"), "%s", out->str);

    g_string_free (out, TRUE);
    g_strfreev (lines);
    g_string_free (text, TRUE);
    g_free (who);
}

/* --------------------------------------------------------------------------------------------- */

/* All the rights one box each; the result goes to the Special radio button. */
static int
nt_advanced_cb (WButton *button, int action)
{
    Widget *owner = WIDGET (WIDGET (button)->owner);
    gboolean on[G_N_ELEMENTS (acl_nt_rights)];
    quick_widget_t quick_widgets[G_N_ELEMENTS (acl_nt_rights) + 4];
    guint32 mask = nt_edit_mask;
    size_t i, n = 0;
    Widget *radio;

    (void) action;

    /* a preset shows as its rights */
    if (nt_rights_id != 0)
    {
        radio = widget_find_by_id (owner, nt_rights_id);
        if (radio != NULL && RADIO (radio)->sel < ACL_NT_PRESET_COUNT)
            mask = acl_nt_presets[RADIO (radio)->sel].mask;
    }

    mask = acl_nt_map_generic (mask);
    for (i = 0; i < G_N_ELEMENTS (acl_nt_rights); i++)
    {
        const char *label = nt_is_dir ? acl_nt_rights[i].dir : acl_nt_rights[i].file;
        quick_widget_t box = QUICK_CHECKBOX (label, &on[i], NULL);

        on[i] = (mask & acl_nt_rights[i].bit) != 0;
        quick_widgets[n++] = box;
    }
    {
        quick_widget_t tail[] = { QUICK_BUTTONS_OK_CANCEL, QUICK_END };

        for (i = 0; i < G_N_ELEMENTS (tail); i++)
            quick_widgets[n++] = tail[i];
    }

    {
        WRect r = { -1, -1, 0, 50 };
        quick_dialog_t qdlg = {
            .rect = r,
            .title = _ ("Advanced rights"),
            .help = "[Access control list]",
            .help_file = acl_help_file (),
            .widgets = quick_widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        if (quick_dialog (&qdlg) != B_ENTER)
            return 0;
    }

    /* keep what the boxes do not show, such as SYNCHRONIZE */
    for (i = 0; i < G_N_ELEMENTS (acl_nt_rights); i++)
    {
        mask &= ~acl_nt_rights[i].bit;
        if (on[i])
            mask |= acl_nt_rights[i].bit;
    }
    nt_edit_mask = mask | ACL_NT_SYNCHRONIZE;

    radio = widget_find_by_id (owner, nt_rights_id);
    if (radio != NULL)
    {
        RADIO (radio)->sel = acl_nt_preset_index (nt_edit_mask);
        RADIO (radio)->pos = RADIO (radio)->sel;
        widget_draw (radio);
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
nt_choose_cb (WButton *button, int action)
{
    Widget *owner = WIDGET (WIDGET (button)->owner);
    Widget *account, *input;
    char *name;

    (void) action;

    account = widget_find_by_id (owner, nt_account_id);
    input = widget_find_by_id (owner, nt_name_id);
    if (account == NULL || input == NULL)
        return 0;

    name = acl_choose (RADIO (account)->sel == 1, input_get_ctext (INPUT (input)));
    if (name != NULL)
    {
        input_assign_text (INPUT (input), name);
        g_free (name);
    }
    widget_select (input);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Add entries, or change one.  Several names separated with commas are taken
 * when adding, all with the same rights; @sids gets their SIDs.
 */
static gboolean
nt_entry_dialog (const char *title, acl_nt_ace_t *ace, gboolean is_new, GPtrArray *sids)
{
    int type = ace->type == ACL_NT_ACE_DENIED ? 1 : 0;
    int account = 0;
    int rights = acl_nt_preset_index (ace->mask);
    int scope = nt_scope_index (ace->flags);
    char *name = NULL;
    const char *types[] = { N_ ("A&llow"), N_ ("De&ny") };
    const char *accounts[] = { N_ ("&User"), N_ ("&Group") };
    const char *nt_preset_names[ACL_NT_PRESET_COUNT + 1];
    int i;

    for (i = 0; i < ACL_NT_PRESET_COUNT; i++)
        nt_preset_names[i] = acl_nt_presets[i].label;
    nt_preset_names[ACL_NT_PRESET_COUNT] = N_ ("&Special");

    nt_edit_mask = ace->mask;
    if (!is_new)
    {
        name = acl_nt_sid_name (ace->sid);
        account = g_str_has_prefix (name, "Unix Group\\") ? 1 : 0;
    }

    while (TRUE)
    {
        char *typed = NULL;
        int ret;
        gchar **names;
        const char *unknown = NULL;
        int k;

        nt_rights_id = 0;

        {
            quick_widget_t dir_widgets[] = {
                // clang-format off
                QUICK_START_COLUMNS,
                    QUICK_RADIO (2, types, &type, NULL),
                    QUICK_SEPARATOR (FALSE),
                    QUICK_RADIO (2, accounts, &account, &nt_account_id),
                    QUICK_SEPARATOR (FALSE),
                    QUICK_RADIO (7, nt_scope_names, &scope, NULL),
                QUICK_NEXT_COLUMN,
                    QUICK_RADIO (ACL_NT_PRESET_COUNT + 1, nt_preset_names, &rights, &nt_rights_id),
                    QUICK_SEPARATOR (FALSE),
                    QUICK_BUTTON (N_ ("Ad&vanced..."), NT_B_ADVANCED, nt_advanced_cb, NULL),
                QUICK_STOP_COLUMNS,
                QUICK_SEPARATOR (FALSE),
                QUICK_LABELED_INPUT (N_ ("Names:"), input_label_above, name != NULL ? name : "",
                                     "mc.acl.name", &typed, &nt_name_id, FALSE, FALSE,
                                     INPUT_COMPLETE_NONE),
                QUICK_BUTTON (N_ ("C&hoose..."), NT_B_CHOOSE, nt_choose_cb, NULL),
                QUICK_BUTTONS_OK_CANCEL,
                QUICK_END,
                // clang-format on
            };
            quick_widget_t file_widgets[] = {
                // clang-format off
                QUICK_START_COLUMNS,
                    QUICK_RADIO (2, types, &type, NULL),
                    QUICK_SEPARATOR (FALSE),
                    QUICK_RADIO (2, accounts, &account, &nt_account_id),
                QUICK_NEXT_COLUMN,
                    QUICK_RADIO (ACL_NT_PRESET_COUNT + 1, nt_preset_names, &rights, &nt_rights_id),
                    QUICK_SEPARATOR (FALSE),
                    QUICK_BUTTON (N_ ("Ad&vanced..."), NT_B_ADVANCED, nt_advanced_cb, NULL),
                QUICK_STOP_COLUMNS,
                QUICK_SEPARATOR (FALSE),
                QUICK_LABELED_INPUT (N_ ("Names:"), input_label_above, name != NULL ? name : "",
                                     "mc.acl.name", &typed, &nt_name_id, FALSE, FALSE,
                                     INPUT_COMPLETE_NONE),
                QUICK_BUTTON (N_ ("C&hoose..."), NT_B_CHOOSE, nt_choose_cb, NULL),
                QUICK_BUTTONS_OK_CANCEL,
                QUICK_END,
                // clang-format on
            };
            WRect r = { -1, -1, 0, 70 };
            quick_dialog_t qdlg = {
                .rect = r,
                .title = title,
                .help = "[Access control list]",
                .help_file = acl_help_file (),
                .widgets = nt_is_dir ? dir_widgets : file_widgets,
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

        names = acl_nt_split_names (name);
        g_ptr_array_set_size (sids, 0);
        for (k = 0; names[k] != NULL && unknown == NULL; k++)
        {
            char *sid = acl_nt_sid_for (names[k], account == 1, nt_attr);

            if (sid == NULL)
                unknown = names[k];
            else
                g_ptr_array_add (sids, sid);
        }

        if (unknown != NULL || sids->len == 0)
            message (D_ERROR, MSG_ERROR, _ ("There is no user or group \"%s\""),
                     unknown != NULL ? unknown : "");
        else if (!is_new && sids->len > 1)
            message (D_ERROR, MSG_ERROR, "%s",
                     _ ("An entry is changed one at a time: give one name."));
        else
        {
            ace->type = type == 1 ? ACL_NT_ACE_DENIED : ACL_NT_ACE_ALLOWED;
            ace->mask = rights < ACL_NT_PRESET_COUNT ? acl_nt_presets[rights].mask : nt_edit_mask;
            ace->flags = (guint8) ((ace->flags
                                    & ~(ACL_NT_OBJECT_INHERIT | ACL_NT_CONTAINER_INHERIT
                                        | ACL_NT_INHERIT_ONLY))
                                   | (nt_is_dir ? nt_scope_flags[scope] : 0));
            g_strfreev (names);
            g_free (name);
            return TRUE;
        }

        g_strfreev (names);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
nt_do_add (void)
{
    acl_nt_ace_t proto = { ACL_NT_ACE_ALLOWED, 0, acl_nt_presets[2].mask, NULL };
    GPtrArray *sids = g_ptr_array_new_with_free_func (g_free);
    guint i;

    if (nt_is_dir)
        proto.flags = ACL_NT_OBJECT_INHERIT | ACL_NT_CONTAINER_INHERIT;

    if (nt_entry_dialog (_ ("Add entry"), &proto, TRUE, sids))
        for (i = 0; i < sids->len; i++)
            acl_nt_insert_ace (
                nt_sd,
                acl_nt_ace_new (proto.type, proto.flags, proto.mask, g_ptr_array_index (sids, i)));

    g_ptr_array_free (sids, TRUE);
    nt_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
nt_refuse_inherited (const acl_nt_ace_t *ace)
{
    if ((ace->flags & ACL_NT_INHERITED) == 0)
        return FALSE;

    message (D_ERROR, MSG_ERROR, "%s",
             _ ("This entry is inherited from the folder above.\nChange it there."));
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
nt_do_edit (void)
{
    acl_nt_ace_t *ace = nt_current ();
    acl_nt_ace_t copy;
    GPtrArray *sids;

    if (ace == NULL || nt_refuse_inherited (ace))
        return;

    copy = *ace;
    sids = g_ptr_array_new_with_free_func (g_free);
    if (nt_entry_dialog (_ ("Edit entry"), &copy, FALSE, sids))
    {
        acl_nt_ace_t *edited =
            acl_nt_ace_new (copy.type, copy.flags, copy.mask, g_ptr_array_index (sids, 0));
        guint position;

        acl_nt_replace_ace (nt_sd, (guint) nt_list->current, edited);
        if (g_ptr_array_find (nt_sd->aces, edited, &position))
            nt_list->current = (int) position;
    }
    g_ptr_array_free (sids, TRUE);
    nt_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

static void
nt_do_delete (void)
{
    acl_nt_ace_t *ace = nt_current ();

    if (ace == NULL || nt_refuse_inherited (ace))
        return;

    g_ptr_array_remove_index (nt_sd->aces, (guint) nt_list->current);
    nt_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

static int
nt_button_cb (WButton *button, int action)
{
    (void) button;

    switch (action)
    {
    case NT_B_ADD:
        nt_do_add ();
        break;
    case NT_B_EDIT:
        nt_do_edit ();
        break;
    case NT_B_DELETE:
        nt_do_delete ();
        break;
    default:
        break;
    }

    widget_select (WIDGET (nt_list));
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
nt_dialog_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    if (msg == MSG_KEY)
    {
        switch (parm)
        {
        case KEY_IC:
            nt_do_add ();
            return MSG_HANDLED;
        case KEY_F (4):
            nt_do_edit ();
            return MSG_HANDLED;
        case KEY_F (3):
            nt_show_entry ();
            return MSG_HANDLED;
        case KEY_DC:
        case KEY_F (8):
            nt_do_delete ();
            return MSG_HANDLED;
        default:
            break;
        }

        if ((parm == '\n' || parm == KEY_ENTER) && GROUP (w)->current != NULL
            && GROUP (w)->current->data == (void *) nt_list)
        {
            nt_do_edit ();
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;
    }

    return dlg_default_callback (w, sender, msg, parm, data);
}

/*** public functions ****************************************************************************/

/**
 * The Windows access control list of a file on cifs or NTFS: add, change and
 * remove entries.  @sd is changed in place; ACL_DLG_SET asks to write it.
 */
acl_dlg_result_t
acl_nt_dialog_run (const char *shown_path, const char *source, const char *attr, int index,
                   int count, gboolean is_dir, acl_nt_sd_t *sd)
{
    WDialog *dlg;
    WGroup *g;
    WButton *b;
    int cols, lines, list_lines, y, x, ret;
    char *text, *owner, *group;

    nt_sd = sd;
    nt_attr = attr;
    nt_is_dir = is_dir;

    cols = MIN (76, COLS - 4);
    list_lines = MAX (4, MIN ((int) sd->aces->len + 2, LINES - 18));
    /* file, owner, group, source, blank, header, list, blank, buttons, hint, line,
       buttons, and the frame around them */
    lines = list_lines + 15;

    dlg =
        dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors, nt_dialog_callback,
                    NULL, "[Access control list]", _ ("Access control list (Windows)"));
    dlg->help_file = acl_help_file ();
    g = GROUP (dlg);

    y = 2;
    if (count > 1)
    {
        char *num = g_strdup_printf ("(%d of %d)", index, count);
        int nw = str_term_width1 (num);

        text = g_strdup_printf ("%s %s", _ ("File:"),
                                str_fit_to_term (shown_path, cols - 15 - nw, J_LEFT_FIT));
        group_add_widget (g, label_new (y, 3, text));
        group_add_widget (g, label_new (y, cols - 4 - nw, num));
        g_free (num);
    }
    else
    {
        text = g_strdup_printf ("%s %s", _ ("File:"),
                                str_fit_to_term (shown_path, cols - 12, J_LEFT_FIT));
        group_add_widget (g, label_new (y, 3, text));
    }
    g_free (text);
    y++;

    owner = acl_nt_sid_name (sd->owner);
    group = acl_nt_sid_name (sd->group);
    text = g_strdup_printf ("%s %s", _ ("Owner:"), owner);
    group_add_widget (g, label_new (y++, 3, str_fit_to_term (text, cols - 6, J_LEFT_FIT)));
    g_free (text);
    text = g_strdup_printf ("%s %s", _ ("Group:"), group);
    group_add_widget (g, label_new (y++, 3, str_fit_to_term (text, cols - 6, J_LEFT_FIT)));
    g_free (text);
    text = g_strdup_printf ("%s %s", _ ("From:"), source);
    group_add_widget (g, label_new (y++, 3, str_fit_to_term (text, cols - 6, J_LEFT_FIT)));
    g_free (text);
    g_free (owner);
    g_free (group);

    y++;
    text = g_strdup_printf ("%-6s %s %s", _ ("Type"),
                            str_fit_to_term (_ ("Who"), NT_WHO_COLUMN, J_LEFT_FIT), _ ("Rights"));
    group_add_widget (g, label_new (y++, 4, text));
    g_free (text);

    nt_list = listbox_new (y, 3, list_lines, cols - 6, FALSE, NULL);
    group_add_widget (g, nt_list);
    y += list_lines + 1;

    x = 3;
    b = button_new (y, x, NT_B_ADD, NORMAL_BUTTON, _ ("&Add"), nt_button_cb);
    group_add_widget (g, b);
    x += button_get_width (b) + 1;
    b = button_new (y, x, NT_B_EDIT, NORMAL_BUTTON, _ ("Ed&it"), nt_button_cb);
    group_add_widget (g, b);
    x += button_get_width (b) + 1;
    b = button_new (y, x, NT_B_DELETE, NORMAL_BUTTON, _ ("&Delete"), nt_button_cb);
    group_add_widget (g, b);
    y++;

    group_add_widget (g, label_new (y++, 3, _ ("F3 shows the rights of an entry one by one.")));
    group_add_widget (g, hline_new (y++, -1, -1));
    group_add_widget (g, button_new (y, cols / 2 - 12, B_ENTER, DEFPUSH_BUTTON, _ ("&Set"), NULL));
    group_add_widget (g,
                      button_new (y, cols / 2 + 2, B_CANCEL, NORMAL_BUTTON, _ ("&Cancel"), NULL));

    nt_refresh ();
    widget_select (WIDGET (nt_list));

    ret = dlg_run (dlg);
    widget_destroy (WIDGET (dlg));

    nt_sd = NULL;
    nt_list = NULL;
    nt_attr = NULL;

    return ret == B_ENTER ? ACL_DLG_SET : ACL_DLG_CANCEL;
}
