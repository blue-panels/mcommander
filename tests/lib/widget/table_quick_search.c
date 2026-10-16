/*
   lib/widget - tests for the quick search and the quick filter of WTable

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

#define TEST_SUITE_NAME "lib/widget/table"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/tty/key.h"
#include "lib/widget.h"

/* --------------------------------------------------------------------------------------------- */

static const char *names[] = { "alpha", "beta", "Gamma", "beta2", "\xd0\x94\xd0\xbe\xd0\xbc" };

static WTable *t;

/* --------------------------------------------------------------------------------------------- */

static int
get_nrows (const void *data)
{
    (void) data;
    return (int) G_N_ELEMENTS (names);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
get_text (const void *data, int row, int col)
{
    (void) data;
    (void) col;
    return names[row];
}

/* --------------------------------------------------------------------------------------------- */

static void
keys (const char *text)
{
    for (; *text != '\0'; text++)
        send_message (t, NULL, MSG_KEY, (unsigned char) *text, NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    static const table_column_def_t cols[] = {
        { 10, J_LEFT, TABLE_COL_TEXT },
    };
    table_datasource_t ds = { get_nrows, get_text, NULL, NULL, NULL, NULL };

    str_init_strings (NULL);
    t = table_new (0, 0, 10, 10, G_N_ELEMENTS (cols), cols);
    table_set_datasource (t, ds);
    table_set_quick_search (t, 0, TRUE, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    widget_destroy (WIDGET (t));
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_search)
{
    /* a letter starts the search; the name begins with the text, case aside */
    keys ("g");
    ck_assert (t->qs_active);
    ck_assert_int_eq (table_get_current (t), 2);

    /* a letter that finds nothing is dropped */
    keys ("x");
    ck_assert_str_eq (t->qs_text->str, "g");

    /* a multibyte name */
    send_message (t, NULL, MSG_KEY, ESC_CHAR, NULL);
    ck_assert (!t->qs_active);
    keys ("\xd0\xb4");
    ck_assert_int_eq (table_get_current (t), 4);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_search_next)
{
    keys ("be");
    ck_assert_int_eq (table_get_current (t), 1);
    send_message (t, NULL, MSG_KEY, XCTRL ('s'), NULL);
    ck_assert_int_eq (table_get_current (t), 3);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_filter)
{
    send_message (t, NULL, MSG_KEY, ALT ('S'), NULL);
    ck_assert (t->qs_filtering);

    /* the rows that have the text anywhere; the rows given out are the data rows */
    keys ("ta");
    ck_assert_int_eq (t->qs_rows->len, 2);
    ck_assert_int_eq (table_get_current (t), 1);
    send_message (t, NULL, MSG_KEY, KEY_DOWN, NULL);
    ck_assert_int_eq (table_get_current (t), 3);

    /* a hidden row cannot be made current */
    table_set_current (t, 0);
    ck_assert_int_eq (table_get_current (t), 3);

    /* Esc brings every row back, the current one stays */
    send_message (t, NULL, MSG_KEY, ESC_CHAR, NULL);
    ck_assert_ptr_null (t->qs_rows);
    ck_assert_int_eq (table_get_current (t), 3);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fuzzy_score)
{
    /* the letters in their order, anywhere */
    ck_assert_int_gt (quick_search_fuzzy_score ("filechooser_dialog.c", "fcd", FALSE), 0);
    ck_assert_int_eq (quick_search_fuzzy_score ("filechooser_dialog.c", "dcf", FALSE), 0);

    /* side by side and at the start of a word rank higher */
    ck_assert_int_gt (quick_search_fuzzy_score ("beta.txt", "tx", FALSE),
                      quick_search_fuzzy_score ("text", "tx", FALSE));
    ck_assert_int_gt (quick_search_fuzzy_score ("my_dialog", "dia", FALSE),
                      quick_search_fuzzy_score ("media", "dia", FALSE));
    ck_assert_int_gt (quick_search_fuzzy_score ("FileChooser", "fc", FALSE),
                      quick_search_fuzzy_score ("office", "fc", FALSE));

    /* the case of a multibyte letter */
    ck_assert_int_gt (
        quick_search_fuzzy_score ("\xd0\x94\xd0\xbe\xd0\xbc", "\xd0\xb4\xd0\xbc", FALSE), 0);
    ck_assert_int_eq (
        quick_search_fuzzy_score ("\xd0\x94\xd0\xbe\xd0\xbc", "\xd0\xb4\xd0\xbc", TRUE), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_filter_fuzzy)
{
    table_set_quick_filter_fuzzy (t, TRUE, NULL);

    /* a letter starts the filter; the best match comes first and is current */
    keys ("bt");
    ck_assert (t->qs_filtering);
    ck_assert_int_eq (t->qs_rows->len, 2);
    ck_assert_int_eq (table_get_current (t), 1);

    keys ("2");
    ck_assert_int_eq (t->qs_rows->len, 1);
    ck_assert_int_eq (table_get_current (t), 3);

    /* Backspace widens it again */
    send_message (t, NULL, MSG_KEY, KEY_BACKSPACE, NULL);
    ck_assert_int_eq (t->qs_rows->len, 2);

    /* a letter that matches nothing is dropped */
    keys ("q");
    ck_assert_str_eq (t->qs_text->str, "bt");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static int
group_of (void *data, int row)
{
    (void) data;
    return row == 0 ? -1 : 0;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_filter_fuzzy_kept_row)
{
    table_set_quick_filter_fuzzy (t, TRUE, group_of);

    /* row 0 stays first though it does not match; the cursor is on the match */
    keys ("gm");
    ck_assert_int_eq (t->qs_rows->len, 2);
    ck_assert_int_eq (g_array_index (t->qs_rows, int, 0), 0);
    ck_assert_int_eq (table_get_current (t), 2);

    /* it does not count as a match: a letter that leaves only it is dropped */
    keys ("q");
    ck_assert_str_eq (t->qs_text->str, "gm");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_search);
    tcase_add_test (tc_core, test_search_next);
    tcase_add_test (tc_core, test_filter);
    tcase_add_test (tc_core, test_fuzzy_score);
    tcase_add_test (tc_core, test_filter_fuzzy);
    tcase_add_test (tc_core, test_filter_fuzzy_kept_row);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
