/*
   Widgets for the M-Commander
   The text of a quick search, as the panels and the tables type it

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

/** \file quicksearch.c
 *  \brief Source: the text of a quick search, as the panels and the tables type it
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"
#include "lib/search.h"
#include "lib/strutil.h"
#include "lib/tty/key.h" /* KEY_BACKSPACE */

#include "lib/widget/quicksearch.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* the weights of the fuzzy match of the ctags plugin */
#define FUZZY_MATCH         1 /* each character found */
#define FUZZY_CONSECUTIVE   8 /* each character more in a run of found ones */
#define FUZZY_WORD_BOUNDARY 5 /* a character found at the start of a word */

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Whether @c begins a word: the first character, one after a separator, or an upper case one
   after a lower case one */
static gboolean
fuzzy_word_start (gunichar prev, gunichar c, gboolean first)
{
    if (first)
        return TRUE;
    if (prev == '_' || prev == '-' || prev == '.' || prev == ' ' || prev == '/')
        return TRUE;
    return g_unichar_islower (prev) && g_unichar_isupper (c);
}

/* --------------------------------------------------------------------------------------------- */

/* The characters of @text; a byte that is no character stands for itself */
static gunichar *
fuzzy_chars (const char *text, int *len)
{
    gunichar *out = g_new (gunichar, strlen (text) + 1);
    int n = 0;

    while (*text != '\0')
    {
        gunichar c = g_utf8_get_char_validated (text, -1);

        if (c == (gunichar) -1 || c == (gunichar) -2)
        {
            c = (guchar) *text;
            text++;
        }
        else
            text = g_utf8_next_char (text);
        out[n++] = c;
    }

    *len = n;
    return out;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
quick_search_drop_char (GString *text)
{
    char *act;

    if (text->len == 0)
        return;

    act = text->str + text->len;
    str_prev_noncomb_char (&act, text->str);
    g_string_set_size (text, act - text->str);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
quick_search_take_key (GString *text, char *ch, size_t ch_size, int *chpoint, int key)
{
    if (key == KEY_BACKSPACE)
    {
        quick_search_drop_char (text);
        *chpoint = 0;
        return TRUE;
    }

    if (key != 0 && (gsize) *chpoint < ch_size)
    {
        ch[*chpoint] = key;
        (*chpoint)++;
    }

    if (*chpoint == 0)
        return key == 0;

    switch (str_is_valid_char (ch, *chpoint))
    {
    case -2:
        return FALSE;
    case -1:
        *chpoint = 0;
        return FALSE;
    default:
        g_string_append_len (text, ch, *chpoint);
        *chpoint = 0;
        return TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

mc_search_t *
quick_search_new (const char *text, gboolean contains, gboolean case_sensitive)
{
    mc_search_t *search;
    char *reg_exp, *esc_str;

    if (contains)
        reg_exp = g_strdup_printf ("*%s*", text);
    else
        reg_exp = g_strdup_printf ("%s*", text);
    esc_str = str_escape (reg_exp, -1, ",|\\{}[]", TRUE);
    search = mc_search_new (esc_str, NULL);
    search->search_type = MC_SEARCH_T_GLOB;
    search->is_entire_line = TRUE;
    search->is_case_sensitive = case_sensitive;

    g_free (reg_exp);
    g_free (esc_str);

    return search;
}

/* --------------------------------------------------------------------------------------------- */

int
quick_search_fuzzy_score (const char *name, const char *query, gboolean case_sensitive)
{
    gunichar *nc, *qc;
    int *prev, *prev_run, *cur, *cur_run;
    int n, m, i, j, best = 0;

    if (name == NULL || query == NULL || *query == '\0')
        return 0;

    nc = fuzzy_chars (name, &n);
    qc = fuzzy_chars (query, &m);
    if (m > n)
    {
        g_free (nc);
        g_free (qc);
        return 0;
    }

    /* the best score of the letters up to qc[j] with qc[j] at nc[i], or -1; and the run of
       letters side by side that ends there */
    prev = g_new (int, 4 * n);
    prev_run = prev + n;
    cur = prev_run + n;
    cur_run = cur + n;

    for (j = 0; j < m; j++)
    {
        int before = -1; /* the best of prev[] up to i - 2 */

        for (i = 0; i < n; i++)
        {
            int side = -1, apart = -1, bonus;

            if (j > 0 && i >= 2 && prev[i - 2] > before)
                before = prev[i - 2];

            cur[i] = -1;
            cur_run[i] = 0;
            if (case_sensitive ? nc[i] != qc[j]
                               : g_unichar_tolower (nc[i]) != g_unichar_tolower (qc[j]))
                continue;

            bonus = FUZZY_MATCH;
            if (fuzzy_word_start (i > 0 ? nc[i - 1] : 0, nc[i], i == 0))
                bonus += FUZZY_WORD_BOUNDARY;

            if (j == 0)
            {
                cur[i] = bonus;
                continue;
            }
            if (i > 0 && prev[i - 1] >= 0)
                side = prev[i - 1] + bonus + FUZZY_CONSECUTIVE * (prev_run[i - 1] + 1);
            if (before >= 0)
                apart = before + bonus;
            if (side >= 0 && side >= apart)
            {
                cur[i] = side;
                cur_run[i] = prev_run[i - 1] + 1;
            }
            else
                cur[i] = apart;
        }

        memcpy (prev, cur, n * sizeof (int));
        memcpy (prev_run, cur_run, n * sizeof (int));
    }

    for (i = 0; i < n; i++)
        best = MAX (best, prev[i]);

    g_free (prev);
    g_free (nc);
    g_free (qc);
    return best;
}

/* --------------------------------------------------------------------------------------------- */
