/*
   src/diffviewer - tests for lining up three files

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

typedef struct
{
    char **left;
    char **right;
} lines_t;

/* --------------------------------------------------------------------------------------------- */

static char *
temp_file (char **lines)
{
    char *name = NULL;
    char *text;
    int fd;

    fd = g_file_open_tmp ("mc-align3-XXXXXX", &name, NULL);
    ck_assert_int_ge (fd, 0);
    close (fd);
    text = g_strjoinv ("\n", lines);
    if (lines[0] != NULL)
    {
        char *t = g_strconcat (text, "\n", NULL);

        g_free (text);
        text = t;
    }
    ck_assert (g_file_set_contents (name, text, -1, NULL));
    g_free (text);
    return name;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
same_lines (void *ctx, int ls, int ln, int rs, int rn)
{
    const lines_t *l = (const lines_t *) ctx;
    int i;

    if (ln != rn)
        return FALSE;
    for (i = 0; i < ln; i++)
        if (strcmp (l->left[ls + i], l->right[rs + i]) != 0)
            return FALSE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static GArray *
align (char **left, char **mid, char **right)
{
    char *fl = temp_file (left);
    char *fm = temp_file (mid);
    char *fr = temp_file (right);
    GArray *ops_l = g_array_new (FALSE, FALSE, sizeof (DIFFCMD));
    GArray *ops_r = g_array_new (FALSE, FALSE, sizeof (DIFFCMD));
    GArray *chunks = g_array_new (FALSE, FALSE, sizeof (MERGE3_CHUNK));
    diff_options_t opt;
    lines_t ctx = { left, right };

    memset (&opt, 0, sizeof (opt));
    ck_assert_int_ge (dview_compare_files (&opt, fl, fm, ops_l), 0);
    ck_assert_int_ge (dview_compare_files (&opt, fr, fm, ops_r), 0);
    dview_align3 (ops_l, ops_r, same_lines, &ctx, chunks);

    g_array_free (ops_l, TRUE);
    g_array_free (ops_r, TRUE);
    unlink (fl);
    unlink (fm);
    unlink (fr);
    g_free (fl);
    g_free (fm);
    g_free (fr);
    return chunks;
}

/* --------------------------------------------------------------------------------------------- */

/* "L 1+1 1+1 1+1": kind, then the start and count of the left, middle and right lines */
static char *
describe (GArray *chunks)
{
    static const char kinds[] = "LRSC";
    GString *out = g_string_new (NULL);
    guint i;

    for (i = 0; i < chunks->len; i++)
    {
        const MERGE3_CHUNK *c = &g_array_index (chunks, MERGE3_CHUNK, i);

        if (i != 0)
            g_string_append (out, ", ");
        g_string_append_printf (out, "%c %d+%d %d+%d %d+%d", kinds[c->kind], c->start[DIFF_LEFT],
                                c->count[DIFF_LEFT], c->start[DIFF_MERGED], c->count[DIFF_MERGED],
                                c->start[DIFF_RIGHT], c->count[DIFF_RIGHT]);
    }
    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
check (const char *l, const char *m, const char *r, const char *expected)
{
    /* g_strsplit() gives no lines for "" */
    char **left = g_strsplit (l, " ", -1);
    char **mid = g_strsplit (m, " ", -1);
    char **right = g_strsplit (r, " ", -1);
    GArray *chunks;
    char *got;

    chunks = align (left, mid, right);
    got = describe (chunks);
    ck_assert_str_eq (got, expected);

    g_free (got);
    g_array_free (chunks, TRUE);
    g_strfreev (left);
    g_strfreev (mid);
    g_strfreev (right);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kinds)
{
    check ("a b c", "a b c", "a b c", "");
    check ("a X c", "a b c", "a b c", "L 1+1 1+1 1+1");
    check ("a b c", "a b c", "a b Y", "R 2+1 2+1 2+1");
    check ("a X c", "a b c", "a X c", "S 1+1 1+1 1+1");
    check ("a X c", "a b c", "a Y c", "C 1+1 1+1 1+1");
    check ("X b c d e", "a b c d e", "a b c d Y", "L 0+1 0+1 0+1, R 4+1 4+1 4+1");
    /* changes that only touch in the middle stay apart */
    check ("a X c d", "a b c d", "a b Y d", "L 1+1 1+1 1+1, R 2+1 2+1 2+1");
    /* both sides insert at one place */
    check ("a X b", "a b", "a Y b", "C 1+1 1+0 1+1");
    /* the middle has the lines of both sides: no conflict */
    check ("a X b", "a X Y b", "a Y b", "R 1+1 1+1 1+0, L 2+0 2+1 1+1");
    /* an insertion on one side, a deletion on the other */
    check ("a N b c", "a b c", "a c", "C 1+2 1+1 1+0");
    check ("a b c N", "a b c", "a b c", "L 3+1 3+0 3+0");
    check ("", "a", "a", "L 0+0 0+1 0+1");
    check ("a", "", "b", "C 0+1 0+0 0+1");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The rows of the view: the chunks follow each other, and between them the three files have
   the same lines. */
static void
check_invariants (char **left, guint nl, char **mid, guint nm, char **right, guint nr,
                  GArray *chunks)
{
    char **files[DIFF_COUNT];
    guint n[DIFF_COUNT];
    int done[DIFF_COUNT] = { 0, 0, 0 };
    int row = 0;
    guint i;
    int k;

    files[DIFF_LEFT] = left;
    files[DIFF_MERGED] = mid;
    files[DIFF_RIGHT] = right;
    n[DIFF_LEFT] = nl;
    n[DIFF_MERGED] = nm;
    n[DIFF_RIGHT] = nr;

    for (i = 0; i <= chunks->len; i++)
    {
        const MERGE3_CHUNK *c = NULL;
        int gap;

        if (i < chunks->len)
        {
            c = &g_array_index (chunks, MERGE3_CHUNK, i);
            gap = c->start[DIFF_MERGED] - done[DIFF_MERGED];
        }
        else
            gap = (int) n[DIFF_MERGED] - done[DIFF_MERGED];

        ck_assert_int_ge (gap, 0);
        for (k = 0; k < DIFF_COUNT; k++)
        {
            int j;

            if (c != NULL)
                ck_assert_int_eq (c->start[k] - done[k], gap);
            else
                ck_assert_int_eq ((int) n[k] - done[k], gap);
            for (j = 0; j < gap; j++)
                ck_assert_str_eq (files[k][done[k] + j], mid[done[DIFF_MERGED] + j]);
        }

        if (c == NULL)
            break;

        ck_assert_int_eq (c->first_row, row + gap);
        ck_assert_int_eq (
            c->rows, MAX (c->count[DIFF_LEFT], MAX (c->count[DIFF_MERGED], c->count[DIFF_RIGHT])));
        ck_assert_int_gt (c->rows, 0);
        row = c->first_row + c->rows;

        /* the side that did not change has the lines of the middle */
        if (c->kind == MERGE3_LEFT || c->kind == MERGE3_RIGHT)
        {
            int o = c->kind == MERGE3_LEFT ? DIFF_RIGHT : DIFF_LEFT;
            int j;

            ck_assert_int_eq (c->count[o], c->count[DIFF_MERGED]);
            for (j = 0; j < c->count[o]; j++)
                ck_assert_str_eq (files[o][c->start[o] + j], mid[c->start[DIFF_MERGED] + j]);
        }

        for (k = 0; k < DIFF_COUNT; k++)
            done[k] = c->start[k] + c->count[k];
    }
}

/* --------------------------------------------------------------------------------------------- */

static char **
random_edit (GRand *rand, char **mid, guint nm, guint *n, const char *tag)
{
    GPtrArray *out = g_ptr_array_new ();
    guint i;

    for (i = 0; i < nm; i++)
    {
        int what = g_rand_int_range (rand, 0, 10);

        if (what == 0)
            continue;  // deleted
        if (what == 1)
            g_ptr_array_add (out, g_strdup_printf ("%s%u", tag, i));  // changed
        else
            g_ptr_array_add (out, g_strdup (mid[i]));
        if (what == 2)
            g_ptr_array_add (out, g_strdup_printf ("%s+%u", tag, i));  // inserted
    }
    *n = out->len;
    g_ptr_array_add (out, NULL);
    return (char **) g_ptr_array_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_random)
{
    GRand *rand = g_rand_new_with_seed (392);
    int round;

    for (round = 0; round < 300; round++)
    {
        guint nm = (guint) g_rand_int_range (rand, 0, 30);
        guint nl, nr, i;
        char **mid = g_new0 (char *, nm + 1);
        char **left, **right;
        GArray *chunks;

        for (i = 0; i < nm; i++)
            mid[i] = g_strdup_printf ("m%u", i);
        left = random_edit (rand, mid, nm, &nl, g_rand_boolean (rand) ? "x" : "l");
        right = random_edit (rand, mid, nm, &nr, g_rand_boolean (rand) ? "x" : "r");

        chunks = align (left, mid, right);
        check_invariants (left, nl, mid, nm, right, nr, chunks);

        g_array_free (chunks, TRUE);
        g_strfreev (left);
        g_strfreev (mid);
        g_strfreev (right);
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
    tcase_add_test (tc_core, test_kinds);
    tcase_add_test (tc_core, test_random);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
