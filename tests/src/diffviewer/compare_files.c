/*
   src/diffviewer - tests for the file comparison done by xdiff

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

#define TEST_SUITE_NAME "/src/diffviewer"

#include "tests/mctest.h"

#include <string.h>
#include <unistd.h>

#include "src/diffviewer/internal.h"

/* --------------------------------------------------------------------------------------------- */

static char *
temp_file_len (const char *text, gssize len)
{
    char *name = NULL;
    int fd;

    fd = g_file_open_tmp ("mc-compare-XXXXXX", &name, NULL);
    ck_assert_int_ge (fd, 0);
    close (fd);
    ck_assert (g_file_set_contents (name, text, len, NULL));
    return name;
}

/* --------------------------------------------------------------------------------------------- */

static char *
temp_file (const char *text)
{
    return temp_file_len (text, -1);
}

/* --------------------------------------------------------------------------------------------- */

/* The statements for two texts, as diff printed them: "2c2", "0a1", "3,4d2". */
static char *
compare (const diff_options_t *opt, const char *a, const char *b, int *n)
{
    char *fa = temp_file (a);
    char *fb = temp_file (b);
    GArray *ops = g_array_new (FALSE, FALSE, sizeof (DIFFCMD));
    GString *out = g_string_new (NULL);
    guint i;

    *n = dview_compare_files (opt, fa, fb, ops);
    for (i = 0; i < ops->len; i++)
    {
        const DIFFCMD *op = &g_array_index (ops, DIFFCMD, i);

        if (i != 0)
            g_string_append_c (out, ' ');
        g_string_append_printf (out, "%d", op->a[0][0]);
        if (op->a[0][1] != op->a[0][0])
            g_string_append_printf (out, ",%d", op->a[0][1]);
        g_string_append_printf (out, "%c%d", op->cmd, op->a[1][0]);
        if (op->a[1][1] != op->a[1][0])
            g_string_append_printf (out, ",%d", op->a[1][1]);
    }

    g_array_free (ops, TRUE);
    unlink (fa);
    unlink (fb);
    g_free (fa);
    g_free (fb);
    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
check (const diff_options_t *opt, const char *a, const char *b, const char *expected)
{
    int n;
    char *got = compare (opt, a, b, &n);

    ck_assert_int_ge (n, 0);
    ck_assert_str_eq (got, expected);
    g_free (got);
}

/* --------------------------------------------------------------------------------------------- */

/* Build b from the lines of a and the statements, the way the viewer lines them up. */
static void
check_rebuilds (char **la, guint na, char **lb, guint nb)
{
    char *a = g_strjoinv ("\n", la);
    char *b = g_strjoinv ("\n", lb);
    char *ta = na != 0 ? g_strconcat (a, "\n", NULL) : g_strdup ("");
    char *tb = nb != 0 ? g_strconcat (b, "\n", NULL) : g_strdup ("");
    char *fa = temp_file (ta);
    char *fb = temp_file (tb);
    diff_options_t opt;
    GArray *ops = g_array_new (FALSE, FALSE, sizeof (DIFFCMD));
    GPtrArray *built = g_ptr_array_new ();
    guint i, pa = 0;
    int n;

    memset (&opt, 0, sizeof (opt));
    n = dview_compare_files (&opt, fa, fb, ops);
    ck_assert_int_ge (n, 0);

    for (i = 0; i < ops->len; i++)
    {
        const DIFFCMD *op = &g_array_index (ops, DIFFCMD, i);
        guint upto = (guint) (op->cmd == 'a' ? op->a[0][0] : op->a[0][0] - 1);
        int k;

        ck_assert_uint_ge (upto, pa);
        for (; pa < upto; pa++)
            g_ptr_array_add (built, la[pa]);
        if (op->cmd != 'a')
            pa = (guint) op->a[0][1];
        if (op->cmd != 'd')
            for (k = op->a[1][0]; k <= op->a[1][1]; k++)
                g_ptr_array_add (built, lb[k - 1]);
        /* the lines before the statement are the same in both files */
        ck_assert_uint_eq (built->len, (guint) (op->cmd == 'd' ? op->a[1][0] : op->a[1][1]));
    }
    for (; pa < na; pa++)
        g_ptr_array_add (built, la[pa]);

    ck_assert_uint_eq (built->len, nb);
    for (i = 0; i < nb; i++)
        ck_assert_str_eq ((const char *) g_ptr_array_index (built, i), lb[i]);

    g_ptr_array_free (built, TRUE);
    g_array_free (ops, TRUE);
    unlink (fa);
    unlink (fb);
    g_free (fa);
    g_free (fb);
    g_free (ta);
    g_free (tb);
    g_free (a);
    g_free (b);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_statements)
{
    diff_options_t opt;

    memset (&opt, 0, sizeof (opt));
    check (&opt, "1\n2\n3\n", "1\n2\n3\n", "");
    check (&opt, "1\n2\n3\n", "1\nX\n3\n", "2c2");
    check (&opt, "b\n", "a\nb\n", "0a1");
    check (&opt, "a\n", "a\nb\nc\n", "1a2,3");
    check (&opt, "a\nb\nc\n", "a\n", "2,3d1");
    check (&opt, "a\nb\n", "b\n", "1d0");
    check (&opt, "a\nb\nc\nd\n", "a\nX\nY\nd\n", "2,3c2,3");
    check (&opt, "", "a\n", "0a1");
    check (&opt, "a\n", "", "1d0");
    /* the last line without a newline is not the same line */
    check (&opt, "a\nb", "a\nb\n", "2c2");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_options)
{
    diff_options_t opt;

    memset (&opt, 0, sizeof (opt));
    check (&opt, "Hello\n", "hello\n", "1c1");
    check (&opt, "a\tb\n", "a       b\n", "1c1");
    check (&opt, "a  b\n", "a b\n", "1c1");
    check (&opt, "ab\n", "a b\n", "1c1");
    check (&opt, "a\r\n", "a\n", "1c1");

    opt.ignore_case = TRUE;
    check (&opt, "Hello\nWORLD\n", "hello\nworld\n", "");
    check (&opt, "\xd0\x9f\xd0\xa0\xd0\x98\n", "\xd0\xbf\xd1\x80\xd0\xb8\n", "");
    opt.ignore_case = FALSE;

    opt.ignore_tab_expansion = TRUE;
    check (&opt, "a\tb\n\tc\n", "a       b\n        c\n", "");
    check (&opt, "a\tb\n", "a b\n", "1c1");
    opt.ignore_tab_expansion = FALSE;

    opt.ignore_space_change = TRUE;
    check (&opt, "a  b \n", "a b\n", "");
    check (&opt, "ab\n", "a b\n", "1c1");
    opt.ignore_space_change = FALSE;

    opt.ignore_all_space = TRUE;
    check (&opt, "ab\n", " a b \n", "");
    opt.ignore_all_space = FALSE;

    opt.strip_trailing_cr = TRUE;
    check (&opt, "a\r\nb\r\n", "a\nb\n", "");
    opt.strip_trailing_cr = FALSE;

    opt.quality = 2;
    check (&opt, "a\nb\nc\n", "a\nX\nc\n", "2c2");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Binary files: what follows a NUL byte still counts with the case and tab options. */
START_TEST (test_nul_bytes)
{
    static const char a[] = "x\nAB\0X\tz\ny\n";
    static const char b[] = "x\nab\0Y\tz\ny\n";
    static const char c[] = "x\nab\0X    z\ny\n";
    diff_options_t opt;
    GArray *ops = g_array_new (FALSE, FALSE, sizeof (DIFFCMD));
    char *fa = temp_file_len (a, sizeof (a) - 1);
    char *fb = temp_file_len (b, sizeof (b) - 1);
    char *fc = temp_file_len (c, sizeof (c) - 1);
    const DIFFCMD *op;

    memset (&opt, 0, sizeof (opt));
    opt.ignore_case = TRUE;
    opt.ignore_tab_expansion = TRUE;

    ck_assert_int_eq (dview_compare_files (&opt, fa, fb, ops), 1);
    op = &g_array_index (ops, DIFFCMD, 0);
    ck_assert_int_eq (op->cmd, 'c');
    ck_assert_int_eq (op->a[0][0], 2);
    ck_assert_int_eq (op->a[1][0], 2);

    g_array_set_size (ops, 0);
    ck_assert_int_eq (dview_compare_files (&opt, fa, fc, ops), 0);

    unlink (fa);
    unlink (fb);
    unlink (fc);
    g_free (fa);
    g_free (fb);
    g_free (fc);
    g_array_free (ops, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_missing_file)
{
    diff_options_t opt;
    GArray *ops = g_array_new (FALSE, FALSE, sizeof (DIFFCMD));
    char *fa = temp_file ("a\n");

    memset (&opt, 0, sizeof (opt));
    ck_assert_int_lt (dview_compare_files (&opt, fa, "/nonexistent/mc-compare-file", ops), 0);
    ck_assert_int_lt (dview_compare_files (&opt, "/nonexistent/mc-compare-file", fa, ops), 0);

    unlink (fa);
    g_free (fa);
    g_array_free (ops, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Random files from a few lines: the statements must turn the first into the second. */
START_TEST (test_random_rebuild)
{
    static const char *words[] = { "a", "b", "c", "d", "", "e" };
    GRand *rand = g_rand_new_with_seed (392);
    int round;

    for (round = 0; round < 300; round++)
    {
        guint na = (guint) g_rand_int_range (rand, 0, 40);
        guint nb = (guint) g_rand_int_range (rand, 0, 40);
        char **la = g_new0 (char *, na + 1);
        char **lb = g_new0 (char *, nb + 1);
        guint i;

        for (i = 0; i < na; i++)
            la[i] = g_strdup (words[g_rand_int_range (rand, 0, G_N_ELEMENTS (words))]);
        for (i = 0; i < nb; i++)
            lb[i] = g_strdup (words[g_rand_int_range (rand, 0, G_N_ELEMENTS (words))]);

        check_rebuilds (la, na, lb, nb);

        g_strfreev (la);
        g_strfreev (lb);
    }

    g_rand_free (rand);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_test (tc_core, test_statements);
    tcase_add_test (tc_core, test_options);
    tcase_add_test (tc_core, test_nul_bytes);
    tcase_add_test (tc_core, test_missing_file);
    tcase_add_test (tc_core, test_random_rebuild);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
