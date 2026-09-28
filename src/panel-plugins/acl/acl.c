/*
   Panel plugin acl for the M-Commander
   Access control lists of local files, from the Attributes menu

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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/xattr.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/panel-plugin.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "acl-dialog.h"
#include "acl-model.h"
#include "acl-nt.h"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *path;       /* full path of the file */
    char *shown_path; /* what the dialog shows */
    struct stat st;   /* of the file a symbolic link points to */
    aclm_list_t *lists[2];
} acl_file_t;

typedef enum
{
    ACL_GO_ON = 0,
    ACL_SKIP,
    ACL_ABORT
} acl_step_t;

/*** file scope functions ************************************************************************/

/* The plugin has no panel of its own. */
static void *
acl_open (mc_panel_host_t *host, const char *open_path)
{
    (void) host;
    (void) open_path;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_close (void *plugin_data)
{
    (void) plugin_data;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
acl_get_items (void *plugin_data, void *list)
{
    (void) plugin_data;
    (void) list;
    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_file_free (acl_file_t *f)
{
    g_free (f->path);
    g_free (f->shown_path);
    aclm_list_free (f->lists[ACLM_ACCESS]);
    aclm_list_free (f->lists[ACLM_DEFAULT]);
    g_free (f);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
acl_path_is_local (const char *dir)
{
    return dir != NULL && g_path_is_absolute (dir) && strstr (dir, "://") == NULL;
}

/* --------------------------------------------------------------------------------------------- */

static acl_step_t
acl_report (const char *what, const char *path, int err)
{
    char *text;
    int r;

    text = g_strdup_printf ("%s\n%s\n%s", what, path,
                            err == EINVAL ? _ ("The list is not valid") : unix_error_string (err));
    r = query_dialog (MSG_ERROR, text, D_ERROR, 2, _ ("&Skip"), _ ("&Abort"));
    g_free (text);
    return r == 0 ? ACL_SKIP : ACL_ABORT;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read the POSIX lists. An unsupported access ACL leaves the file loaded so
 * the caller can look for a Windows or NFSv4 list.
 */
static acl_file_t *
acl_file_load (const char *dir, const char *name, acl_step_t *step)
{
    acl_file_t *f;
    struct stat lst;
    int err = 0;

    *step = ACL_GO_ON;

    f = g_new0 (acl_file_t, 1);
    /* ".." is the directory above, as for Chmod; it is shown by its own name */
    if (strcmp (name, "..") == 0)
        f->path = g_path_get_dirname (dir);
    else if (g_path_is_absolute (name))
        f->path = g_strdup (name);
    else
        f->path = g_build_filename (dir, name, (char *) NULL);

    if (lstat (f->path, &lst) != 0 || stat (f->path, &f->st) != 0)
    {
        *step = acl_report (_ ("Cannot stat"), f->path, errno);
        acl_file_free (f);
        return NULL;
    }

    if (S_ISLNK (lst.st_mode))
    {
        char *target = realpath (f->path, NULL);

        f->shown_path = g_strdup_printf ("%s -> %s", f->path, target != NULL ? target : "?");
        free (target);
    }
    else
        f->shown_path = g_strdup (f->path);

    f->lists[ACLM_ACCESS] = aclm_read (f->path, ACLM_ACCESS, &err);
    if (f->lists[ACLM_ACCESS] == NULL)
    {
        /* no POSIX ACL here: the caller looks for another kind */
        if (err == ENOTSUP || err == EOPNOTSUPP)
            return f;

        *step = acl_report (_ ("Cannot read the access control list of"), f->path, err);
        acl_file_free (f);
        return NULL;
    }

    if (S_ISDIR (f->st.st_mode))
    {
        f->lists[ACLM_DEFAULT] = aclm_read (f->path, ACLM_DEFAULT, &err);
        if (f->lists[ACLM_DEFAULT] == NULL)
        {
            if (err == ENODATA)
                f->lists[ACLM_DEFAULT] = aclm_list_new ();
            else
            {
                *step =
                    acl_report (_ ("Cannot read the default access control list of"), f->path, err);
                acl_file_free (f);
                return NULL;
            }
        }
    }

    return f;
}

/* --------------------------------------------------------------------------------------------- */

/* The name of the file system of @path, for the messages. */
static char *
acl_fs_name (const char *path)
{
    static const struct
    {
        unsigned long magic;
        const char *name;
    } names[] = {
        { 0xEF53, "ext2/3/4" },      { 0x58465342, "xfs" },      { 0x9123683E, "btrfs" },
        { 0x01021994, "tmpfs" },     { 0xFF534D42, "cifs" },     { 0xFE534D42, "smb3" },
        { 0x6969, "nfs" },           { 0x65735546, "fuse" },     { 0x4d44, "vfat" },
        { 0x2011BAB0, "exfat" },     { 0x7366746e, "ntfs3" },    { 0x5346544e, "ntfs" },
        { 0x9660, "iso9660" },       { 0x73717368, "squashfs" }, { 0x2FC12FC1, "zfs" },
        { 0xF2F52010, "f2fs" },      { 0x9fa0, "proc" },         { 0x62656572, "sysfs" },
        { 0x1cd1, "devpts" },        { 0x63677270, "cgroup2" },  { 0x01021997, "9p" },
        { 0x794c7630, "overlayfs" },
    };
    struct statfs sfs;
    size_t i;

    if (statfs (path, &sfs) != 0)
        return g_strdup ("?");

    for (i = 0; i < G_N_ELEMENTS (names); i++)
        if ((unsigned long) sfs.f_type == names[i].magic)
            return g_strdup (names[i].name);

    return g_strdup_printf ("0x%lx", (unsigned long) sfs.f_type);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * A file with no POSIX ACL: show the Windows list it has, say that it has an
 * NFSv4 one, or say that the file system has none.  An attribute counts only
 * when its value can be read.
 */
static acl_step_t
acl_other_kind (const acl_file_t *f, int index, int count)
{
    acl_nt_sd_t *sd;
    const char *attr = NULL;
    char *fs;
    acl_step_t step = ACL_GO_ON;
    ssize_t n;

    fs = acl_fs_name (f->path);

    sd = acl_nt_read (f->path, &attr, NULL);
    if (sd != NULL)
    {
        char *source;
        acl_dlg_result_t r;

        /* statfs can fail on a cifs mount whose session is stuck; the attribute
           still tells where the list comes from */
        if (strcmp (fs, "?") == 0)
        {
            g_free (fs);
            fs = g_strdup (strcmp (attr, "system.cifs_acl") == 0 ? "cifs" : "ntfs");
        }
        source = g_strdup_printf ("%s, %s", fs, attr);

        r = acl_nt_dialog_run (f->shown_path, source, attr, index, count, S_ISDIR (f->st.st_mode),
                               sd);
        if (r == ACL_DLG_CANCEL)
            step = ACL_ABORT;
        else
        {
            acl_nt_sd_t *before = acl_nt_read (f->path, NULL, NULL);
            int err = 0;

            switch (acl_nt_write (f->path, attr, before, sd, &err))
            {
            case ACL_NT_WRITE_OK:
                break;
            case ACL_NT_WRITE_CHANGED:
                message (D_NORMAL, _ ("Access control list"),
                         _ ("%s\nThe server stored the list in a form of its own.\n"
                            "Open the dialog again to see it."),
                         f->shown_path);
                break;
            default:
                step = acl_report (_ ("Cannot set the access control list of"), f->path, err);
                break;
            }
            acl_nt_free (before);
        }
        g_free (source);
        acl_nt_free (sd);
    }
    else if ((n = getxattr (f->path, "system.nfs4_acl", NULL, 0)) > 0)
        message (D_NORMAL, _ ("Access control list"),
                 _ ("%s\nhas an NFSv4 access control list (%s).\n"
                    "M-Commander does not show these yet; nfs4_getfacl does."),
                 f->shown_path, fs);
    else
    {
        message (D_ERROR, MSG_ERROR,
                 _ ("Access control lists are not supported on this file system (%s)"), fs);
        step = ACL_ABORT;
    }

    g_free (fs);
    return step;
}

/* --------------------------------------------------------------------------------------------- */

static acl_step_t
acl_file_store (const acl_file_t *f, const aclm_list_t *access, const aclm_list_t *def)
{
    int err = 0;

    if (!aclm_write (f->path, ACLM_ACCESS, access, &err))
        return acl_report (_ ("Cannot set the access control list of"), f->path, err);

    if (S_ISDIR (f->st.st_mode) && def != NULL
        && !(aclm_list_is_empty (def) && aclm_list_is_empty (f->lists[ACLM_DEFAULT]))
        && !aclm_write (f->path, ACLM_DEFAULT, def, &err))
        return acl_report (_ ("Cannot set the default access control list of"), f->path, err);

    return ACL_GO_ON;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
acl_targets (mc_panel_host_t *host)
{
    GPtrArray *names;

    names = g_ptr_array_new_with_free_func (g_free);

    if (host->get_marked_count != NULL && host->get_next_marked != NULL
        && host->get_marked_count (host) > 0)
    {
        int current = 0;
        const GString *name;

        /* it returns the marked entry at or after @current and leaves moving past
           it to the caller */
        while ((name = host->get_next_marked (host, &current)) != NULL)
        {
            g_ptr_array_add (names, g_strdup (name->str));
            current++;
        }
    }
    else if (host->get_current != NULL)
    {
        const GString *name = host->get_current (host);

        if (name != NULL && name->len != 0)
            g_ptr_array_add (names, g_strdup (name->str));
    }

    return names;
}

/* --------------------------------------------------------------------------------------------- */

/* The rest of the marked files after "Set all" or "Changes to all". */
static void
acl_to_the_rest (GPtrArray *names, guint from, const char *dir, const acl_file_t *first,
                 const aclm_list_t *access, const aclm_list_t *def, const GArray *changes,
                 gboolean recalc_mask)
{
    guint i;

    for (i = from; i < names->len; i++)
    {
        acl_step_t step;
        acl_file_t *f;
        aclm_list_t *a, *d = NULL;

        f = acl_file_load (dir, g_ptr_array_index (names, i), &step);
        if (f == NULL)
        {
            if (step == ACL_ABORT)
                return;
            continue;
        }
        if (f->lists[ACLM_ACCESS] == NULL)
        {
            step = acl_report (_ ("Cannot set a POSIX access control list on"), f->path, ENOTSUP);
            acl_file_free (f);
            if (step == ACL_ABORT)
                return;
            continue;
        }

        if (changes == NULL)
        {
            a = aclm_list_copy (access);
            /* A default ACL copied from a file, which has none, is left alone. */
            if (S_ISDIR (f->st.st_mode) && S_ISDIR (first->st.st_mode))
                d = aclm_list_copy (def);
            else if (S_ISDIR (f->st.st_mode))
                d = aclm_list_copy (f->lists[ACLM_DEFAULT]);
        }
        else
        {
            a = aclm_list_copy (f->lists[ACLM_ACCESS]);
            aclm_apply (a, changes, ACLM_ACCESS, recalc_mask);
            if (S_ISDIR (f->st.st_mode))
            {
                guint k;
                gboolean adds = FALSE;

                d = aclm_list_copy (f->lists[ACLM_DEFAULT]);
                for (k = 0; k < changes->len; k++)
                {
                    const aclm_change_t *c = &g_array_index (changes, aclm_change_t, k);

                    adds = adds || (c->type == ACLM_DEFAULT && !c->remove);
                }
                if (adds && aclm_list_is_empty (d))
                    aclm_list_base_from (d, a);
                aclm_apply (d, changes, ACLM_DEFAULT, recalc_mask);
            }
        }

        step = acl_file_store (f, a, d);
        aclm_list_free (a);
        aclm_list_free (d);
        acl_file_free (f);
        if (step == ACL_ABORT)
            return;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void *
acl_action_edit (mc_panel_host_t *host, const char *open_path)
{
    GPtrArray *names;
    const GString *current;
    guint i;
    gboolean recalc_mask = TRUE;

    if (host == NULL)
        return NULL;

    if (!acl_path_is_local (open_path))
    {
        message (D_ERROR, MSG_ERROR, "%s", _ ("Access control lists work only on local files"));
        return NULL;
    }

    names = acl_targets (host);
    if (names->len == 0)
    {
        message (D_ERROR, MSG_ERROR, "%s", _ ("There is no file under the cursor"));
        g_ptr_array_free (names, TRUE);
        return NULL;
    }

    for (i = 0; i < names->len; i++)
    {
        acl_step_t step;
        acl_file_t *f;
        acl_dlg_state_t st;
        acl_dlg_result_t r;
        GArray *changes = NULL;

        f = acl_file_load (open_path, g_ptr_array_index (names, i), &step);
        if (f == NULL)
        {
            if (step == ACL_ABORT)
                break;
            continue;
        }
        if (f->lists[ACLM_ACCESS] == NULL)
        {
            step = acl_other_kind (f, (int) i + 1, (int) names->len);
            acl_file_free (f);
            if (step == ACL_ABORT)
                break;
            continue;
        }

        memset (&st, 0, sizeof (st));
        st.shown_path = f->shown_path;
        st.index = (int) i + 1;
        st.count = (int) names->len;
        st.owner = f->st.st_uid;
        st.group = f->st.st_gid;
        st.mode = f->st.st_mode;
        st.is_dir = S_ISDIR (f->st.st_mode);
        st.read_only = geteuid () != 0 && geteuid () != f->st.st_uid;
        st.lists[ACLM_ACCESS] = aclm_list_copy (f->lists[ACLM_ACCESS]);
        st.lists[ACLM_DEFAULT] = st.is_dir ? aclm_list_copy (f->lists[ACLM_DEFAULT]) : NULL;
        st.recalc_mask = recalc_mask;

        r = acl_dialog_run (&st);
        recalc_mask = st.recalc_mask;

        if (r != ACL_DLG_CANCEL)
            step = acl_file_store (f, st.lists[ACLM_ACCESS], st.lists[ACLM_DEFAULT]);

        if (r == ACL_DLG_CHANGES_TO_ALL)
        {
            changes = aclm_diff (f->lists[ACLM_ACCESS], st.lists[ACLM_ACCESS], ACLM_ACCESS, NULL);
            if (st.is_dir)
                changes = aclm_diff (f->lists[ACLM_DEFAULT], st.lists[ACLM_DEFAULT], ACLM_DEFAULT,
                                     changes);
        }

        if ((r == ACL_DLG_SET_ALL || r == ACL_DLG_CHANGES_TO_ALL) && step != ACL_ABORT)
            acl_to_the_rest (names, i + 1, open_path, f, st.lists[ACLM_ACCESS],
                             st.lists[ACLM_DEFAULT], changes, recalc_mask);

        if (changes != NULL)
            g_array_free (changes, TRUE);
        aclm_list_free (st.lists[ACLM_ACCESS]);
        aclm_list_free (st.lists[ACLM_DEFAULT]);
        acl_file_free (f);

        if (r == ACL_DLG_CANCEL || r == ACL_DLG_SET_ALL || r == ACL_DLG_CHANGES_TO_ALL
            || step == ACL_ABORT)
            break;
    }

    g_ptr_array_free (names, TRUE);

    /* Read the panel again: the mode of a file changes with its mask. */
    current = host->get_current != NULL ? host->get_current (host) : NULL;
    g_free (host->focus_after);
    host->focus_after = g_strdup (current != NULL ? current->str : "");

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
acl_get_help_info (void *plugin_data, const char **filename, const char **node)
{
    (void) plugin_data;

    if (node != NULL)
        *node = "[Access control list]";

    if (filename != NULL)
    {
        *filename = acl_help_file ();
        if (*filename == NULL)
            return MC_PPR_NOT_SUPPORTED;
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const mc_pp_action_t acl_actions[] = {
    { N_ ("Access control list"), acl_action_edit },
};

static const mc_pp_cmd_menu_entry_t acl_menu[] = {
    { N_ ("Access control &list..."), 0, NULL, 0, MC_PP_MENU_ATTRIBUTES },
};

static const mc_panel_plugin_t acl_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "acl",
    .display_name = N_ ("Access control list"),
    .proto = NULL,
    .prefix = NULL,
    .flags = MC_PPF_NONE,
    .open = acl_open,
    .close = acl_close,
    .get_items = acl_get_items,
    .get_help_info = acl_get_help_info,
    .actions = acl_actions,
    .action_count = G_N_ELEMENTS (acl_actions),
    .cmd_menu_entries = acl_menu,
    .cmd_menu_entry_count = G_N_ELEMENTS (acl_menu),
};

/*** public functions ****************************************************************************/

const char *
acl_help_file (void)
{
    static char *path = NULL;

    if (path == NULL)
        path = g_build_filename (MC_PLUGIN_DIR, "acl_panel.md", (char *) NULL);

    return g_file_test (path, G_FILE_TEST_IS_REGULAR) ? path : NULL;
}

/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &acl_plugin;
}
