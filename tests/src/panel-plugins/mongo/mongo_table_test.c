/*
   src/panel-plugins/mongo - unit tests for the table of documents

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

#include <string.h>

#include <bson/bson.h>

#include "lib/strutil.h"

#include "src/panel-plugins/mongo/mongo_render.h"

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    str_init_strings ("UTF-8");
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown (void)
{
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

static void
doc_free (gpointer p)
{
    bson_destroy ((bson_t *) p);
}

/* --------------------------------------------------------------------------------------------- */

/* The documents of @json, one per line, rendered */
static char *
render (const char *const *json)
{
    GPtrArray *docs = g_ptr_array_new_with_free_func (doc_free);
    char *out;

    for (; *json != NULL; json++)
    {
        bson_t *doc = bson_new_from_json ((const uint8_t *) *json, -1, NULL);

        ck_assert_ptr_nonnull (doc);
        g_ptr_array_add (docs, doc);
    }
    out = mongo_table_render (docs);
    g_ptr_array_free (docs, TRUE);
    return out;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_table_columns_in_order_met)
{
    const char *json[] = {
        "{\"_id\": 1, \"name\": \"ab\"}",
        "{\"_id\": 22, \"year\": 1999, \"name\": \"c\"}",
        NULL,
    };
    char *out = render (json);

    mctest_assert_str_eq (out,
                          " _id | name | year \n"
                          "-----+------+------\n"
                          "   1 | ab   |      \n"
                          "  22 | c    | 1999 \n"
                          "(2 rows)\n");
    g_free (out);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_table_mixed_column_left)
{
    const char *json[] = {
        "{\"v\": 5}",
        "{\"v\": \"x\"}",
        NULL,
    };
    char *out = render (json);

    mctest_assert_str_eq (out,
                          " v \n"
                          "---\n"
                          " 5 \n"
                          " x \n"
                          "(2 rows)\n");
    g_free (out);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_table_nested_values)
{
    const char *json[] = {
        "{\"a\": [1, \"s\"], \"d\": {\"k\": true}, \"t\": \"x\\ny\"}",
        NULL,
    };
    char *out = render (json);

    mctest_assert_str_eq (out,
                          " a        | d         | t   \n"
                          "----------+-----------+-----\n"
                          " [1, \"s\"] | {k: true} | x y \n"
                          "(1 row)\n");
    g_free (out);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_table_date_utc)
{
    const char *json[] = {
        "{\"d\": {\"$date\": {\"$numberLong\": \"0\"}}}",
        NULL,
    };
    char *out = render (json);

    ck_assert_ptr_nonnull (strstr (out, " 1970-01-01 00:00:00 \n"));
    g_free (out);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_table_date_before_1970)
{
    const char *json[] = {
        "{\"d\": {\"$date\": {\"$numberLong\": \"-500\"}}}",
        NULL,
    };
    char *out = render (json);

    ck_assert_ptr_nonnull (strstr (out, " 1969-12-31 23:59:59 \n"));
    g_free (out);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_table_key_on_one_line)
{
    const char *json[] = {
        "{\"a\\nb\": 1}",
        NULL,
    };
    char *out = render (json);

    mctest_assert_str_eq (out,
                          " a b \n"
                          "-----\n"
                          "   1 \n"
                          "(1 row)\n");
    g_free (out);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_table_wide_cell_cut)
{
    const char *json[] = {
        "{\"s\": \"0123456789012345678901234567890123456789ABCDEF\"}",
        NULL,
    };
    char *out = render (json);
    char **lines = g_strsplit (out, "\n", -1);

    // a column is at most 40 wide, plus a space on each side
    ck_assert_int_eq (strlen (lines[2]), 42);
    g_strfreev (lines);
    g_free (out);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_table_empty)
{
    const char *json[] = { NULL };
    char *out = render (json);

    mctest_assert_str_eq (out, "\n\n(0 rows)\n");
    g_free (out);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_table_columns_in_order_met);
    tcase_add_test (tc_core, test_table_mixed_column_left);
    tcase_add_test (tc_core, test_table_nested_values);
    tcase_add_test (tc_core, test_table_date_utc);
    tcase_add_test (tc_core, test_table_date_before_1970);
    tcase_add_test (tc_core, test_table_key_on_one_line);
    tcase_add_test (tc_core, test_table_wide_cell_cut);
    tcase_add_test (tc_core, test_table_empty);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
