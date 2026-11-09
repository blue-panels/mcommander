/*
   Tests for the field of several lines.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/lib/widget/textarea"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/widget.h"

/* --------------------------------------------------------------------------------------------- */

static WTextArea *
area_new (const char *text)
{
    return textarea_new (0, 0, 8, 40, text);
}

/* --------------------------------------------------------------------------------------------- */

static void
area_free (WTextArea *area)
{
    send_message (area, NULL, MSG_DESTROY, 0, NULL);
    g_free (area);
}

/* --------------------------------------------------------------------------------------------- */

static void
cursor_at (WTextArea *area, int line, int point)
{
    area->line = line;
    area->point = point;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_text_is_kept_line_by_line)
{
    WTextArea *area;
    char *text;

    area = area_new ("alpha\nbeta\ngamma");
    ck_assert_uint_eq (area->lines->len, 3);

    text = textarea_get_text (area);
    ck_assert_str_eq (text, "alpha\nbeta\ngamma");
    g_free (text);

    area_free (area);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_an_empty_text_is_one_empty_line)
{
    WTextArea *area;
    char *text;

    area = area_new ("");
    ck_assert_uint_eq (area->lines->len, 1);

    text = textarea_get_text (area);
    ck_assert_str_eq (text, "");
    g_free (text);

    area_free (area);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_nothing_is_marked_to_begin_with)
{
    WTextArea *area;

    area = area_new ("alpha\nbeta");
    ck_assert_int_lt (area->mark_line, 0);
    ck_assert_ptr_eq (textarea_get_marked (area), NULL);

    area_free (area);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_the_mark_takes_what_lies_between_it_and_the_cursor)
{
    WTextArea *area;
    char *text;

    area = area_new ("alpha\nbeta\ngamma");

    // from the middle of the first line to the middle of the third
    cursor_at (area, 0, 2);
    textarea_mark (area, TRUE);
    cursor_at (area, 2, 3);

    text = textarea_get_marked (area);
    ck_assert_str_eq (text, "pha\nbeta\ngam");
    g_free (text);

    area_free (area);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_the_mark_reads_the_same_in_either_direction)
{
    WTextArea *area;
    char *forward, *backward;

    area = area_new ("alpha\nbeta");

    cursor_at (area, 0, 1);
    textarea_mark (area, TRUE);
    cursor_at (area, 1, 2);
    forward = textarea_get_marked (area);

    // the mark put where the cursor was, and the cursor where the mark was
    cursor_at (area, 1, 2);
    textarea_mark (area, TRUE);
    cursor_at (area, 0, 1);
    backward = textarea_get_marked (area);

    ck_assert_str_eq (forward, backward);
    ck_assert_str_eq (forward, "lpha\nbe");

    g_free (forward);
    g_free (backward);
    area_free (area);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_deleting_the_mark_joins_what_is_left)
{
    WTextArea *area;
    char *text;

    area = area_new ("alpha\nbeta\ngamma");

    cursor_at (area, 0, 2);
    textarea_mark (area, TRUE);
    cursor_at (area, 2, 3);

    textarea_delete_marked (area);

    text = textarea_get_text (area);
    ck_assert_str_eq (text, "alma");
    g_free (text);

    // the cursor stands where the marked text began, and nothing is marked
    ck_assert_int_eq (area->line, 0);
    ck_assert_int_eq (area->point, 2);
    ck_assert_int_lt (area->mark_line, 0);

    area_free (area);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_mark_of_no_width_holds_nothing)
{
    WTextArea *area;

    area = area_new ("alpha");

    cursor_at (area, 0, 3);
    textarea_mark (area, TRUE);

    ck_assert_ptr_eq (textarea_get_marked (area), NULL);

    // and deleting it leaves the text alone
    textarea_delete_marked (area);
    {
        char *text;

        text = textarea_get_text (area);
        ck_assert_str_eq (text, "alpha");
        g_free (text);
    }

    area_free (area);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_the_mark_goes_away_when_it_is_turned_off)
{
    WTextArea *area;
    char *text;

    area = area_new ("alpha\nbeta");

    cursor_at (area, 0, 0);
    textarea_mark (area, TRUE);
    cursor_at (area, 1, 4);
    text = textarea_get_marked (area);
    ck_assert_ptr_ne (text, NULL);
    g_free (text);

    textarea_mark (area, FALSE);
    ck_assert_ptr_eq (textarea_get_marked (area), NULL);

    area_free (area);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_the_mark_counts_characters_not_bytes)
{
    WTextArea *area;
    char *text;

    // Cyrillic: every letter is two bytes in UTF-8
    area = area_new ("\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82\n"
                     "\xd0\xbc\xd0\xb8\xd1\x80");

    cursor_at (area, 0, 3);
    textarea_mark (area, TRUE);
    cursor_at (area, 1, 2);

    text = textarea_get_marked (area);
    ck_assert_str_eq (text, "\xd0\xb2\xd0\xb5\xd1\x82\n\xd0\xbc\xd0\xb8");
    g_free (text);

    textarea_delete_marked (area);
    text = textarea_get_text (area);
    ck_assert_str_eq (text, "\xd0\xbf\xd1\x80\xd0\xb8\xd1\x80");
    g_free (text);

    area_free (area);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;
    int ret;

    // the widget counts characters, and that needs the string layer; UTF-8 is
    // named outright, or the locale of whoever runs the tests decides it
    str_init_strings ("UTF-8");

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_a_text_is_kept_line_by_line);
    tcase_add_test (tc_core, test_an_empty_text_is_one_empty_line);
    tcase_add_test (tc_core, test_nothing_is_marked_to_begin_with);
    tcase_add_test (tc_core, test_the_mark_takes_what_lies_between_it_and_the_cursor);
    tcase_add_test (tc_core, test_the_mark_reads_the_same_in_either_direction);
    tcase_add_test (tc_core, test_deleting_the_mark_joins_what_is_left);
    tcase_add_test (tc_core, test_a_mark_of_no_width_holds_nothing);
    tcase_add_test (tc_core, test_the_mark_goes_away_when_it_is_turned_off);
    tcase_add_test (tc_core, test_the_mark_counts_characters_not_bytes);

    ret = mctest_run_all (tc_core);

    str_uninit_strings ();

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
