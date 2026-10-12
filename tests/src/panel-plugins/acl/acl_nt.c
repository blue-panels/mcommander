/*
   src/panel-plugins/acl - tests for the Windows security descriptors

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

#define TEST_SUITE_NAME "/src/panel-plugins/acl"

#include "tests/mctest.h"

#include <string.h>

#include "src/panel-plugins/acl/acl-nt.h"

/* --------------------------------------------------------------------------------------------- */

/* system.cifs_acl of a file written through cifs to Samba with acl_xattr */
static const guint8 sd_cifs[212] = {
    0x01, 0x00, 0x04, 0x80, 0x14, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x15, 0x00, 0x00, 0x00,
    0x5c, 0x27, 0xa8, 0xef, 0xee, 0x73, 0x08, 0xd4, 0x1f, 0xa3, 0xa7, 0xe3, 0xe8, 0x03, 0x00, 0x00,
    0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x02, 0x00, 0x00, 0x00, 0xe8, 0x03, 0x00, 0x00,
    0x02, 0x00, 0x94, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0xff, 0x01, 0x1f, 0x00,
    0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x15, 0x00, 0x00, 0x00, 0x5c, 0x27, 0xa8, 0xef,
    0xee, 0x73, 0x08, 0xd4, 0x1f, 0xa3, 0xa7, 0xe3, 0xe8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00,
    0xa9, 0x00, 0x12, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x02, 0x00, 0x00, 0x00,
    0xe8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0xa9, 0x00, 0x12, 0x00, 0x01, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x16, 0x02, 0x00, 0x00, 0x00, 0xe8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00,
    0xff, 0x01, 0x1f, 0x00, 0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x15, 0x00, 0x00, 0x00,
    0x5c, 0x27, 0xa8, 0xef, 0xee, 0x73, 0x08, 0xd4, 0x1f, 0xa3, 0xa7, 0xe3, 0xe8, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x14, 0x00, 0xa9, 0x00, 0x12, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
};

/* system.ntfs_acl of a file root wrote on ntfs-3g with the default user mapping */
static const guint8 sd_ntfs[172] = {
    0x01, 0x00, 0x04, 0x90, 0x8c, 0x00, 0x00, 0x00, 0x9c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x00, 0x00, 0x02, 0x00, 0x78, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x04, 0x18, 0x00,
    0x9f, 0x01, 0x1f, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x20, 0x00, 0x00, 0x00,
    0x20, 0x02, 0x00, 0x00, 0x00, 0x04, 0x18, 0x00, 0x89, 0x00, 0x12, 0x00, 0x01, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x05, 0x20, 0x00, 0x00, 0x00, 0x20, 0x02, 0x00, 0x00, 0x00, 0x04, 0x14, 0x00,
    0x89, 0x00, 0x12, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x18, 0x00, 0xbf, 0x01, 0x1f, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0x20, 0x00, 0x00, 0x00, 0x20, 0x02, 0x00, 0x00, 0x00, 0x04, 0x14, 0x00, 0xbf, 0x01, 0x1f, 0x00,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x12, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x05, 0x20, 0x00, 0x00, 0x00, 0x20, 0x02, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x05, 0x20, 0x00, 0x00, 0x00, 0x20, 0x02, 0x00, 0x00,
};

/* --------------------------------------------------------------------------------------------- */

static const acl_nt_ace_t *
ace (const acl_nt_sd_t *sd, guint i)
{
    ck_assert_uint_lt (i, sd->aces->len);
    return g_ptr_array_index (sd->aces, i);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_parse_cifs)
{
    acl_nt_sd_t *sd = acl_nt_parse (sd_cifs, sizeof (sd_cifs));

    ck_assert (sd != NULL);
    ck_assert_str_eq (sd->owner, "S-1-5-21-4020774748-3557323758-3819414303-1000");
    ck_assert_str_eq (sd->group, "S-1-22-2-1000");
    ck_assert (sd->has_dacl);
    ck_assert_uint_eq (sd->aces->len, 5);

    ck_assert_uint_eq (ace (sd, 0)->type, ACL_NT_ACE_ALLOWED);
    ck_assert_uint_eq (ace (sd, 0)->mask, 0x001f01ff);
    ck_assert_str_eq (acl_nt_rights_preset (ace (sd, 0)->mask), "Full control");
    ck_assert_uint_eq (ace (sd, 1)->mask, 0x001200a9);
    ck_assert_str_eq (acl_nt_rights_preset (ace (sd, 1)->mask), "Read & execute");
    ck_assert_str_eq (ace (sd, 4)->sid, "S-1-1-0");

    acl_nt_free (sd);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_parse_ntfs)
{
    acl_nt_sd_t *sd = acl_nt_parse (sd_ntfs, sizeof (sd_ntfs));
    char *name;

    ck_assert (sd != NULL);
    ck_assert_str_eq (sd->owner, "S-1-5-32-544");
    ck_assert_uint_eq (sd->aces->len, 5);
    ck_assert_uint_eq (ace (sd, 0)->flags, ACL_NT_NO_PROPAGATE_INHERIT);
    ck_assert_str_eq (acl_nt_rights_preset (ace (sd, 1)->mask), "Read");
    /* full control without traverse and delete child has no name of its own */
    ck_assert (acl_nt_rights_preset (ace (sd, 0)->mask) == NULL);
    ck_assert_str_eq (ace (sd, 4)->sid, "S-1-5-18");

    name = acl_nt_sid_name (sd->owner);
    ck_assert_str_eq (name, "Administrators");
    g_free (name);

    acl_nt_free (sd);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A descriptor cut anywhere is refused, not read past its end. */
START_TEST (test_parse_truncated)
{
    gsize n;

    for (n = 0; n < sizeof (sd_cifs); n++)
    {
        acl_nt_sd_t *sd = acl_nt_parse (sd_cifs, n);

        if (sd != NULL)
        {
            ck_assert_uint_le (sd->aces->len, 5);
            acl_nt_free (sd);
        }
    }

    ck_assert (acl_nt_parse (NULL, 0) == NULL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Large 32-bit offsets must not wrap when a descriptor is parsed on 32-bit hosts. */
START_TEST (test_parse_large_offsets)
{
    static const guint8 huge_offset[4] = { 0xfc, 0xff, 0xff, 0xff };
    guint8 data[sizeof (sd_cifs)];
    guint i;

    for (i = 0; i < 4; i++)
    {
        acl_nt_sd_t *sd;

        memcpy (data, sd_cifs, sizeof (data));
        memcpy (data + 4 + 4 * i, huge_offset, sizeof (huge_offset));
        if (i == 2)
            data[2] |= 0x10; /* SE_SACL_PRESENT */

        sd = acl_nt_parse (data, sizeof (data));
        if (i < 2)
        {
            ck_assert (sd != NULL);
            ck_assert (i == 0 ? sd->owner == NULL : sd->group == NULL);
            acl_nt_free (sd);
        }
        else
            ck_assert (sd == NULL);
    }
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sid_names)
{
    char *s;

    s = acl_nt_sid_name ("S-1-1-0");
    ck_assert_str_eq (s, "Everyone");
    g_free (s);
    s = acl_nt_sid_name ("S-1-22-1-0");
    ck_assert_str_eq (s, "Unix User\\root");
    g_free (s);
    s = acl_nt_sid_name ("S-1-22-2-0");
    ck_assert_str_eq (s, "Unix Group\\root");
    g_free (s);
    s = acl_nt_sid_name ("S-1-5-21-1-2-3-500");
    ck_assert_str_eq (s, "S-1-5-21-1-2-3-500");
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_rights)
{
    GPtrArray *l;

    ck_assert_str_eq (acl_nt_rights_preset (0x10000000), "Full control");
    ck_assert_str_eq (acl_nt_rights_preset (0x001301bf), "Modify");
    ck_assert_str_eq (acl_nt_rights_preset (0x00100116), "Write");

    l = acl_nt_rights_list (0x00000021, TRUE);
    ck_assert_uint_eq (l->len, 2);
    ck_assert_str_eq ((const char *) g_ptr_array_index (l, 0), "Traverse folder");
    ck_assert_str_eq ((const char *) g_ptr_array_index (l, 1), "List folder");
    g_ptr_array_free (l, TRUE);

    ck_assert_str_eq (acl_nt_applies_to (0x03), "This folder, subfolders and files");
    ck_assert_str_eq (acl_nt_applies_to (0x0b), "Subfolders and files only");
    ck_assert_str_eq (acl_nt_applies_to (0x00), "This folder only");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static void
check_round_trip (const guint8 *data, gsize size, gboolean same_bytes)
{
    acl_nt_sd_t *a, *b;
    guint8 *built;
    gsize n;

    a = acl_nt_parse (data, size);
    ck_assert (a != NULL);
    built = acl_nt_build (a, &n);
    ck_assert (built != NULL);
    b = acl_nt_parse (built, n);
    ck_assert (b != NULL);

    ck_assert_str_eq (a->owner, b->owner);
    ck_assert_str_eq (a->group, b->group);
    ck_assert (acl_nt_dacl_equal (a, b));
    if (same_bytes)
    {
        ck_assert_uint_eq (n, size);
        ck_assert (memcmp (built, data, n) == 0);
    }

    g_free (built);
    acl_nt_free (a);
    acl_nt_free (b);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_build)
{
    check_round_trip (sd_cifs, sizeof (sd_cifs), TRUE);
    check_round_trip (sd_ntfs, sizeof (sd_ntfs), FALSE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* An object ACE carries fields before its SID. Keep those bytes when another
   entry in the same DACL is edited. */
START_TEST (test_build_with_object_ace)
{
    guint8 data[sizeof (sd_cifs) + 4];
    acl_nt_sd_t *sd, *back;
    guint8 *built;
    gsize size;

    memcpy (data, sd_cifs, 80);
    memset (data + 80, 0, 4); /* object flags: no GUIDs */
    memcpy (data + 84, sd_cifs + 80, sizeof (sd_cifs) - 80);
    data[64] = 4;    /* ACL revision for object ACEs */
    data[66] = 0x98; /* ACL size: 152 */
    data[72] = 5;    /* ACCESS_ALLOWED_OBJECT_ACE_TYPE */
    data[74] = 40;   /* first ACE size */

    sd = acl_nt_parse (data, sizeof (data));
    ck_assert (sd != NULL);
    ck_assert_uint_eq (sd->dacl_revision, 4);
    ck_assert (ace (sd, 0)->raw != NULL);

    built = acl_nt_build (sd, &size);
    ck_assert (built != NULL);
    ck_assert_uint_eq (size, sizeof (data));
    ck_assert (memcmp (built, data, size) == 0);
    g_free (built);

    ((acl_nt_ace_t *) g_ptr_array_index (sd->aces, 1))->mask ^= 1;
    built = acl_nt_build (sd, &size);
    ck_assert (built != NULL);
    back = acl_nt_parse (built, size);
    ck_assert (back != NULL);
    ck_assert (acl_nt_dacl_equal (sd, back));
    ck_assert (memcmp (g_bytes_get_data (ace (back, 0)->raw, NULL), data + 72, 40) == 0);

    acl_nt_free (back);
    g_free (built);
    acl_nt_free (sd);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* An audit ACL is outside the editor, but must survive a DACL write. */
START_TEST (test_build_with_sacl)
{
    static const guint8 empty_sacl[8] = { 2, 0, 8, 0, 0, 0, 0, 0 };
    static const guint8 sacl_only[28] = {
        1, 0, 0x10, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0,
        0, 0, 0, 0, 2, 0, 8, 0, 0, 0, 0, 0,
    };
    guint8 with_dacl[sizeof (sd_cifs) + sizeof (empty_sacl)];
    acl_nt_sd_t *sd, *back;
    guint8 *built;
    gsize size;

    sd = acl_nt_parse (sacl_only, sizeof (sacl_only));
    ck_assert (sd != NULL);
    ck_assert (sd->sacl != NULL);
    built = acl_nt_build (sd, &size);
    ck_assert (built != NULL);
    ck_assert_uint_eq (size, sizeof (sacl_only));
    ck_assert (memcmp (built, sacl_only, size) == 0);
    g_free (built);
    acl_nt_free (sd);

    memcpy (with_dacl, sd_cifs, sizeof (sd_cifs));
    memcpy (with_dacl + sizeof (sd_cifs), empty_sacl, sizeof (empty_sacl));
    with_dacl[2] |= 0x10; /* SE_SACL_PRESENT */
    with_dacl[12] = (guint8) sizeof (sd_cifs);

    sd = acl_nt_parse (with_dacl, sizeof (with_dacl));
    ck_assert (sd != NULL);
    ck_assert (sd->sacl != NULL);
    built = acl_nt_build (sd, &size);
    ck_assert (built != NULL);
    back = acl_nt_parse (built, size);
    ck_assert (back != NULL);
    ck_assert (back->sacl != NULL);
    ck_assert (g_bytes_equal (sd->sacl, back->sacl));
    ck_assert (acl_nt_dacl_equal (sd, back));

    acl_nt_free (back);
    g_free (built);
    acl_nt_free (sd);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Changes outside the DACL made while the dialog is open must survive Set. */
START_TEST (test_build_update_keeps_current_metadata)
{
    static const guint8 old_sacl[8] = { 2, 0, 8, 0, 0, 0, 0, 0 };
    static const guint8 current_sacl[8] = { 2, 0, 8, 0, 0, 0, 0, 1 };
    acl_nt_sd_t *edited = acl_nt_parse (sd_cifs, sizeof (sd_cifs));
    acl_nt_sd_t *before = acl_nt_parse (sd_cifs, sizeof (sd_cifs));
    acl_nt_sd_t *back;
    guint8 *built;
    gsize size;

    ck_assert (edited != NULL);
    ck_assert (before != NULL);
    edited->control |= 0x0010; /* SE_SACL_PRESENT */
    edited->sacl = g_bytes_new_static (old_sacl, sizeof (old_sacl));
    before->control |= 0x0010;
    before->sacl = g_bytes_new_static (current_sacl, sizeof (current_sacl));
    g_free (before->owner);
    before->owner = g_strdup ("S-1-1-0");
    ((acl_nt_ace_t *) g_ptr_array_index (edited->aces, 0))->mask ^= 1;

    built = acl_nt_build_update (before, edited, &size);
    ck_assert (built != NULL);
    back = acl_nt_parse (built, size);
    ck_assert (back != NULL);
    ck_assert_str_eq (back->owner, before->owner);
    ck_assert (back->sacl != NULL);
    ck_assert (g_bytes_equal (back->sacl, before->sacl));
    ck_assert (acl_nt_dacl_equal (back, edited));
    acl_nt_free (back);
    g_free (built);

    g_bytes_unref (before->sacl);
    before->sacl = NULL;
    before->control &= ~0x0010;
    built = acl_nt_build_update (before, edited, &size);
    ck_assert (built != NULL);
    back = acl_nt_parse (built, size);
    ck_assert (back != NULL);
    ck_assert (back->sacl == NULL);
    ck_assert ((back->control & 0x0010) == 0);
    ck_assert (acl_nt_dacl_equal (back, edited));

    acl_nt_free (back);
    g_free (built);
    acl_nt_free (before);
    acl_nt_free (edited);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The original DACL remains available for conflict checks after dialog edits. */
START_TEST (test_dacl_copy_snapshot)
{
    static const guint8 raw_ace[8] = { 5, 0, 8, 0, 1, 0, 0, 0 };
    acl_nt_sd_t *sd = acl_nt_parse (sd_cifs, sizeof (sd_cifs));
    acl_nt_sd_t *original;
    acl_nt_ace_t *entry;
    guint32 mask;

    ck_assert (sd != NULL);
    entry = acl_nt_ace_new (5, 0, 1, NULL);
    entry->raw = g_bytes_new_static (raw_ace, sizeof (raw_ace));
    g_ptr_array_add (sd->aces, entry);
    original = acl_nt_dacl_copy (sd);
    ck_assert (acl_nt_dacl_equal (original, sd));

    entry = g_ptr_array_index (sd->aces, 0);
    mask = entry->mask;
    entry->mask ^= 1;
    ck_assert (!acl_nt_dacl_equal (original, sd));
    entry->mask = mask;
    ck_assert (acl_nt_dacl_equal (original, sd));

    acl_nt_free (sd);
    entry = g_ptr_array_index (original->aces, 5);
    ck_assert (entry->raw != NULL);
    ck_assert_uint_eq (g_bytes_get_size (entry->raw), sizeof (raw_ace));
    ck_assert (memcmp (g_bytes_get_data (entry->raw, NULL), raw_ace, sizeof (raw_ace)) == 0);
    acl_nt_free (original);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Absent, NULL and empty DACLs have different access semantics. */
START_TEST (test_build_dacl_states)
{
    static const guint8 descriptors[][28] = {
        { 1, 0, 0, 0x80 }, /* no DACL present */
        { 1, 0, 4, 0x80 }, /* present, NULL DACL */
        { 1, 0, 4, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 2, 0, 8, 0 }, /* present,
                                                                                           empty
                                                                                           DACL */
    };
    acl_nt_sd_t *sd[3];
    guint i;

    for (i = 0; i < G_N_ELEMENTS (descriptors); i++)
    {
        gsize size = i == 2 ? 28 : 20, built_size;
        guint8 *built;

        sd[i] = acl_nt_parse (descriptors[i], size);
        ck_assert (sd[i] != NULL);
        built = acl_nt_build (sd[i], &built_size);
        ck_assert (built != NULL);
        ck_assert_uint_eq (built_size, size);
        ck_assert (memcmp (built, descriptors[i], size) == 0);
        g_free (built);
    }
    for (i = 0; i < 2; i++)
    {
        ck_assert (!acl_nt_dacl_equal (sd[i], sd[2]));
        ck_assert (!acl_nt_dacl_same_entries (sd[i], sd[2]));

        /* Adding an entry creates a real DACL; deleting it leaves an empty one. */
        acl_nt_insert_ace (sd[i], acl_nt_ace_new (ACL_NT_ACE_ALLOWED, 0, 1, "S-1-1-0"));
        ck_assert (sd[i]->has_dacl);
        g_ptr_array_set_size (sd[i]->aces, 0);
        ck_assert (acl_nt_dacl_equal (sd[i], sd[2]));
    }
    for (i = 0; i < G_N_ELEMENTS (sd); i++)
        acl_nt_free (sd[i]);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sid_for)
{
    char *s;

    s = acl_nt_sid_for ("Everyone", FALSE, "system.cifs_acl");
    ck_assert_str_eq (s, "S-1-1-0");
    g_free (s);
    s = acl_nt_sid_for ("root", FALSE, "system.cifs_acl");
    ck_assert_str_eq (s, "S-1-22-1-0");
    g_free (s);
    s = acl_nt_sid_for ("Unix Group\\root", FALSE, "system.cifs_acl");
    ck_assert_str_eq (s, "S-1-22-2-0");
    g_free (s);
    s = acl_nt_sid_for ("root", TRUE, "system.ntfs_acl");
    ck_assert_str_eq (s, "S-1-5-21-3141592653-589793238-462843383-10001");
    g_free (s);
    s = acl_nt_sid_for ("S-1-5-32-545", FALSE, NULL);
    ck_assert_str_eq (s, "S-1-5-32-545");
    g_free (s);
    ck_assert (acl_nt_sid_for ("no-such-user-here", FALSE, NULL) == NULL);

    s = acl_nt_sid_name ("S-1-5-21-3141592653-589793238-462843383-10000");
    ck_assert_str_eq (s, "Unix User\\root");
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Explicit denies first, explicit allows next, inherited entries last. */
START_TEST (test_insert_order)
{
    acl_nt_sd_t *sd = acl_nt_parse (sd_cifs, sizeof (sd_cifs));
    const acl_nt_ace_t *a;

    ck_assert (sd != NULL);
    acl_nt_insert_ace (sd, acl_nt_ace_new (ACL_NT_ACE_ALLOWED, ACL_NT_INHERITED, 1, "S-1-5-18"));
    acl_nt_insert_ace (sd, acl_nt_ace_new (ACL_NT_ACE_ALLOWED, 0, 2, "S-1-5-11"));
    acl_nt_insert_ace (sd, acl_nt_ace_new (ACL_NT_ACE_DENIED, 0, 4, "S-1-1-0"));

    a = g_ptr_array_index (sd->aces, 0);
    ck_assert_uint_eq (a->type, ACL_NT_ACE_DENIED);
    /* the new allow comes after the five explicit allows the file had, before
       the inherited one */
    a = g_ptr_array_index (sd->aces, 6);
    ck_assert_str_eq (a->sid, "S-1-5-11");
    a = g_ptr_array_index (sd->aces, 7);
    ck_assert_uint_eq (a->flags, ACL_NT_INHERITED);

    acl_nt_free (sd);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sid_validation)
{
    static const char *const invalid[] = {
        "S-1-1-4294967296",
        "S-1-281474976710656-0",
        "S-1-1-18446744073709551616",
        "S-1-1-abc",
        "S-1-1-",
        "S-1--1",
        "S-1-1-+1",
        "S-1-1- 1",
        "S-2-1-0",
        "S-1-1-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0",
    };
    guint i;

    for (i = 0; i < G_N_ELEMENTS (invalid); i++)
    {
        acl_nt_sd_t *sd = acl_nt_parse (sd_cifs, sizeof (sd_cifs));
        gsize size;

        ck_assert (acl_nt_sid_for (invalid[i], FALSE, NULL) == NULL);
        acl_nt_insert_ace (sd, acl_nt_ace_new (ACL_NT_ACE_ALLOWED, 0, 1, invalid[i]));
        ck_assert (acl_nt_build (sd, &size) == NULL);
        acl_nt_free (sd);
    }
    {
        const char *sid = "S-1-281474976710655-4294967295";
        acl_nt_sd_t *sd = acl_nt_parse (sd_cifs, sizeof (sd_cifs)), *back;
        gsize size;
        guint8 *built;
        char *resolved = acl_nt_sid_for (sid, FALSE, NULL);

        ck_assert_str_eq (resolved, sid);
        g_free (resolved);
        acl_nt_insert_ace (sd, acl_nt_ace_new (ACL_NT_ACE_DENIED, 0, 1, sid));
        built = acl_nt_build (sd, &size);
        ck_assert (built != NULL);
        back = acl_nt_parse (built, size);
        ck_assert (back != NULL);
        ck_assert_str_eq (ace (back, 0)->sid, sid);
        g_free (built);
        acl_nt_free (back);
        acl_nt_free (sd);
    }
    ck_assert (acl_nt_sid_for ("Unix User\\#4294967296", FALSE, "system.cifs_acl") == NULL);
    ck_assert (acl_nt_sid_for ("Unix Group\\#2147483647", TRUE, "system.ntfs_acl") == NULL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_windows_names)
{
    gchar **names = acl_nt_split_names (
        " Authenticated Users, Unix User\\root, ,Unix Group\\#100000007, CREATOR OWNER ");
    char *sid;

    ck_assert_uint_eq (g_strv_length (names), 4);
    ck_assert_str_eq (names[0], "Authenticated Users");
    ck_assert_str_eq (names[1], "Unix User\\root");
    ck_assert_str_eq (names[2], "Unix Group\\#100000007");
    ck_assert_str_eq (names[3], "CREATOR OWNER");
    sid = acl_nt_sid_for (names[0], FALSE, NULL);
    ck_assert_str_eq (sid, "S-1-5-11");
    g_free (sid);
    sid = acl_nt_sid_for (names[1], FALSE, "system.cifs_acl");
    ck_assert_str_eq (sid, "S-1-22-1-0");
    g_free (sid);
    sid = acl_nt_sid_for (names[2], TRUE, "system.cifs_acl");
    ck_assert_str_eq (sid, "S-1-22-2-100000007");
    g_free (sid);
    g_strfreev (names);
    names = acl_nt_split_names (NULL);
    ck_assert_uint_eq (g_strv_length (names), 0);
    g_strfreev (names);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_replace_order)
{
    acl_nt_sd_t *sd = acl_nt_parse (sd_cifs, sizeof (sd_cifs));
    acl_nt_ace_t *changed;

    changed = acl_nt_ace_new (ACL_NT_ACE_DENIED, 0, 1, "S-1-1-0");
    acl_nt_replace_ace (sd, 4, changed);
    ck_assert (ace (sd, 0) == changed);
    ck_assert_uint_eq (sd->aces->len, 5);
    changed = acl_nt_ace_new (ACL_NT_ACE_ALLOWED, 0, 2, "S-1-1-0");
    acl_nt_replace_ace (sd, 0, changed);
    ck_assert (ace (sd, 4) == changed);
    changed = acl_nt_ace_new (ACL_NT_ACE_ALLOWED, 0, 3, "S-1-5-11");
    acl_nt_replace_ace (sd, 1, changed);
    ck_assert (ace (sd, 1) == changed);
    ck_assert_uint_eq (sd->aces->len, 5);
    acl_nt_free (sd);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_parse_cifs);
    tcase_add_test (tc_core, test_parse_ntfs);
    tcase_add_test (tc_core, test_parse_truncated);
    tcase_add_test (tc_core, test_parse_large_offsets);
    tcase_add_test (tc_core, test_sid_names);
    tcase_add_test (tc_core, test_rights);
    tcase_add_test (tc_core, test_build);
    tcase_add_test (tc_core, test_build_with_object_ace);
    tcase_add_test (tc_core, test_build_with_sacl);
    tcase_add_test (tc_core, test_build_update_keeps_current_metadata);
    tcase_add_test (tc_core, test_dacl_copy_snapshot);
    tcase_add_test (tc_core, test_build_dacl_states);
    tcase_add_test (tc_core, test_sid_for);
    tcase_add_test (tc_core, test_insert_order);
    tcase_add_test (tc_core, test_sid_validation);
    tcase_add_test (tc_core, test_windows_names);
    tcase_add_test (tc_core, test_replace_order);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
