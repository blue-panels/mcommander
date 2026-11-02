/*
   lib - unit tests for the table of text

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

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"

#include <string.h>

#include "lib/strutil.h"
#include "lib/text_table.h"

/* --------------------------------------------------------------------------------------------- */

/* A table of a "name" column with @value in its one row, rendered in the terminal @codeset */
static char *
render_one (const char *codeset, const char *value, int screen_width)
{
    mc_text_table_t *table;
    char *out;

    str_init_strings (codeset);
    table = mc_text_table_new ();
    (void) mc_text_table_add_column (table, "name");
    (void) mc_text_table_add_column (table, "n");
    mc_text_table_add_row (table);
    mc_text_table_set_cell (table, 0, value, -1, FALSE);
    mc_text_table_set_cell (table, 1, "7", -1, TRUE);
    out = mc_text_table_render (table, NULL, screen_width);
    mc_text_table_free (table);
    str_uninit_strings ();
    return out;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_utf8_terminal)
{
    char *out = render_one ("UTF-8", "Привет", 0);

    mctest_assert_str_eq (out,
                          " name   | n \n"
                          "--------+---\n"
                          " Привет | 7 \n"
                          "(1 row)\n");
    g_free (out);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_koi8r_terminal)
{
    char *out = render_one ("KOI8-R", "Привет", 0);
    char **lines = g_strsplit (out, "\n", -1);

    /* The text is recoded, a letter is one byte and one column: the lines keep their places.
       Its bytes are not compared: without a KOI8-R locale on the machine the 8-bit class prints
       them as dots. */
    ck_assert_int_eq (strlen (lines[0]), 12);
    ck_assert_int_eq (strlen (lines[2]), 12);
    mctest_assert_str_eq (lines[2] + 7, " | 7 ");
    g_strfreev (lines);
    g_free (out);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_ascii_terminal)
{
    char *out = render_one ("ANSI_X3.4-1968", "Привет", 0);
    char **lines = g_strsplit (out, "\n", -1);

    /* A letter the terminal has not is one ASCII character, one column: "?" from GLib, or the
       "*" musl's iconv puts there itself */
    mctest_assert_str_eq (lines[0], " name   | n ");
    ck_assert_int_eq (strlen (lines[2]), 12);
    for (int i = 1; i <= 6; i++)
        ck_assert_int_eq (lines[2][i], lines[2][1]);
    ck_assert (lines[2][1] == '?' || lines[2][1] == '*');
    mctest_assert_str_eq (lines[2] + 7, " | 7 ");
    g_strfreev (lines);
    g_free (out);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cut_at_the_end)
{
    char *out = render_one ("UTF-8", "0123456789abcdefghij", 16);

    // 16 columns: "|", " n " and the spaces around the name leave it 10, 9 of the text and "~"
    mctest_assert_str_eq (out,
                          " name       | n \n"
                          "------------+---\n"
                          " 012345678~ | 7 \n"
                          "(1 row)\n");
    g_free (out);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_caption_and_control_characters)
{
    mc_text_table_t *table;
    char *out;

    str_init_strings ("UTF-8");
    table = mc_text_table_new ();
    (void) mc_text_table_add_column (table, "a\tb");
    mc_text_table_add_row (table);
    mc_text_table_set_cell (table, 0, "x\ny", -1, FALSE);
    mc_text_table_add_row (table);
    out = mc_text_table_render (table, "t\x1b[1m", 0);
    mctest_assert_str_eq (out,
                          "-- t [1m\n"
                          "\n"
                          " a b \n"
                          "-----\n"
                          " x y \n"
                          "     \n"
                          "(2 rows)\n");
    g_free (out);
    mc_text_table_free (table);
    str_uninit_strings ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_long_caption_is_not_cut)
{
    mc_text_table_t *table;
    char *name;
    char *caption;
    char *out;

    str_init_strings ("UTF-8");
    name = g_strnfill (300, 'n');
    caption = g_strconcat (name, ", rows 201-400", (char *) NULL);
    g_free (name);
    table = mc_text_table_new ();
    (void) mc_text_table_add_column (table, "a");
    out = mc_text_table_render (table, caption, 80);
    // the end of it says which rows these are
    ck_assert_ptr_nonnull (strstr (out, "nnn, rows 201-400\n"));
    g_free (out);
    g_free (caption);
    mc_text_table_free (table);
    str_uninit_strings ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_many_wide_columns_are_fast)
{
    mc_text_table_t *table;
    char *value;
    char *out;
    gint64 start;

    str_init_strings ("UTF-8");
    value = g_strnfill (300, 'x');
    table = mc_text_table_new ();
    for (int c = 0; c < 2000; c++)
        (void) mc_text_table_add_column (table, "c");
    mc_text_table_add_row (table);
    for (int c = 0; c < 2000; c++)
        mc_text_table_set_cell (table, c, value, -1, FALSE);

    start = g_get_monotonic_time ();
    out = mc_text_table_render (table, NULL, 120);
    // every column at its minimum of 8: the line does not fit and is not cut further
    ck_assert_ptr_nonnull (strstr (out, " xxxxxxx~ | xxxxxxx~ "));
    ck_assert_int_lt (g_get_monotonic_time () - start, G_USEC_PER_SEC / 2);

    g_free (out);
    g_free (value);
    mc_text_table_free (table);
    str_uninit_strings ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_utf8_terminal);
    tcase_add_test (tc_core, test_koi8r_terminal);
    tcase_add_test (tc_core, test_ascii_terminal);
    tcase_add_test (tc_core, test_cut_at_the_end);
    tcase_add_test (tc_core, test_caption_and_control_characters);
    tcase_add_test (tc_core, test_long_caption_is_not_cut);
    tcase_add_test (tc_core, test_many_wide_columns_are_fast);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
