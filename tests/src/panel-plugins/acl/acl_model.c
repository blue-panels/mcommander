/*
   src/panel-plugins/acl - tests for the access control list model

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

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "src/panel-plugins/acl/acl-model.h"

/* --------------------------------------------------------------------------------------------- */

#define R ACL_PERM_READ
#define W ACL_PERM_WRITE
#define X ACL_PERM_EXECUTE

static aclm_list_t *
base (unsigned int owner, unsigned int group, unsigned int other)
{
    aclm_list_t *l = aclm_list_new ();

    aclm_list_set (l, ACLM_OWNER, 0, owner);
    aclm_list_set (l, ACLM_GROUP_OBJ, 0, group);
    aclm_list_set (l, ACLM_OTHER, 0, other);
    return l;
}

/* --------------------------------------------------------------------------------------------- */

static unsigned int
perm_of (const aclm_list_t *l, aclm_kind_t kind, unsigned int id)
{
    const aclm_entry_t *e = aclm_list_find (l, kind, id);

    ck_assert_msg (e != NULL, "no entry %d/%u", (int) kind, id);
    return e->perm;
}

/* --------------------------------------------------------------------------------------------- */

/* The mask is the union of the named entries and the group of the file. */
START_TEST (test_calc_mask)
{
    aclm_list_t *l = base (R | W | X, R, 0);

    aclm_calc_mask (l);
    ck_assert (aclm_list_find (l, ACLM_MASK, 0) == NULL);

    aclm_list_set (l, ACLM_USER, 1001, R | W);
    aclm_list_set (l, ACLM_GROUP, 50, X);
    aclm_calc_mask (l);
    ck_assert_uint_eq (perm_of (l, ACLM_MASK, 0), R | W | X);

    aclm_list_remove (l, ACLM_USER, 1001);
    aclm_list_remove (l, ACLM_GROUP, 50);
    aclm_calc_mask (l);
    ck_assert (aclm_list_find (l, ACLM_MASK, 0) == NULL);

    aclm_list_free (l);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_effective)
{
    aclm_list_t *l = base (R | W | X, R | W | X, R);
    const aclm_entry_t *e;

    aclm_list_set (l, ACLM_USER, 1001, R | W | X);
    aclm_list_set (l, ACLM_MASK, 0, R | X);

    e = aclm_list_find (l, ACLM_USER, 1001);
    ck_assert_uint_eq (aclm_effective (l, e), R | X);
    e = aclm_list_find (l, ACLM_GROUP_OBJ, 0);
    ck_assert_uint_eq (aclm_effective (l, e), R | X);
    /* owner and other are not limited by the mask */
    e = aclm_list_find (l, ACLM_OWNER, 0);
    ck_assert_uint_eq (aclm_effective (l, e), R | W | X);
    e = aclm_list_find (l, ACLM_OTHER, 0);
    ck_assert_uint_eq (aclm_effective (l, e), R);

    aclm_list_free (l);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* With the mask kept by hand, a named entry still gets a mask, and loses it
   with the last named entry. */
START_TEST (test_mask_by_hand)
{
    aclm_list_t *l = base (R | W, R, 0);

    aclm_list_set (l, ACLM_USER, 1001, R | W);
    aclm_drop_unused_mask (l);
    ck_assert_uint_eq (perm_of (l, ACLM_MASK, 0), R | W);

    aclm_list_set (l, ACLM_MASK, 0, R);
    aclm_drop_unused_mask (l);
    ck_assert_uint_eq (perm_of (l, ACLM_MASK, 0), R);

    aclm_list_remove (l, ACLM_USER, 1001);
    aclm_drop_unused_mask (l);
    ck_assert (aclm_list_find (l, ACLM_MASK, 0) == NULL);

    aclm_list_free (l);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_list_order)
{
    aclm_list_t *l = aclm_list_new ();
    const aclm_kind_t order[] = { ACLM_OWNER, ACLM_USER, ACLM_USER, ACLM_GROUP_OBJ,
                                  ACLM_GROUP, ACLM_MASK, ACLM_OTHER };
    guint i;

    aclm_list_set (l, ACLM_OTHER, 0, 0);
    aclm_list_set (l, ACLM_MASK, 0, R);
    aclm_list_set (l, ACLM_GROUP, 7, R);
    aclm_list_set (l, ACLM_USER, 2000, R);
    aclm_list_set (l, ACLM_GROUP_OBJ, 0, R);
    aclm_list_set (l, ACLM_USER, 1000, R);
    aclm_list_set (l, ACLM_OWNER, 0, R);

    ck_assert_uint_eq (l->entries->len, G_N_ELEMENTS (order));
    for (i = 0; i < l->entries->len; i++)
        ck_assert_int_eq (g_array_index (l->entries, aclm_entry_t, i).kind, order[i]);
    ck_assert_uint_eq (g_array_index (l->entries, aclm_entry_t, 1).id, 1000);

    /* setting an entry again changes it, it does not add a second one */
    aclm_list_set (l, ACLM_USER, 1000, R | W);
    ck_assert_uint_eq (l->entries->len, G_N_ELEMENTS (order));
    ck_assert_uint_eq (perm_of (l, ACLM_USER, 1000), R | W);

    aclm_list_free (l);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* "Changes to all": only what the dialog changed reaches the other file. */
START_TEST (test_changes_to_all)
{
    aclm_list_t *before = base (R | W | X, R | X, 0);
    aclm_list_t *after;
    aclm_list_t *other = base (R | W, R, R);
    GArray *changes;

    aclm_list_set (before, ACLM_USER, 1002, R); /* bob */
    aclm_calc_mask (before);

    after = aclm_list_copy (before);
    aclm_list_set (after, ACLM_USER, 1001, R | W); /* add alice */
    aclm_list_remove (after, ACLM_USER, 1002);     /* remove bob */
    aclm_calc_mask (after);

    /* the other file has its own admin entry and no bob */
    aclm_list_set (other, ACLM_USER, 1003, R | W | X);
    aclm_calc_mask (other);

    changes = aclm_diff (before, after, ACLM_ACCESS, NULL);
    aclm_apply (other, changes, ACLM_ACCESS, TRUE);

    ck_assert_uint_eq (perm_of (other, ACLM_USER, 1001), R | W);
    ck_assert_uint_eq (perm_of (other, ACLM_USER, 1003), R | W | X);
    ck_assert (aclm_list_find (other, ACLM_USER, 1002) == NULL);
    /* its own base entries are kept */
    ck_assert_uint_eq (perm_of (other, ACLM_OWNER, 0), R | W);
    ck_assert_uint_eq (perm_of (other, ACLM_OTHER, 0), R);
    /* and the mask follows what it now has */
    ck_assert_uint_eq (perm_of (other, ACLM_MASK, 0), R | W | X);

    g_array_free (changes, TRUE);
    aclm_list_free (before);
    aclm_list_free (after);
    aclm_list_free (other);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Removing the base entries of a default ACL removes the whole default ACL. */
START_TEST (test_default_removed)
{
    aclm_list_t *before = base (R | W | X, R | X, 0);
    aclm_list_t *after = aclm_list_new ();
    aclm_list_t *other = base (R | W | X, R, R);
    GArray *changes;

    changes = aclm_diff (before, after, ACLM_DEFAULT, NULL);
    aclm_apply (other, changes, ACLM_DEFAULT, TRUE);
    ck_assert (aclm_list_is_empty (other));

    g_array_free (changes, TRUE);
    aclm_list_free (before);
    aclm_list_free (after);
    aclm_list_free (other);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_base_from)
{
    aclm_list_t *access = base (R | W | X, R | X, R);
    aclm_list_t *def = aclm_list_new ();

    aclm_list_set (access, ACLM_USER, 1001, R);
    aclm_list_base_from (def, access);

    ck_assert_uint_eq (def->entries->len, 3);
    ck_assert_uint_eq (perm_of (def, ACLM_OWNER, 0), R | W | X);
    ck_assert_uint_eq (perm_of (def, ACLM_GROUP_OBJ, 0), R | X);
    ck_assert_uint_eq (perm_of (def, ACLM_OTHER, 0), R);

    aclm_list_free (access);
    aclm_list_free (def);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_perm_to_str)
{
    char s[4];

    aclm_perm_to_str (R | X, s);
    ck_assert_str_eq (s, "r-x");
    aclm_perm_to_str (0, s);
    ck_assert_str_eq (s, "---");
    aclm_perm_to_str (R | W | X, s);
    ck_assert_str_eq (s, "rwx");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_account_id)
{
    static const char *const invalid[] = {
        "4294967296", "4294967295", "18446744073709551616", "-1", "+1", "12x", "#-1", "#",
    };
    unsigned int id;
    guint i;

    ck_assert (acl_id_of (FALSE, "#0", &id));
    ck_assert_uint_eq (id, 0);
    ck_assert (acl_id_of (TRUE, "#4294967294", &id));
    ck_assert_uint_eq (id, 4294967294u);
    for (i = 0; i < G_N_ELEMENTS (invalid); i++)
    {
        id = 123;
        ck_assert (!acl_id_of (FALSE, invalid[i], &id));
        ck_assert_uint_eq (id, 123);
        ck_assert (!acl_id_of (TRUE, invalid[i], &id));
        ck_assert_uint_eq (id, 123);
    }
    ck_assert (!acl_id_of (FALSE, NULL, &id));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A list written to a file reads back the same, where the file system can
   hold an ACL at all. */
START_TEST (test_write_read)
{
    char path[] = "/tmp/mc-acl-test-XXXXXX";
    aclm_list_t *l, *back;
    unsigned int me = (unsigned int) getuid ();
    int fd, err = 0;

    fd = mkstemp (path);
    ck_assert_int_ge (fd, 0);
    close (fd);

    l = base (R | W, R, 0);
    aclm_list_set (l, ACLM_USER, me + 1, R | W | X);
    aclm_calc_mask (l);

    if (!aclm_write (path, ACLM_ACCESS, l, &err))
    {
        unlink (path);
        aclm_list_free (l);
        if (err == ENOTSUP || err == EOPNOTSUPP)
            return;
        ck_abort_msg ("aclm_write: %s", g_strerror (err));
    }

    back = aclm_read (path, ACLM_ACCESS, &err);
    ck_assert (back != NULL);
    ck_assert_uint_eq (back->entries->len, l->entries->len);
    ck_assert_uint_eq (perm_of (back, ACLM_USER, me + 1), R | W | X);
    ck_assert_uint_eq (perm_of (back, ACLM_MASK, 0), R | W | X);
    ck_assert_uint_eq (perm_of (back, ACLM_OTHER, 0), 0);

    unlink (path);
    aclm_list_free (back);
    aclm_list_free (l);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_calc_mask);
    tcase_add_test (tc_core, test_effective);
    tcase_add_test (tc_core, test_mask_by_hand);
    tcase_add_test (tc_core, test_list_order);
    tcase_add_test (tc_core, test_changes_to_all);
    tcase_add_test (tc_core, test_default_removed);
    tcase_add_test (tc_core, test_base_from);
    tcase_add_test (tc_core, test_perm_to_str);
    tcase_add_test (tc_core, test_account_id);
    tcase_add_test (tc_core, test_write_read);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
