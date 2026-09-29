/*
   src/runtime-host - unit tests for the syntax service given to runtime plugins

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

#define TEST_SUITE_NAME "/src/runtime-host/syntax"

#include "tests/mctest.h"

#include <unistd.h>

#include "lib/global.h"
#include "lib/fileloc.h"

#include "src/runtime-host.h"

/*** fixtures ************************************************************************************/

static char *tmpdir = NULL;
static char *share_dir = NULL;
static char *saved_share_data_dir = NULL;

static void
write_file (const char *path, const char *content)
{
    FILE *f = fopen (path, "w");

    ck_assert_msg (f != NULL, "cannot write %s", path);
    fputs (content, f);
    fclose (f);
}

/* @Before */
static void
setup (void)
{
    char *syntax_dir;
    char *lang;
    char *local;
    char *top;

    tmpdir = g_dir_make_tmp ("mc-runtime-syntax-XXXXXX", NULL);
    ck_assert_ptr_nonnull (tmpdir);
    /* the rules are looked up in the user configuration and data directories
       first and in the share directory after them; the tests own all of them,
       or a Syntax file of the user's own is taken for theirs */
    g_setenv ("XDG_CONFIG_HOME", tmpdir, TRUE);
    g_setenv ("XDG_DATA_HOME", tmpdir, TRUE);
    share_dir = g_build_filename (tmpdir, "share", (char *) NULL);
    syntax_dir = g_build_filename (share_dir, EDIT_SYNTAX_DIR, (char *) NULL);
    g_mkdir_with_parents (syntax_dir, 0755);

    lang = g_build_filename (syntax_dir, "tested.syntax", (char *) NULL);
    write_file (lang,
                "context default\n"
                "    keyword whole int yellow\n"
                "    keyword ; brightcyan\n");

    local = g_build_filename (syntax_dir, "local.syntax", (char *) NULL);
    write_file (local,
                "line-local\n"
                "number 16 brightcyan\n"
                "string \" brightgreen\n");

    top = g_strconcat ("file ..\\*\\\\.c$ Tested\\sProgram\ninclude ", lang, "\n",
                       "file ..\\*\\\\.sh$ Shell\\sscript ^#!.\\*/bin/sh\ninclude ", local, "\n",
                       (char *) NULL);
    syntax_dir = g_build_filename (share_dir, EDIT_SYNTAX_FILE, (char *) NULL);
    write_file (syntax_dir, top);

    g_free (top);
    g_free (local);
    g_free (lang);
    g_free (syntax_dir);

    saved_share_data_dir = mc_global.share_data_dir;
    mc_global.share_data_dir = share_dir;
}

/* @After */
static void
teardown (void)
{
    mc_global.share_data_dir = saved_share_data_dir;
    g_free (share_dir);
    share_dir = NULL;
    g_free (tmpdir);
    tmpdir = NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* the colors of every byte, so that a test can look at any of them */
static guint *
colors_of (const mc_runtime_syntax_result_t *result, gsize length)
{
    guint *out = g_new0 (guint, length + 1);
    gsize i;

    for (i = 0; i < result->runs_count; i++)
    {
        gsize k;

        for (k = 0; k < result->runs[i].length; k++)
            if (result->runs[i].offset + k < length)
                out[result->runs[i].offset + k] = result->runs[i].color;
    }
    return out;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_rule_set_by_type)
{
    mc_runtime_syntax_result_t result;
    const char *error = NULL;
    const char *text = "int x;";
    guint *colors;

    mctest_assert_true (
        runtime_host_syntax_scan (text, strlen (text), "Tested Program", NULL, &result, &error));
    ck_assert_str_eq (result.type, "Tested Program");
    ck_assert_uint_gt (result.runs_count, 0);
    colors = colors_of (&result, strlen (text));
    // "int" is a keyword, the space after it is not
    ck_assert_uint_ne (colors[0], colors[3]);
    g_free (colors);
    runtime_host_syntax_result_free (&result);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_rule_set_by_filename)
{
    mc_runtime_syntax_result_t result;
    const char *error = NULL;
    const char *text = "int x;";

    mctest_assert_true (
        runtime_host_syntax_scan (text, strlen (text), NULL, "/tmp/a.c", &result, &error));
    ck_assert_str_eq (result.type, "Tested Program");
    runtime_host_syntax_result_free (&result);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_rule_set_by_first_line)
{
    mc_runtime_syntax_result_t result;
    const char *error = NULL;
    const char *text = "#!/bin/sh\necho 1\n";

    // no type and no name: the first line has to decide
    mctest_assert_true (
        runtime_host_syntax_scan (text, strlen (text), NULL, NULL, &result, &error));
    ck_assert_str_eq (result.type, "Shell script");
    runtime_host_syntax_result_free (&result);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_line_local_rules_color_the_text)
{
    mc_runtime_syntax_result_t result;
    const char *error = NULL;
    const char *text = "echo 42\n";
    guint *colors;

    mctest_assert_true (
        runtime_host_syntax_scan (text, strlen (text), "Shell script", NULL, &result, &error));
    ck_assert_str_eq (result.type, "Shell script");
    ck_assert_uint_gt (result.runs_count, 1);
    colors = colors_of (&result, strlen (text));
    // the number is colored, the word before it is not
    ck_assert_uint_ne (colors[0], colors[5]);
    ck_assert_uint_eq (colors[5], colors[6]);
    g_free (colors);
    runtime_host_syntax_result_free (&result);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_runs_cover_the_text_and_name_their_colors)
{
    mc_runtime_syntax_result_t result;
    const char *error = NULL;
    const char *text = "int x;";
    gsize covered = 0;
    gsize i;

    mctest_assert_true (
        runtime_host_syntax_scan (text, strlen (text), "Tested Program", NULL, &result, &error));
    for (i = 0; i < result.runs_count; i++)
    {
        ck_assert_uint_eq (result.runs[i].offset, covered);
        ck_assert_uint_lt (result.runs[i].color, result.colors_count);
        covered += result.runs[i].length;
    }
    ck_assert_uint_eq (covered, strlen (text));
    ck_assert_uint_gt (result.colors_count, 0);
    runtime_host_syntax_result_free (&result);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_text_too_large_is_refused)
{
    mc_runtime_syntax_result_t result;
    const char *error = NULL;
    gsize size = 4 * 1024 * 1024 + 1;
    char *text = g_new0 (char, size);

    memset (text, 'a', size);
    mctest_assert_false (
        runtime_host_syntax_scan (text, size, "Tested Program", NULL, &result, &error));
    ck_assert_str_eq (error, "too_large");
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_rule_set_by_type);
    tcase_add_test (tc_core, test_rule_set_by_filename);
    tcase_add_test (tc_core, test_rule_set_by_first_line);
    tcase_add_test (tc_core, test_line_local_rules_color_the_text);
    tcase_add_test (tc_core, test_runs_cover_the_text_and_name_their_colors);
    tcase_add_test (tc_core, test_text_too_large_is_refused);

    return mctest_run_all (tc_core);
}
