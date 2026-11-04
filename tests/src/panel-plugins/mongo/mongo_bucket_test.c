/*
   src/panel-plugins/mongo - unit tests for the bounds of the buckets

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

#define TEST_SUITE_NAME "/src/panel-plugins/mongo"

#include "tests/mctest.h"

#include <bson/bson.h>

#include "src/panel-plugins/mongo/mongo_conn.h"

/* --------------------------------------------------------------------------------------------- */

static void
int_value (bson_value_t *v, int n)
{
    v->value_type = BSON_TYPE_INT32;
    v->value.v_int32 = n;
}

/* --------------------------------------------------------------------------------------------- */

/* The buckets of $bucketAuto over 0..2999 in three: [0, 1000) [1000, 2000) [2000, 2999]. */
static GArray *
three_buckets (void)
{
    GArray *a = g_array_new (FALSE, TRUE, sizeof (mongo_bucket_info_t));
    int k;

    for (k = 0; k < 3; k++)
    {
        mongo_bucket_info_t b = { 0 };

        int_value (&b.lo, k * 1000);
        int_value (&b.hi, k == 2 ? 2999 : (k + 1) * 1000);
        b.has_hi = TRUE;
        b.count = 1000;
        g_array_append_val (a, b);
    }
    return a;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_bucket_hi_middle_keeps_max)
{
    GArray *a = three_buckets ();
    const bson_value_t *hi;

    hi = mongo_conn_bucket_hi (a, 0, NULL);
    ck_assert_ptr_nonnull (hi);
    ck_assert_int_eq (hi->value.v_int32, 1000);
    hi = mongo_conn_bucket_hi (a, 1, NULL);
    ck_assert_ptr_nonnull (hi);
    ck_assert_int_eq (hi->value.v_int32, 2000);
    g_array_free (a, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_bucket_hi_last_is_open_at_top)
{
    GArray *a = three_buckets ();

    // the inclusive max 2999 is not a bound: _id 2999 would be lost with $lt
    ck_assert_ptr_null (mongo_conn_bucket_hi (a, 2, NULL));
    g_array_free (a, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_bucket_hi_last_ends_at_scope)
{
    GArray *a = three_buckets ();
    bson_value_t scope;

    int_value (&scope, 3000);
    ck_assert_ptr_eq (mongo_conn_bucket_hi (a, 2, &scope), &scope);
    g_array_free (a, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_bucket_hi_no_max_takes_next_lo)
{
    GArray *a = three_buckets ();
    const bson_value_t *hi;

    g_array_index (a, mongo_bucket_info_t, 0).has_hi = FALSE;
    hi = mongo_conn_bucket_hi (a, 0, NULL);
    ck_assert_ptr_nonnull (hi);
    ck_assert_int_eq (hi->value.v_int32, 1000);
    g_array_free (a, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_bucket_hi_middle_keeps_max);
    tcase_add_test (tc_core, test_bucket_hi_last_is_open_at_top);
    tcase_add_test (tc_core, test_bucket_hi_last_ends_at_scope);
    tcase_add_test (tc_core, test_bucket_hi_no_max_takes_next_lo);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
