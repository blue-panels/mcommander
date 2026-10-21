/*
   Panel plugin gh for the M-Commander
   The owners, their repositories, and what a repository holds

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

#include <config.h>

#include <string.h>

#include "lib/global.h"

#include "src/panel-plugins/gh/gh-internal.h"

/*** file scope variables ************************************************************************/

static const mc_panel_column_t gh_owner_columns[] = {
    { "origin", "Origin", 8, FALSE, J_LEFT_FIT, TRUE },
};

static const mc_panel_column_t gh_repo_columns[] = {
    { "vis", "Visibility", 7, FALSE, J_LEFT_FIT, TRUE },
    { "stars", "Stars", 5, FALSE, J_RIGHT, TRUE },
    { "flags", "Flags", 8, FALSE, J_LEFT_FIT, TRUE },
    { "descr", "Description", 12, TRUE, J_LEFT_FIT, TRUE },
};

/* --------------------------------------------------------------------------------------------- */

#define GH_REPO_FIELDS "name,visibility,isFork,isArchived,stargazerCount,updatedAt,description"
#define GH_REPO_ROW                                                                                \
    "[.name,(.visibility|ascii_downcase),(.isFork|tostring),(.isArchived|tostring),"               \
    "(.stargazerCount|tostring),.updatedAt,(.description//\"\")]|@tsv"

/*** file scope functions ************************************************************************/

static void
gh_owners_add (gh_data_t *data, const char *login, const char *origin)
{
    gh_item_t *item;
    guint i;

    if (login == NULL || login[0] == '\0')
        return;
    for (i = 0; i < data->items->len; i++)
        if (strcmp (((const gh_item_t *) g_ptr_array_index (data->items, i))->name, login) == 0)
            return;

    item = gh_item_new (login, GH_ITEM_OWNER, TRUE);
    item->state = g_strdup (origin);
    g_ptr_array_add (data->items, item);
}

/*** public functions ****************************************************************************/

/* gh:/ lists the account gh is logged in as, the owner of the repository the
   panel came from, and the places the user saved. A saved owner is listed
   like the others; a place below one goes by its path with colons for the
   slashes, which no owner or repository name has. */
gboolean
gh_owners_reload (gh_data_t *data, char **err_text)
{
    static const char *const args[] = { "api", "user", "--jq", ".login", NULL };
    char *login = NULL;
    guint i;

    data->items = g_ptr_array_new_with_free_func (gh_item_free);

    if (!gh_run (data, NULL, args, FALSE, &login, err_text))
    {
        if (*err_text != NULL && (*err_text)[0] != '\0')
            gh_error (data, *err_text);
        login = NULL;
    }
    else
        g_strstrip (login);
    g_clear_pointer (err_text, g_free);

    gh_owners_add (data, login, "you");
    gh_owners_add (data, data->here_owner, "here");
    for (i = 0; i < data->favorites->len; i++)
    {
        const char *location = (const char *) g_ptr_array_index (data->favorites, i);

        if (strchr (location, '/') == NULL)
            gh_owners_add (data, location, "favorite");
        else
        {
            char *name;
            gh_item_t *item;

            name = g_strdelimit (g_strdup (location), "/", ':');
            item = gh_item_new (name, GH_ITEM_FAVORITE, TRUE);
            item->state = g_strdup ("favorite");
            item->url = g_strdup (location);
            g_ptr_array_add (data->items, item);
            g_free (name);
        }
    }
    g_free (login);

    /* not logged in: say so, but the owners the user added still go */
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
gh_repos_reload (gh_data_t *data, char **err_text)
{
    char *limit;
    char *output = NULL;
    char **lines;
    guint i;
    gboolean ok;

    limit = g_strdup_printf ("%d", data->limit);
    {
        const char *const args[] = {
            "repo",   "list",         data->owner, "--limit",          limit,
            "--json", GH_REPO_FIELDS, "--jq",      ".[]|" GH_REPO_ROW, NULL
        };

        ok = gh_run (data, NULL, args, FALSE, &output, err_text);
    }
    g_free (limit);
    if (!ok)
        return FALSE;

    data->items = g_ptr_array_new_with_free_func (gh_item_free);
    lines = g_strsplit (output, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        char **f;
        guint n;
        gh_item_t *item;
        GString *flags;

        if (lines[i][0] == '\0')
            continue;
        f = gh_split_row (lines[i], &n);
        if (n < 7 || f[0][0] == '\0')
        {
            g_strfreev (f);
            continue;
        }

        item = gh_item_new (f[0], GH_ITEM_REPO, TRUE);
        item->title = g_strdup (f[0]);
        item->state = g_strdup (f[1]);
        flags = g_string_new (NULL);
        if (strcmp (f[2], "true") == 0)
            g_string_append (flags, "fork");
        if (strcmp (f[3], "true") == 0)
            g_string_append_printf (flags, "%sarchived", flags->len != 0 ? "," : "");
        item->labels = g_string_free (flags, FALSE);
        item->branch = g_strdup (f[4]);
        item->updated = gh_parse_time (f[5]);
        item->body = g_strdup (f[6]);
        g_ptr_array_add (data->items, item);
        g_strfreev (f);
    }
    g_strfreev (lines);
    g_free (output);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
gh_repo_reload (gh_data_t *data)
{
    data->items = g_ptr_array_new_with_free_func (gh_item_free);
    g_ptr_array_add (data->items, gh_item_new (gh_pulls_entry, GH_ITEM_SECTION, TRUE));
    g_ptr_array_add (data->items, gh_item_new (gh_issues_entry, GH_ITEM_SECTION, TRUE));
    g_ptr_array_add (data->items, gh_item_new (gh_repo_file, GH_ITEM_FILE, FALSE));
}

/* --------------------------------------------------------------------------------------------- */

/* What the list of repositories already knows about one, for Quick View. */
char *
gh_repo_summary (const gh_item_t *item, const char *owner)
{
    GString *s;
    char *updated;

    updated = gh_format_time (item->updated);
    s = g_string_new (NULL);
    g_string_append_printf (s, "%s/%s\n\n", owner, item->title);
    if (item->body != NULL && item->body[0] != '\0')
        g_string_append_printf (s, "%s\n\n", item->body);
    g_string_append_printf (s, "Visibility: %s\n", item->state != NULL ? item->state : "");
    if (item->labels != NULL && item->labels[0] != '\0')
        g_string_append_printf (s, "Flags:      %s\n", item->labels);
    g_string_append_printf (s, "Stars:      %s\n", item->branch != NULL ? item->branch : "");
    g_string_append_printf (s, "Updated:    %s\n", updated);
    g_free (updated);

    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

const mc_panel_column_t *
gh_owners_get_columns (size_t *count)
{
    *count = G_N_ELEMENTS (gh_owner_columns);
    return gh_owner_columns;
}

/* --------------------------------------------------------------------------------------------- */

const mc_panel_column_t *
gh_repos_get_columns (size_t *count)
{
    *count = G_N_ELEMENTS (gh_repo_columns);
    return gh_repo_columns;
}

/* --------------------------------------------------------------------------------------------- */

const char *
gh_repos_get_column_value (const gh_item_t *item, const char *col)
{
    if (strcmp (col, "origin") == 0 || strcmp (col, "vis") == 0)
        return item->state;
    if (strcmp (col, "stars") == 0)
        return item->branch;
    if (strcmp (col, "flags") == 0)
        return item->labels;
    if (strcmp (col, "descr") == 0)
        return item->body;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* The GitHub repository of a local directory, the one gh itself takes there:
   the default set by "gh repo set-default", else the remote. */
gboolean
gh_repo_of_dir (const gh_data_t *data, const char *dir, char **owner, char **repo)
{
    static const char *const args[] = { "repo",       "view", "--json",
                                        "owner,name", "--jq", "[.owner.login,.name]|@tsv",
                                        NULL };
    char *line;
    char **f;
    guint n;
    gboolean ok = FALSE;

    if (dir == NULL || !g_file_test (dir, G_FILE_TEST_IS_DIR))
        return FALSE;

    line = gh_run_line (data, dir, args);
    if (line == NULL)
        return FALSE;

    f = gh_split_row (line, &n);
    g_free (line);
    if (n == 2 && f[0][0] != '\0' && f[1][0] != '\0')
    {
        *owner = g_strdup (f[0]);
        *repo = g_strdup (f[1]);
        ok = TRUE;
    }
    g_strfreev (f);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* The repository of the directory the panel was opened from. Its clone is
   remembered, so that Alt-G finds it from anywhere later. */
gboolean
gh_detect_repo (gh_data_t *data, const char *dir)
{
    char *top;

    if (!gh_repo_of_dir (data, dir, &data->here_owner, &data->here_repo))
        return FALSE;

    top = gh_git_toplevel (dir);
    data->here_dir = top != NULL ? top : g_canonicalize_filename (dir, NULL);
    gh_clone_save (data->here_owner, data->here_repo, data->here_dir);
    return TRUE;
}
