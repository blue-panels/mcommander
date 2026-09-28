/*
   Panel plugin acl for the M-Commander
   POSIX ACL entries: the list the dialog edits, read and written through libacl

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
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/acl.h>
#include <acl/libacl.h>

#include "acl-model.h"

/*** file scope functions ************************************************************************/

static int
aclm_entry_compare (gconstpointer a, gconstpointer b)
{
    const aclm_entry_t *x = (const aclm_entry_t *) a;
    const aclm_entry_t *y = (const aclm_entry_t *) b;

    if (x->kind != y->kind)
        return x->kind < y->kind ? -1 : 1;
    if (x->id != y->id)
        return x->id < y->id ? -1 : 1;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
aclm_has_base (const aclm_list_t *list)
{
    return aclm_list_find (list, ACLM_OWNER, 0) != NULL
        && aclm_list_find (list, ACLM_GROUP_OBJ, 0) != NULL
        && aclm_list_find (list, ACLM_OTHER, 0) != NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
aclm_list_clear (aclm_list_t *list)
{
    g_array_set_size (list->entries, 0);
}

/* --------------------------------------------------------------------------------------------- */

static aclm_kind_t
aclm_kind_from_tag (acl_tag_t tag, gboolean *ok)
{
    *ok = TRUE;

    switch (tag)
    {
    case ACL_USER_OBJ:
        return ACLM_OWNER;
    case ACL_USER:
        return ACLM_USER;
    case ACL_GROUP_OBJ:
        return ACLM_GROUP_OBJ;
    case ACL_GROUP:
        return ACLM_GROUP;
    case ACL_MASK:
        return ACLM_MASK;
    case ACL_OTHER:
        return ACLM_OTHER;
    default:
        *ok = FALSE;
        return ACLM_OTHER;
    }
}

/* --------------------------------------------------------------------------------------------- */

static acl_tag_t
aclm_tag_from_kind (aclm_kind_t kind)
{
    switch (kind)
    {
    case ACLM_OWNER:
        return ACL_USER_OBJ;
    case ACLM_USER:
        return ACL_USER;
    case ACLM_GROUP_OBJ:
        return ACL_GROUP_OBJ;
    case ACLM_GROUP:
        return ACL_GROUP;
    case ACLM_MASK:
        return ACL_MASK;
    case ACLM_OTHER:
    default:
        return ACL_OTHER;
    }
}

/*** public functions ****************************************************************************/

aclm_list_t *
aclm_list_new (void)
{
    aclm_list_t *list;

    list = g_new0 (aclm_list_t, 1);
    list->entries = g_array_new (FALSE, TRUE, sizeof (aclm_entry_t));
    return list;
}

/* --------------------------------------------------------------------------------------------- */

aclm_list_t *
aclm_list_copy (const aclm_list_t *list)
{
    aclm_list_t *copy;

    copy = aclm_list_new ();
    if (list != NULL && list->entries->len != 0)
        g_array_append_vals (copy->entries, list->entries->data, list->entries->len);
    return copy;
}

/* --------------------------------------------------------------------------------------------- */

void
aclm_list_free (aclm_list_t *list)
{
    if (list == NULL)
        return;
    g_array_free (list->entries, TRUE);
    g_free (list);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
aclm_list_is_empty (const aclm_list_t *list)
{
    return list == NULL || list->entries->len == 0;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
aclm_is_named (aclm_kind_t kind)
{
    return kind == ACLM_USER || kind == ACLM_GROUP;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
aclm_has_named (const aclm_list_t *list)
{
    guint i;

    for (i = 0; i < list->entries->len; i++)
        if (aclm_is_named (g_array_index (list->entries, aclm_entry_t, i).kind))
            return TRUE;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

aclm_entry_t *
aclm_list_find (const aclm_list_t *list, aclm_kind_t kind, unsigned int id)
{
    guint i;

    if (!aclm_is_named (kind))
        id = 0;

    for (i = 0; i < list->entries->len; i++)
    {
        aclm_entry_t *e = &g_array_index (list->entries, aclm_entry_t, i);

        if (e->kind == kind && e->id == id)
            return e;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
aclm_list_set (aclm_list_t *list, aclm_kind_t kind, unsigned int id, unsigned int perm)
{
    aclm_entry_t *e;

    if (!aclm_is_named (kind))
        id = 0;

    e = aclm_list_find (list, kind, id);
    if (e != NULL)
        e->perm = perm & ACL_PERM_ALL;
    else
    {
        aclm_entry_t n = { kind, id, perm & ACL_PERM_ALL };

        g_array_append_val (list->entries, n);
        aclm_list_sort (list);
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
aclm_list_remove (aclm_list_t *list, aclm_kind_t kind, unsigned int id)
{
    guint i;

    if (!aclm_is_named (kind))
        id = 0;

    for (i = 0; i < list->entries->len; i++)
    {
        const aclm_entry_t *e = &g_array_index (list->entries, aclm_entry_t, i);

        if (e->kind == kind && e->id == id)
        {
            g_array_remove_index (list->entries, i);
            return TRUE;
        }
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
aclm_list_sort (aclm_list_t *list)
{
    g_array_sort (list->entries, aclm_entry_compare);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The mask becomes the union of what the named entries and the group of the
 * file are given, as setfacl does.  A list without named entries needs no
 * mask, and keeping one would leave the file looking as if it had an ACL.
 */
void
aclm_calc_mask (aclm_list_t *list)
{
    unsigned int mask = 0;
    guint i;

    if (!aclm_has_named (list))
    {
        aclm_list_remove (list, ACLM_MASK, 0);
        return;
    }

    for (i = 0; i < list->entries->len; i++)
    {
        const aclm_entry_t *e = &g_array_index (list->entries, aclm_entry_t, i);

        if (aclm_is_named (e->kind) || e->kind == ACLM_GROUP_OBJ)
            mask |= e->perm;
    }

    aclm_list_set (list, ACLM_MASK, 0, mask);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * With the mask kept by hand: a named entry cannot live without a mask, and a
 * mask without named entries is dropped.
 */
void
aclm_drop_unused_mask (aclm_list_t *list)
{
    if (!aclm_has_named (list))
        aclm_list_remove (list, ACLM_MASK, 0);
    else if (aclm_list_find (list, ACLM_MASK, 0) == NULL)
        aclm_calc_mask (list);
}

/* --------------------------------------------------------------------------------------------- */

unsigned int
aclm_effective (const aclm_list_t *list, const aclm_entry_t *entry)
{
    const aclm_entry_t *mask;

    if (!aclm_is_named (entry->kind) && entry->kind != ACLM_GROUP_OBJ)
        return entry->perm;

    mask = aclm_list_find (list, ACLM_MASK, 0);
    return mask != NULL ? entry->perm & mask->perm : entry->perm;
}

/* --------------------------------------------------------------------------------------------- */

/* The owner, group and other entries a new default ACL starts with. */
void
aclm_list_base_from (aclm_list_t *list, const aclm_list_t *access)
{
    static const aclm_kind_t base[] = { ACLM_OWNER, ACLM_GROUP_OBJ, ACLM_OTHER };
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (base); i++)
    {
        const aclm_entry_t *e = aclm_list_find (access, base[i], 0);

        if (aclm_list_find (list, base[i], 0) == NULL)
            aclm_list_set (list, base[i], 0, e != NULL ? e->perm : 0);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* What the dialog changed in one list, appended to @changes. */
GArray *
aclm_diff (const aclm_list_t *before, const aclm_list_t *after, aclm_type_t type, GArray *changes)
{
    guint i;

    if (changes == NULL)
        changes = g_array_new (FALSE, TRUE, sizeof (aclm_change_t));

    for (i = 0; i < after->entries->len; i++)
    {
        const aclm_entry_t *e = &g_array_index (after->entries, aclm_entry_t, i);
        const aclm_entry_t *old = aclm_list_find (before, e->kind, e->id);

        if (old == NULL || old->perm != e->perm)
        {
            aclm_change_t c = { FALSE, type, *e };

            g_array_append_val (changes, c);
        }
    }

    for (i = 0; i < before->entries->len; i++)
    {
        const aclm_entry_t *e = &g_array_index (before->entries, aclm_entry_t, i);

        if (aclm_list_find (after, e->kind, e->id) == NULL)
        {
            aclm_change_t c = { TRUE, type, *e };

            g_array_append_val (changes, c);
        }
    }

    return changes;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Replay the changes of one type on another file's list.  Removing an entry
 * the list does not have does nothing, setting one it has replaces its
 * permissions.  A default ACL that lost one of its base entries is gone as a
 * whole; the caller seeds an empty default list with aclm_list_base_from()
 * before replaying additions on it.
 */
void
aclm_apply (aclm_list_t *list, const GArray *changes, aclm_type_t type, gboolean recalc_mask)
{
    guint i;

    for (i = 0; i < changes->len; i++)
    {
        const aclm_change_t *c = &g_array_index (changes, aclm_change_t, i);

        if (c->type != type)
            continue;
        if (recalc_mask && c->entry.kind == ACLM_MASK)
            continue;

        if (c->remove)
            (void) aclm_list_remove (list, c->entry.kind, c->entry.id);
        else
            aclm_list_set (list, c->entry.kind, c->entry.id, c->entry.perm);
    }

    if (!aclm_list_is_empty (list) && !aclm_has_base (list))
    {
        aclm_list_clear (list);
        return;
    }

    if (recalc_mask)
        aclm_calc_mask (list);
    else
        aclm_drop_unused_mask (list);
}

/* --------------------------------------------------------------------------------------------- */

void
aclm_perm_to_str (unsigned int perm, char out[4])
{
    out[0] = (perm & ACL_PERM_READ) != 0 ? 'r' : '-';
    out[1] = (perm & ACL_PERM_WRITE) != 0 ? 'w' : '-';
    out[2] = (perm & ACL_PERM_EXECUTE) != 0 ? 'x' : '-';
    out[3] = '\0';
}

/* --------------------------------------------------------------------------------------------- */

/* The name shown for an entry; "#1007" for an id the system does not know. */
char *
acl_name_of (aclm_kind_t kind, unsigned int id)
{
    if (kind == ACLM_OWNER || kind == ACLM_USER)
    {
        struct passwd *pw = getpwuid ((uid_t) id);

        return pw != NULL ? g_strdup (pw->pw_name) : g_strdup_printf ("#%u", id);
    }

    if (kind == ACLM_GROUP_OBJ || kind == ACLM_GROUP)
    {
        struct group *gr = getgrgid ((gid_t) id);

        return gr != NULL ? g_strdup (gr->gr_name) : g_strdup_printf ("#%u", id);
    }

    return g_strdup ("");
}

/* --------------------------------------------------------------------------------------------- */

/* A user or group name, or a number, as an id. */
gboolean
acl_id_of (gboolean group, const char *name, unsigned int *id)
{
    const char *p = name;
    guint64 v;

    if (p == NULL)
        return FALSE;
    while (*p == ' ')
        p++;
    if (*p == '#')
        p++;
    if (*p == '\0')
        return FALSE;

    if (group)
    {
        struct group *gr = getgrnam (p);

        if (gr != NULL)
        {
            *id = (unsigned int) gr->gr_gid;
            return TRUE;
        }
    }
    else
    {
        struct passwd *pw = getpwnam (p);

        if (pw != NULL)
        {
            *id = (unsigned int) pw->pw_uid;
            return TRUE;
        }
    }

    if (g_ascii_string_to_unsigned (p, 10, 0, G_MAXUINT32 - 1, &v, NULL))
    {
        *id = (unsigned int) v;
        return TRUE;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read one ACL of @path.  A file without extended entries still has an access
 * ACL of three entries made from its mode.  NULL on error, with errno in @err:
 * ENOTSUP when the file system has no ACL support.
 */
aclm_list_t *
aclm_read (const char *path, aclm_type_t type, int *err)
{
    acl_t acl;
    acl_entry_t entry;
    aclm_list_t *list;
    int r;

    acl = acl_get_file (path, type == ACLM_DEFAULT ? ACL_TYPE_DEFAULT : ACL_TYPE_ACCESS);
    if (acl == NULL)
    {
        if (err != NULL)
            *err = errno;
        return NULL;
    }

    list = aclm_list_new ();

    for (r = acl_get_entry (acl, ACL_FIRST_ENTRY, &entry); r == 1;
         r = acl_get_entry (acl, ACL_NEXT_ENTRY, &entry))
    {
        acl_tag_t tag;
        acl_permset_t permset;
        aclm_entry_t e = { ACLM_OTHER, 0, 0 };
        gboolean ok;

        if (acl_get_tag_type (entry, &tag) != 0)
            continue;
        e.kind = aclm_kind_from_tag (tag, &ok);
        if (!ok)
            continue;

        if (aclm_is_named (e.kind))
        {
            void *q = acl_get_qualifier (entry);

            if (q == NULL)
                continue;
            e.id = e.kind == ACLM_USER ? (unsigned int) *(uid_t *) q : (unsigned int) *(gid_t *) q;
            acl_free (q);
        }

        if (acl_get_permset (entry, &permset) == 0)
        {
            if (acl_get_perm (permset, ACL_READ) == 1)
                e.perm |= ACL_PERM_READ;
            if (acl_get_perm (permset, ACL_WRITE) == 1)
                e.perm |= ACL_PERM_WRITE;
            if (acl_get_perm (permset, ACL_EXECUTE) == 1)
                e.perm |= ACL_PERM_EXECUTE;
        }

        g_array_append_val (list->entries, e);
    }

    acl_free (acl);
    aclm_list_sort (list);
    return list;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Write one ACL of @path.  An empty default list removes the default ACL of
 * the directory.  FALSE on error, with errno in @err.
 */
gboolean
aclm_write (const char *path, aclm_type_t type, const aclm_list_t *list, int *err)
{
    acl_t acl;
    guint i;
    int r;

    if (type == ACLM_DEFAULT && aclm_list_is_empty (list))
    {
        if (acl_delete_def_file (path) == 0)
            return TRUE;
        if (err != NULL)
            *err = errno;
        return FALSE;
    }

    acl = acl_init ((int) list->entries->len);
    if (acl == NULL)
    {
        if (err != NULL)
            *err = errno;
        return FALSE;
    }

    for (i = 0; i < list->entries->len; i++)
    {
        const aclm_entry_t *e = &g_array_index (list->entries, aclm_entry_t, i);
        acl_entry_t entry;
        acl_permset_t permset;

        if (acl_create_entry (&acl, &entry) != 0
            || acl_set_tag_type (entry, aclm_tag_from_kind (e->kind)) != 0)
            goto fail;

        if (e->kind == ACLM_USER)
        {
            uid_t uid = (uid_t) e->id;

            if (acl_set_qualifier (entry, &uid) != 0)
                goto fail;
        }
        else if (e->kind == ACLM_GROUP)
        {
            gid_t gid = (gid_t) e->id;

            if (acl_set_qualifier (entry, &gid) != 0)
                goto fail;
        }

        if (acl_get_permset (entry, &permset) != 0 || acl_clear_perms (permset) != 0)
            goto fail;
        if ((e->perm & ACL_PERM_READ) != 0)
            acl_add_perm (permset, ACL_READ);
        if ((e->perm & ACL_PERM_WRITE) != 0)
            acl_add_perm (permset, ACL_WRITE);
        if ((e->perm & ACL_PERM_EXECUTE) != 0)
            acl_add_perm (permset, ACL_EXECUTE);
        if (acl_set_permset (entry, permset) != 0)
            goto fail;
    }

    if (acl_valid (acl) != 0)
    {
        acl_free (acl);
        if (err != NULL)
            *err = EINVAL;
        return FALSE;
    }

    r = acl_set_file (path, type == ACLM_DEFAULT ? ACL_TYPE_DEFAULT : ACL_TYPE_ACCESS, acl);
    if (r != 0 && err != NULL)
        *err = errno;
    acl_free (acl);
    return r == 0;

fail:
    if (err != NULL)
        *err = errno;
    acl_free (acl);
    return FALSE;
}
