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

#include "lib/global.h"
#include "lib/search.h"
#include "lib/strutil.h"
#include "lib/tty/key.h" /* KEY_BACKSPACE */

#include "lib/widget/quicksearch.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

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
