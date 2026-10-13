/*
   File comparison for the diff viewer, done by xdiff.

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

#include <config.h>

#include <regex.h>  // regex_t in xpparam_t
#include <string.h>

#include "lib/global.h"

#include "internal.h"
#include "xdiff/xdiff.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* One change of xdiff as the diff statement scan_line used to read from diff:
   NUMaNUM[,NUM], NUM[,NUM]cNUM[,NUM] or NUM[,NUM]dNUM, lines counted from 1. */
static int
dff_hunk (long start_a, long count_a, long start_b, long count_b, void *cb_data)
{
    DIFFCMD op;

    if (count_a == 0)
    {
        op.cmd = 'a';
        op.a[0][0] = op.a[0][1] = (int) start_a;
        op.a[1][0] = (int) start_b + 1;
        op.a[1][1] = (int) (start_b + count_b);
    }
    else if (count_b == 0)
    {
        op.cmd = 'd';
        op.a[0][0] = (int) start_a + 1;
        op.a[0][1] = (int) (start_a + count_a);
        op.a[1][0] = op.a[1][1] = (int) start_b;
    }
    else
    {
        op.cmd = 'c';
        op.a[0][0] = (int) start_a + 1;
        op.a[0][1] = (int) (start_a + count_a);
        op.a[1][0] = (int) start_b + 1;
        op.a[1][1] = (int) (start_b + count_b);
    }

    g_array_append_val ((GArray *) cb_data, op);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* @text with its tabs expanded to stops of 8 into @out, or only counted when @out is NULL.
   A column is a character: UTF-8 continuation bytes take none. */
static gsize
dff_expand_tabs (const char *text, gsize len, char *out)
{
    gsize i, o = 0, col = 0;

    for (i = 0; i < len; i++)
        if (text[i] == '\t')
            do
            {
                if (out != NULL)
                    out[o] = ' ';
                o++;
                col++;
            }
            while (col % 8 != 0);
        else
        {
            if (out != NULL)
                out[o] = text[i];
            o++;
            if (text[i] == '\n')
                col = 0;
            else if (((guchar) text[i] & 0xC0) != 0x80)
                col++;
        }

    return o;
}

/* --------------------------------------------------------------------------------------------- */

/* @text case folded line by line: a valid UTF-8 line as UTF-8, any other line as ASCII,
   so a line folds the same in both files.  g_utf8_validate() refuses a NUL, so a line
   with NUL bytes folds as ASCII and keeps them. */
static char *
dff_fold_case (const char *text, gsize len, gsize *out_len)
{
    GString *out;
    const char *p = text;
    const char *end = text + len;

    out = g_string_sized_new (len);
    while (p < end)
    {
        const char *eol = memchr (p, '\n', (size_t) (end - p));
        gsize n = (eol != NULL ? (gsize) (eol - p) + 1 : (gsize) (end - p));

        if (g_utf8_validate (p, (gssize) n, NULL))
        {
            char *line = g_utf8_casefold (p, (gssize) n);

            g_string_append (out, line);
            g_free (line);
        }
        else
        {
            gsize i;

            for (i = 0; i < n; i++)
                g_string_append_c (out, g_ascii_tolower (p[i]));
        }
        p += n;
    }

    *out_len = out->len;
    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* @text as diff -i and -E compared it: case folded, tabs expanded to stops of 8.  Bytes,
   NUL among them, are kept as they are; the lines stay where they were.  NULL when there
   is no memory for it. */
static char *
dff_normalize (const char *text, gsize len, gboolean fold_case, gboolean expand_tabs,
               gsize *out_len)
{
    char *folded = NULL;
    char *out;
    gsize n;

    if (fold_case)
    {
        folded = dff_fold_case (text, len, &len);
        text = folded;
    }

    if (!expand_tabs)
    {
        *out_len = len;
        return folded;
    }

    n = dff_expand_tabs (text, len, NULL);
    out = g_try_malloc (n + 1);
    if (out != NULL)
    {
        dff_expand_tabs (text, len, out);
        out[n] = '\0';
        *out_len = n;
    }

    g_free (folded);
    return out;
}

/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Compare two files with xdiff and extract diff statements.
 *
 * @param opt how the files are compared
 * @param file1 first file to compare
 * @param file2 second file to compare
 * @param ops list of diff statements to fill
 *
 * @return number of hunks, or a negative number when a file cannot be read
 */

int
dview_compare_files (const diff_options_t *opt, const char *file1, const char *file2, GArray *ops)
{
    const char *names[2] = { file1, file2 };
    char *data[2] = { NULL, NULL };
    gsize len[2] = { 0, 0 };
    mmfile_t mf[2];
    xpparam_t xpp;
    xdemitconf_t xecfg;
    xdemitcb_t ecb;
    int rv = -1;
    int i;

    for (i = 0; i < 2; i++)
    {
        if (!g_file_get_contents (names[i], &data[i], &len[i], NULL) || len[i] > G_MAXLONG)
            goto out;

        if (opt->ignore_case || opt->ignore_tab_expansion)
        {
            gsize n;
            char *norm;

            norm = dff_normalize (data[i], len[i], opt->ignore_case, opt->ignore_tab_expansion, &n);
            g_free (data[i]);
            data[i] = norm;
            if (norm == NULL)
                goto out;
            len[i] = n;
            if (len[i] > G_MAXLONG)
                goto out;
        }

        mf[i].ptr = data[i];
        mf[i].size = (long) len[i];
    }

    memset (&xpp, 0, sizeof (xpp));
    if (opt->quality == 1)
        xpp.flags |= XDF_HISTOGRAM_DIFF;
    else if (opt->quality == 2)
        xpp.flags |= XDF_NEED_MINIMAL;
    if (opt->strip_trailing_cr)
        xpp.flags |= XDF_IGNORE_CR_AT_EOL;
    if (opt->ignore_space_change)
        xpp.flags |= XDF_IGNORE_WHITESPACE_CHANGE;
    if (opt->ignore_all_space)
        xpp.flags |= XDF_IGNORE_WHITESPACE;

    memset (&xecfg, 0, sizeof (xecfg));
    xecfg.hunk_func = dff_hunk;

    memset (&ecb, 0, sizeof (ecb));
    ecb.priv = ops;

    if (xdl_diff (&mf[0], &mf[1], &xpp, &xecfg, &ecb) == 0)
        rv = (int) ops->len;

out:
    g_free (data[0]);
    g_free (data[1]);
    return rv;
}

/* --------------------------------------------------------------------------------------------- */
