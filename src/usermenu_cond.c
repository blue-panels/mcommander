/*
   User menu for the M-Commander
   Where an entry is shown: the conditions of an entry

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

/** \file usermenu_cond.c
 *  \brief Source: the conditions of the user menu
 */

#include <config.h>

#include <string.h>
#include <sys/stat.h>

#include "lib/global.h"
#include "lib/mcconfig.h"
#include "lib/search.h"
#include "lib/vfs/vfs.h"

#include "src/filemanager/filemanager.h"
#include "src/filemanager/layout.h"
#include "src/filemanager/panel.h"

#include "usermenu_ini.h"

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

typedef gboolean (*um_item_match_t) (const char *item, const void *data);

/**
 * A list of items separated by ';': true when any of them matches, an item with
 * '!' in front matching where it does not.  An empty list asks for nothing.
 */
static gboolean
um_list_match (const char *list, um_item_match_t match, const void *data)
{
    gchar **items;
    gboolean any = FALSE, empty = TRUE;
    int i;

    items = g_strsplit (list, ";", -1);

    for (i = 0; items[i] != NULL && !any; i++)
    {
        const char *item = g_strstrip (items[i]);
        gboolean negate;

        if (*item == '\0')
            continue;

        empty = FALSE;
        negate = *item == '!';
        if (negate)
            item++;

        any = match (item, data) != negate;
    }

    g_strfreev (items);

    return any || empty;
}

/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    mode_t mode;
    gboolean link_to_dir;
    gboolean stale_link;
    gboolean known;  // FALSE once a name is not one of the types
} um_on_data_t;

/**
 * A link to a directory is a directory, and a link to anything else that is
 * there a file: what the panel shows is what the menu goes by.
 */
static gboolean
um_on_one (const char *type, const void *data)
{
    um_on_data_t *d = (um_on_data_t *) data;
    const mode_t mode = d->mode;

    if (strcmp (type, "file") == 0)
        return S_ISREG (mode) || (S_ISLNK (mode) && !d->link_to_dir && !d->stale_link);
    if (strcmp (type, "dir") == 0)
        return S_ISDIR (mode) || d->link_to_dir;
    if (strcmp (type, "link") == 0)
        return S_ISLNK (mode);
    if (strcmp (type, "broken") == 0)
        return S_ISLNK (mode) && d->stale_link;
    if (strcmp (type, "char") == 0)
        return S_ISCHR (mode);
    if (strcmp (type, "block") == 0)
        return S_ISBLK (mode);
    if (strcmp (type, "fifo") == 0)
        return S_ISFIFO (mode);
    if (strcmp (type, "socket") == 0)
        return S_ISSOCK (mode);

    d->known = FALSE;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * One mask against a string, the way .gitignore has it: '*' and '?' do not go
 * over a '/', "[...]" is a set of characters, a backslash takes the next one
 * as it is, and "**" between slashes or at an end is any number of
 * directories.
 */
static gboolean
um_wildmatch (const char *p, const char *s, gboolean at_start)
{
    for (; *p != '\0'; p++, s++)
    {
        switch (*p)
        {
        case '*':
            if (p[1] == '*' && (at_start || p[-1] == '/') && (p[2] == '/' || p[2] == '\0'))
            {
                const char *t;

                if (p[2] == '\0')
                    return *s != '\0';  // "dir/**": everything inside, not dir itself

                // "**/": no directory, or any number of them
                for (t = s; TRUE; t++)
                {
                    if ((t == s || t[-1] == '/') && um_wildmatch (p + 3, t, FALSE))
                        return TRUE;
                    if (*t == '\0')
                        return FALSE;
                }
            }

            while (p[1] == '*')
                p++;
            for (;; s++)
            {
                if (um_wildmatch (p + 1, s, FALSE))
                    return TRUE;
                if (*s == '\0' || *s == '/')
                    return FALSE;
            }

        case '?':
            if (*s == '\0' || *s == '/')
                return FALSE;
            break;

        case '[':
        {
            const char *q = p + 1;
            gboolean negate, found = FALSE;

            if (*s == '\0' || *s == '/')
                return FALSE;

            negate = *q == '!' || *q == '^';
            if (negate)
                q++;

            // a ']' right after the opening is one of the set
            do
            {
                if (*q == '\0')
                    return FALSE;
                // q moves on whether or not this one matches
                if (q[1] == '-' && q[2] != ']' && q[2] != '\0')
                {
                    if (*s >= *q && *s <= q[2])
                        found = TRUE;
                    q += 3;
                }
                else if (*s == *q++)
                    found = TRUE;
            }
            while (*q != ']');

            if (found == negate)
                return FALSE;
            p = q;
            break;
        }

        case '\\':
            if (p[1] != '\0')
                p++;
            MC_FALLTHROUGH;

        default:
            if (*s != *p)
                return FALSE;
            break;
        }

        at_start = FALSE;
    }

    return *s == '\0';
}

/* --------------------------------------------------------------------------------------------- */

/**
 * A mask with no '/' looks at the last part of the path, at any level; one with
 * a '/' at the start or in the middle looks at the whole path.  A relative one
 * is taken from base, the directory of the menu file, and where there is none,
 * at any level.  A '/' at the end asks for a directory.
 */
static gboolean
um_path_one (const char *mask, const char *path, gboolean is_dir, const char *base)
{
    char *m;
    size_t len;
    gboolean ok;

    if (mask[0] == '~' && (mask[1] == '/' || mask[1] == '\0'))
        m = g_build_filename (mc_config_get_home_dir (), mask + 1, (char *) NULL);
    else
        m = g_strdup (mask);

    len = strlen (m);
    if (len > 1 && m[len - 1] == '/')
    {
        m[len - 1] = '\0';
        if (!is_dir)
        {
            g_free (m);
            return FALSE;
        }
    }

    if (strchr (m, '/') == NULL)
    {
        const char *last = strrchr (path, '/');

        ok = um_wildmatch (m, last != NULL ? last + 1 : path, TRUE);
    }
    else if (m[0] == '/')
        ok = um_wildmatch (m, path, TRUE);
    else if (base != NULL)
    {
        char *full;

        full = g_strconcat (base, g_str_has_suffix (base, "/") ? "" : "/", m, (char *) NULL);
        ok = um_wildmatch (full, path, TRUE);
        g_free (full);
    }
    else
    {
        char *any;

        any = g_str_has_prefix (m, "**/") ? g_strdup (m) : g_strconcat ("**/", m, (char *) NULL);
        ok = um_wildmatch (any, path + (path[0] == '/' ? 1 : 0), TRUE);
        g_free (any);
    }

    g_free (m);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Masks separated by ';', read from left to right as .gitignore reads its
 * lines: the last one that matches decides, a '!' in front of it says no, and
 * where none matches the answer is no.  "*.c;!test_*.c" is the C sources but
 * the tests.
 */
gboolean
user_menu_ini_path_match (const char *masks, const char *path, gboolean is_dir, const char *base)
{
    gchar **items;
    gboolean result = FALSE;
    int i;

    if (path == NULL)
        return FALSE;

    items = g_strsplit (masks, ";", -1);

    for (i = 0; items[i] != NULL; i++)
    {
        const char *mask = g_strstrip (items[i]);
        gboolean negate = FALSE;

        if (*mask == '!')
        {
            negate = TRUE;
            mask++;
        }
        else if (mask[0] == '\\' && mask[1] == '!')
            mask++;  // a name that starts with '!'

        if (*mask != '\0' && um_path_one (mask, path, is_dir, base))
            result = !negate;
    }

    g_strfreev (items);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
user_menu_ini_on_match (const char *on, mode_t mode, gboolean link_to_dir, gboolean stale_link)
{
    um_on_data_t d = { mode, link_to_dir, stale_link, TRUE };

    return um_list_match (on, um_on_one, &d);
}

/* --------------------------------------------------------------------------------------------- */

/** Whether every name in the list is a type; the first one that is not goes to bad. */
gboolean
user_menu_ini_on_check (const char *on, char **bad)
{
    gchar **items;
    gboolean ok = TRUE;
    int i;

    items = g_strsplit (on, ";", -1);

    for (i = 0; items[i] != NULL && ok; i++)
    {
        const char *item = g_strstrip (items[i]);
        um_on_data_t d = { 0, FALSE, FALSE, TRUE };

        if (*item == '!')
            item++;
        if (*item == '\0')
            continue;

        (void) um_on_one (item, &d);
        if (!d.known)
        {
            ok = FALSE;
            if (bad != NULL)
                *bad = g_strdup (item);
        }
    }

    g_strfreev (items);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * A regular expression found anywhere in the path, or in its last part where
 * the expression has no '/'; a '!' in front turns it over.
 */
gboolean
user_menu_ini_regex_match (const char *regex, const char *path)
{
    const gboolean negate = *regex == '!';
    const char *re = regex + (negate ? 1 : 0);
    const char *str = path;

    if (path == NULL)
        return FALSE;

    // like a mask: with no '/' it looks at the last part of the path only
    if (strchr (re, '/') == NULL && strrchr (path, '/') != NULL)
        str = strrchr (path, '/') + 1;

    return mc_search (re, NULL, str, MC_SEARCH_T_REGEX) != negate;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The path of what the cursor stands on.  ".." stays "dir/..": it is in the
 * directory of the panel, so a mask of that directory holds on it, and a mask
 * of a name does not.
 */
static char *
um_entry_path (const WPanel *panel, const file_entry_t *fe, gboolean *is_dir)
{
    *is_dir = S_ISDIR (fe->st.st_mode) || fe->f.link_to_dir != 0;
    return g_build_filename (vfs_path_as_str (panel->cwd_vpath), fe->fname->str, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The keys of one panel; cond points at the first of them for that panel, and
 * base is the directory relative masks start from, or NULL.
 */
static gboolean
um_panel_matches (const WPanel *panel, char *const *cond, const char *base)
{
    const file_entry_t *fe;
    char *path = NULL;
    gboolean is_dir = FALSE;
    gboolean ok = TRUE;
    int k;

    for (k = UM_COND_PATH; k <= UM_COND_PANEL_LAST && cond[k] == NULL; k++)
        ;
    if (k > UM_COND_PANEL_LAST)
        return TRUE;

    if (panel == NULL)
        return FALSE;

    fe = panel_current_entry (panel);
    if (fe != NULL)
        path = um_entry_path (panel, fe, &is_dir);

    if (cond[UM_COND_PATH] != NULL)
        ok = user_menu_ini_path_match (cond[UM_COND_PATH], path, is_dir, base);

    if (ok && cond[UM_COND_PATH_RE] != NULL)
        ok = user_menu_ini_regex_match (cond[UM_COND_PATH_RE], path);

    if (ok && cond[UM_COND_ON] != NULL)
        ok = fe != NULL
            && user_menu_ini_on_match (cond[UM_COND_ON], fe->st.st_mode, fe->f.link_to_dir != 0,
                                       fe->f.stale_link != 0);

    if (ok && cond[UM_COND_EXEC] != NULL)
    {
        const gboolean is_exec = fe != NULL && !is_dir && (fe->st.st_mode & 0111) != 0;

        ok = is_exec == (strcmp (cond[UM_COND_EXEC], "true") == 0);
    }

    if (ok && cond[UM_COND_MARKED] != NULL)
        ok = (panel->marked != 0) == (strcmp (cond[UM_COND_MARKED], "true") == 0);

    g_free (path);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/** A set of conditions: the programs it needs, the current panel, the other one. */
static gboolean
um_conds_hold (const user_menu_entry_t *entry, char *const *cond)
{
    const WPanel *other = NULL;
    const char *base = NULL;

    if (cond[UM_COND_NEEDS] != NULL)
    {
        gchar **programs;
        gboolean all = TRUE;
        int i;

        programs = g_strsplit (cond[UM_COND_NEEDS], ";", -1);
        for (i = 0; programs[i] != NULL && all; i++)
        {
            const char *program = g_strstrip (programs[i]);

            if (*program != '\0')
            {
                char *path;

                path = g_find_program_in_path (program);
                all = path != NULL;
                g_free (path);
            }
        }
        g_strfreev (programs);

        if (!all)
            return FALSE;
    }

    // .mc6menu lies in the directory of the panel, and its relative masks start there
    if (entry->level == MENU_LEVEL_LOCAL && current_panel != NULL)
        base = vfs_path_as_str (current_panel->cwd_vpath);

    if (!um_panel_matches (current_panel, cond, base))
        return FALSE;

    if (get_other_type () == view_listing)
        other = other_panel;

    return um_panel_matches (other, cond + UM_COND_OTHER, base);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
user_menu_ini_entry_visible (const user_menu_entry_t *entry)
{
    return um_conds_hold (entry, entry->cond);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Whether the menu opens on the entry: default=true says so wherever it is
 * shown, the default.* keys where they hold.
 */
gboolean
user_menu_ini_entry_default (const user_menu_entry_t *entry)
{
    int k;

    if (entry->is_default)
        return TRUE;

    for (k = 0; k < UM_COND_COUNT && entry->dcond[k] == NULL; k++)
        ;

    return k < UM_COND_COUNT && um_conds_hold (entry, entry->dcond);
}

/* --------------------------------------------------------------------------------------------- */
