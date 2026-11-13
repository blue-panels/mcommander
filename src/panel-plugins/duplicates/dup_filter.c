/*
   Panel plugin duplicates for the M-Commander
   Which files a search for duplicates looks at

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

/** \file dup_filter.c
 *  \brief Source: which files a search for duplicates looks at
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/strutil.h"  // parse_integer()
#include "lib/search.h"

#include "dup_filter.h"

/*** file scope type declarations ****************************************************************/

struct dup_name_filter_s
{
    GPtrArray *searches;  // mc_search_t *
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
search_free (gpointer p)
{
    mc_search_free ((mc_search_t *) p);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
add_search (dup_name_filter_t *filter, const char *pattern, gboolean shell_patterns,
            gboolean case_sens, GError **error)
{
    mc_search_t *search;

    search = mc_search_new (pattern, NULL);
    if (search == NULL)
        return TRUE;

    search->search_type = shell_patterns ? MC_SEARCH_T_GLOB : MC_SEARCH_T_REGEX;
    search->is_case_sensitive = case_sens;
    search->is_entire_line = shell_patterns;

    if (!mc_search_prepare (search))
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s",
                     search->error_str != NULL ? search->error_str : pattern);
        mc_search_free (search);
        return FALSE;
    }

    g_ptr_array_add (filter->searches, search);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* The end of the character class that starts at @p, or NULL when it does not close: then the
   '[' is an ordinary character */
static const char *
class_end (const char *p)
{
    p++;
    if (*p == '!' || *p == '^')
        p++;
    if (*p == ']')
        p++;  // the first ']' is a member
    while (*p != '\0' && *p != ']')
    {
        if (*p == '\\' && p[1] != '\0')
            p++;  // "\]" is a member
        else if (p[0] == '[' && p[1] == ':')
        {
            // "[:alpha:]" is one member, with its own ']'
            const char *end = strstr (p + 2, ":]");

            if (end != NULL)
                p = end + 1;
        }
        p++;
    }
    return *p == ']' ? p : NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* "*.jpg;*.png" or "*.jpg, *.png".  A separator in braces, in brackets or after a backslash
   belongs to the glob: "*.{jpg,png}", "[a,b].txt", "a\,b" */
static char **
split_masks (const char *patterns)
{
    GPtrArray *masks;
    const char *start = patterns, *p;
    int depth = 0;

    masks = g_ptr_array_new ();
    for (p = patterns;; p++)
    {
        if (*p == '\\' && p[1] != '\0')
            p++;
        else if (*p == '[' && class_end (p) != NULL)
            p = class_end (p);
        else if (*p == '{')
            depth++;
        else if (*p == '}' && depth > 0)
            depth--;
        else if (((*p == ';' || *p == ',') && depth == 0) || *p == '\0')
        {
            g_ptr_array_add (masks, g_strndup (start, (gsize) (p - start)));
            if (*p == '\0')
                break;
            start = p + 1;
        }
    }
    g_ptr_array_add (masks, NULL);
    return (char **) g_ptr_array_free (masks, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

dup_name_filter_t *
dup_name_filter_new (const char *patterns, gboolean shell_patterns, gboolean case_sens,
                     GError **error)
{
    dup_name_filter_t *filter;

    filter = g_new0 (dup_name_filter_t, 1);
    filter->searches = g_ptr_array_new_with_free_func (search_free);

    if (patterns == NULL)
        return filter;

    if (shell_patterns)
    {
        char **masks;
        int i;

        masks = split_masks (patterns);
        for (i = 0; masks[i] != NULL; i++)
        {
            char *mask = g_strstrip (masks[i]);

            if (*mask != '\0' && !add_search (filter, mask, TRUE, case_sens, error))
            {
                g_strfreev (masks);
                dup_name_filter_free (filter);
                return NULL;
            }
        }
        g_strfreev (masks);
    }
    else if (*patterns != '\0' && !add_search (filter, patterns, FALSE, case_sens, error))
    {
        dup_name_filter_free (filter);
        return NULL;
    }

    return filter;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dup_name_filter_match (const dup_name_filter_t *filter, const char *name)
{
    guint i;

    if (filter == NULL || filter->searches->len == 0)
        return TRUE;

    for (i = 0; i < filter->searches->len; i++)
    {
        mc_search_t *search = (mc_search_t *) g_ptr_array_index (filter->searches, i);
        gsize found_len;

        if (mc_search_run (search, name, 0, (off_t) strlen (name), &found_len))
            return TRUE;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
dup_name_filter_free (dup_name_filter_t *filter)
{
    if (filter == NULL)
        return;

    g_ptr_array_free (filter->searches, TRUE);
    g_free (filter);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dup_parse_size (const char *text, off_t *size)
{
    char *s;
    uintmax_t n = 0;
    gboolean invalid = FALSE;

    *size = 0;
    if (text == NULL)
        return TRUE;

    s = g_strstrip (g_strdup (text));
    // parse_integer () takes also "b" (512), "c", "w" and "10x2", which nobody means here
    if (*s == '\0')
        n = 0;
    else if (strpbrk (s, "bcwx") != NULL || !g_ascii_isdigit (*s))
        invalid = TRUE;
    else
        n = parse_integer (s, &invalid);
    g_free (s);

    if (invalid || n > (uintmax_t) G_MAXINT64)
        return FALSE;
    *size = (off_t) n;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dup_parse_date (const char *text, gboolean end_of_day, time_t now, time_t *t)
{
    char *s;
    gboolean ok = FALSE;
    GDateTime *dt = NULL;

    *t = 0;

    if (text == NULL)
        return TRUE;

    s = g_strstrip (g_strdup (text));

    if (*s == '\0')
        ok = TRUE;
    else if (g_ascii_isdigit (s[0]))
    {
        int year, month, day;
        char unit;
        char extra;

        if (sscanf (s, "%4d-%2d-%2d%c", &year, &month, &day, &extra) == 3 && year >= 1970
            && g_date_valid_dmy ((GDateDay) day, (GDateMonth) month, (GDateYear) year))
        {
            dt = g_date_time_new_local (year, month, day, 0, 0, 0);
            if (dt != NULL && end_of_day)
            {
                GDateTime *next, *last;

                /* a day is not 24 hours when the clocks change */
                next = g_date_time_add_days (dt, 1);
                last = g_date_time_add_seconds (next, -1);
                g_date_time_unref (next);
                g_date_time_unref (dt);
                dt = last;
            }
        }
        else
        {
            int n;

            if (sscanf (s, "%d%c%c", &n, &unit, &extra) == 2 && n >= 0)
            {
                GDateTime *base;

                base = g_date_time_new_from_unix_local ((gint64) now);
                switch (g_ascii_tolower (unit))
                {
                case 'd':
                    dt = g_date_time_add_days (base, -n);
                    break;
                case 'w':
                    dt = g_date_time_add_weeks (base, -n);
                    break;
                case 'm':
                    dt = g_date_time_add_months (base, -n);
                    break;
                case 'y':
                    dt = g_date_time_add_years (base, -n);
                    break;
                default:
                    break;
                }
                g_date_time_unref (base);
            }
        }

        if (dt != NULL)
        {
            *t = (time_t) g_date_time_to_unix (dt);
            g_date_time_unref (dt);
            ok = TRUE;
        }
    }

    g_free (s);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dup_dir_ignored (char *const *ignore, const char *path, const char *name)
{
    int i;

    if (ignore == NULL)
        return FALSE;

    for (i = 0; ignore[i] != NULL; i++)
    {
        const char *dir = ignore[i];
        size_t len;

        if (*dir == '\0')
            continue;
        // "/tmp/cache/", as the completion of the input line gives it, is "/tmp/cache"
        len = strlen (dir);
        while (len > 1 && dir[len - 1] == '/')
            len--;
        if (g_path_is_absolute (dir) ? strncmp (dir, path, len) == 0 && path[len] == '\0'
                                     : strncmp (dir, name, len) == 0 && name[len] == '\0')
            return TRUE;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
