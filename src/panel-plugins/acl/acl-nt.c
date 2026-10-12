/*
   Panel plugin acl for the M-Commander
   Windows security descriptors on cifs and NTFS mounts

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
#include <string.h>
#include <sys/xattr.h>

#include "acl-nt.h"

/*** file scope macro definitions ****************************************************************/

#define SE_DACL_PRESENT  0x0004
#define SE_SACL_PRESENT  0x0010
#define SE_SELF_RELATIVE 0x8000

/* the domain ntfs-3g gives users and groups when it has no user mapping file:
   user RID 10000 + 2 * uid, group RID 10001 + 2 * gid */
#define NTFS3G_DOMAIN "S-1-5-21-3141592653-589793238-462843383-"

/* access rights of a file or a directory (MS-DTYP 2.4.3, MS-SMB2 2.2.13.1) */
#define NT_READ_DATA        0x00000001 /* list folder */
#define NT_WRITE_DATA       0x00000002 /* create files */
#define NT_APPEND_DATA      0x00000004 /* create folders */
#define NT_READ_EA          0x00000008
#define NT_WRITE_EA         0x00000010
#define NT_EXECUTE          0x00000020 /* traverse folder */
#define NT_DELETE_CHILD     0x00000040
#define NT_READ_ATTRIBUTES  0x00000080
#define NT_WRITE_ATTRIBUTES 0x00000100
#define NT_DELETE           0x00010000
#define NT_READ_CONTROL     0x00020000
#define NT_WRITE_DAC        0x00040000
#define NT_WRITE_OWNER      0x00080000
#define NT_GENERIC_ALL      0x10000000
#define NT_GENERIC_EXECUTE  0x20000000
#define NT_GENERIC_WRITE    0x40000000
#define NT_GENERIC_READ     0x80000000

#define NT_FULL_CONTROL     0x001f01ff
#define NT_MODIFY           0x001301bf
#define NT_READ_EXECUTE     0x001200a9
#define NT_READ             0x00120089
#define NT_WRITE            0x00100116

/*** file scope type declarations ****************************************************************/

typedef struct
{
    const char *sid;
    const char *name;
} acl_nt_known_t;

/*** file scope variables ************************************************************************/

/* The attributes that carry a security descriptor, in the order they are tried:
   cifs, ntfs-3g, the ntfs3 kernel driver. */
static const char *const acl_nt_attrs[] = { "system.cifs_acl", "system.ntfs_acl",
                                            "system.ntfs_security" };

const acl_nt_preset_t acl_nt_presets[ACL_NT_PRESET_COUNT] = {
    { NT_FULL_CONTROL, N_ ("Full control"), N_ ("&Full control") },
    { NT_MODIFY, N_ ("Modify"), N_ ("&Modify") },
    { NT_READ_EXECUTE, N_ ("Read & execute"), N_ ("Read and e&xecute") },
    { NT_READ, N_ ("Read"), N_ ("&Read") },
    { NT_WRITE, N_ ("Write"), N_ ("&Write") },
};

const acl_nt_right_t acl_nt_rights[ACL_NT_RIGHT_COUNT] = {
    { NT_EXECUTE, N_ ("Execute file"), N_ ("Traverse folder") },
    { NT_READ_DATA, N_ ("Read data"), N_ ("List folder") },
    { NT_READ_ATTRIBUTES, N_ ("Read attributes"), N_ ("Read attributes") },
    { NT_READ_EA, N_ ("Read extended attributes"), N_ ("Read extended attributes") },
    { NT_WRITE_DATA, N_ ("Write data"), N_ ("Create files") },
    { NT_APPEND_DATA, N_ ("Append data"), N_ ("Create folders") },
    { NT_WRITE_ATTRIBUTES, N_ ("Write attributes"), N_ ("Write attributes") },
    { NT_WRITE_EA, N_ ("Write extended attributes"), N_ ("Write extended attributes") },
    { NT_DELETE_CHILD, N_ ("Delete subfolders and files"), N_ ("Delete subfolders and files") },
    { NT_DELETE, N_ ("Delete"), N_ ("Delete") },
    { NT_READ_CONTROL, N_ ("Read permissions"), N_ ("Read permissions") },
    { NT_WRITE_DAC, N_ ("Change permissions"), N_ ("Change permissions") },
    { NT_WRITE_OWNER, N_ ("Take ownership"), N_ ("Take ownership") },
};

static const acl_nt_known_t acl_nt_known[] = {
    { "S-1-1-0", "Everyone" },
    { "S-1-3-0", "CREATOR OWNER" },
    { "S-1-3-1", "CREATOR GROUP" },
    { "S-1-5-2", "NETWORK" },
    { "S-1-5-4", "INTERACTIVE" },
    { "S-1-5-7", "ANONYMOUS LOGON" },
    { "S-1-5-11", "Authenticated Users" },
    { "S-1-5-18", "SYSTEM" },
    { "S-1-5-32-544", "Administrators" },
    { "S-1-5-32-545", "Users" },
    { "S-1-5-32-546", "Guests" },
};

/*** file scope functions ************************************************************************/

static guint16
le16 (const guint8 *p)
{
    return (guint16) (p[0] | (p[1] << 8));
}

/* --------------------------------------------------------------------------------------------- */

static guint32
le32 (const guint8 *p)
{
    return (guint32) p[0] | ((guint32) p[1] << 8) | ((guint32) p[2] << 16) | ((guint32) p[3] << 24);
}

/* --------------------------------------------------------------------------------------------- */

/* The SID at @off as "S-1-5-21-...", or NULL when it does not fit in @size. */
static char *
acl_nt_sid_at (const guint8 *data, gsize size, gsize off)
{
    GString *s;
    guint64 auth = 0;
    guint n, i;

    if (off + 8 > size)
        return NULL;
    n = data[off + 1];
    if (n > 15 || off + 8 + 4 * (gsize) n > size)
        return NULL;

    for (i = 2; i < 8; i++)
        auth = (auth << 8) | data[off + i];

    s = g_string_new (NULL);
    g_string_printf (s, "S-%u-%" G_GUINT64_FORMAT, data[off], auth);
    for (i = 0; i < n; i++)
        g_string_append_printf (s, "-%u", le32 (data + off + 8 + 4 * i));

    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
acl_nt_ace_free (gpointer p)
{
    acl_nt_ace_t *ace = (acl_nt_ace_t *) p;

    g_free (ace->sid);
    if (ace->raw != NULL)
        g_bytes_unref (ace->raw);
    g_free (ace);
}

/* --------------------------------------------------------------------------------------------- */

/* Generic rights stand for a set of the specific ones. */
guint32
acl_nt_map_generic (guint32 mask)
{
    guint32 m = mask & ~(NT_GENERIC_ALL | NT_GENERIC_EXECUTE | NT_GENERIC_WRITE | NT_GENERIC_READ);

    if ((mask & NT_GENERIC_ALL) != 0)
        m |= NT_FULL_CONTROL;
    if ((mask & NT_GENERIC_READ) != 0)
        m |= NT_READ;
    if ((mask & NT_GENERIC_WRITE) != 0)
        m |= NT_WRITE | NT_READ_CONTROL;
    if ((mask & NT_GENERIC_EXECUTE) != 0)
        m |= NT_EXECUTE | NT_READ_ATTRIBUTES | NT_READ_CONTROL | ACL_NT_SYNCHRONIZE;

    return m;
}

/*** public functions ****************************************************************************/

/**
 * Parse a self-relative security descriptor (MS-DTYP 2.4.6).  NULL when it is
 * not one or does not fit in @size.
 */
acl_nt_sd_t *
acl_nt_parse (const guint8 *data, gsize size)
{
    acl_nt_sd_t *sd;
    guint32 off_owner, off_group, off_dacl;

    if (data == NULL || size < 20 || data[0] != 1)
        return NULL;

    sd = g_new0 (acl_nt_sd_t, 1);
    sd->aces = g_ptr_array_new_with_free_func (acl_nt_ace_free);
    sd->control = le16 (data + 2);
    off_owner = le32 (data + 4);
    off_group = le32 (data + 8);
    off_dacl = le32 (data + 16);

    if (off_owner != 0)
        sd->owner = acl_nt_sid_at (data, size, off_owner);
    if (off_group != 0)
        sd->group = acl_nt_sid_at (data, size, off_group);

    sd->has_dacl = (sd->control & SE_DACL_PRESENT) != 0 && off_dacl != 0;
    if (sd->has_dacl)
    {
        gsize acl_size, end, p;
        guint count, i;

        if ((gsize) off_dacl + 8 > size)
            goto bad;
        acl_size = le16 (data + off_dacl + 2);
        count = le16 (data + off_dacl + 4);
        end = (gsize) off_dacl + acl_size;
        if (acl_size < 8 || end > size)
            goto bad;
        sd->dacl_revision = data[off_dacl];

        p = (gsize) off_dacl + 8;
        for (i = 0; i < count; i++)
        {
            acl_nt_ace_t *ace;
            gsize ace_size;

            if (p + 8 > end)
                goto bad;
            ace_size = le16 (data + p + 2);
            if (ace_size < 8 || p + ace_size > end)
                goto bad;

            ace = g_new0 (acl_nt_ace_t, 1);
            ace->type = data[p];
            ace->flags = data[p + 1];
            ace->mask = le32 (data + p + 4);
            /* Allowed, denied and audit ACEs start with a SID after the mask.
               Keep other ACE formats whole so editing another entry can
               round-trip them without losing their object or callback data. */
            if (ace->type <= 2)
                ace->sid = acl_nt_sid_at (data, p + ace_size, p + 8);
            else
                ace->raw = g_bytes_new (data + p, ace_size);
            g_ptr_array_add (sd->aces, ace);

            p += ace_size;
        }
    }

    return sd;

bad:
    acl_nt_free (sd);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
acl_nt_free (acl_nt_sd_t *sd)
{
    if (sd == NULL)
        return;
    g_free (sd->owner);
    g_free (sd->group);
    g_ptr_array_free (sd->aces, TRUE);
    g_free (sd);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The security descriptor of @path from the first attribute that has one.  An
 * attribute counts only when its value is read: FUSE answers ENODATA for any
 * name.  The size is asked for first, since ntfs-3g answers a short buffer
 * with EIO instead of ERANGE.  @attr gets the name of the attribute.
 */
acl_nt_sd_t *
acl_nt_read (const char *path, const char **attr, int *err)
{
    size_t i;
    int last = ENOTSUP;

    for (i = 0; i < G_N_ELEMENTS (acl_nt_attrs); i++)
    {
        ssize_t n;
        guint8 *buf;
        acl_nt_sd_t *sd;

        n = getxattr (path, acl_nt_attrs[i], NULL, 0);
        if (n <= 0)
        {
            if (n < 0 && errno != ENOTSUP && errno != EOPNOTSUPP && errno != ENODATA)
                last = errno;
            continue;
        }

        buf = g_malloc ((gsize) n);
        n = getxattr (path, acl_nt_attrs[i], buf, (size_t) n);
        sd = n > 0 ? acl_nt_parse (buf, (gsize) n) : NULL;
        if (n < 0)
            last = errno;
        g_free (buf);

        if (sd != NULL)
        {
            if (attr != NULL)
                *attr = acl_nt_attrs[i];
            return sd;
        }
    }

    if (err != NULL)
        *err = last;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * A readable name for a SID: the well known ones by name, the Unix users and
 * groups Samba writes as S-1-22-1-uid and S-1-22-2-gid by their local name,
 * the rest as the SID itself.
 */
char *
acl_nt_sid_name (const char *sid)
{
    size_t i;

    if (sid == NULL)
        return g_strdup ("?");

    for (i = 0; i < G_N_ELEMENTS (acl_nt_known); i++)
        if (strcmp (sid, acl_nt_known[i].sid) == 0)
            return g_strdup (acl_nt_known[i].name);

    if (g_str_has_prefix (sid, "S-1-22-1-") || g_str_has_prefix (sid, "S-1-22-2-"))
    {
        gboolean group = sid[7] == '2';
        const char *num = sid + 9;
        guint64 id;

        if (g_ascii_string_to_unsigned (num, 10, 0, G_MAXUINT32 - 1, &id, NULL))
        {
            if (group)
            {
                struct group *gr = getgrgid ((gid_t) id);

                if (gr != NULL)
                    return g_strdup_printf ("Unix Group\\%s", gr->gr_name);
                return g_strdup_printf ("Unix Group\\#%" G_GUINT64_FORMAT, id);
            }
            else
            {
                struct passwd *pw = getpwuid ((uid_t) id);

                if (pw != NULL)
                    return g_strdup_printf ("Unix User\\%s", pw->pw_name);
                return g_strdup_printf ("Unix User\\#%" G_GUINT64_FORMAT, id);
            }
        }
    }

    if (g_str_has_prefix (sid, NTFS3G_DOMAIN))
    {
        const char *num = sid + strlen (NTFS3G_DOMAIN);
        guint64 rid;

        if (g_ascii_string_to_unsigned (num, 10, 10000, G_MAXUINT32, &rid, NULL))
        {
            if ((rid & 1) == 0)
            {
                struct passwd *pw = getpwuid ((uid_t) ((rid - 10000) / 2));

                if (pw != NULL)
                    return g_strdup_printf ("Unix User\\%s", pw->pw_name);
            }
            else
            {
                struct group *gr = getgrgid ((gid_t) ((rid - 10001) / 2));

                if (gr != NULL)
                    return g_strdup_printf ("Unix Group\\%s", gr->gr_name);
            }
        }
    }

    return g_strdup (sid);
}

/* --------------------------------------------------------------------------------------------- */

/* The name Windows gives this set of rights, or NULL when it has none. */
const char *
acl_nt_rights_preset (guint32 mask)
{
    int i = acl_nt_preset_index (mask);

    return i < ACL_NT_PRESET_COUNT ? acl_nt_presets[i].name : NULL;
}

/* --------------------------------------------------------------------------------------------- */

int
acl_nt_preset_index (guint32 mask)
{
    guint32 m = acl_nt_map_generic (mask) | ACL_NT_SYNCHRONIZE;
    int i;

    for (i = 0; i < ACL_NT_PRESET_COUNT; i++)
        if (m == acl_nt_presets[i].mask)
            return i;
    return ACL_NT_PRESET_COUNT;
}

/* --------------------------------------------------------------------------------------------- */

/* The rights of @mask one by one, with the names a directory gives them. */
GPtrArray *
acl_nt_rights_list (guint32 mask, gboolean directory)
{
    GPtrArray *list;
    guint32 m = acl_nt_map_generic (mask);
    size_t i;

    list = g_ptr_array_new ();
    for (i = 0; i < G_N_ELEMENTS (acl_nt_rights); i++)
        if ((m & acl_nt_rights[i].bit) != 0)
            g_ptr_array_add (list,
                             (gpointer) (directory ? acl_nt_rights[i].dir : acl_nt_rights[i].file));

    return list;
}

/* --------------------------------------------------------------------------------------------- */

/* Where an ACE of a directory applies, the way the Windows dialog puts it. */
const char *
acl_nt_applies_to (guint8 flags)
{
    gboolean oi = (flags & ACL_NT_OBJECT_INHERIT) != 0;
    gboolean ci = (flags & ACL_NT_CONTAINER_INHERIT) != 0;

    if ((flags & ACL_NT_INHERIT_ONLY) != 0)
    {
        if (oi && ci)
            return N_ ("Subfolders and files only");
        if (ci)
            return N_ ("Subfolders only");
        if (oi)
            return N_ ("Files only");
        return N_ ("Nothing");
    }

    if (oi && ci)
        return N_ ("This folder, subfolders and files");
    if (ci)
        return N_ ("This folder and subfolders");
    if (oi)
        return N_ ("This folder and files");
    return N_ ("This folder only");
}

/* --------------------------------------------------------------------------------------------- */

static void
set16 (guint8 *p, guint16 v)
{
    p[0] = (guint8) (v & 0xff);
    p[1] = (guint8) (v >> 8);
}

/* --------------------------------------------------------------------------------------------- */

static void
set32 (guint8 *p, guint32 v)
{
    p[0] = (guint8) (v & 0xff);
    p[1] = (guint8) ((v >> 8) & 0xff);
    p[2] = (guint8) ((v >> 16) & 0xff);
    p[3] = (guint8) (v >> 24);
}

/* --------------------------------------------------------------------------------------------- */

static void
put32 (GByteArray *b, guint32 v)
{
    guint8 x[4] = { (guint8) (v & 0xff), (guint8) ((v >> 8) & 0xff), (guint8) ((v >> 16) & 0xff),
                    (guint8) (v >> 24) };

    g_byte_array_append (b, x, 4);
}

/* --------------------------------------------------------------------------------------------- */

/* "S-1-5-21-..." in binary form appended to @b; FALSE when it is not a SID. */
static gboolean
put_sid (GByteArray *b, const char *sid)
{
    gchar **parts;
    guint n, i;
    guint64 auth, revision, subauth[15];
    gboolean ok = FALSE;

    if (sid == NULL || !g_str_has_prefix (sid, "S-"))
        return FALSE;

    parts = g_strsplit (sid + 2, "-", -1);
    n = g_strv_length (parts);
    if (n >= 2 && n - 2 <= 15 && g_ascii_string_to_unsigned (parts[0], 10, 1, 1, &revision, NULL)
        && g_ascii_string_to_unsigned (parts[1], 10, 0, G_GUINT64_CONSTANT (0xffffffffffff), &auth,
                                       NULL))
    {
        guint8 head[8];

        for (i = 2; i < n; i++)
            if (!g_ascii_string_to_unsigned (parts[i], 10, 0, G_MAXUINT32, &subauth[i - 2], NULL))
                goto out;

        head[0] = (guint8) revision;
        head[1] = (guint8) (n - 2);
        for (i = 0; i < 6; i++)
            head[2 + i] = (guint8) (auth >> (8 * (5 - i)));
        g_byte_array_append (b, head, 8);
        for (i = 2; i < n; i++)
            put32 (b, (guint32) subauth[i - 2]);
        ok = TRUE;
    }

out:
    g_strfreev (parts);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * A self-relative security descriptor of the owner, the group and the DACL of
 * @sd.  No SACL: reading one needs a privilege, and writing one back unread
 * would drop it.  NULL when a SID does not parse.
 */
guint8 *
acl_nt_build (const acl_nt_sd_t *sd, gsize *size)
{
    GByteArray *b;
    guint32 off_owner = 0, off_group = 0, off_dacl = 0;
    guint16 control;
    gsize dacl_start;
    guint i;

    b = g_byte_array_new ();
    g_byte_array_set_size (b, 20);

    if (sd->owner != NULL)
    {
        off_owner = b->len;
        if (!put_sid (b, sd->owner))
            goto bad;
    }
    if (sd->group != NULL)
    {
        off_group = b->len;
        if (!put_sid (b, sd->group))
            goto bad;
    }

    if (sd->has_dacl)
    {
        guint16 acl_size;

        off_dacl = b->len;
        dacl_start = b->len;
        g_byte_array_set_size (b, b->len + 8);
        for (i = 0; i < sd->aces->len; i++)
        {
            const acl_nt_ace_t *ace = g_ptr_array_index (sd->aces, i);
            gsize start = b->len;
            guint16 ace_size;

            if (ace->raw != NULL)
            {
                gsize raw_size;
                const guint8 *raw = g_bytes_get_data (ace->raw, &raw_size);

                g_byte_array_append (b, raw, raw_size);
                continue;
            }

            g_byte_array_set_size (b, b->len + 4);
            put32 (b, ace->mask);
            if (!put_sid (b, ace->sid))
                goto bad;
            ace_size = (guint16) (b->len - start);
            b->data[start] = ace->type;
            b->data[start + 1] = ace->flags;
            set16 (b->data + start + 2, ace_size);
        }
        if (b->len - dacl_start > G_MAXUINT16 || sd->aces->len > G_MAXUINT16)
            goto bad;
        acl_size = (guint16) (b->len - dacl_start);
        b->data[dacl_start] = sd->dacl_revision != 0 ? sd->dacl_revision : 2;
        b->data[dacl_start + 1] = 0;
        set16 (b->data + dacl_start + 2, acl_size);
        set16 (b->data + dacl_start + 4, (guint16) sd->aces->len);
        set16 (b->data + dacl_start + 6, 0);
    }

    control = (guint16) ((sd->control & ~SE_SACL_PRESENT) | SE_SELF_RELATIVE);
    if (sd->has_dacl)
        control |= SE_DACL_PRESENT;
    b->data[0] = 1;
    b->data[1] = 0;
    set16 (b->data + 2, control);
    set32 (b->data + 4, off_owner);
    set32 (b->data + 8, off_group);
    set32 (b->data + 12, 0);
    set32 (b->data + 16, off_dacl);

    *size = b->len;
    return g_byte_array_free (b, FALSE);

bad:
    g_byte_array_free (b, TRUE);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
acl_nt_dacl_equal (const acl_nt_sd_t *a, const acl_nt_sd_t *b)
{
    guint i;

    if (a->has_dacl != b->has_dacl || ((a->control ^ b->control) & SE_DACL_PRESENT) != 0)
        return FALSE;
    if (a->has_dacl && a->dacl_revision != b->dacl_revision)
        return FALSE;
    if (a->aces->len != b->aces->len)
        return FALSE;

    for (i = 0; i < a->aces->len; i++)
    {
        const acl_nt_ace_t *x = g_ptr_array_index (a->aces, i);
        const acl_nt_ace_t *y = g_ptr_array_index (b->aces, i);

        if (x->type != y->type || x->flags != y->flags || x->mask != y->mask
            || g_strcmp0 (x->sid, y->sid) != 0)
            return FALSE;
        if ((x->raw == NULL) != (y->raw == NULL)
            || (x->raw != NULL && !g_bytes_equal (x->raw, y->raw)))
            return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
acl_nt_has_ace (const acl_nt_sd_t *sd, const acl_nt_ace_t *x)
{
    guint k;

    for (k = 0; k < sd->aces->len; k++)
    {
        const acl_nt_ace_t *y = g_ptr_array_index (sd->aces, k);

        if (x->type == y->type && x->flags == y->flags && x->mask == y->mask
            && g_strcmp0 (x->sid, y->sid) == 0 && (x->raw == NULL) == (y->raw == NULL)
            && (x->raw == NULL || g_bytes_equal (x->raw, y->raw)))
            return TRUE;
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* The same entries, in any order and counting repeats once: Samba hands the
   DACL back in an order of its own, and with repeats of its own. */
gboolean
acl_nt_dacl_same_entries (const acl_nt_sd_t *a, const acl_nt_sd_t *b)
{
    guint i;

    if (a->has_dacl != b->has_dacl || ((a->control ^ b->control) & SE_DACL_PRESENT) != 0)
        return FALSE;
    for (i = 0; i < a->aces->len; i++)
        if (!acl_nt_has_ace (b, g_ptr_array_index (a->aces, i)))
            return FALSE;
    for (i = 0; i < b->aces->len; i++)
        if (!acl_nt_has_ace (a, g_ptr_array_index (b->aces, i)))
            return FALSE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Write @sd into @attr of @path.  cifs answers EACCES when Samba refuses the
 * owner and group that go along with the DACL, after the DACL is set.  A
 * refused write is therefore read back: the entries written, in any order,
 * mean it went through; the list of @before still there means it did not;
 * anything else is the server's own form of the list.
 */
acl_nt_write_t
acl_nt_write (const char *path, const char *attr, const acl_nt_sd_t *before, const acl_nt_sd_t *sd,
              int *err)
{
    guint8 *data;
    gsize size;
    int saved;
    acl_nt_write_t result = ACL_NT_WRITE_FAILED;

    data = acl_nt_build (sd, &size);
    if (data == NULL)
    {
        if (err != NULL)
            *err = EINVAL;
        return ACL_NT_WRITE_FAILED;
    }

    if (setxattr (path, attr, data, size, 0) == 0)
    {
        g_free (data);
        return ACL_NT_WRITE_OK;
    }
    saved = errno;
    g_free (data);

    if (saved == EACCES || saved == EPERM)
    {
        acl_nt_sd_t *back = acl_nt_read (path, NULL, NULL);

        if (back != NULL)
        {
            if (acl_nt_dacl_same_entries (back, sd))
                result = ACL_NT_WRITE_OK;
            else if (before == NULL || !acl_nt_dacl_same_entries (back, before))
                result = ACL_NT_WRITE_CHANGED;
            acl_nt_free (back);
        }
    }

    if (result == ACL_NT_WRITE_FAILED && err != NULL)
        *err = saved;
    return result;
}

/* --------------------------------------------------------------------------------------------- */

acl_nt_ace_t *
acl_nt_ace_new (guint8 type, guint8 flags, guint32 mask, const char *sid)
{
    acl_nt_ace_t *ace = g_new0 (acl_nt_ace_t, 1);

    ace->type = type;
    ace->flags = flags;
    ace->mask = mask;
    ace->sid = g_strdup (sid);
    return ace;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Put a new entry where Windows keeps it: explicit denies first, then explicit
 * allows, then what is inherited.
 */
void
acl_nt_insert_ace (acl_nt_sd_t *sd, acl_nt_ace_t *ace)
{
    guint i;

    for (i = 0; i < sd->aces->len; i++)
    {
        const acl_nt_ace_t *a = g_ptr_array_index (sd->aces, i);

        if ((a->flags & ACL_NT_INHERITED) != 0)
            break;
        if (ace->type == ACL_NT_ACE_DENIED && a->type != ACL_NT_ACE_DENIED)
            break;
    }

    g_ptr_array_insert (sd->aces, (gint) i, ace);
    sd->has_dacl = TRUE;
    if (sd->dacl_revision == 0)
        sd->dacl_revision = 2;
    sd->control |= SE_DACL_PRESENT;
}

/* --------------------------------------------------------------------------------------------- */

/* Takes ownership of the replacement; only a type change moves the entry. */
void
acl_nt_replace_ace (acl_nt_sd_t *sd, guint index, acl_nt_ace_t *ace)
{
    acl_nt_ace_t *old = g_ptr_array_index (sd->aces, index);

    if (old->type == ace->type)
    {
        g_ptr_array_index (sd->aces, index) = ace;
        acl_nt_ace_free (old);
    }
    else
    {
        g_ptr_array_remove_index (sd->aces, index);
        acl_nt_insert_ace (sd, ace);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Spaces belong to Windows account names; only commas separate entries. */
gchar **
acl_nt_split_names (const char *text)
{
    gchar **parts = g_strsplit (text != NULL ? text : "", ",", -1);
    GPtrArray *names = g_ptr_array_new ();
    int i;

    for (i = 0; parts[i] != NULL; i++)
    {
        char *name = g_strstrip (parts[i]);

        if (*name != '\0')
            g_ptr_array_add (names, g_strdup (name));
    }
    g_strfreev (parts);
    g_ptr_array_add (names, NULL);
    return (gchar **) g_ptr_array_free (names, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The SID to write for a name typed or picked: a SID as it is, a well known
 * name, or a local user or group ("alice", "Unix User\\alice") in the form the
 * file system understands: S-1-22-1-uid on cifs, the ntfs-3g domain on ntfs.
 * NULL when the name is not known.
 */
char *
acl_nt_sid_for (const char *name, gboolean group, const char *attr)
{
    size_t i;
    const char *p = name;
    guint64 id;

    if (name == NULL || *name == '\0')
        return NULL;
    if (g_str_has_prefix (name, "S-1-"))
    {
        GByteArray *binary = g_byte_array_new ();
        gboolean valid = put_sid (binary, name);

        g_byte_array_free (binary, TRUE);
        return valid ? g_strdup (name) : NULL;
    }

    for (i = 0; i < G_N_ELEMENTS (acl_nt_known); i++)
        if (g_ascii_strcasecmp (name, acl_nt_known[i].name) == 0)
            return g_strdup (acl_nt_known[i].sid);

    if (g_str_has_prefix (p, "Unix User\\"))
    {
        p += strlen ("Unix User\\");
        group = FALSE;
    }
    else if (g_str_has_prefix (p, "Unix Group\\"))
    {
        p += strlen ("Unix Group\\");
        group = TRUE;
    }

    if (*p == '#')
    {
        if (!g_ascii_string_to_unsigned (p + 1, 10, 0, G_MAXUINT32 - 1, &id, NULL))
            return NULL;
    }
    else if (group)
    {
        struct group *gr = getgrnam (p);

        if (gr == NULL)
            return NULL;
        id = gr->gr_gid;
    }
    else
    {
        struct passwd *pw = getpwnam (p);

        if (pw == NULL)
            return NULL;
        id = pw->pw_uid;
    }

    if (attr != NULL && strcmp (attr, "system.cifs_acl") != 0)
    {
        guint64 rid = (group ? 10001 : 10000) + 2 * id;

        if (rid > G_MAXUINT32)
            return NULL;
        return g_strdup_printf ("%s%" G_GUINT64_FORMAT, NTFS3G_DOMAIN, rid);
    }
    return g_strdup_printf ("S-1-22-%d-%" G_GUINT64_FORMAT, group ? 2 : 1, id);
}
