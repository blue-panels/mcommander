/*
   Panel plugin gh for the M-Commander
   GitHub pull requests and issues as a tree, through the gh command

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
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/keybind.h"
#include "lib/tty/key.h"
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "src/viewer/mcviewer.h"

#include "src/panel-plugins/gh/gh-internal.h"

/*** forward declarations (file scope functions) *************************************************/

static void *gh_open (mc_panel_host_t *host, const char *open_path);
static void gh_close (void *plugin_data);
static mc_pp_result_t gh_get_items (void *plugin_data, void *list_ptr);
static mc_pp_result_t gh_chdir (void *plugin_data, const char *path);
static mc_pp_result_t gh_enter (void *plugin_data, const char *name, const struct stat *st);
static mc_pp_result_t gh_view (void *plugin_data, const char *fname, const struct stat *st,
                               gboolean plain_view);
static mc_pp_result_t gh_get_quick_view (void *plugin_data, const char *fname,
                                         const struct stat *st, char **local_path);
static mc_pp_result_t gh_get_local_copy (void *plugin_data, const char *fname, char **local_path);
static mc_pp_result_t gh_get_help_info (void *plugin_data, const char **filename,
                                        const char **node);
static mc_pp_result_t gh_delete_items (void *plugin_data, const char **names, int count);
static const char *gh_get_title (void *plugin_data);
static char *gh_get_location (void *plugin_data);
static mc_pp_result_t gh_handle_key (void *plugin_data, int key);
static mc_pp_result_t gh_create_item (void *plugin_data);
static const mc_panel_column_t *gh_get_columns (void *plugin_data, size_t *count);
static const char *gh_get_column_value (void *plugin_data, const char *fname,
                                        const char *column_id);
static const char *gh_get_footer (void *plugin_data);
static const char *gh_get_focus_name (void *plugin_data);
static const char *gh_get_default_format (void *plugin_data);
static mc_pp_result_t gh_reload (void *plugin_data);

/*** file scope variables ************************************************************************/

const char gh_pulls_entry[] = "pulls";
const char gh_issues_entry[] = "issues";
const char gh_repo_file[] = "repo.md";
const char gh_description_file[] = "description.md";
const char gh_conversation_entry[] = "conversation";
const char gh_files_entry[] = "files";
const char gh_diff_file[] = "diff.patch";
const char gh_commits_entry[] = "commits";
const char gh_checks_file[] = "checks.txt";

static const mc_panel_plugin_t gh_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "gh",
    .display_name = N_ ("GitHub"),
    .proto = "gh",
    .prefix = "gh:",
    .flags = MC_PPF_NAVIGATE | MC_PPF_GET_FILES | MC_PPF_DELETE | MC_PPF_CUSTOM_TITLE
        | MC_PPF_CREATE | MC_PPF_VIEW_ON_ENTER | MC_PPF_SHOW_IN_DRIVE_MENU,

    .open = gh_open,
    .close = gh_close,
    .get_items = gh_get_items,

    .chdir = gh_chdir,
    .enter = gh_enter,
    .view = gh_view,
    .get_help_info = gh_get_help_info,
    .get_local_copy = gh_get_local_copy,
    .delete_items = gh_delete_items,
    .get_title = gh_get_title,
    .get_location = gh_get_location,
    .handle_key = gh_handle_key,
    .create_item = gh_create_item,

    .get_columns = gh_get_columns,
    .get_column_value = gh_get_column_value,
    .get_footer = gh_get_footer,
    .get_focus_name = gh_get_focus_name,
    .get_default_format = gh_get_default_format,
    .default_sort_id = "mtime",
    .default_sort_reverse = TRUE,
    .reload = gh_reload,
    .get_quick_view = gh_get_quick_view,
};

/*** file scope functions ************************************************************************/

/* Inside one pull request or issue, where the panel keeps it as data->topic */
static gboolean
gh_view_in_topic (gh_view_t view)
{
    return view == GH_VIEW_PULL || view == GH_VIEW_ISSUE || view == GH_VIEW_COMMITS
        || view == GH_VIEW_FILES || view == GH_VIEW_PULL_CONV || view == GH_VIEW_ISSUE_CONV;
}

/* --------------------------------------------------------------------------------------------- */

/* About pull requests rather than issues */
static gboolean
gh_view_of_pull (gh_view_t view)
{
    return view == GH_VIEW_PULLS || view == GH_VIEW_PULL || view == GH_VIEW_COMMITS
        || view == GH_VIEW_FILES || view == GH_VIEW_PULL_CONV;
}

/* --------------------------------------------------------------------------------------------- */

static gh_item_t *
gh_item_copy (const gh_item_t *src)
{
    gh_item_t *item;

    item = gh_item_new (src->name, src->kind, src->is_dir);
    item->number = src->number;
    item->title = g_strdup (src->title);
    item->author = g_strdup (src->author);
    item->state = g_strdup (src->state);
    item->labels = g_strdup (src->labels);
    item->branch = g_strdup (src->branch);
    item->review = g_strdup (src->review);
    item->url = g_strdup (src->url);
    item->body = g_strdup (src->body);
    item->created = src->created;
    item->updated = src->updated;
    return item;
}

/* --------------------------------------------------------------------------------------------- */

static void
gh_set_pending_focus (gh_data_t *data, const char *name)
{
    g_free (data->pending_focus);
    data->pending_focus = g_strdup (name);
}

/* --------------------------------------------------------------------------------------------- */

/* The open pull request or issue; NULL closes it. Its files go with it. */
static void
gh_set_topic (gh_data_t *data, gh_item_t *topic)
{
    if (data->topic != NULL)
        gh_item_free (data->topic);
    data->topic = topic;
    g_hash_table_remove_all (data->texts);
    g_hash_table_remove_all (data->blobs);
    g_clear_pointer (&data->base_sha, g_free);
    g_clear_pointer (&data->head_sha, g_free);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
gh_get_path (gh_data_t *data)
{
    g_free (data->title_buf);

    switch (data->view)
    {
    case GH_VIEW_OWNER:
        data->title_buf = g_strdup_printf ("/%s", data->owner);
        break;
    case GH_VIEW_REPO:
        data->title_buf = g_strdup_printf ("/%s/%s", data->owner, data->repo);
        break;
    case GH_VIEW_PULLS:
        data->title_buf = g_strdup_printf ("/%s/%s/%s", data->owner, data->repo, gh_pulls_entry);
        break;
    case GH_VIEW_PULL:
        data->title_buf =
            g_strdup_printf ("/%s/%s/%s/%d", data->owner, data->repo, gh_pulls_entry, data->number);
        break;
    case GH_VIEW_COMMITS:
        data->title_buf = g_strdup_printf ("/%s/%s/%s/%d/%s", data->owner, data->repo,
                                           gh_pulls_entry, data->number, gh_commits_entry);
        break;
    case GH_VIEW_PULL_CONV:
        data->title_buf = g_strdup_printf ("/%s/%s/%s/%d/%s", data->owner, data->repo,
                                           gh_pulls_entry, data->number, gh_conversation_entry);
        break;
    case GH_VIEW_FILES:
        data->title_buf = g_strdup_printf ("/%s/%s/%s/%d/%s", data->owner, data->repo,
                                           gh_pulls_entry, data->number, gh_files_entry);
        break;
    case GH_VIEW_ISSUE_CONV:
        data->title_buf = g_strdup_printf ("/%s/%s/%s/%d/%s", data->owner, data->repo,
                                           gh_issues_entry, data->number, gh_conversation_entry);
        break;
    case GH_VIEW_ISSUES:
        data->title_buf = g_strdup_printf ("/%s/%s/%s", data->owner, data->repo, gh_issues_entry);
        break;
    case GH_VIEW_ISSUE:
        data->title_buf = g_strdup_printf ("/%s/%s/%s/%d", data->owner, data->repo, gh_issues_entry,
                                           data->number);
        break;
    default:
        data->title_buf = g_strdup ("/");
        break;
    }

    return data->title_buf;
}

/* --------------------------------------------------------------------------------------------- */

static void
gh_set_view (gh_data_t *data, gh_view_t view)
{
    data->view = view;
    gh_clear_items (data);
    if (!gh_view_in_topic (view))
        gh_set_topic (data, NULL);
    else
        g_hash_table_remove_all (data->texts);

    if (data->host != NULL && data->host->add_history != NULL)
    {
        char *location;

        location = g_strdup_printf ("%s:%s", gh_plugin.proto, gh_get_path (data));
        data->host->add_history (data->host, location);
        g_free (location);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
gh_reload_items (gh_data_t *data)
{
    char *err_text = NULL;
    gboolean ok = TRUE;

    gh_clear_items (data);

    switch (data->view)
    {
    case GH_VIEW_ROOT:
        ok = gh_owners_reload (data, &err_text);
        break;
    case GH_VIEW_OWNER:
        ok = gh_repos_reload (data, &err_text);
        break;
    case GH_VIEW_REPO:
        gh_repo_reload (data);
        break;
    case GH_VIEW_PULLS:
        ok = gh_pulls_reload (data, &err_text);
        break;
    case GH_VIEW_ISSUES:
        ok = gh_issues_reload (data, &err_text);
        break;
    case GH_VIEW_PULL:
    case GH_VIEW_ISSUE:
    case GH_VIEW_COMMITS:
    case GH_VIEW_FILES:
    case GH_VIEW_PULL_CONV:
    case GH_VIEW_ISSUE_CONV:
        /* opened by its address: the list it would come from was never read */
        if (data->topic == NULL)
        {
            gh_item_t *topic;

            topic = gh_view_of_pull (data->view) ? gh_pull_fetch (data, data->number, &err_text)
                                                 : gh_issue_fetch (data, data->number, &err_text);
            if (topic != NULL)
                gh_set_topic (data, topic);
            else
                ok = FALSE;
        }
        if (data->view == GH_VIEW_PULL)
            gh_pull_reload (data);
        else if (data->view == GH_VIEW_ISSUE)
            gh_issue_reload (data);
        else if (!ok)
            break;
        else if (data->view == GH_VIEW_COMMITS)
            ok = gh_commits_reload (data, &err_text);
        else if (data->view == GH_VIEW_FILES)
            ok = gh_files_reload (data, &err_text);
        else if (data->view == GH_VIEW_PULL_CONV)
            ok = gh_pull_replies_reload (data, &err_text);
        else
            ok = gh_issue_replies_reload (data, &err_text);
        break;
    default:
        break;
    }

    if (!ok)
        gh_error (data,
                  err_text != NULL && err_text[0] != '\0' ? err_text
                                                          : _ ("gh failed with no message"));
    g_free (err_text);

    if (data->items == NULL)
        data->items = g_ptr_array_new_with_free_func (gh_item_free);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* The text of a file of the current directory, fetched once. NULL with an
   error said when gh cannot give it. The caller does not free it. */
static const char *
gh_file_text (gh_data_t *data, const char *fname)
{
    char *text;
    char *err_text = NULL;

    text = g_hash_table_lookup (data->texts, fname);
    if (text != NULL)
        return text;

    switch (data->view)
    {
    case GH_VIEW_REPO:
        if (strcmp (fname, gh_repo_file) == 0)
        {
            char *repo;

            repo = g_strdup_printf ("%s/%s", data->owner, data->repo);
            {
                const char *const args[] = { "repo", "view", repo, NULL };

                (void) gh_run (data, NULL, args, FALSE, &text, &err_text);
            }
            g_free (repo);
        }
        break;
    case GH_VIEW_PULL:
        text = gh_pull_text (data, fname, &err_text);
        break;
    case GH_VIEW_ISSUE:
        text = gh_issue_text (data, fname, &err_text);
        break;
    case GH_VIEW_COMMITS:
    case GH_VIEW_PULL_CONV:
    case GH_VIEW_ISSUE_CONV:
    {
        const gh_item_t *item = gh_find_item (data, fname);

        if (item != NULL && item->kind == GH_ITEM_COMMIT)
            text = gh_commit_text (data, item, &err_text);
        else if (item != NULL && item->kind == GH_ITEM_REPLY)
            text = gh_reply_text (item);
        break;
    }
    default:
        break;
    }

    if (text == NULL)
    {
        if (err_text != NULL)
            gh_error (data, err_text);
        g_free (err_text);
        return NULL;
    }
    g_free (err_text);

    g_hash_table_insert (data->texts, g_strdup (fname), text);
    return text;
}

/* --------------------------------------------------------------------------------------------- */

/* The text an entry that is a directory stands for: the description of a
   pull request or an issue, what is known of a repository. */
static char *
gh_dir_text (gh_data_t *data, const char *fname)
{
    const gh_item_t *item;

    item = gh_find_item (data, fname);
    if (item == NULL)
        return NULL;

    switch (item->kind)
    {
    case GH_ITEM_PULL:
        return gh_topic_description (item, TRUE);
    case GH_ITEM_ISSUE:
        return gh_topic_description (item, FALSE);
    case GH_ITEM_REPO:
        return gh_repo_summary (item, data->owner);
    default:
        return NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* A temporary copy of @text named like @fname, so the viewer knows its kind. */
static mc_pp_result_t
gh_text_to_temp (const char *text, const char *fname, char **local_path)
{
    if (!mc_pp_write_temp_file ("mc-gh-XXXXXX", text, -1, local_path))
        return MC_PPR_FAILED;
    mc_pp_rename_with_ext (local_path, fname);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* gh:, gh:/<owner>, gh:/<owner>/<repo>, gh:/<owner>/<repo>/pulls[/<number>],
   gh:/<owner>/<repo>/issues[/<number>]. A number may carry the title after
   it, as the panel shows it. */
static gboolean
gh_parse_location (gh_data_t *data, const char *path)
{
    gchar **segs;
    guint n;

    while (*path == '/')
        path++;
    if (*path == '\0')
        return TRUE;

    segs = g_strsplit (path, "/", -1);
    n = g_strv_length (segs);
    while (n > 0 && segs[n - 1][0] == '\0')
        n--;

    if (n >= 1)
    {
        data->owner = g_strdup (segs[0]);
        data->view = GH_VIEW_OWNER;
    }
    if (n >= 2)
    {
        data->repo = g_strdup (segs[1]);
        data->view = GH_VIEW_REPO;
    }
    if (n >= 3)
    {
        if (strcmp (segs[2], gh_pulls_entry) == 0)
            data->view = GH_VIEW_PULLS;
        else if (strcmp (segs[2], gh_issues_entry) == 0)
            data->view = GH_VIEW_ISSUES;
    }
    if (n >= 4 && (data->view == GH_VIEW_PULLS || data->view == GH_VIEW_ISSUES))
    {
        data->number = atoi (segs[3]);
        if (data->number > 0)
            data->view = data->view == GH_VIEW_PULLS ? GH_VIEW_PULL : GH_VIEW_ISSUE;
    }
    if (n >= 5 && data->view == GH_VIEW_PULL && strcmp (segs[4], gh_commits_entry) == 0)
        data->view = GH_VIEW_COMMITS;
    else if (n >= 5 && data->view == GH_VIEW_PULL && strcmp (segs[4], gh_files_entry) == 0)
        data->view = GH_VIEW_FILES;
    else if (n >= 5 && (data->view == GH_VIEW_PULL || data->view == GH_VIEW_ISSUE)
             && strcmp (segs[4], gh_conversation_entry) == 0)
        data->view = data->view == GH_VIEW_PULL ? GH_VIEW_PULL_CONV : GH_VIEW_ISSUE_CONV;

    g_strfreev (segs);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
gh_ask_location (const char *title, const char *initial, char **location)
{
    char *text = NULL;
    gboolean ok;

    /* clang-format off */
    quick_widget_t quick_widgets[] = {
        QUICK_LABELED_INPUT (N_ ("Owner, or a path below it:"), input_label_above,
                             initial, "gh-add-favorite", &text, NULL, FALSE, FALSE,
                             INPUT_COMPLETE_NONE),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };
    /* clang-format on */

    WRect r = { -1, -1, 0, 56 };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = title,
        .help = NULL,
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    ok = quick_dialog (&qdlg) == B_ENTER && text != NULL;
    if (ok)
    {
        *location = gh_normalize_location (text);
        ok = *location != NULL;
        if (!ok)
        {
            char *msg;

            msg = g_strdup_printf (
                _ ("Neither an owner, a path below one\nnor a GitHub address:\n%s"), text);
            gh_error (NULL, msg);
            g_free (msg);
        }
    }
    g_free (text);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Save @location (owner[/repo[/...]]) among the favorites; FALSE when it is
   there already. */
static gboolean
gh_favorite_add (gh_data_t *data, const char *location)
{
    guint i;

    for (i = 0; i < data->favorites->len; i++)
        if (strcmp ((const char *) g_ptr_array_index (data->favorites, i), location) == 0)
            return FALSE;

    g_ptr_array_add (data->favorites, g_strdup (location));
    gh_favorites_save (data->favorites);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Ctrl-B: the place the panel is in, or in gh:/ the owner under the cursor. */
static mc_pp_result_t
gh_favorite_here (gh_data_t *data)
{
    const char *location = NULL;
    gboolean added;

    if (data->view == GH_VIEW_ROOT)
    {
        if (data->host != NULL && data->host->get_current != NULL)
        {
            const GString *cur = data->host->get_current (data->host);
            const gh_item_t *item = cur != NULL ? gh_find_item (data, cur->str) : NULL;

            if (item != NULL && item->kind == GH_ITEM_OWNER)
                location = item->name;
        }
        if (location == NULL)
            return MC_PPR_NOT_SUPPORTED;
    }
    else
        location = gh_get_path (data) + 1; /* without the leading slash */

    added = gh_favorite_add (data, location);
    if (data->view == GH_VIEW_ROOT)
        gh_clear_items (data);

    if (data->host != NULL && data->host->set_hint != NULL)
        data->host->set_hint (data->host,
                              added ? _ ("Added to favorites.") : _ ("Already in favorites."));
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* Shift-F4 anywhere, F7 in gh:/: a favorite typed in, the place the panel
   is in to begin with. */
static mc_pp_result_t
gh_favorite_ask (gh_data_t *data)
{
    char *initial;
    char *location = NULL;
    gboolean added;

    initial = g_strdup (data->view == GH_VIEW_ROOT ? "" : gh_get_path (data) + 1);
    if (!gh_ask_location (_ ("Add to favorites"), initial, &location))
    {
        g_free (initial);
        return MC_PPR_SKIPPED;
    }
    g_free (initial);

    added = gh_favorite_add (data, location);

    if (data->view == GH_VIEW_ROOT)
    {
        /* an owner is listed by its name; a deeper place by another one */
        if (strchr (location, '/') == NULL)
            gh_set_pending_focus (data, location);
        gh_clear_items (data);
    }
    else if (data->host != NULL && data->host->set_hint != NULL)
        data->host->set_hint (data->host,
                              added ? _ ("Added to favorites.") : _ ("Already in favorites."));
    g_free (location);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* GitHub takes an owner or a repository in any case. */
static gboolean
gh_same_name (const char *a, const char *b)
{
    return a != NULL && b != NULL && g_ascii_strcasecmp (a, b) == 0;
}

/* --------------------------------------------------------------------------------------------- */

/* The directory typed for Alt-G: ~ is the home directory, and a relative one
   is taken from where the panel was opened, else from the home directory. */
static char *
gh_clone_dir_of_input (const gh_data_t *data, const char *text)
{
    const char *base = data->start_dir != NULL ? data->start_dir : g_get_home_dir ();

    if (text[0] == '~' && (text[1] == '\0' || text[1] == '/'))
    {
        char *home;
        char *dir;

        home = g_build_filename (g_get_home_dir (), text + 1, (char *) NULL);
        dir = g_canonicalize_filename (home, NULL);
        g_free (home);
        return dir;
    }
    return g_canonicalize_filename (text, base);
}

/* --------------------------------------------------------------------------------------------- */

/* F4 in gh:/: change the path of the favorite under the cursor. */
static mc_pp_result_t
gh_favorite_edit (gh_data_t *data)
{
    const gh_item_t *item = NULL;
    char *old;
    char *location = NULL;
    guint i, at;

    if (data->host != NULL && data->host->get_current != NULL)
    {
        const GString *cur = data->host->get_current (data->host);

        if (cur != NULL)
            item = gh_find_item (data, cur->str);
    }
    if (item == NULL)
        return MC_PPR_SKIPPED;

    old = g_strdup (item->kind == GH_ITEM_FAVORITE ? item->url : item->name);
    for (at = 0; at < data->favorites->len; at++)
        if (strcmp ((const char *) g_ptr_array_index (data->favorites, at), old) == 0)
            break;
    if (at == data->favorites->len)
    {
        gh_error (data, _ ("Only a favorite can be changed."));
        g_free (old);
        return MC_PPR_SKIPPED;
    }

    if (!gh_ask_location (_ ("Change the favorite"), old, &location))
    {
        g_free (old);
        return MC_PPR_SKIPPED;
    }
    g_free (old);

    /* changed into one that is there already: the two become one */
    for (i = 0; i < data->favorites->len; i++)
        if (i != at
            && strcmp ((const char *) g_ptr_array_index (data->favorites, i), location) == 0)
            break;
    if (i < data->favorites->len)
    {
        g_ptr_array_remove_index (data->favorites, at);
        g_free (location);
        location =
            g_strdup ((const char *) g_ptr_array_index (data->favorites, i < at ? i : i - 1));
    }
    else
    {
        g_free (g_ptr_array_index (data->favorites, at));
        g_ptr_array_index (data->favorites, at) = g_strdup (location);
    }
    gh_favorites_save (data->favorites);

    gh_set_pending_focus (data, NULL);
    data->pending_focus = g_strdelimit (location, "/", ':');
    gh_clear_items (data);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* Where the clone of the repository the panel is in lies: known from an earlier
   visit, or asked for and checked to be one of it. NULL when there is none. */
static char *
gh_find_clone (gh_data_t *data)
{
    char *dir;
    char *text = NULL;
    char *owner = NULL;
    char *repo = NULL;
    char *label;
    gboolean ok;

    if (data->here_dir != NULL && gh_same_name (data->here_owner, data->owner)
        && gh_same_name (data->here_repo, data->repo))
        return g_strdup (data->here_dir);

    dir = gh_clone_load (data->owner, data->repo);
    if (dir != NULL)
        return dir;

    label = g_strdup_printf (_ ("The local clone of %s/%s:"), data->owner, data->repo);
    {
        /* clang-format off */
        quick_widget_t quick_widgets[] = {
            QUICK_LABELED_INPUT (label, input_label_above, "", "gh-clone-dir", &text, NULL,
                                 FALSE, FALSE, INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END,
        };
        /* clang-format on */

        WRect r = { -1, -1, 0, 64 };

        quick_dialog_t qdlg = {
            .rect = r,
            .title = _ ("Open in Git"),
            .help = NULL,
            .widgets = quick_widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        ok = quick_dialog (&qdlg) == B_ENTER && text != NULL && g_strstrip (text)[0] != '\0';
    }
    g_free (label);
    if (!ok)
    {
        g_free (text);
        return NULL;
    }

    /* a clone of another repository would show what this one is not */
    dir = gh_clone_dir_of_input (data, text);
    g_free (text);
    if (!gh_repo_of_dir (data, dir, &owner, &repo) || !gh_same_name (owner, data->owner)
        || !gh_same_name (repo, data->repo))
    {
        char *msg;

        msg = owner != NULL ? g_strdup_printf (_ ("%s is a clone of %s/%s, not of %s/%s."), dir,
                                               owner, repo, data->owner, data->repo)
                            : g_strdup_printf (_ ("%s is no clone of a GitHub repository."), dir);
        gh_error (data, msg);
        g_free (msg);
        g_free (owner);
        g_free (repo);
        g_free (dir);
        return NULL;
    }
    g_free (owner);
    g_free (repo);

    text = gh_git_toplevel (dir);
    if (text != NULL)
    {
        g_free (dir);
        dir = text;
    }
    gh_clone_save (data->owner, data->repo, dir);
    return dir;
}

/* --------------------------------------------------------------------------------------------- */

/* Alt-G: the clone of the repository the panel is in, in the Git panel on the
   other side. */
static mc_pp_result_t
gh_open_git (gh_data_t *data)
{
    char *dir;
    char *location;

    if (data->owner == NULL || data->repo == NULL)
        return MC_PPR_NOT_SUPPORTED;

    if (data->host == NULL || data->host->open_location == NULL)
    {
        gh_error (data, _ ("This mc cannot open another plugin from a panel."));
        return MC_PPR_SKIPPED;
    }

    dir = gh_find_clone (data);
    if (dir == NULL)
        return MC_PPR_SKIPPED;

    location = g_strdup_printf ("git:%s", dir);
    // the core says why when it cannot
    (void) data->host->open_location (data->host, location, TRUE);
    g_free (location);
    g_free (dir);

    return MC_PPR_SKIPPED;
}

/* --------------------------------------------------------------------------------------------- */

/* Alt-W: the entry under the cursor in the browser, or the place itself. */
static mc_pp_result_t
gh_web (gh_data_t *data)
{
    const gh_item_t *item = NULL;
    const char *repo_name = data->repo;
    char *repo;
    char *number = NULL;
    gboolean ok = FALSE;

    if (data->host != NULL && data->host->get_current != NULL)
    {
        const GString *cur = data->host->get_current (data->host);

        if (cur != NULL)
            item = gh_find_item (data, cur->str);
    }

    if (data->view == GH_VIEW_OWNER && item != NULL)
        repo_name = item->name;
    if (data->owner == NULL || repo_name == NULL)
        return MC_PPR_NOT_SUPPORTED;

    repo = g_strdup_printf ("%s/%s", data->owner, repo_name);
    if (item != NULL && item->kind == GH_ITEM_COMMIT)
    {
        const char *const args[] = { "browse", item->url, "-R", repo, NULL };

        ok = gh_open_web (data, args);
        g_free (repo);
        return ok ? MC_PPR_OK : MC_PPR_FAILED;
    }
    if (item != NULL && (item->kind == GH_ITEM_PULL || item->kind == GH_ITEM_ISSUE))
        number = g_strdup_printf ("%d", item->number);
    else if (gh_view_in_topic (data->view))
        number = g_strdup_printf ("%d", data->number);

    if (number == NULL)
    {
        const char *const args[] = { "repo", "view", repo, "--web", NULL };

        ok = gh_open_web (data, args);
    }
    else
    {
        gboolean pull = gh_view_of_pull (data->view);
        const char *const args[] = {
            pull ? "pr" : "issue", "view", number, "-R", repo, "--web", NULL
        };

        ok = gh_open_web (data, args);
    }
    g_free (number);
    g_free (repo);

    return ok ? MC_PPR_OK : MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

/* Alt-F: open, closed, merged (pull requests only), all, and round again. */
static mc_pp_result_t
gh_next_state (gh_data_t *data)
{
    gh_state_filter_t *state;
    char *hint;

    if (data->view == GH_VIEW_PULLS)
    {
        state = &data->pulls_state;
        *state = (gh_state_filter_t) ((*state + 1) % (GH_STATE_ALL + 1));
    }
    else if (data->view == GH_VIEW_ISSUES)
    {
        state = &data->issues_state;
        *state = *state == GH_STATE_OPEN ? GH_STATE_CLOSED
            : *state == GH_STATE_CLOSED  ? GH_STATE_ALL
                                         : GH_STATE_OPEN;
    }
    else
        return MC_PPR_NOT_SUPPORTED;

    gh_clear_items (data);
    if (data->host != NULL && data->host->set_hint != NULL)
    {
        hint = g_strdup_printf (_ ("Showing: %s"), gh_state_filter_name (*state));
        data->host->set_hint (data->host, hint);
        g_free (hint);
    }
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */
/* Plugin callbacks */
/* --------------------------------------------------------------------------------------------- */

static void *
gh_open (mc_panel_host_t *host, const char *open_path)
{
    gh_data_t *data;
    char *user_help;

    data = g_new0 (gh_data_t, 1);
    data->host = host;
    data->view = GH_VIEW_ROOT;
    data->texts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    data->blobs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, gh_blob_free);
    data->favorites = gh_favorites_load ();
    data->gh_cmd = gh_read_config_string (GH_PANEL_CONFIG_GH, "gh");
    data->limit = gh_read_config_int (GH_PANEL_CONFIG_LIMIT, GH_LIST_LIMIT_DEFAULT);
    data->key_state = gh_load_hotkey (GH_PANEL_KEY_STATE, GH_PANEL_KEY_STATE_DEFAULT, ALT ('f'));
    data->key_web = gh_load_hotkey (GH_PANEL_KEY_WEB, GH_PANEL_KEY_WEB_DEFAULT, ALT ('w'));
    data->key_fav_add =
        gh_load_hotkey (GH_PANEL_KEY_FAV_ADD, GH_PANEL_KEY_FAV_ADD_DEFAULT, XCTRL ('b'));
    data->key_git = gh_load_hotkey (GH_PANEL_KEY_GIT, GH_PANEL_KEY_GIT_DEFAULT, ALT ('g'));
    data->key_diff = gh_load_hotkey (GH_PANEL_KEY_DIFF, GH_PANEL_KEY_DIFF_DEFAULT, KEY_F (13));

    /* the user's copy first, so the help can be changed without root */
    user_help = g_build_filename (g_get_home_dir (), ".local", "lib", MC_USERCONF_DIR,
                                  "panel-plugins", "gh", "gh_panel.md", (char *) NULL);
    if (g_file_test (user_help, G_FILE_TEST_IS_REGULAR))
        data->help_filename = user_help;
    else
    {
        g_free (user_help);
        data->help_filename = g_build_filename (MC_PLUGIN_DIR, "gh_panel.md", (char *) NULL);
    }

    if (open_path != NULL && g_str_has_prefix (open_path, gh_plugin.prefix))
        gh_parse_location (data, open_path + strlen (gh_plugin.prefix));
    else
    {
        /* opened from a directory: its repository, if it has one on GitHub */
        const char *dir = open_path != NULL && open_path[0] == '/' ? open_path : ".";

        data->start_dir = g_canonicalize_filename (dir, NULL);
        if (gh_detect_repo (data, dir))
        {
            data->owner = g_strdup (data->here_owner);
            data->repo = g_strdup (data->here_repo);
            data->view = GH_VIEW_REPO;
        }
    }

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
gh_close (void *plugin_data)
{
    gh_data_t *data = (gh_data_t *) plugin_data;

    gh_clear_items (data);
    if (data->item_index != NULL)
        g_hash_table_destroy (data->item_index);
    if (data->topic != NULL)
        gh_item_free (data->topic);
    g_hash_table_destroy (data->texts);
    g_hash_table_destroy (data->blobs);
    g_free (data->base_sha);
    g_free (data->head_sha);
    g_ptr_array_free (data->favorites, TRUE);
    g_free (data->owner);
    g_free (data->repo);
    g_free (data->here_owner);
    g_free (data->here_repo);
    g_free (data->here_dir);
    g_free (data->start_dir);
    g_free (data->pending_focus);
    g_free (data->title_buf);
    g_free (data->footer_buf);
    g_free (data->help_filename);
    g_free (data->gh_cmd);
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
gh_get_items (void *plugin_data, void *list_ptr)
{
    gh_data_t *data = (gh_data_t *) plugin_data;
    time_t now;
    guint i;

    if (data->items == NULL)
        gh_reload_items (data);

    now = time (NULL);
    for (i = 0; i < data->items->len; i++)
    {
        const gh_item_t *item = (const gh_item_t *) g_ptr_array_index (data->items, i);
        mode_t mode = item->is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0644);
        const char *text;

        text = item->is_dir ? NULL : g_hash_table_lookup (data->texts, item->name);
        mc_pp_add_entry (list_ptr, item->name, mode, text != NULL ? (off_t) strlen (text) : 0,
                         item->updated != 0 ? item->updated : now);
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
gh_chdir (void *plugin_data, const char *path)
{
    gh_data_t *data = (gh_data_t *) plugin_data;
    const gh_item_t *item;

    if (strcmp (path, "..") == 0)
    {
        switch (data->view)
        {
        case GH_VIEW_ROOT:
            return MC_PPR_CLOSE;
        case GH_VIEW_OWNER:
            gh_set_pending_focus (data, data->owner);
            g_clear_pointer (&data->owner, g_free);
            gh_set_view (data, GH_VIEW_ROOT);
            break;
        case GH_VIEW_REPO:
            gh_set_pending_focus (data, data->repo);
            g_clear_pointer (&data->repo, g_free);
            gh_set_view (data, GH_VIEW_OWNER);
            break;
        case GH_VIEW_PULLS:
            gh_set_pending_focus (data, gh_pulls_entry);
            gh_set_view (data, GH_VIEW_REPO);
            break;
        case GH_VIEW_ISSUES:
            gh_set_pending_focus (data, gh_issues_entry);
            gh_set_view (data, GH_VIEW_REPO);
            break;
        case GH_VIEW_COMMITS:
        case GH_VIEW_FILES:
            gh_set_pending_focus (data,
                                  data->view == GH_VIEW_FILES ? gh_files_entry : gh_commits_entry);
            gh_set_view (data, GH_VIEW_PULL);
            break;
        case GH_VIEW_PULL_CONV:
        case GH_VIEW_ISSUE_CONV:
            gh_set_pending_focus (data, gh_conversation_entry);
            gh_set_view (data, data->view == GH_VIEW_PULL_CONV ? GH_VIEW_PULL : GH_VIEW_ISSUE);
            break;
        case GH_VIEW_PULL:
        case GH_VIEW_ISSUE:
            gh_set_pending_focus (data, data->topic != NULL ? data->topic->name : NULL);
            data->number = 0;
            gh_set_view (data, data->view == GH_VIEW_PULL ? GH_VIEW_PULLS : GH_VIEW_ISSUES);
            break;
        default:
            return MC_PPR_FAILED;
        }
        return MC_PPR_OK;
    }

    item = gh_find_item (data, path);
    if (item == NULL || !item->is_dir)
        return MC_PPR_FAILED;

    switch (data->view)
    {
    case GH_VIEW_ROOT:
        if (item->kind == GH_ITEM_FAVORITE)
        {
            /* a saved place: as if its address were typed */
            gh_parse_location (data, item->url);
            gh_set_view (data, data->view);
        }
        else
        {
            data->owner = g_strdup (item->name);
            gh_set_view (data, GH_VIEW_OWNER);
        }
        break;
    case GH_VIEW_OWNER:
        data->repo = g_strdup (item->name);
        gh_set_view (data, GH_VIEW_REPO);
        break;
    case GH_VIEW_REPO:
        gh_set_view (data,
                     strcmp (item->name, gh_pulls_entry) == 0 ? GH_VIEW_PULLS : GH_VIEW_ISSUES);
        break;
    case GH_VIEW_PULLS:
    case GH_VIEW_ISSUES:
        /* the row of the list is all the description needs */
        data->number = item->number;
        gh_set_topic (data, gh_item_copy (item));
        gh_set_view (data, data->view == GH_VIEW_PULLS ? GH_VIEW_PULL : GH_VIEW_ISSUE);
        break;
    case GH_VIEW_PULL:
    case GH_VIEW_ISSUE:
        if (strcmp (item->name, gh_conversation_entry) == 0)
            gh_set_view (data, data->view == GH_VIEW_PULL ? GH_VIEW_PULL_CONV : GH_VIEW_ISSUE_CONV);
        else if (data->view == GH_VIEW_PULL && strcmp (item->name, gh_commits_entry) == 0)
            gh_set_view (data, GH_VIEW_COMMITS);
        else if (data->view == GH_VIEW_PULL && strcmp (item->name, gh_files_entry) == 0)
            gh_set_view (data, GH_VIEW_FILES);
        else
            return MC_PPR_FAILED;
        break;
    default:
        return MC_PPR_FAILED;
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
gh_enter (void *plugin_data, const char *name, const struct stat *st)
{
    if (st != NULL && S_ISDIR (st->st_mode))
        return gh_chdir (plugin_data, name);

    /* a file: MC_PPF_VIEW_ON_ENTER sends the core to the viewer */
    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

/* F3 on a pull request, an issue or a repository shows what it is; the core
   enters any other directory and views a file through get_local_copy(). */
static mc_pp_result_t
gh_view (void *plugin_data, const char *fname, const struct stat *st, gboolean plain_view)
{
    gh_data_t *data = (gh_data_t *) plugin_data;
    char *text;
    char *local_path = NULL;
    vfs_path_t *vpath;

    (void) plain_view;

    if (fname == NULL || st == NULL || !S_ISDIR (st->st_mode))
        return MC_PPR_NOT_SUPPORTED;

    text = gh_dir_text (data, fname);
    if (text == NULL)
        return MC_PPR_NOT_SUPPORTED;

    if (gh_text_to_temp (text, gh_description_file, &local_path) != MC_PPR_OK)
    {
        g_free (text);
        return MC_PPR_FAILED;
    }
    g_free (text);

    vpath = vfs_path_from_str (local_path);
    (void) mcview_viewer (NULL, vpath, 0, 0, 0);
    vfs_path_free (vpath, TRUE);
    unlink (local_path);
    g_free (local_path);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
gh_get_quick_view (void *plugin_data, const char *fname, const struct stat *st, char **local_path)
{
    gh_data_t *data = (gh_data_t *) plugin_data;
    mc_pp_result_t result;
    char *text;

    {
        const gh_item_t *item = gh_find_item (data, fname);

        /* the hunks came with the list: no file is fetched for a glance */
        if (item != NULL && item->kind == GH_ITEM_CHANGE)
        {
            text = gh_files_patch_text (item);
            result = gh_text_to_temp (text, "change.patch", local_path);
            g_free (text);
            return result;
        }
    }

    if (st == NULL || !S_ISDIR (st->st_mode))
        return MC_PPR_NOT_SUPPORTED; /* a file: get_local_copy() */

    text = gh_dir_text (data, fname);
    if (text == NULL)
        return MC_PPR_NOT_SUPPORTED;

    result = gh_text_to_temp (text, gh_description_file, local_path);
    g_free (text);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
gh_get_local_copy (void *plugin_data, const char *fname, char **local_path)
{
    gh_data_t *data = (gh_data_t *) plugin_data;
    const gh_item_t *item;
    const char *text;

    item = gh_find_item (data, fname);
    if (item != NULL && item->kind == GH_ITEM_CHANGE)
        return gh_files_local_copy (data, item, local_path);
    if (item == NULL
        || (item->kind != GH_ITEM_FILE && item->kind != GH_ITEM_COMMIT
            && item->kind != GH_ITEM_REPLY))
        return MC_PPR_NOT_SUPPORTED;

    text = gh_file_text (data, fname);
    if (text == NULL)
        return MC_PPR_FAILED;

    /* a headline has no extension worth the name: the viewer is told a patch */
    return gh_text_to_temp (text, item->kind == GH_ITEM_COMMIT ? "commit.patch" : fname,
                            local_path);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
gh_get_help_info (void *plugin_data, const char **filename, const char **node)
{
    gh_data_t *data = (gh_data_t *) plugin_data;

    if (filename != NULL)
        *filename = data->help_filename;
    if (node != NULL)
        *node = "GitHub Plugin";
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* F8 on gh:/ forgets a saved place. Nothing on GitHub is removed. */
static mc_pp_result_t
gh_delete_items (void *plugin_data, const char **names, int count)
{
    gh_data_t *data = (gh_data_t *) plugin_data;
    gboolean changed = FALSE;
    int i;

    if (data->view != GH_VIEW_ROOT)
        return MC_PPR_NOT_SUPPORTED;

    for (i = 0; i < count; i++)
    {
        const gh_item_t *item;
        const char *location;
        guint j;

        item = gh_find_item (data, names[i]);
        if (item == NULL)
            continue;
        location = item->kind == GH_ITEM_FAVORITE ? item->url : item->name;

        for (j = 0; j < data->favorites->len; j++)
            if (strcmp ((const char *) g_ptr_array_index (data->favorites, j), location) == 0)
            {
                g_ptr_array_remove_index (data->favorites, j);
                changed = TRUE;
                break;
            }
    }

    if (!changed)
        return MC_PPR_NOT_SUPPORTED;

    gh_favorites_save (data->favorites);
    gh_clear_items (data);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
gh_get_title (void *plugin_data)
{
    return gh_get_path ((gh_data_t *) plugin_data);
}

/* --------------------------------------------------------------------------------------------- */

static char *
gh_get_location (void *plugin_data)
{
    gh_data_t *data = (gh_data_t *) plugin_data;

    return g_strdup_printf ("%s:%s", gh_plugin.proto, gh_get_path (data));
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
gh_handle_key (void *plugin_data, int key)
{
    gh_data_t *data = (gh_data_t *) plugin_data;

    if (data->key_state != 0 && key == data->key_state)
        return gh_next_state (data);

    if (data->key_web != 0 && key == data->key_web)
        return gh_web (data);

    if (data->key_fav_add != 0 && key == data->key_fav_add)
        return gh_favorite_here (data);

    if (data->key_git != 0 && key == data->key_git)
        return gh_open_git (data);

    if (data->key_diff != 0 && key == data->key_diff && data->view == GH_VIEW_FILES
        && data->host != NULL && data->host->get_current != NULL)
    {
        const GString *cur = data->host->get_current (data->host);
        const gh_item_t *item = cur != NULL ? gh_find_item (data, cur->str) : NULL;

        if (item != NULL && item->kind == GH_ITEM_CHANGE)
        {
            (void) gh_files_diff (data, item);
            return MC_PPR_SKIPPED; /* nothing in the list changed */
        }
    }

    if (key == CK_EditNew)
        return gh_favorite_ask (data);

    if (key == CK_Edit && data->view == GH_VIEW_ROOT)
        return gh_favorite_edit (data);

    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

/* F7 on gh:/ saves an owner or a place below one, typed in. */
static mc_pp_result_t
gh_create_item (void *plugin_data)
{
    gh_data_t *data = (gh_data_t *) plugin_data;

    if (data->view != GH_VIEW_ROOT)
        return MC_PPR_NOT_SUPPORTED;

    return gh_favorite_ask (data);
}

/* --------------------------------------------------------------------------------------------- */

static const mc_panel_column_t *
gh_get_columns (void *plugin_data, size_t *count)
{
    gh_data_t *data = (gh_data_t *) plugin_data;

    switch (data->view)
    {
    case GH_VIEW_ROOT:
        return gh_owners_get_columns (count);
    case GH_VIEW_OWNER:
        return gh_repos_get_columns (count);
    case GH_VIEW_PULLS:
        return gh_pulls_get_columns (count);
    case GH_VIEW_COMMITS:
    case GH_VIEW_PULL_CONV:
    case GH_VIEW_ISSUE_CONV:
        return gh_commits_get_columns (count);
    case GH_VIEW_FILES:
        return gh_files_get_columns (count);
    case GH_VIEW_ISSUES:
        return gh_issues_get_columns (count);
    default:
        *count = 0;
        return NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

static const char *
gh_get_column_value (void *plugin_data, const char *fname, const char *column_id)
{
    gh_data_t *data = (gh_data_t *) plugin_data;
    const gh_item_t *item;

    if (fname == NULL || column_id == NULL)
        return NULL;

    item = gh_find_item (data, fname);
    if (item == NULL)
        return NULL;

    switch (data->view)
    {
    case GH_VIEW_ROOT:
    case GH_VIEW_OWNER:
        return gh_repos_get_column_value (item, column_id);
    case GH_VIEW_PULLS:
    case GH_VIEW_COMMITS:
    case GH_VIEW_PULL_CONV:
    case GH_VIEW_ISSUE_CONV:
        return gh_pulls_get_column_value (item, column_id);
    case GH_VIEW_ISSUES:
        return gh_issues_get_column_value (item, column_id);
    case GH_VIEW_FILES:
        return gh_files_get_column_value (item, column_id);
    default:
        return NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

static const char *
gh_get_footer (void *plugin_data)
{
    gh_data_t *data = (gh_data_t *) plugin_data;

    g_clear_pointer (&data->footer_buf, g_free);

    switch (data->view)
    {
    case GH_VIEW_PULLS:
        data->footer_buf = g_strdup_printf ("%s, %u", gh_state_filter_name (data->pulls_state),
                                            data->items != NULL ? data->items->len : 0);
        break;
    case GH_VIEW_ISSUES:
        data->footer_buf = g_strdup_printf ("%s, %u", gh_state_filter_name (data->issues_state),
                                            data->items != NULL ? data->items->len : 0);
        break;
    case GH_VIEW_FILES:
        if (data->items != NULL && data->base_sha != NULL)
            data->footer_buf =
                g_strdup_printf (data->files_truncated ? _ ("the first %u files, base %.7s")
                                                       : _ ("%u files, base %.7s"),
                                 data->items->len, data->base_sha);
        break;
    case GH_VIEW_PULL:
    case GH_VIEW_ISSUE:
    case GH_VIEW_COMMITS:
    case GH_VIEW_PULL_CONV:
    case GH_VIEW_ISSUE_CONV:
        if (data->topic != NULL && data->topic->title != NULL)
            data->footer_buf = g_strdup (data->topic->title);
        break;
    default:
        break;
    }

    return data->footer_buf;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
gh_get_focus_name (void *plugin_data)
{
    return ((gh_data_t *) plugin_data)->pending_focus;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
gh_get_default_format (void *plugin_data)
{
    gh_data_t *data = (gh_data_t *) plugin_data;

    switch (data->view)
    {
    case GH_VIEW_ROOT:
        return "type name | origin:8";
    case GH_VIEW_OWNER:
        return "type name | vis:7 | mtime";
    case GH_VIEW_PULLS:
    case GH_VIEW_ISSUES:
        return "type name | state:6 | author:10 | mtime";
    case GH_VIEW_COMMITS:
    case GH_VIEW_PULL_CONV:
    case GH_VIEW_ISSUE_CONV:
        return "type name | author:10 | mtime";
    case GH_VIEW_FILES:
        return "type name | status:8 | changes:9";
    default:
        return NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Ctrl-R asks GitHub again, for the open pull request or issue too. */
static mc_pp_result_t
gh_reload (void *plugin_data)
{
    gh_data_t *data = (gh_data_t *) plugin_data;

    gh_clear_items (data);
    g_hash_table_remove_all (data->texts);
    if (gh_view_in_topic (data->view))
        gh_set_topic (data, NULL);
    return MC_PPR_OK;
}

/*** public functions ****************************************************************************/

gh_item_t *
gh_item_new (const char *name, gh_item_kind_t kind, gboolean is_dir)
{
    gh_item_t *item;

    item = g_new0 (gh_item_t, 1);
    item->name = g_strdup (name);
    item->kind = kind;
    item->is_dir = is_dir;
    return item;
}

/* --------------------------------------------------------------------------------------------- */

void
gh_item_free (gpointer p)
{
    gh_item_t *item = (gh_item_t *) p;

    if (item == NULL)
        return;
    g_free (item->name);
    g_free (item->title);
    g_free (item->author);
    g_free (item->state);
    g_free (item->labels);
    g_free (item->branch);
    g_free (item->review);
    g_free (item->url);
    g_free (item->body);
    g_free (item);
}

/* --------------------------------------------------------------------------------------------- */

void
gh_clear_items (gh_data_t *data)
{
    if (data->items != NULL)
    {
        g_ptr_array_free (data->items, TRUE);
        data->items = NULL;
    }
    /* item_index keys borrow item->name */
    data->index_for = NULL;
    if (data->item_index != NULL)
        g_hash_table_remove_all (data->item_index);
}

/* --------------------------------------------------------------------------------------------- */

const gh_item_t *
gh_find_item (gh_data_t *data, const char *name)
{
    if (data->items == NULL || name == NULL)
        return NULL;

    /* built again when the items array is a new one */
    if (data->item_index == NULL || data->index_for != data->items)
    {
        guint i;

        if (data->item_index == NULL)
            data->item_index = g_hash_table_new (g_str_hash, g_str_equal);
        else
            g_hash_table_remove_all (data->item_index);

        for (i = 0; i < data->items->len; i++)
        {
            gh_item_t *item = (gh_item_t *) g_ptr_array_index (data->items, i);

            if (!g_hash_table_contains (data->item_index, item->name))
                g_hash_table_insert (data->item_index, item->name, item);
        }
        data->index_for = data->items;
    }

    return (const gh_item_t *) g_hash_table_lookup (data->item_index, name);
}

/* --------------------------------------------------------------------------------------------- */

/* Quick View asks quietly: what it cannot show stays blank. */
void
gh_error (const gh_data_t *data, const char *text)
{
    if (mc_pp_quiet_messages ())
        return;

    if (data != NULL && data->host != NULL && data->host->message != NULL)
        data->host->message (data->host, D_ERROR, _ ("GitHub"), text);
    else
        message (D_ERROR, MSG_ERROR, "%s", text);
}

/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &gh_plugin;
}
