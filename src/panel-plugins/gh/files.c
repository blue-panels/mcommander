/*
   Panel plugin gh for the M-Commander
   The files a pull request changes, and the two sides of each

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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lib/global.h"

#include "src/panel-plugins/gh/gh-internal.h"

/*** file scope variables ************************************************************************/

static const mc_panel_column_t gh_file_columns[] = {
    { "status", "Status", 8, FALSE, J_LEFT_FIT, TRUE },
    { "changes", "Changes", 9, FALSE, J_RIGHT, TRUE },
};

/* --------------------------------------------------------------------------------------------- */

/* GitHub compares from where the branch left the base, as its own page does:
   the merge base first, then a row for each file */
#define GH_COMPARE_ROWS                                                                            \
    ".merge_base_commit.sha,(.files[]|[.status,.filename,(.previous_filename//\"\"),"              \
    "(.additions|tostring),(.deletions|tostring),(.patch//\"\")]|@tsv)"

/* the compare of GitHub stops there */
#define GH_COMPARE_MAX_FILES 300

/*** file scope functions ************************************************************************/

/* The head of the pull request and the branch it goes into. */
static gboolean
gh_files_heads (gh_data_t *data, char **base_ref, char **err_text)
{
    char *repo;
    char *number;
    char *output = NULL;
    char **f;
    guint n;
    gboolean ok;

    repo = g_strdup_printf ("%s/%s", data->owner, data->repo);
    number = g_strdup_printf ("%d", data->number);
    {
        const char *const args[] = { "pr",
                                     "view",
                                     number,
                                     "-R",
                                     repo,
                                     "--json",
                                     "headRefOid,baseRefName",
                                     "--jq",
                                     "[.headRefOid,.baseRefName]|@tsv",
                                     NULL };

        ok = gh_run (data, NULL, args, FALSE, &output, err_text);
    }
    g_free (number);
    g_free (repo);
    if (!ok)
        return FALSE;

    f = gh_split_row (g_strchomp (output), &n);
    g_free (output);
    ok = n == 2 && f[0][0] != '\0' && f[1][0] != '\0';
    if (ok)
    {
        g_free (data->head_sha);
        data->head_sha = g_strdup (f[0]);
        *base_ref = g_strdup (f[1]);
    }
    g_strfreev (f);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* A file of the repository as it is at @sha, downloaded once while the pull
   request is open; "" stands for a side that has no file. The path stays the
   plugin's: the caller does not remove it. */
static const char *
gh_files_blob (gh_data_t *data, const char *sha, const char *path)
{
    char *key;
    char *local = NULL;
    const char *cached;
    GError *error = NULL;
    gboolean ok = TRUE;
    int fd;

    key = sha != NULL ? g_strdup_printf ("%s:%s", sha, path) : g_strdup ("");
    cached = g_hash_table_lookup (data->blobs, key);
    if (cached != NULL)
    {
        g_free (key);
        return cached;
    }

    fd = g_file_open_tmp ("mc-gh-XXXXXX", &local, &error);
    if (fd == -1)
    {
        g_clear_error (&error);
        g_free (key);
        return NULL;
    }

    if (sha != NULL)
    {
        char *escaped;
        char *url;

        escaped = g_uri_escape_string (path, "/", FALSE);
        url = g_strdup_printf ("repos/%s/%s/contents/%s?ref=%s", data->owner, data->repo, escaped,
                               sha);
        {
            const char *const args[] = { "api", "-H", "Accept: application/vnd.github.raw", url,
                                         NULL };

            ok = gh_run_to_fd (data, args, fd);
        }
        g_free (url);
        g_free (escaped);
    }
    if (close (fd) != 0)
        ok = FALSE;

    if (!ok)
    {
        char *msg;

        unlink (local);
        g_free (local);
        g_free (key);
        msg = g_strdup_printf (_ ("Cannot fetch %s at %.10s."), path, sha != NULL ? sha : "");
        gh_error (data, msg);
        g_free (msg);
        return NULL;
    }

    /* named like the file, so the viewer knows its kind */
    mc_pp_rename_with_ext (&local, path);
    g_hash_table_insert (data->blobs, key, local);
    return local;
}

/*** public functions ****************************************************************************/

void
gh_blob_free (gpointer p)
{
    char *local = (char *) p;

    if (local != NULL)
        unlink (local);
    g_free (local);
}

/* --------------------------------------------------------------------------------------------- */

/* files/ of the open pull request, in the order GitHub gives them. */
gboolean
gh_files_reload (gh_data_t *data, char **err_text)
{
    char *base_ref = NULL;
    char *escaped;
    char *url;
    char *output = NULL;
    char **lines;
    time_t now;
    guint i;
    gboolean ok;

    if (!gh_files_heads (data, &base_ref, err_text))
        return FALSE;

    escaped = g_uri_escape_string (base_ref, NULL, FALSE);
    url = g_strdup_printf ("repos/%s/%s/compare/%s...%s", data->owner, data->repo, escaped,
                           data->head_sha);
    {
        const char *const args[] = { "api", url, "--jq", GH_COMPARE_ROWS, NULL };

        ok = gh_run (data, NULL, args, FALSE, &output, err_text);
    }
    g_free (url);
    g_free (escaped);
    g_free (base_ref);
    if (!ok)
        return FALSE;

    data->items = g_ptr_array_new_with_free_func (gh_item_free);
    lines = g_strsplit (output, "\n", -1);
    g_free (output);

    g_free (data->base_sha);
    data->base_sha = lines[0] != NULL && lines[0][0] != '\0' ? g_strdup (lines[0]) : NULL;

    /* The panel puts the newest on top: each file is a second older than the
       one before it, so GitHub's order, by path, is what shows. */
    now = time (NULL);
    for (i = 1; lines[0] != NULL && lines[i] != NULL; i++)
    {
        char **f;
        guint n;

        f = gh_split_row (lines[i], &n);
        if (n == 6 && f[1][0] != '\0')
        {
            gh_item_t *item;

            item = gh_item_new (f[1], GH_ITEM_CHANGE, FALSE);
            item->state = g_strdup (f[0]);
            item->url = g_strdup (f[2]);
            item->labels = g_strdup_printf ("+%s -%s", f[3], f[4]);
            item->body = g_strdup (f[5]);
            item->updated = now - (time_t) i;
            g_ptr_array_add (data->items, item);
        }
        g_strfreev (f);
    }
    g_strfreev (lines);

    data->files_truncated = data->items->len >= GH_COMPARE_MAX_FILES;
    return data->base_sha != NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* Quick View: the hunks GitHub sent with the list, so moving the cursor costs
   no request. */
char *
gh_files_patch_text (const gh_item_t *item)
{
    const char *old = item->url != NULL && item->url[0] != '\0' ? item->url : item->name;

    if (item->body == NULL || item->body[0] == '\0')
        return g_strdup_printf ("%s: %s\n\nNo patch: the file is binary, or too large for "
                                "GitHub to show one.\n",
                                item->name, item->state);

    return g_strdup_printf ("--- a/%s\n+++ b/%s\n%s\n", old, item->name, item->body);
}

/* --------------------------------------------------------------------------------------------- */

/* Shift-F3: where the branch left the base against the head of the pull
   request, both sides read only. An added or a removed file has an empty side. */
mc_pp_result_t
gh_files_diff (gh_data_t *data, const gh_item_t *item)
{
    const char *old = item->url != NULL && item->url[0] != '\0' ? item->url : item->name;
    gboolean added = strcmp (item->state, "added") == 0;
    gboolean removed = strcmp (item->state, "removed") == 0;
    const char *left;
    const char *right;
    char *left_label;
    char *right_label;
    gboolean ok;

    if (data->base_sha == NULL || data->head_sha == NULL)
        return MC_PPR_FAILED;

    left = gh_files_blob (data, added ? NULL : data->base_sha, old);
    if (left == NULL)
        return MC_PPR_FAILED;
    right = gh_files_blob (data, removed ? NULL : data->head_sha, item->name);
    if (right == NULL)
        return MC_PPR_FAILED;

    /* from the top of the repository: the view takes a relative name for one
       below the current directory */
    left_label = g_strdup_printf ("/%s (base %.7s)", old, data->base_sha);
    right_label = g_strdup_printf ("/%s (#%d %.7s)", item->name, data->number, data->head_sha);

    if (data->host->open_diff_labeled != NULL)
        ok = data->host->open_diff_labeled (data->host, left, left_label, right, right_label,
                                            MC_PP_DIFF_LEFT_READ_ONLY | MC_PP_DIFF_RIGHT_READ_ONLY);
    else
        ok = data->host->open_diff != NULL && data->host->open_diff (data->host, left, right);
    g_free (left_label);
    g_free (right_label);

    if (!ok)
        gh_error (data, _ ("Cannot open the diff viewer."));
    return ok ? MC_PPR_OK : MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

/* F3 and copying out: the file as the pull request has it, or as it was
   before for one the pull request removes. A copy, which the core removes. */
mc_pp_result_t
gh_files_local_copy (gh_data_t *data, const gh_item_t *item, char **local_path)
{
    gboolean removed = strcmp (item->state, "removed") == 0;
    const char *old = item->url != NULL && item->url[0] != '\0' ? item->url : item->name;
    const char *blob;
    char *contents = NULL;
    gsize len = 0;
    gboolean ok;

    if (data->base_sha == NULL || data->head_sha == NULL)
        return MC_PPR_FAILED;

    blob = removed ? gh_files_blob (data, data->base_sha, old)
                   : gh_files_blob (data, data->head_sha, item->name);
    if (blob == NULL || !g_file_get_contents (blob, &contents, &len, NULL))
        return MC_PPR_FAILED;

    ok = mc_pp_write_temp_file ("mc-gh-XXXXXX", contents, (gssize) len, local_path);
    g_free (contents);
    if (!ok)
        return MC_PPR_FAILED;

    mc_pp_rename_with_ext (local_path, item->name);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

const mc_panel_column_t *
gh_files_get_columns (size_t *count)
{
    *count = G_N_ELEMENTS (gh_file_columns);
    return gh_file_columns;
}

/* --------------------------------------------------------------------------------------------- */

const char *
gh_files_get_column_value (const gh_item_t *item, const char *col)
{
    if (strcmp (col, "status") == 0)
        return item->state;
    if (strcmp (col, "changes") == 0)
        return item->labels;
    return NULL;
}
