/*
   Git status panel plugin.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

   This file is part of the M-Commander
   a fork of GNU Midnight Commander.

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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/vfs/utilvfs.h"
#include "lib/glibcompat.h"
#include "lib/logging.h"
#include "lib/mcconfig.h"
#include "lib/panel-plugin.h"
#include "lib/keybind.h"
#include "lib/tty/key.h"
#include "lib/plugin-prefs.h"
#include "lib/util.h"
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "src/filemanager/dir.h"
#include "src/viewer/mcviewer.h"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    int kind;
    char *repo_path;
    char *old_repo_path;
    char *full_path;
    char *state_text;
    char *state_mark_text;
    char *commit_sha;
    char *base_sha;  // GIT_ITEM_DIFF_DIR: where the changes of the listed commits start
    gboolean is_deleted;
    gboolean is_staged;
    gboolean is_conflict;  // unmerged: Show diff opens the merge
} git_entry_info_t;

typedef struct
{
    mc_panel_host_t *host;
    char *repo_root;
    char *title;
    char *help_filename;
    char *default_format;
    int key_stage;
    int key_unstage;
    int key_toggle;
    int key_diff;
    int key_diff_alt;
    int key_refresh;
    int key_reset;
    int key_fav_add;
    int key_coole;
    int key_commit;
    int key_amend;
    int key_show;
    int key_reword;
    char *coole_executable;
    int view;
    char *selected_commit;
    char *selected_commit_base;  // diff: the files from here to selected_commit
    char *current_branch;
    char *selected_branch;
    char *selected_remote;
    char *selected_commit_name;
    char *selected_commit_list_name;
    int selected_branch_scope;
    char *pending_focus;
    GPtrArray *commit_stack;     /* stack of git_commit_nav_t* */
    GHashTable *display_to_info; /* key: panel display name, value: git_entry_info_t* */
    GPtrArray *fav_repos;        /* list of char* repo paths; non-NULL in favorites mode */
} git_data_t;

typedef struct
{
    char *commit_sha;
    char *commit_name;
    char *commit_list_name;
} git_commit_nav_t;

/*** forward declarations (file scope functions) *************************************************/

static void *git_open (mc_panel_host_t *host, const char *open_path);
static void git_close (void *plugin_data);
static mc_pp_result_t git_get_items (void *plugin_data, void *list_ptr);
static mc_pp_result_t git_chdir (void *plugin_data, const char *path);
static mc_pp_result_t git_enter (void *plugin_data, const char *fname, const struct stat *st);
static mc_pp_result_t git_view_item (void *plugin_data, const char *fname, const struct stat *st,
                                     gboolean plain_view);
static mc_pp_result_t git_get_help_info (void *plugin_data, const char **filename,
                                         const char **node);
static mc_pp_result_t git_get_local_copy (void *plugin_data, const char *fname, char **local_path);
static mc_pp_result_t git_get_quick_view (void *plugin_data, const char *fname,
                                          const struct stat *st, char **local_path);
static const char *git_get_title (void *plugin_data);
static char *git_get_location (void *plugin_data);
static mc_pp_result_t git_handle_key (void *plugin_data, int key);
static mc_pp_result_t git_create_item (void *plugin_data);
static mc_pp_result_t git_delete_items (void *plugin_data, const char **names, int count);
static const mc_panel_column_t *git_get_columns (void *plugin_data, size_t *count);
static const char *git_get_column_value (void *plugin_data, const char *fname,
                                         const char *column_id);
static const char *git_get_footer (void *plugin_data);
static const char *git_get_focus_name (void *plugin_data);
static const char *git_get_default_format (void *plugin_data);
static void git_debug_log (const char *fmt, ...) G_GNUC_PRINTF (1, 2);
static const char *git_view_name (int view);

/*** file scope variables ************************************************************************/

static const mc_panel_plugin_t git_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "git",
    .display_name = "Git status",
    .proto = "git",
    .prefix = "git:",
    .flags = MC_PPF_NAVIGATE | MC_PPF_GET_FILES | MC_PPF_CUSTOM_TITLE | MC_PPF_SHOW_IN_DRIVE_MENU
        | MC_PPF_CREATE | MC_PPF_DELETE,

    .open = git_open,
    .close = git_close,
    .get_items = git_get_items,

    .chdir = git_chdir,
    .enter = git_enter,
    .view = git_view_item,
    .get_help_info = git_get_help_info,
    .get_local_copy = git_get_local_copy,
    .get_quick_view = git_get_quick_view,
    .put_file = NULL,
    .save_file = NULL,
    .delete_items = git_delete_items,
    .get_title = git_get_title,
    .get_location = git_get_location,
    .handle_key = git_handle_key,
    .create_item = git_create_item,
    .get_columns = git_get_columns,
    .get_column_value = git_get_column_value,
    .get_footer = git_get_footer,
    .get_focus_name = git_get_focus_name,
    .get_default_format = git_get_default_format,
    .default_sort_id = "mtime",
    .default_sort_reverse = TRUE,
};

static const mc_panel_column_t git_columns[] = {
    { "type", "", 1, FALSE, J_LEFT_FIT, TRUE },
    { "status", "Status", 11, FALSE, J_LEFT_FIT, TRUE },
};

/*** file scope functions ************************************************************************/

#define GIT_PANEL_CONFIG_FILE          "panels.git.ini"
#define GIT_PANEL_CONFIG_FILE_LEGACY   "git-panel.ini"
#define GIT_PANEL_CONFIG_GROUP         "git-panel"
#define GIT_PANEL_FORMAT_KEY           "default_format"
#define GIT_PANEL_FORMAT_DEFAULT       "type name | status | mtime"
#define GIT_PANEL_KEY_FAV_ADD          "hotkey_fav_add"
#define GIT_PANEL_KEY_FAV_ADD_DEFAULT  "ctrl-b"
#define GIT_PANEL_COOLE_EXECUTABLE     "coole_executable"
#define GIT_PANEL_KEY_COOLE            "hotkey_coole"
#define GIT_PANEL_KEY_COMMIT           "hotkey_commit"
#define GIT_PANEL_KEY_AMEND            "hotkey_amend"
#define GIT_PANEL_KEY_SHOW             "hotkey_show"
#define GIT_PANEL_KEY_REWORD           "hotkey_reword"
#define GIT_PANEL_KEY_COOLE_DEFAULT    "alt-g"
#define GIT_PANEL_KEY_COMMIT_DEFAULT   "alt-c"
#define GIT_PANEL_KEY_AMEND_DEFAULT    "alt-a"
#define GIT_PANEL_KEY_SHOW_DEFAULT     "alt-v"
#define GIT_PANEL_KEY_REWORD_DEFAULT   "alt-r"
#define GIT_FAVORITES_CONFIG_GROUP     "git-favorites"
#define GIT_FAVORITES_CONFIG_KEY       "repos"

#define GIT_PANEL_KEY_STAGE            "hotkey_stage"
#define GIT_PANEL_KEY_UNSTAGE          "hotkey_unstage"
#define GIT_PANEL_KEY_TOGGLE           "hotkey_toggle"
#define GIT_PANEL_KEY_DIFF             "hotkey_diff"
#define GIT_PANEL_KEY_DIFF_ALT         "hotkey_diff_alt"
#define GIT_PANEL_KEY_REFRESH          "hotkey_refresh"
#define GIT_PANEL_KEY_RESET            "hotkey_reset"
#define GIT_PANEL_KEY_STAGE_DEFAULT    "f15" /* Shift-F5 */
#define GIT_PANEL_KEY_UNSTAGE_DEFAULT  "f16" /* Shift-F6 */
#define GIT_PANEL_KEY_TOGGLE_DEFAULT   "none"
#define GIT_PANEL_KEY_DIFF_DEFAULT     "ctrl-d"
#define GIT_PANEL_KEY_DIFF_ALT_DEFAULT "f13"
#define GIT_PANEL_KEY_REFRESH_DEFAULT  "ctrl-r"
#define GIT_PANEL_KEY_RESET_DEFAULT    "f18" /* Shift-F8 */

/* --------------------------------------------------------------------------------------------- */

typedef enum
{
    GIT_ACTION_TOGGLE = 0,
    GIT_ACTION_STAGE,
    GIT_ACTION_UNSTAGE,
    GIT_ACTION_RESET
} git_action_t;

typedef enum
{
    GIT_VIEW_STATUS = 0,
    GIT_VIEW_BRANCHES,
    GIT_VIEW_BRANCHES_LOCAL,
    GIT_VIEW_BRANCHES_REMOTE_ITEMS,
    GIT_VIEW_COMMITS,
    GIT_VIEW_COMMIT_FILES,
    GIT_VIEW_FAVORITES
} git_view_t;

typedef enum
{
    GIT_ITEM_STATUS_FILE = 0,
    GIT_ITEM_BRANCHES_DIR,
    GIT_ITEM_BRANCHES_LOCAL_DIR,
    GIT_ITEM_REMOTE_DIR_ENTRY,
    GIT_ITEM_BRANCH_ENTRY,
    GIT_ITEM_COMMITS_DIR,
    GIT_ITEM_COMMIT_DIR,
    GIT_ITEM_COMMIT_PARENT_ENTRY,
    GIT_ITEM_COMMIT_FILE,
    GIT_ITEM_FAVORITE_REPO,
    GIT_ITEM_DIFF_DIR
} git_item_kind_t;

#define GIT_COMMITS_DIR_NAME        "commits"
#define GIT_DIFF_DIR_NAME           "diff"
#define GIT_BRANCHES_DIR_NAME       "branches"
#define GIT_BRANCHES_LOCAL_DIR_NAME "local"
#define GIT_BRANCHES_REMOTE_PREFIX  "remote/"

#define GIT_BRANCH_SCOPE_NONE       0
#define GIT_BRANCH_SCOPE_LOCAL      1
#define GIT_BRANCH_SCOPE_REMOTE     2

/* --------------------------------------------------------------------------------------------- */

static void
git_debug_log (const char *fmt, ...)
{
    char *msg;
    va_list ap;

    va_start (ap, fmt);
    msg = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    if (msg == NULL)
        return;

    g_debug ("%s", msg);

    {
        FILE *fp = fopen ("/tmp/mc-git.log", "a");

        if (fp != NULL)
        {
            time_t now = time (NULL);
            struct tm tm_buf;
            struct tm *tm_now = localtime_r (&now, &tm_buf);

            if (tm_now != NULL)
            {
                char ts[32];

                if (strftime (ts, sizeof (ts), "%Y-%m-%d %H:%M:%S", tm_now) > 0)
                    fprintf (fp, "%s %s\n", ts, msg);
                else
                    fprintf (fp, "%s\n", msg);
            }
            else
                fprintf (fp, "%s\n", msg);
            fclose (fp);
        }
    }
    g_free (msg);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_view_name (int view)
{
    switch ((git_view_t) view)
    {
    case GIT_VIEW_STATUS:
        return "status";
    case GIT_VIEW_BRANCHES:
        return "branches";
    case GIT_VIEW_BRANCHES_LOCAL:
        return "branches_local";
    case GIT_VIEW_BRANCHES_REMOTE_ITEMS:
        return "branches_remote_items";
    case GIT_VIEW_COMMITS:
        return "commits";
    case GIT_VIEW_COMMIT_FILES:
        return "commit_files";
    case GIT_VIEW_FAVORITES:
        return "favorites";
    default:
        return "unknown";
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
git_set_pending_focus (git_data_t *data, const char *name)
{
    g_free (data->pending_focus);
    data->pending_focus = g_strdup (name);
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_read_config_string_from_file (const char *path, const char *key)
{
    char *value;
    mc_config_t *cfg;

    if (path == NULL || !exist_file (path))
        return NULL;

    cfg = mc_config_init (path, TRUE);
    value = mc_config_get_string (cfg, GIT_PANEL_CONFIG_GROUP, key, NULL);
    mc_config_deinit (cfg);

    if (value == NULL)
        return NULL;

    g_strstrip (value);
    if (*value == '\0')
    {
        g_free (value);
        return NULL;
    }

    return value;
}

/* --------------------------------------------------------------------------------------------- */

static void
git_save_defaults_to_user_file (const char *path)
{
    mc_config_t *cfg;

    if (path == NULL || exist_file (path))
        return;

    cfg = mc_config_init (path, FALSE);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_FORMAT_KEY,
                          GIT_PANEL_FORMAT_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_STAGE,
                          GIT_PANEL_KEY_STAGE_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_UNSTAGE,
                          GIT_PANEL_KEY_UNSTAGE_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_TOGGLE,
                          GIT_PANEL_KEY_TOGGLE_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_DIFF,
                          GIT_PANEL_KEY_DIFF_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_DIFF_ALT,
                          GIT_PANEL_KEY_DIFF_ALT_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_REFRESH,
                          GIT_PANEL_KEY_REFRESH_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_RESET,
                          GIT_PANEL_KEY_RESET_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_COOLE_EXECUTABLE, "coole");
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_COOLE,
                          GIT_PANEL_KEY_COOLE_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_COMMIT,
                          GIT_PANEL_KEY_COMMIT_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_AMEND,
                          GIT_PANEL_KEY_AMEND_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_SHOW,
                          GIT_PANEL_KEY_SHOW_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_REWORD,
                          GIT_PANEL_KEY_REWORD_DEFAULT);
    mc_config_save_file (cfg, NULL);
    mc_config_deinit (cfg);
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_load_config_value (const char *key, const char *fallback)
{
    char *value = NULL;
    char *user_path = NULL;
    char *user_path_legacy = NULL;
    char *sys_path = NULL;
    char *sys_path_legacy = NULL;

    user_path = g_build_filename (mc_config_get_path (), GIT_PANEL_CONFIG_FILE, (char *) NULL);
    user_path_legacy =
        g_build_filename (mc_config_get_path (), GIT_PANEL_CONFIG_FILE_LEGACY, (char *) NULL);

    value = git_read_config_string_from_file (user_path, key);
    if (value == NULL)
        value = git_read_config_string_from_file (user_path_legacy, key);

    if (value == NULL && mc_global.sysconfig_dir != NULL)
    {
        sys_path = g_build_filename (mc_global.sysconfig_dir, GIT_PANEL_CONFIG_FILE, (char *) NULL);
        sys_path_legacy =
            g_build_filename (mc_global.sysconfig_dir, GIT_PANEL_CONFIG_FILE_LEGACY, (char *) NULL);

        value = git_read_config_string_from_file (sys_path, key);
        if (value == NULL)
            value = git_read_config_string_from_file (sys_path_legacy, key);
    }

    git_save_defaults_to_user_file (user_path);

    g_free (sys_path_legacy);
    g_free (sys_path);
    g_free (user_path_legacy);
    g_free (user_path);
    return (value != NULL) ? value : g_strdup (fallback);
}

/* --------------------------------------------------------------------------------------------- */

static int
git_load_hotkey (const char *key, const char *fallback_text, int fallback_key)
{
    char *value;
    int hotkey;

    value = git_load_config_value (key, fallback_text);
    hotkey = mc_plugin_prefs_parse_hotkey (value, fallback_text, fallback_key, NULL);
    g_free (value);
    return hotkey;
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_load_default_format (void)
{
    return git_load_config_value (GIT_PANEL_FORMAT_KEY, GIT_PANEL_FORMAT_DEFAULT);
}

static void
git_entry_info_free (gpointer ptr)
{
    git_entry_info_t *info = (git_entry_info_t *) ptr;

    if (info == NULL)
        return;

    g_free (info->repo_path);
    g_free (info->old_repo_path);
    g_free (info->full_path);
    g_free (info->state_text);
    g_free (info->state_mark_text);
    g_free (info->commit_sha);
    g_free (info->base_sha);
    g_free (info);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_commit_nav_free (gpointer ptr)
{
    git_commit_nav_t *it = (git_commit_nav_t *) ptr;

    if (it == NULL)
        return;

    g_free (it->commit_sha);
    g_free (it->commit_name);
    g_free (it->commit_list_name);
    g_free (it);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_commit_stack_clear (git_data_t *data)
{
    if (data->commit_stack != NULL)
        g_ptr_array_set_size (data->commit_stack, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_commit_stack_push_current (git_data_t *data)
{
    git_commit_nav_t *it;

    if (data->commit_stack == NULL || data->selected_commit == NULL)
        return;

    it = g_new0 (git_commit_nav_t, 1);
    it->commit_sha = g_strdup (data->selected_commit);
    it->commit_name = g_strdup (data->selected_commit_name);
    it->commit_list_name = g_strdup (data->selected_commit_list_name);
    g_ptr_array_add (data->commit_stack, it);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_commit_stack_pop_to_current (git_data_t *data)
{
    git_commit_nav_t *it;

    if (data->commit_stack == NULL || data->commit_stack->len == 0)
        return FALSE;

    it = (git_commit_nav_t *) g_ptr_array_steal_index (data->commit_stack,
                                                       data->commit_stack->len - 1);
    if (it == NULL)
        return FALSE;

    g_free (data->selected_commit);
    data->selected_commit = g_strdup (it->commit_sha);
    g_free (data->selected_commit_name);
    data->selected_commit_name = g_strdup (it->commit_name);
    g_free (data->selected_commit_list_name);
    data->selected_commit_list_name = g_strdup (it->commit_list_name);

    git_commit_nav_free (it);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_run_capture (char **argv, char **out, char **err)
{
    GError *error = NULL;
    int status = 0;
    char *std_out = NULL;
    char *std_err = NULL;
    gboolean ok;

    ok = g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &std_out, &std_err,
                       &status, &error);
    if (!ok)
    {
        if (err != NULL)
        {
            if (error != NULL && error->message != NULL)
                *err = g_strdup (error->message);
            else
                *err = g_strdup ("Failed to start git command");
        }

        if (out != NULL)
            *out = NULL;

        if (error != NULL)
            g_error_free (error);

        g_free (std_out);
        g_free (std_err);
        return FALSE;
    }

#if GLIB_CHECK_VERSION(2, 70, 0)
    if (!g_spawn_check_wait_status (status, NULL))
#else
    if (!g_spawn_check_exit_status (status, NULL))
#endif
    {
        if (err != NULL)
            *err = g_strdup (std_err != NULL ? std_err : "git command failed");

        if (out != NULL)
            *out = std_out;
        else
            g_free (std_out);

        g_free (std_err);
        return FALSE;
    }

    if (out != NULL)
        *out = std_out;
    else
        g_free (std_out);

    if (err != NULL)
        *err = std_err;
    else
        g_free (std_err);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_run_stdout (char **argv, char **out)
{
    return git_run_capture (argv, out, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_run_simple (char **argv, char **err)
{
    return git_run_capture (argv, NULL, err);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_make_temp_copy (const char *src_path, char **tmp_path)
{
    GError *error = NULL;
    gchar *content = NULL;
    gsize content_len = 0;
    int fd;
    gboolean ok;

    if (!g_file_get_contents (src_path, &content, &content_len, &error))
    {
        if (error != NULL)
            g_error_free (error);
        return FALSE;
    }

    fd = g_file_open_tmp ("mc-git-edit-XXXXXX", tmp_path, &error);
    if (fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        g_free (content);
        return FALSE;
    }
    close (fd);

    ok = g_file_set_contents (*tmp_path, content, (gssize) content_len, &error);
    g_free (content);

    if (!ok)
    {
        if (error != NULL)
            g_error_free (error);
        unlink (*tmp_path);
        g_free (*tmp_path);
        *tmp_path = NULL;
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_detect_repo_root (const char *start_path)
{
    char *argv[] = { (char *) "git",
                     (char *) "-C",
                     (char *) start_path,
                     (char *) "rev-parse",
                     (char *) "--show-toplevel",
                     NULL };
    char *out = NULL;
    char *trimmed;

    if (!git_run_stdout (argv, &out))
        return NULL;

    g_strchomp (out);
    if (out[0] == '\0')
    {
        g_free (out);
        return NULL;
    }

    trimmed = g_strdup (out);
    g_free (out);
    return trimmed;
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_detect_upstream_ref (git_data_t *data)
{
    char *argv[] = { (char *) "git",          (char *) "-C",
                     data->repo_root,         (char *) "rev-parse",
                     (char *) "--abbrev-ref", (char *) "--symbolic-full-name",
                     (char *) "@{upstream}",  NULL };
    char *out = NULL;
    char *trimmed;

    if (!git_run_stdout (argv, &out))
        return NULL;

    g_strchomp (out);
    if (out[0] == '\0')
    {
        g_free (out);
        return NULL;
    }

    trimmed = g_strdup (out);
    g_free (out);
    return trimmed;
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_detect_current_branch (git_data_t *data)
{
    char *argv[] = { (char *) "git",     (char *) "-C",
                     data->repo_root,    (char *) "symbolic-ref",
                     (char *) "--quiet", (char *) "--short",
                     (char *) "HEAD",    NULL };
    char *out = NULL;

    if (git_run_stdout (argv, &out))
    {
        g_strchomp (out);
        if (out[0] != '\0')
            return out;
        g_free (out);
    }

    {
        char *argv2[] = { (char *) "git",
                          (char *) "-C",
                          data->repo_root,
                          (char *) "rev-parse",
                          (char *) "--short",
                          (char *) "HEAD",
                          NULL };

        if (git_run_stdout (argv2, &out))
        {
            char *detached;

            g_strchomp (out);
            detached = g_strdup_printf ("detached@%s", out[0] != '\0' ? out : "?");
            g_free (out);
            return detached;
        }
    }

    return g_strdup ("?");
}

/* --------------------------------------------------------------------------------------------- */

/* Try to find a base branch (master or main) to use as fallback when no upstream is configured. */
static char *
git_detect_base_ref (git_data_t *data)
{
    static const char *candidates[] = { "master", "main" };
    char *current_branch;
    size_t i;

    current_branch = git_detect_current_branch (data);

    for (i = 0; i < G_N_ELEMENTS (candidates); i++)
    {
        char *argv[] = { (char *) "git",
                         (char *) "-C",
                         data->repo_root,
                         (char *) "rev-parse",
                         (char *) "--verify",
                         (char *) "--quiet",
                         NULL,
                         NULL };
        char *ref;
        char *out = NULL;

        ref = g_strdup_printf ("refs/heads/%s", candidates[i]);
        argv[6] = ref;

        if (git_run_stdout (argv, &out))
        {
            g_free (out);

            /* Don't use base ref if we ARE on that branch */
            if (current_branch != NULL && strcmp (current_branch, candidates[i]) == 0)
            {
                g_free (ref);
                continue;
            }

            g_free (ref);
            g_free (current_branch);
            return g_strdup (candidates[i]);
        }

        g_free (ref);
        g_free (out);
    }

    g_free (current_branch);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
git_update_title (git_data_t *data)
{
    g_free (data->title);

    switch ((git_view_t) data->view)
    {
    case GIT_VIEW_STATUS:
        data->title = g_strdup (data->repo_root);
        break;
    case GIT_VIEW_BRANCHES:
        data->title = g_strdup ("/branches");
        break;
    case GIT_VIEW_BRANCHES_LOCAL:
        data->title = g_strdup ("/branches/local");
        break;
    case GIT_VIEW_BRANCHES_REMOTE_ITEMS:
        data->title = g_strdup_printf ("/branches/remote/%s",
                                       data->selected_remote != NULL ? data->selected_remote : "");
        break;
    case GIT_VIEW_COMMITS:
        if (data->selected_branch != NULL)
            data->title = g_strdup_printf ("/commits/%s", data->selected_branch);
        else
            data->title = g_strdup ("/commits");
        break;
    case GIT_VIEW_COMMIT_FILES:
        if (data->selected_branch != NULL)
            data->title = g_strdup_printf (
                "/commits/%s/%s", data->selected_branch,
                data->selected_commit_name != NULL
                    ? data->selected_commit_name
                    : (data->selected_commit != NULL ? data->selected_commit : ""));
        else
            data->title = g_strdup_printf (
                "/commits/%s",
                data->selected_commit_name != NULL
                    ? data->selected_commit_name
                    : (data->selected_commit != NULL ? data->selected_commit : ""));
        break;
    case GIT_VIEW_FAVORITES:
        data->title = g_strdup ("Git repositories");
        break;
    default:
        data->title = g_strdup (data->repo_root);
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
git_add_virtual_dir (git_data_t *data, dir_list *list, const char *name, git_item_kind_t kind,
                     const char *commit_sha, const char *status_text, time_t mtime)
{
    struct stat st;
    git_entry_info_t *info;

    memset (&st, 0, sizeof (st));
    st.st_mode = S_IFDIR | 0755;
    st.st_size = 0;
    st.st_mtime = mtime;
    st.st_uid = getuid ();
    st.st_gid = getgid ();
    st.st_nlink = 1;

    (void) dir_list_append (list, name, &st, TRUE, FALSE);

    info = g_new0 (git_entry_info_t, 1);
    info->kind = kind;
    info->state_mark_text = g_strdup ("/");
    info->state_text = g_strdup (status_text);
    info->commit_sha = g_strdup (commit_sha);
    g_hash_table_insert (data->display_to_info, g_strdup (name), info);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_add_branch_entry (git_data_t *data, dir_list *list, const char *display_name,
                      const char *branch_ref, const char *status_text, time_t mtime)
{
    struct stat st;
    git_entry_info_t *info;

    memset (&st, 0, sizeof (st));
    st.st_mode = S_IFDIR | 0755;
    st.st_size = 0;
    st.st_mtime = mtime;
    st.st_uid = getuid ();
    st.st_gid = getgid ();
    st.st_nlink = 1;

    (void) dir_list_append (list, display_name, &st, TRUE, FALSE);

    info = g_new0 (git_entry_info_t, 1);
    info->kind = GIT_ITEM_BRANCH_ENTRY;
    info->repo_path = g_strdup (branch_ref != NULL ? branch_ref : display_name);
    info->state_mark_text = g_strdup ("/");
    info->state_text = g_strdup (status_text);
    g_hash_table_insert (data->display_to_info, g_strdup (display_name), info);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_add_entry (git_data_t *data, dir_list *list, char state_mark, const char *state_prefix,
               const char *repo_path, gboolean is_deleted, gboolean is_staged, git_item_kind_t kind,
               const char *commit_sha, const char *old_repo_path)
{
    struct stat st;
    git_entry_info_t *info;
    char *name;

    info = g_new0 (git_entry_info_t, 1);
    info->kind = kind;
    info->repo_path = g_strdup (repo_path);
    info->old_repo_path = g_strdup (old_repo_path);
    info->full_path = g_build_filename (data->repo_root, repo_path, NULL);
    info->state_text = g_strdup (state_prefix);
    info->state_mark_text = g_strdup_printf ("%c", state_mark);
    info->commit_sha = g_strdup (commit_sha);
    info->is_deleted = is_deleted;
    info->is_staged = is_staged;

    name = g_strdup (repo_path);

    if (stat (info->full_path, &st) == -1)
    {
        memset (&st, 0, sizeof (st));
        st.st_mode = S_IFREG | 0644;
        st.st_size = 0;
        st.st_mtime = time (NULL);
        st.st_uid = getuid ();
        st.st_gid = getgid ();
        st.st_nlink = 1;
    }

    (void) dir_list_append (list, name, &st, S_ISDIR (st.st_mode), FALSE);

    g_hash_table_insert (data->display_to_info, g_strdup (name), info);

    g_free (name);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_parse_branches_and_fill (git_data_t *data, dir_list *list, const char *out,
                             const char *remote_prefix)
{
    char **lines;
    int i;
    gboolean is_remote = (remote_prefix != NULL);

    lines = g_strsplit (out, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        char **parts;
        const char *branch_name;
        const char *display_name;
        const char *short_hash;
        const char *head_mark;
        char *status;
        time_t commit_time;

        if (lines[i][0] == '\0')
            continue;

        parts = g_strsplit (lines[i], "\t", 4);
        if (parts[0] == NULL || parts[1] == NULL || parts[2] == NULL)
        {
            g_strfreev (parts);
            continue;
        }

        branch_name = parts[0];
        display_name = branch_name;
        if (is_remote)
        {
            size_t prefix_len;

            prefix_len = strlen (remote_prefix);
            if (!g_str_has_prefix (branch_name, remote_prefix) || branch_name[prefix_len] != '/')
            {
                g_strfreev (parts);
                continue;
            }

            display_name = branch_name + prefix_len + 1;
            if (strcmp (display_name, "HEAD") == 0)
            {
                g_strfreev (parts);
                continue;
            }
        }

        short_hash = parts[1];
        commit_time = (time_t) g_ascii_strtoll (parts[2], NULL, 10);
        head_mark = parts[3] != NULL ? g_strstrip (parts[3]) : "";

        if (!is_remote && strcmp (head_mark, "*") == 0)
            status = g_strdup_printf ("* %s", short_hash);
        else
            status = g_strdup (short_hash);

        git_add_branch_entry (data, list, display_name, branch_name, status, commit_time);
        g_free (status);
        g_strfreev (parts);
    }

    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_parse_remotes_and_fill (git_data_t *data, dir_list *list, const char *out)
{
    char **lines;
    int i;

    lines = g_strsplit (out, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        const char *remote_name = g_strstrip (lines[i]);
        char *name;

        if (remote_name == NULL || *remote_name == '\0')
            continue;

        name = g_strdup_printf ("remote/%s", remote_name);
        git_add_virtual_dir (data, list, name, GIT_ITEM_REMOTE_DIR_ENTRY, NULL, "REMOTE",
                             time (NULL));
        g_free (name);
    }

    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */

/* An unmerged path of git status: DD, AU, UD, UA, DU, AA, UU */
static gboolean
git_status_is_conflict (const char *status)
{
    const char x = status[0];
    const char y = status[1];

    return x == 'U' || y == 'U' || (x == 'A' && y == 'A') || (x == 'D' && y == 'D');
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_stage_status_from_status (const char *status)
{
    const char x = status[0];

    if (x == '?' && status[1] == '?')
        return "UNTRACKED";

    if (x != ' ')
        return "STAGED";

    return "UNSTAGED";
}

/* --------------------------------------------------------------------------------------------- */

static char
git_type_mark_from_status (const char *status)
{
    const char x = status[0];
    const char y = status[1];
    char t;

    if (x == '?' && y == '?')
        return '+';

    /* Type reflects what changed with file, independent from staged/unstaged bucket. */
    t = (y != ' ') ? y : x;

    if (t == 'R')
        return '=';

    if (t == 'D')
        return '-';

    if (t == 'A')
        return '+';

    return '*';
}

/* --------------------------------------------------------------------------------------------- */

static void
git_parse_status_and_fill (git_data_t *data, dir_list *list, const char *out)
{
    char **lines;
    int i;

    lines = g_strsplit (out, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        const char *line = lines[i];
        const char *status;
        const char *stage_status;
        char state_mark;
        const char *path;
        const char *arrow;
        gboolean is_deleted;
        gboolean is_staged;

        if (line[0] == '\0')
            continue;

        if (line[0] == '?' && line[1] == '?')
        {
            status = "??";
            path = line + 3;
        }
        else
        {
            static char st_code[3];

            if (strlen (line) < 4)
                continue;

            st_code[0] = line[0];
            st_code[1] = line[1];
            st_code[2] = '\0';
            status = st_code;
            path = line + 3;
        }

        if (path[0] == '\0')
            continue;

        arrow = strstr (path, " -> ");
        if (arrow != NULL)
            path = arrow + 4;

        if (git_status_is_conflict (status))
        {
            git_entry_info_t *info;

            git_add_entry (data, list, '!', "CONFLICT", path, FALSE, FALSE, GIT_ITEM_STATUS_FILE,
                           NULL, NULL);
            info = g_hash_table_lookup (data->display_to_info, path);
            if (info != NULL)
                info->is_conflict = TRUE;
            continue;
        }

        stage_status = git_stage_status_from_status (status);
        state_mark = git_type_mark_from_status (status);
        is_deleted = (status[0] == 'D' || status[1] == 'D');
        is_staged = (status[0] != ' ' && !(status[0] == '?' && status[1] == '?'));
        git_add_entry (data, list, state_mark, stage_status, path, is_deleted, is_staged,
                       GIT_ITEM_STATUS_FILE, NULL, NULL);
    }

    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */

/* First commit on the first-parent line of BASE that has REF merged in, or NULL. */
static char *
git_find_merge_commit (git_data_t *data, const char *base, const char *ref)
{
    char *range;
    char *argv[] = { (char *) "git",
                     (char *) "-C",
                     data->repo_root,
                     (char *) "rev-list",
                     (char *) "--first-parent",
                     (char *) "--ancestry-path",
                     NULL,
                     NULL };
    char *out = NULL;
    char *merge = NULL;

    range = g_strdup_printf ("%s..%s", ref, base);
    argv[6] = range;

    if (git_run_stdout (argv, &out))
    {
        char *last;

        g_strchomp (out);
        last = strrchr (out, '\n');
        last = (last != NULL) ? last + 1 : out;
        if (*last != '\0')
            merge = g_strdup (last);
    }

    g_free (out);
    g_free (range);
    return merge;
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_merge_base (git_data_t *data, const char *a, const char *b)
{
    char *argv[] = {
        (char *) "git", (char *) "-C", data->repo_root, (char *) "merge-base", (char *) a,
        (char *) b,     NULL
    };
    char *out = NULL;

    if (!git_run_stdout (argv, &out))
    {
        g_free (out);
        return NULL;
    }
    g_strchomp (out);
    if (*out == '\0')
    {
        g_free (out);
        return NULL;
    }

    return out;
}

/* --------------------------------------------------------------------------------------------- */

/* Log of BASE..REF. When the range is empty, REF is already merged into BASE:
 * show the commits that the merge brought in, else the whole log of REF.
 * FROM, when not NULL, gets the commit the listed changes start from: NULL for the whole log. */
static gboolean
git_run_branch_log (git_data_t *data, const char *base, const char *ref, char **out, char **from)
{
    char *argv[] = { (char *) "git",
                     (char *) "-C",
                     data->repo_root,
                     (char *) "log",
                     (char *) "--no-color",
                     (char *) "--decorate=no",
                     (char *) "--first-parent",
                     (char *) "--pretty=format:%H%x09%h%x09%ct%x09%s",
                     (char *) "-n",
                     (char *) "200",
                     NULL,
                     NULL };
    char *range;
    char *merge;
    gboolean ok;

    if (from != NULL)
        *from = NULL;

    if (base != NULL)
    {
        range = g_strdup_printf ("%s..%s", base, ref);
        argv[10] = range;
        ok = git_run_stdout (argv, out);
        g_free (range);
        if (!ok || **out != '\0')
        {
            if (ok && from != NULL)
                *from = git_merge_base (data, base, ref);
            return ok;
        }
        g_free (*out);

        merge = git_find_merge_commit (data, base, ref);
        if (merge != NULL)
        {
            char *first_parent;

            first_parent = g_strdup_printf ("%s^1", merge);
            g_free (merge);
            range = g_strdup_printf ("%s..%s", first_parent, ref);
            argv[10] = range;
            ok = git_run_stdout (argv, out);
            g_free (range);
            if (!ok || **out != '\0')
            {
                if (ok && from != NULL)
                    *from = git_merge_base (data, first_parent, ref);
                g_free (first_parent);
                return ok;
            }
            g_free (first_parent);
            g_free (*out);
        }
    }

    argv[10] = (char *) ref;
    return git_run_stdout (argv, out);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_parse_log_and_fill (git_data_t *data, dir_list *list, const char *out)
{
    char **lines;
    int i;

    lines = g_strsplit (out, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        char **parts;
        char *name;
        time_t commit_time;

        if (lines[i][0] == '\0')
            continue;

        parts = g_strsplit (lines[i], "\t", 4);
        if (parts[0] == NULL || parts[1] == NULL || parts[2] == NULL)
        {
            g_strfreev (parts);
            continue;
        }

        commit_time = (time_t) g_ascii_strtoll (parts[2], NULL, 10);
        name = g_strdup (parts[3] != NULL ? parts[3] : "");
        git_add_virtual_dir (data, list, name, GIT_ITEM_COMMIT_DIR, parts[0], parts[1],
                             commit_time);
        g_free (name);
        g_strfreev (parts);
    }

    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */

/* diff, the first entry of a commit list: every file the listed commits change, FROM..the
   newest one. BASE is what the list is taken against. */

static void
git_add_diff_dir (git_data_t *data, dir_list *list, const char *log, const char *from,
                  const char *base)
{
    const char *tab;
    char *tip;
    git_entry_info_t *info;

    if (from == NULL || log == NULL || (tab = strchr (log, '\t')) == NULL)
        return;

    // the newest commit comes first: its sha ends the range
    tip = g_strndup (log, (gsize) (tab - log));
    git_add_virtual_dir (data, list, GIT_DIFF_DIR_NAME, GIT_ITEM_DIFF_DIR, tip,
                         base != NULL ? base : "", time (NULL));
    info = (git_entry_info_t *) g_hash_table_lookup (data->display_to_info, GIT_DIFF_DIR_NAME);
    if (info != NULL)
        info->base_sha = g_strdup (from);
    g_free (tip);
}

/* --------------------------------------------------------------------------------------------- */

static char
git_type_mark_from_name_status (const char *status_token)
{
    char c;

    if (status_token == NULL || status_token[0] == '\0')
        return '*';

    c = status_token[0];
    if (c == 'A')
        return '+';
    if (c == 'D')
        return '-';
    if (c == 'R')
        return '=';
    return '*';
}

/* --------------------------------------------------------------------------------------------- */

static int
git_parse_commit_files_and_fill (git_data_t *data, dir_list *list, const char *out)
{
    char **lines;
    int i;
    int added = 0;

    lines = g_strsplit (out, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        char **parts;
        const char *status;
        const char *path;
        const char *old_path = NULL;
        gboolean is_deleted;

        if (lines[i][0] == '\0')
            continue;

        parts = g_strsplit (lines[i], "\t", 4);
        if (parts[0] == NULL || parts[1] == NULL)
        {
            g_strfreev (parts);
            continue;
        }

        status = parts[0];
        path = parts[1];
        if (status[0] == 'R' && parts[2] != NULL)
        {
            old_path = parts[1];
            path = parts[2];
        }

        is_deleted = (status[0] == 'D');
        git_add_entry (data, list, git_type_mark_from_name_status (status),
                       data->selected_commit_base != NULL ? "DIFF" : "COMMIT", path, is_deleted,
                       FALSE, GIT_ITEM_COMMIT_FILE, data->selected_commit, old_path);
        added++;

        g_strfreev (parts);
    }

    g_strfreev (lines);
    return added;
}

/* --------------------------------------------------------------------------------------------- */

static void
git_add_commit_parent_entry (git_data_t *data, dir_list *list, const char *parent_sha)
{
    struct stat st;
    git_entry_info_t *info;
    char short_sha[10];

    if (parent_sha == NULL || *parent_sha == '\0')
        return;

    memset (&st, 0, sizeof (st));
    st.st_mode = S_IFDIR | 0755;
    st.st_size = 0;
    st.st_mtime = time (NULL);
    st.st_uid = getuid ();
    st.st_gid = getgid ();
    st.st_nlink = 1;

    g_strlcpy (short_sha, parent_sha, sizeof (short_sha));
    (void) dir_list_append (list, short_sha, &st, TRUE, FALSE);

    info = g_new0 (git_entry_info_t, 1);
    info->kind = GIT_ITEM_COMMIT_PARENT_ENTRY;
    info->commit_sha = g_strdup (parent_sha);
    info->state_mark_text = g_strdup ("/");
    info->state_text = g_strdup ("PARENT");
    g_hash_table_insert (data->display_to_info, g_strdup (short_sha), info);
}

/* --------------------------------------------------------------------------------------------- */

static int
git_parse_commit_parents_and_fill (git_data_t *data, dir_list *list, const char *commit_sha)
{
    char *argv[] = { (char *) "git",      (char *) "-C",        data->repo_root,
                     (char *) "rev-list", (char *) "--parents", (char *) "-n",
                     (char *) "1",        (char *) commit_sha,  NULL };
    char *out = NULL;
    char **parts;
    int i;
    int added = 0;

    if (!git_run_stdout (argv, &out))
        return 0;

    g_strchomp (out);
    if (out[0] == '\0')
    {
        g_free (out);
        return 0;
    }

    parts = g_strsplit (out, " ", -1);
    for (i = 1; parts[i] != NULL; i++)
    {
        if (parts[i][0] == '\0')
            continue;
        git_add_commit_parent_entry (data, list, parts[i]);
        added++;
    }

    g_strfreev (parts);
    g_free (out);
    return added;
}

/* --------------------------------------------------------------------------------------------- */

static void
git_show_error (git_data_t *data, const char *title, const char *text)
{
    if (data->host != NULL && data->host->message != NULL)
        data->host->message (data->host, 0, title, text);
}

/* --------------------------------------------------------------------------------------------- */

static const git_entry_info_t *
git_lookup_info (const git_data_t *data, const char *name)
{
    const git_entry_info_t *info;

    if (data == NULL || name == NULL || *name == '\0')
        return NULL;

    info = (const git_entry_info_t *) g_hash_table_lookup (data->display_to_info, name);
    if (info != NULL)
        return info;

    if (name[0] == '/')
        return (const git_entry_info_t *) g_hash_table_lookup (data->display_to_info, name + 1);

    {
        char *slash_name = g_strconcat ("/", name, NULL);
        info = (const git_entry_info_t *) g_hash_table_lookup (data->display_to_info, slash_name);
        g_free (slash_name);
    }

    return info;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_apply_one (git_data_t *data, const char *display_name, git_action_t action)
{
    const git_entry_info_t *info;
    char *err = NULL;
    gboolean ok;

    info = git_lookup_info (data, display_name);
    if (info == NULL || info->repo_path == NULL)
        return FALSE;

    git_debug_log ("git: action=%d file=%s staged=%d status=%s", (int) action, info->repo_path,
                   info->is_staged ? 1 : 0, info->state_text != NULL ? info->state_text : "?");

    if (action == GIT_ACTION_STAGE && info->is_staged)
        return TRUE;

    if (action == GIT_ACTION_UNSTAGE && !info->is_staged)
        return TRUE;

    if (action == GIT_ACTION_RESET)
    {
        if (info->state_text != NULL && strcmp (info->state_text, "UNTRACKED") == 0)
        {
            char *argv[] = { (char *) "git",           (char *) "-C", data->repo_root,
                             (char *) "clean",         (char *) "-f", (char *) "--",
                             (char *) info->repo_path, NULL };

            git_debug_log ("git: run clean -f -- %s", info->repo_path);
            ok = git_run_simple (argv, &err);
        }
        else
        {
            char *argv[] = { (char *) "git",           (char *) "-C",
                             data->repo_root,          (char *) "restore",
                             (char *) "--source=HEAD", (char *) "--staged",
                             (char *) "--worktree",    (char *) "--",
                             (char *) info->repo_path, NULL };

            git_debug_log ("git: run restore --source=HEAD --staged --worktree -- %s",
                           info->repo_path);
            ok = git_run_simple (argv, &err);
        }
    }
    else if (action == GIT_ACTION_UNSTAGE || (action == GIT_ACTION_TOGGLE && info->is_staged))
    {
        char *argv[] = { (char *) "git",           (char *) "-C",
                         data->repo_root,          (char *) "restore",
                         (char *) "--staged",      (char *) "--",
                         (char *) info->repo_path, NULL };

        git_debug_log ("git: run restore --staged -- %s", info->repo_path);
        ok = git_run_simple (argv, &err);
    }
    else
    {
        char *argv[] = { (char *) "git", (char *) "-C", data->repo_root,          (char *) "add",
                         (char *) "-A",  (char *) "--", (char *) info->repo_path, NULL };

        git_debug_log ("git: run add -A -- %s", info->repo_path);
        ok = git_run_simple (argv, &err);
    }

    if (!ok)
    {
        git_debug_log ("git: command failed: %s", err != NULL ? err : "(empty)");
        if (err == NULL || err[0] == '\0')
            git_show_error (data, "Git panel", "Git command failed.");
        else
            git_show_error (data, "Git panel", err);

        g_free (err);
        return FALSE;
    }

    g_free (err);
    git_debug_log ("git: command success");
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_apply_selected (git_data_t *data, git_action_t action)
{
    int marked_count;
    int current = 0;
    const GString *name = NULL;
    gboolean any_processed = FALSE;
    gboolean had_errors = FALSE;

    marked_count = (data->host != NULL && data->host->get_marked_count != NULL)
        ? data->host->get_marked_count (data->host)
        : 0;

    if (data->host == NULL)
        return FALSE;

    if (action == GIT_ACTION_RESET
        && query_dialog (_ ("Git panel"),
                         _ ("This operation is irreversible.\n"
                            "Selected files will be reset to HEAD.\n"
                            "Untracked files will be deleted.\n\n"
                            "Do you want to continue?"),
                         D_NORMAL, 2, _ ("&Reset"), _ ("&Cancel"))
            != 0)
    {
        git_debug_log ("git: reset canceled by user");
        return FALSE;
    }

    if (marked_count > 0 && data->host->get_next_marked != NULL)
    {
        while ((name = data->host->get_next_marked (data->host, &current)) != NULL)
        {
            if (name->str == NULL || strcmp (name->str, "..") == 0)
                continue;

            any_processed = TRUE;
            git_debug_log ("git: selected(marked)='%s' action=%d", name->str, (int) action);
            if (!git_apply_one (data, name->str, action))
                had_errors = TRUE;

            current++;
        }

        if (!any_processed)
        {
            git_show_error (data, "Git panel", "No marked files selected for operation.");
            git_debug_log ("git: no marked files for action=%d", (int) action);
            return FALSE;
        }

        if (had_errors)
            git_show_error (data, "Git panel",
                            "Some marked files failed to process. Check mc log.");
        return !had_errors;
    }

    if (data->host->get_current != NULL)
        name = data->host->get_current (data->host);
    if (name == NULL || name->str == NULL || strcmp (name->str, "..") == 0)
    {
        git_show_error (data, "Git panel", "No file selected for operation.");
        git_debug_log ("git: no selected file for action=%d", (int) action);
        return FALSE;
    }

    git_debug_log ("git: selected(current)='%s' action=%d", name->str, (int) action);
    return git_apply_one (data, name->str, action);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Write a blob to a temporary file named after the file it came from.
 *
 * The name matters: the viewer and the diff viewer pick a syntax by it, and a
 * generated name tells them nothing.  vfs_mkstemps() keeps the original basename
 * as the tail of the temporary one, extension and all.
 *
 * @param contents what to write
 * @param len its length
 * @param orig_name name the temporary one is built from
 * @param tmp_path where the path of the temporary file is stored
 * @return TRUE on success
 */

static gboolean
git_write_temp_contents (const char *contents, gssize len, const char *orig_name, char **tmp_path)
{
    GError *error = NULL;
    vfs_path_t *vpath = NULL;
    int fd;
    gboolean ok;

    fd = vfs_mkstemps (&vpath, "mc-git-diff-", orig_name != NULL ? orig_name : "file");
    if (fd == -1)
        return FALSE;
    close (fd);

    *tmp_path = g_strdup (vfs_path_as_str (vpath));
    vfs_path_free (vpath, TRUE);

    ok = g_file_set_contents (*tmp_path, contents, len, &error);
    if (!ok)
    {
        if (error != NULL)
            g_error_free (error);
        unlink (*tmp_path);
        g_free (*tmp_path);
        *tmp_path = NULL;
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_write_temp_from_git_show (git_data_t *data, const char *object_spec, const char *orig_name,
                              char **tmp_path)
{
    char *out = NULL;
    char *err = NULL;
    gboolean ok;
    char *argv[] = { (char *) "git",  (char *) "-C",        data->repo_root,
                     (char *) "show", (char *) object_spec, NULL };

    ok = git_run_capture (argv, &out, &err);
    if (!ok)
    {
        g_free (out);
        g_free (err);
        return FALSE;
    }

    ok = git_write_temp_contents (out, (gssize) strlen (out), orig_name, tmp_path);
    g_free (out);
    g_free (err);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* The merge of a conflicted file: its stages 2 (ours) and 3 (theirs) at the sides, their merge
   against stage 1 (the base) in the middle, saved into the file of the work tree.  A merge
   saved with no conflict left is added to the index, the way git mergetool marks it
   resolved; with conflicts left the file stays a conflict. */
static gboolean
git_merge_conflict (git_data_t *data, const git_entry_info_t *info)
{
    char *stage_path[3] = { NULL, NULL, NULL };
    gboolean ok = TRUE;
    int rv = -1;
    int i;

    if (data->host->open_merge == NULL)
        return FALSE;

    for (i = 0; ok && i < 3; i++)
    {
        char *spec;

        /* a stage the file does not have, as when both sides added it, is empty */
        spec = g_strdup_printf (":%d:%s", i + 1, info->repo_path);
        if (!git_write_temp_from_git_show (data, spec, info->repo_path, &stage_path[i]))
            ok = git_write_temp_contents ("", 0, info->repo_path, &stage_path[i]);
        g_free (spec);
    }

    if (ok)
    {
        char *ours = g_strdup_printf ("%s (ours)", info->repo_path);
        char *theirs = g_strdup_printf ("%s (theirs)", info->repo_path);

        rv = data->host->open_merge (data->host, stage_path[1], ours, stage_path[0], stage_path[2],
                                     theirs, info->full_path);
        g_free (ours);
        g_free (theirs);
    }

    if (rv == 1)
    {
        char *err = NULL;
        char *argv[] = {
            (char *) "git", (char *) "-C", data->repo_root, (char *) "add", (char *) "--",
            NULL,           NULL
        };

        argv[5] = info->repo_path;
        if (!git_run_simple (argv, &err))
            git_show_error (data, "Git panel", err != NULL ? err : "git add failed.");
        g_free (err);
    }
    else if (rv < 0)
        git_show_error (data, "Git panel", "Cannot open the merge of the conflict.");

    for (i = 0; i < 3; i++)
        if (stage_path[i] != NULL)
        {
            unlink (stage_path[i]);
            g_free (stage_path[i]);
        }

    return rv >= 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_show_diff_selected (git_data_t *data)
{
    const GString *name = NULL;
    const git_entry_info_t *info;
    int current = 0;
    char *left_path = NULL;
    char *right_path = NULL;
    char *left_label = NULL;
    char *right_label = NULL;
    gboolean left_is_temp = FALSE;
    gboolean right_is_temp = FALSE;
    gboolean ok = FALSE;

    if (data->host == NULL || data->host->open_diff == NULL)
        return FALSE;

    if (data->host->get_next_marked != NULL)
        name = data->host->get_next_marked (data->host, &current);
    if ((name == NULL || name->str == NULL || strcmp (name->str, "..") == 0)
        && data->host->get_current != NULL)
        name = data->host->get_current (data->host);
    if (name == NULL || name->str == NULL || strcmp (name->str, "..") == 0)
    {
        git_show_error (data, "Git panel", "No file selected for diff.");
        return FALSE;
    }

    info = git_lookup_info (data, name->str);
    if (info == NULL || info->repo_path == NULL)
    {
        git_show_error (data, "Git panel", "Cannot resolve selected file for diff.");
        return FALSE;
    }

    if (info->is_conflict)
        return git_merge_conflict (data, info);

    if (info->kind == GIT_ITEM_COMMIT_FILE)
    {
        char *object_old = NULL;
        char *object_new = NULL;
        const char *old_path =
            (info->old_repo_path != NULL) ? info->old_repo_path : info->repo_path;
        const gboolean is_added =
            (info->state_mark_text != NULL && info->state_mark_text[0] == '+');

        if (is_added)
        {
            if (!git_write_temp_contents ("", 0, info->repo_path, &left_path))
                goto fail;
            left_is_temp = TRUE;
        }
        else
        {
            if (data->selected_commit_base != NULL)
                object_old = g_strdup_printf ("%s:%s", data->selected_commit_base, old_path);
            else
                object_old = g_strdup_printf ("%s^:%s", info->commit_sha, old_path);
            ok = git_write_temp_from_git_show (data, object_old, old_path, &left_path);
            if (!ok && !git_write_temp_contents ("", 0, old_path, &left_path))
            {
                g_free (object_old);
                goto fail;
            }
            left_is_temp = TRUE;
            g_free (object_old);
        }

        if (info->is_deleted)
        {
            if (!git_write_temp_contents ("", 0, info->repo_path, &right_path))
                goto fail;
            right_is_temp = TRUE;
        }
        else
        {
            object_new = g_strdup_printf ("%s:%s", info->commit_sha, info->repo_path);
            ok = git_write_temp_from_git_show (data, object_new, info->repo_path, &right_path);
            g_free (object_new);
            if (!ok)
                goto fail;
            right_is_temp = TRUE;
        }
    }
    else if (info->is_deleted)
    {
        char *head_spec;

        head_spec = g_strdup_printf ("HEAD:%s", info->repo_path);
        ok = git_write_temp_from_git_show (data, head_spec, info->repo_path, &left_path);
        g_free (head_spec);
        if (!ok)
            goto fail;
        left_is_temp = TRUE;

        if (!git_write_temp_contents ("", 0, info->repo_path, &right_path))
            goto fail;
        right_is_temp = TRUE;
    }
    else
    {
        char *head_spec;

        if (info->full_path == NULL || access (info->full_path, R_OK) != 0)
            goto fail;
        right_path = g_strdup (info->full_path);

        head_spec = g_strdup_printf ("HEAD:%s", info->repo_path);
        ok = git_write_temp_from_git_show (data, head_spec, info->repo_path, &left_path);
        g_free (head_spec);
        if (!ok)
        {
            if (!git_write_temp_contents ("", 0, info->repo_path, &left_path))
                goto fail;
        }
        left_is_temp = TRUE;
    }

    /* the copies have names made up for them; the view shows what they are */
    if (info->kind == GIT_ITEM_COMMIT_FILE)
    {
        char sha[8];
        char *before;
        const char *old_path =
            (info->old_repo_path != NULL) ? info->old_repo_path : info->repo_path;

        g_strlcpy (sha, info->commit_sha != NULL ? info->commit_sha : "", sizeof (sha));
        // the left side is where diff starts, else the parent
        if (data->selected_commit_base != NULL)
            before = g_strdup_printf ("%.7s", data->selected_commit_base);
        else
            before = g_strdup_printf ("%s^", sha);
        if (info->state_mark_text != NULL && info->state_mark_text[0] == '+')
            left_label = g_strdup_printf ("%s (not in %s)", info->repo_path, before);
        else
            left_label = g_strdup_printf ("%s (%s)", old_path, before);
        g_free (before);
        if (info->is_deleted)
            right_label = g_strdup_printf ("%s (deleted in %s)", info->repo_path, sha);
        else
            right_label = g_strdup_printf ("%s (%s)", info->repo_path, sha);
    }
    else
    {
        left_label = g_strdup_printf ("%s (HEAD)", info->repo_path);
        if (info->is_deleted)
            right_label = g_strdup_printf ("%s (deleted)", info->repo_path);
        else
            right_label = g_strdup (info->repo_path);
    }

    if (data->host->open_diff_labeled != NULL)
        ok = data->host->open_diff_labeled (data->host, left_path, left_label, right_path,
                                            right_label,
                                            (left_is_temp ? MC_PP_DIFF_LEFT_READ_ONLY : 0)
                                                | (right_is_temp ? MC_PP_DIFF_RIGHT_READ_ONLY : 0));
    else
        ok = data->host->open_diff (data->host, left_path, right_path);
    g_free (left_label);
    g_free (right_label);
    if (!ok)
    {
        git_show_error (data, "Git panel", "Cannot open internal diff viewer.");
        goto fail;
    }

    if (left_is_temp)
        unlink (left_path);
    if (right_is_temp)
        unlink (right_path);
    g_free (left_path);
    g_free (right_path);
    return TRUE;

fail:
    if (left_path != NULL)
    {
        if (left_is_temp)
            unlink (left_path);
        g_free (left_path);
    }
    if (right_path != NULL)
    {
        if (right_is_temp)
            unlink (right_path);
        g_free (right_path);
    }
    git_show_error (data, "Git panel",
                    "Cannot prepare git diff view. Check file state and repository history.");
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* The output of a git command in a temporary file: what F3 and Quick view show for an entry
   that is not a file */

static gboolean
git_output_to_temp (git_data_t *data, char **argv, const char *header, char **tmp_path)
{
    char *out = NULL;
    char *err = NULL;
    char *text;
    gboolean ok;

    if (!git_run_capture (argv, &out, &err))
    {
        if (err != NULL && *err != '\0')
            git_show_error (data, "Git panel", err);
        g_free (out);
        g_free (err);
        return FALSE;
    }

    text = g_strconcat (header != NULL ? header : "", out != NULL ? out : "", (char *) NULL);
    // a commit message or a summary, not a file: no name to keep
    ok = git_write_temp_contents (text, (gssize) strlen (text), NULL, tmp_path);
    g_free (text);
    g_free (out);
    g_free (err);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_commit_description_to_temp (git_data_t *data, const char *sha, char **tmp_path)
{
    char *argv[] = { (char *) "git",
                     (char *) "-C",
                     data->repo_root,
                     (char *) "show",
                     (char *) "--no-color",
                     (char *) "-s",
                     (char *) "--format=fuller",
                     (char *) sha,
                     NULL };

    if (sha == NULL || *sha == '\0')
        return FALSE;

    return git_output_to_temp (data, argv, NULL, tmp_path);
}

/* --------------------------------------------------------------------------------------------- */

/* diff: how much each file of the listed commits changes */

static gboolean
git_diff_stat_to_temp (git_data_t *data, const git_entry_info_t *info, char **tmp_path)
{
    char *argv[] = { (char *) "git",
                     (char *) "-C",
                     data->repo_root,
                     (char *) "diff",
                     (char *) "--no-color",
                     (char *) "--stat",
                     (char *) "--find-renames",
                     info->base_sha,
                     info->commit_sha,
                     NULL };
    char *header;
    gboolean ok;

    if (info->base_sha == NULL || info->commit_sha == NULL)
        return FALSE;

    header =
        g_strdup_printf ("git diff --stat %.12s %.12s (the branch against %s)\n\n", info->base_sha,
                         info->commit_sha, info->state_text != NULL ? info->state_text : "");
    ok = git_output_to_temp (data, argv, header, tmp_path);
    g_free (header);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_view_temp (char *tmp_path)
{
    vfs_path_t *tmp_vpath;

    tmp_vpath = vfs_path_from_str (tmp_path);
    (void) mcview_viewer (NULL, tmp_vpath, 0, 0, 0);
    vfs_path_free (tmp_vpath, TRUE);
    unlink (tmp_path);
    g_free (tmp_path);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_show_commit_description_by_sha (git_data_t *data, const char *sha, const char *name_for_log)
{
    char *tmp_path = NULL;

    git_debug_log ("git: F3 commit description name='%s' sha=%s view=%s",
                   name_for_log != NULL ? name_for_log : "(null)", sha != NULL ? sha : "(null)",
                   git_view_name (data->view));

    if (!git_commit_description_to_temp (data, sha, &tmp_path))
        return FALSE;

    return git_view_temp (tmp_path);
}

/* --------------------------------------------------------------------------------------------- */

typedef enum
{
    GIT_COOLE_WINDOW,
    GIT_COOLE_COMMIT,
    GIT_COOLE_AMEND,
    GIT_COOLE_SHOW,
    GIT_COOLE_REWORD
} git_coole_action_t;

static void
git_load_coole_settings (git_data_t *data)
{
    g_free (data->coole_executable);
    data->coole_executable = git_load_config_value (GIT_PANEL_COOLE_EXECUTABLE, "coole");
    data->key_coole = git_load_hotkey (GIT_PANEL_KEY_COOLE, GIT_PANEL_KEY_COOLE_DEFAULT, ALT ('g'));
    data->key_commit =
        git_load_hotkey (GIT_PANEL_KEY_COMMIT, GIT_PANEL_KEY_COMMIT_DEFAULT, ALT ('c'));
    data->key_amend = git_load_hotkey (GIT_PANEL_KEY_AMEND, GIT_PANEL_KEY_AMEND_DEFAULT, ALT ('a'));
    data->key_show = git_load_hotkey (GIT_PANEL_KEY_SHOW, GIT_PANEL_KEY_SHOW_DEFAULT, ALT ('v'));
    data->key_reword =
        git_load_hotkey (GIT_PANEL_KEY_REWORD, GIT_PANEL_KEY_REWORD_DEFAULT, ALT ('r'));
}

static char *
git_read_head (git_data_t *data)
{
    char *argv[] = {
        (char *) "git",  (char *) "-C", data->repo_root, (char *) "rev-parse", (char *) "--verify",
        (char *) "HEAD", NULL
    };
    char *head = NULL;

    if (!git_run_stdout (argv, &head))
        return NULL;
    g_strchomp (head);
    if (*head == '\0')
        g_clear_pointer (&head, g_free);
    return head;
}

/* coole owns the message editor and the Git operation. Run it in the foreground so the panel
   reloads only after the editor has closed, including when MC has an overlay terminal. */
static mc_pp_result_t
git_launch_coole (git_data_t *data, git_coole_action_t action, const char *sha)
{
    char *executable;
    char *quoted_executable;
    char *quoted_root;
    char *command;
    char *option = NULL;
    char *head_before = NULL;

    if (data->host == NULL || data->host->run_command == NULL || data->repo_root == NULL)
        return MC_PPR_FAILED;

    if (action == GIT_COOLE_SHOW || action == GIT_COOLE_REWORD)
    {
        if (sha == NULL || *sha == '\0')
        {
            git_show_error (data, _ ("Git panel"), _ ("Select a commit first."));
            return MC_PPR_SKIPPED;
        }

        if (action == GIT_COOLE_REWORD)
        {
            char *argv[] = { (char *) "git",           (char *) "-C",
                             data->repo_root,          (char *) "merge-base",
                             (char *) "--is-ancestor", (char *) sha,
                             (char *) "HEAD",          NULL };

            if (!git_run_simple (argv, NULL))
            {
                git_show_error (data, _ ("Git panel"),
                                _ ("Reword is available only for commits on the current branch."));
                return MC_PPR_SKIPPED;
            }
        }
    }

    executable = g_find_program_in_path (data->coole_executable);
    if (executable == NULL)
    {
        char *msg =
            g_strdup_printf (_ ("Cannot find coole executable: %s"), data->coole_executable);

        git_show_error (data, _ ("Git panel"), msg);
        g_free (msg);
        return MC_PPR_SKIPPED;
    }

    switch (action)
    {
    case GIT_COOLE_WINDOW:
        if ((git_view_t) data->view == GIT_VIEW_COMMITS)
            option = g_strdup ("--git-log");
        break;
    case GIT_COOLE_COMMIT:
        /* Keep the full Git workspace open after a commit. Its message window is ready in -G. */
        break;
    case GIT_COOLE_AMEND:
        option = g_strdup ("--git-amend");
        break;
    case GIT_COOLE_SHOW:
        option = g_strdup_printf ("--git-show=%s", sha);
        break;
    case GIT_COOLE_REWORD:
        option = g_strdup_printf ("--git-reword=%s", sha);
        break;
    default:
        g_free (executable);
        return MC_PPR_FAILED;
    }

    quoted_executable = g_shell_quote (executable);
    quoted_root = g_shell_quote (data->repo_root);
    command = g_strdup_printf ("%s -G %s%s%s", quoted_executable, quoted_root,
                               option != NULL ? " " : "", option != NULL ? option : "");
    if (action == GIT_COOLE_REWORD)
        head_before = git_read_head (data);
    data->host->run_command (data->host, command, EXECUTE_INTERNAL);

    if (head_before != NULL)
    {
        char *head_after = git_read_head (data);

        if (head_after != NULL && strcmp (head_before, head_after) != 0)
        {
            /* The old commit object still exists, but is no longer in the rewritten branch. */
            data->view = GIT_VIEW_COMMITS;
            g_clear_pointer (&data->selected_commit, g_free);
            g_clear_pointer (&data->selected_commit_name, g_free);
            g_clear_pointer (&data->selected_commit_list_name, g_free);
            g_clear_pointer (&data->selected_branch, g_free);
            g_clear_pointer (&data->selected_remote, g_free);
            data->selected_branch_scope = GIT_BRANCH_SCOPE_NONE;
            git_commit_stack_clear (data);
            git_set_pending_focus (data, NULL);
            git_update_title (data);
        }
        g_free (head_after);
    }

    g_free (head_before);
    g_free (command);
    g_free (quoted_root);
    g_free (quoted_executable);
    g_free (option);
    g_free (executable);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
git_favorites_load (void)
{
    char *cfg_path;
    mc_config_t *cfg;
    gchar **list;
    gsize len = 0;
    GPtrArray *favs;
    gsize i;

    cfg_path = g_build_filename (mc_config_get_path (), GIT_PANEL_CONFIG_FILE, (char *) NULL);
    cfg = mc_config_init (cfg_path, TRUE);
    g_free (cfg_path);

    favs = g_ptr_array_new_with_free_func (g_free);
    list =
        mc_config_get_string_list (cfg, GIT_FAVORITES_CONFIG_GROUP, GIT_FAVORITES_CONFIG_KEY, &len);
    if (list != NULL)
    {
        for (i = 0; i < len; i++)
            if (list[i] != NULL && list[i][0] != '\0')
                g_ptr_array_add (favs, g_strdup (list[i]));
        g_strfreev (list);
    }
    mc_config_deinit (cfg);
    return favs;
}

/* --------------------------------------------------------------------------------------------- */

static void
git_favorites_save (GPtrArray *favs)
{
    char *cfg_path;
    mc_config_t *cfg;

    cfg_path = g_build_filename (mc_config_get_path (), GIT_PANEL_CONFIG_FILE, (char *) NULL);
    cfg = mc_config_init (cfg_path, FALSE);
    g_free (cfg_path);

    if (favs->len == 0)
        mc_config_del_key (cfg, GIT_FAVORITES_CONFIG_GROUP, GIT_FAVORITES_CONFIG_KEY);
    else
        mc_config_set_string_list (cfg, GIT_FAVORITES_CONFIG_GROUP, GIT_FAVORITES_CONFIG_KEY,
                                   (const gchar **) favs->pdata, favs->len);
    mc_config_save_file (cfg, NULL);
    mc_config_deinit (cfg);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_favorites_add (const char *repo_path)
{
    GPtrArray *favs;
    guint i;
    gboolean found = FALSE;

    favs = git_favorites_load ();
    for (i = 0; i < favs->len; i++)
        if (strcmp ((const char *) favs->pdata[i], repo_path) == 0)
        {
            found = TRUE;
            break;
        }
    if (!found)
        g_ptr_array_add (favs, g_strdup (repo_path));
    git_favorites_save (favs);
    g_ptr_array_free (favs, TRUE);
    return !found;
}

/* --------------------------------------------------------------------------------------------- */
/* Plugin callbacks */
/* --------------------------------------------------------------------------------------------- */

static void *
git_open (mc_panel_host_t *host, const char *open_path)
{
    git_data_t *data;
    char *repo_root;
    const char *start_path;

    if (open_path != NULL && g_str_has_prefix (open_path, git_plugin.prefix))
        start_path = open_path + strlen (git_plugin.prefix);
    else
        start_path = (open_path != NULL && *open_path != '\0') ? open_path : ".";
    if (*start_path == '\0')
        start_path = ".";
    repo_root = git_detect_repo_root (start_path);
    if (repo_root == NULL)
    {
        data = g_new0 (git_data_t, 1);
        data->host = host;
        data->display_to_info =
            g_hash_table_new_full (g_str_hash, g_str_equal, g_free, git_entry_info_free);
        data->default_format = git_load_default_format ();
        data->key_refresh =
            git_load_hotkey (GIT_PANEL_KEY_REFRESH, GIT_PANEL_KEY_REFRESH_DEFAULT, XCTRL ('r'));
        data->key_fav_add =
            git_load_hotkey (GIT_PANEL_KEY_FAV_ADD, GIT_PANEL_KEY_FAV_ADD_DEFAULT, XCTRL ('b'));
        data->view = GIT_VIEW_FAVORITES;
        data->commit_stack = g_ptr_array_new_with_free_func (git_commit_nav_free);
        data->fav_repos = git_favorites_load ();
        data->help_filename = g_build_filename (MC_PLUGIN_DIR, "git_panel.md", (char *) NULL);
        git_update_title (data);
        return data;
    }

    data = g_new0 (git_data_t, 1);
    data->host = host;
    data->repo_root = repo_root;
    data->title = g_strdup (repo_root);
    data->help_filename = g_build_filename (MC_PLUGIN_DIR, "git_panel.md", (char *) NULL);
    data->display_to_info =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free, git_entry_info_free);
    data->default_format = git_load_default_format ();
    data->key_stage =
        git_load_hotkey (GIT_PANEL_KEY_STAGE, GIT_PANEL_KEY_STAGE_DEFAULT, KEY_F (15));
    data->key_unstage =
        git_load_hotkey (GIT_PANEL_KEY_UNSTAGE, GIT_PANEL_KEY_UNSTAGE_DEFAULT, KEY_F (16));
    data->key_toggle = git_load_hotkey (GIT_PANEL_KEY_TOGGLE, GIT_PANEL_KEY_TOGGLE_DEFAULT, -1);
    data->key_diff = git_load_hotkey (GIT_PANEL_KEY_DIFF, GIT_PANEL_KEY_DIFF_DEFAULT, XCTRL ('d'));
    data->key_diff_alt =
        git_load_hotkey (GIT_PANEL_KEY_DIFF_ALT, GIT_PANEL_KEY_DIFF_ALT_DEFAULT, KEY_F (13));
    data->key_refresh =
        git_load_hotkey (GIT_PANEL_KEY_REFRESH, GIT_PANEL_KEY_REFRESH_DEFAULT, XCTRL ('r'));
    data->key_reset =
        git_load_hotkey (GIT_PANEL_KEY_RESET, GIT_PANEL_KEY_RESET_DEFAULT, KEY_F (18));
    data->key_fav_add =
        git_load_hotkey (GIT_PANEL_KEY_FAV_ADD, GIT_PANEL_KEY_FAV_ADD_DEFAULT, XCTRL ('b'));
    git_load_coole_settings (data);
    data->view = GIT_VIEW_STATUS;
    data->selected_commit = NULL;
    data->current_branch = git_detect_current_branch (data);
    data->selected_branch = NULL;
    data->selected_remote = NULL;
    data->selected_commit_name = NULL;
    data->selected_commit_list_name = NULL;
    data->selected_branch_scope = GIT_BRANCH_SCOPE_NONE;
    data->pending_focus = NULL;
    data->commit_stack = g_ptr_array_new_with_free_func (git_commit_nav_free);
    git_update_title (data);

    git_debug_log (
        "git: keys stage=%d unstage=%d toggle=%d diff=%d diff_alt=%d refresh=%d reset=%d",
        data->key_stage, data->key_unstage, data->key_toggle, data->key_diff, data->key_diff_alt,
        data->key_refresh, data->key_reset);

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
git_close (void *plugin_data)
{
    git_data_t *data = (git_data_t *) plugin_data;

    g_hash_table_destroy (data->display_to_info);
    g_free (data->title);
    g_free (data->help_filename);
    g_free (data->repo_root);
    g_free (data->default_format);
    g_free (data->coole_executable);
    g_free (data->selected_commit);
    g_free (data->selected_commit_base);
    g_free (data->current_branch);
    g_free (data->selected_branch);
    g_free (data->selected_remote);
    g_free (data->selected_commit_name);
    g_free (data->selected_commit_list_name);
    g_free (data->pending_focus);
    g_ptr_array_free (data->commit_stack, TRUE);
    if (data->fav_repos != NULL)
        g_ptr_array_free (data->fav_repos, TRUE);
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_get_items (void *plugin_data, void *list_ptr)
{
    git_data_t *data = (git_data_t *) plugin_data;
    dir_list *list = (dir_list *) list_ptr;
    char *out = NULL;
    gboolean ok = FALSE;

    g_hash_table_remove_all (data->display_to_info);
    if (data->repo_root != NULL)
    {
        g_free (data->current_branch);
        data->current_branch = git_detect_current_branch (data);
    }

    if ((git_view_t) data->view == GIT_VIEW_STATUS)
    {
        char *argv[] = { (char *) "git",
                         (char *) "-C",
                         data->repo_root,
                         (char *) "status",
                         (char *) "--porcelain=v1",
                         NULL };

        git_update_title (data);
        git_add_virtual_dir (data, list, GIT_BRANCHES_DIR_NAME, GIT_ITEM_BRANCHES_DIR, NULL, "",
                             time (NULL));
        git_add_virtual_dir (data, list, GIT_COMMITS_DIR_NAME, GIT_ITEM_COMMITS_DIR, NULL, "",
                             time (NULL));
        ok = git_run_stdout (argv, &out);
        if (!ok)
            return MC_PPR_FAILED;
        git_parse_status_and_fill (data, list, out);
    }
    else if ((git_view_t) data->view == GIT_VIEW_BRANCHES)
    {
        git_add_virtual_dir (data, list, GIT_BRANCHES_LOCAL_DIR_NAME, GIT_ITEM_BRANCHES_LOCAL_DIR,
                             NULL, "", time (NULL));

        {
            char *argv[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "remote",
                             NULL };

            ok = git_run_stdout (argv, &out);
            if (!ok)
                return MC_PPR_FAILED;
            git_parse_remotes_and_fill (data, list, out);
        }
    }
    else if ((git_view_t) data->view == GIT_VIEW_BRANCHES_LOCAL)
    {
        char *argv[] = { (char *) "git",
                         (char *) "-C",
                         data->repo_root,
                         (char *) "for-each-ref",
                         (char *) "--sort=-committerdate",
                         (char *) "--format=%(refname:short)\t%(objectname:short)\t%(committerdate:"
                                  "unix)\t%(HEAD)",
                         (char *) "refs/heads",
                         NULL };

        ok = git_run_stdout (argv, &out);
        if (!ok)
            return MC_PPR_FAILED;
        git_parse_branches_and_fill (data, list, out, NULL);
    }
    else if ((git_view_t) data->view == GIT_VIEW_BRANCHES_REMOTE_ITEMS
             && data->selected_remote != NULL)
    {
        char *remote_ref = NULL;
        char *argv[] = {
            (char *) "git",
            (char *) "-C",
            data->repo_root,
            (char *) "for-each-ref",
            (char *) "--sort=-committerdate",
            (char *) "--format=%(refname:short)\t%(objectname:short)\t%(committerdate:unix)",
            NULL,
            NULL
        };

        remote_ref = g_strdup_printf ("refs/remotes/%s", data->selected_remote);
        argv[6] = remote_ref;

        ok = git_run_stdout (argv, &out);
        g_free (remote_ref);
        if (!ok)
            return MC_PPR_FAILED;
        git_parse_branches_and_fill (data, list, out, data->selected_remote);
    }
    else if ((git_view_t) data->view == GIT_VIEW_COMMITS)
    {
        char *out2 = NULL;
        char *from = NULL;
        char *base = NULL;
        gboolean ok2;

        if (data->selected_branch != NULL)
        {
            char *sel_upstream = NULL;
            /* Local name portion: strip remote prefix (e.g. "origin/master" -> "master"). */
            const char *sel_slash = strrchr (data->selected_branch, '/');
            const char *sel_local = (sel_slash != NULL) ? sel_slash + 1 : data->selected_branch;

            /* Try the configured upstream for the selected branch. */
            {
                char *up_ref = g_strdup_printf ("%s@{upstream}", data->selected_branch);
                char *up_argv[] = { (char *) "git",
                                    (char *) "-C",
                                    data->repo_root,
                                    (char *) "rev-parse",
                                    (char *) "--abbrev-ref",
                                    (char *) "--symbolic-full-name",
                                    up_ref,
                                    NULL };
                char *up_out = NULL;

                if (git_run_stdout (up_argv, &up_out))
                {
                    g_strchomp (up_out);
                    if (up_out[0] != '\0')
                    {
                        /* Skip if upstream is just the remote tracking copy of this branch
                         * (e.g. origin/master for master) -- range would be empty when synced. */
                        const char *up_slash = strrchr (up_out, '/');
                        const char *up_tail = (up_slash != NULL) ? up_slash + 1 : up_out;

                        if (strcmp (up_tail, sel_local) != 0)
                            sel_upstream = up_out;
                        else
                            g_free (up_out);
                    }
                    else
                        g_free (up_out);
                }
                g_free (up_ref);
            }

            /* Fall back to local base branch (master/main). */
            if (sel_upstream == NULL)
            {
                static const char *candidates[] = { "master", "main" };
                size_t ci;

                for (ci = 0; ci < G_N_ELEMENTS (candidates); ci++)
                {
                    char *ref_check = g_strdup_printf ("refs/heads/%s", candidates[ci]);
                    char *chk_argv[] = { (char *) "git",      (char *) "-C",
                                         data->repo_root,     (char *) "rev-parse",
                                         (char *) "--verify", (char *) "--quiet",
                                         ref_check,           NULL };
                    char *chk_out = NULL;

                    if (git_run_stdout (chk_argv, &chk_out)
                        && strcmp (sel_local, candidates[ci]) != 0)
                    {
                        g_free (chk_out);
                        g_free (ref_check);
                        sel_upstream = g_strdup (candidates[ci]);
                        break;
                    }
                    g_free (chk_out);
                    g_free (ref_check);
                }
            }

            ok2 = git_run_branch_log (data, sel_upstream, data->selected_branch, &out2, &from);
            base = sel_upstream;
        }
        else
        {
            char *base_ref;

            base_ref = git_detect_upstream_ref (data);
            if (base_ref == NULL)
                base_ref = git_detect_base_ref (data);
            ok2 = git_run_branch_log (data, base_ref, "HEAD", &out2, &from);
            base = base_ref;
        }

        if (ok2)
        {
            git_add_diff_dir (data, list, out2, from, base);
            git_parse_log_and_fill (data, list, out2);
        }
        g_free (base);
        g_free (from);
        g_free (out2);
        if (!ok2)
            return MC_PPR_FAILED;
        return MC_PPR_OK;
    }
    else if ((git_view_t) data->view == GIT_VIEW_COMMIT_FILES && data->selected_commit != NULL
             && data->selected_commit_base != NULL)
    {
        char *argv[] = { (char *) "git",
                         (char *) "-C",
                         data->repo_root,
                         (char *) "diff",
                         (char *) "--no-color",
                         (char *) "--name-status",
                         (char *) "--find-renames",
                         data->selected_commit_base,
                         data->selected_commit,
                         NULL };

        ok = git_run_stdout (argv, &out);
        if (!ok)
            return MC_PPR_FAILED;
        (void) git_parse_commit_files_and_fill (data, list, out);
    }
    else if ((git_view_t) data->view == GIT_VIEW_COMMIT_FILES && data->selected_commit != NULL)
    {
        int files_added;
        char *argv[] = { (char *) "git",
                         (char *) "-C",
                         data->repo_root,
                         (char *) "show",
                         (char *) "--name-status",
                         (char *) "--format=",
                         (char *) "--find-renames",
                         data->selected_commit,
                         NULL };

        ok = git_run_stdout (argv, &out);
        if (!ok)
            return MC_PPR_FAILED;
        files_added = git_parse_commit_files_and_fill (data, list, out);
        if (files_added == 0)
            (void) git_parse_commit_parents_and_fill (data, list, data->selected_commit);
    }
    else if ((git_view_t) data->view == GIT_VIEW_FAVORITES)
    {
        guint i;

        if (data->fav_repos != NULL)
            for (i = 0; i < data->fav_repos->len; i++)
            {
                const char *repo_path = (const char *) data->fav_repos->pdata[i];
                const char *display_name = (repo_path[0] == '/') ? repo_path + 1 : repo_path;
                git_entry_info_t *einfo;

                git_add_virtual_dir (data, list, display_name, GIT_ITEM_FAVORITE_REPO, NULL, "", 0);
                einfo =
                    (git_entry_info_t *) g_hash_table_lookup (data->display_to_info, display_name);
                if (einfo != NULL)
                    einfo->full_path = g_strdup (repo_path);
            }
    }
    else
        return MC_PPR_FAILED;

    g_free (out);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_get_local_copy (void *plugin_data, const char *fname, char **local_path)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const git_entry_info_t *info;
    const char *source_name = NULL;

    if (fname == NULL || local_path == NULL)
        return MC_PPR_FAILED;

    info = git_lookup_info (data, fname);
    if (info == NULL)
        return MC_PPR_FAILED;

    if (info->kind == GIT_ITEM_STATUS_FILE)
    {
        if (info->full_path == NULL || info->is_deleted || access (info->full_path, R_OK) != 0)
            return MC_PPR_FAILED;

        if (!git_make_temp_copy (info->full_path, local_path))
            return MC_PPR_FAILED;

        source_name = info->full_path;
    }
    else if (info->kind == GIT_ITEM_COMMIT_FILE)
    {
        char *object_spec;
        gboolean ok;

        if (info->is_deleted || info->commit_sha == NULL || info->repo_path == NULL)
            return MC_PPR_FAILED;

        object_spec = g_strdup_printf ("%s:%s", info->commit_sha, info->repo_path);
        ok = git_write_temp_from_git_show (data, object_spec, info->repo_path, local_path);
        g_free (object_spec);
        if (!ok)
            return MC_PPR_FAILED;
        // the name of the copy ends with the name of the file already
    }
    else
        return MC_PPR_FAILED;

    if (*local_path == NULL)
        return MC_PPR_FAILED;

    /* Rename temp file to preserve the original extension so that
     * mc.ext.ini rules can match it (e.g. syntax highlighting for .c). */
    if (source_name != NULL)
        mc_pp_rename_with_ext (local_path, source_name);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_chdir (void *plugin_data, const char *path)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const git_entry_info_t *info;

    if (data == NULL || path == NULL)
        return MC_PPR_FAILED;

    git_set_pending_focus (data, NULL);
    git_debug_log ("git: chdir path='%s' view=%s", path, git_view_name (data->view));

    if ((git_view_t) data->view == GIT_VIEW_FAVORITES)
    {
        char *repo_root;

        if (strcmp (path, "..") == 0)
            return MC_PPR_CLOSE;

        info = git_lookup_info (data, path);
        if (info == NULL || info->kind != GIT_ITEM_FAVORITE_REPO)
            return MC_PPR_FAILED;

        repo_root = git_detect_repo_root (info->full_path != NULL ? info->full_path : path);
        if (repo_root == NULL)
        {
            if (data->host != NULL && data->host->message != NULL)
            {
                char *msg;

                msg = g_strdup_printf ("Repository not found: %s", path);
                data->host->message (data->host, D_ERROR, "Git panel", msg);
                g_free (msg);
            }
            return MC_PPR_FAILED;
        }

        data->repo_root = repo_root;
        g_free (data->current_branch);
        data->current_branch = git_detect_current_branch (data);
        data->view = GIT_VIEW_STATUS;
        /* Load keys that were skipped in the favorites open path. */
        data->key_stage =
            git_load_hotkey (GIT_PANEL_KEY_STAGE, GIT_PANEL_KEY_STAGE_DEFAULT, KEY_F (15));
        data->key_unstage =
            git_load_hotkey (GIT_PANEL_KEY_UNSTAGE, GIT_PANEL_KEY_UNSTAGE_DEFAULT, KEY_F (16));
        data->key_toggle = git_load_hotkey (GIT_PANEL_KEY_TOGGLE, GIT_PANEL_KEY_TOGGLE_DEFAULT, -1);
        data->key_diff =
            git_load_hotkey (GIT_PANEL_KEY_DIFF, GIT_PANEL_KEY_DIFF_DEFAULT, XCTRL ('d'));
        data->key_diff_alt =
            git_load_hotkey (GIT_PANEL_KEY_DIFF_ALT, GIT_PANEL_KEY_DIFF_ALT_DEFAULT, KEY_F (13));
        data->key_reset =
            git_load_hotkey (GIT_PANEL_KEY_RESET, GIT_PANEL_KEY_RESET_DEFAULT, KEY_F (18));
        git_load_coole_settings (data);
        git_update_title (data);

        if (data->host->add_history != NULL)
        {
            char *hist_path;

            hist_path = g_strdup_printf ("%s%s", git_plugin.prefix, data->repo_root);
            data->host->add_history (data->host, hist_path);
            g_free (hist_path);
        }

        return MC_PPR_OK;
    }

    if (strcmp (path, "..") == 0)
    {
        if ((git_view_t) data->view == GIT_VIEW_STATUS)
            return MC_PPR_CLOSE; /* close plugin */

        if ((git_view_t) data->view == GIT_VIEW_BRANCHES)
        {
            git_set_pending_focus (data, GIT_BRANCHES_DIR_NAME);
            data->view = GIT_VIEW_STATUS;
            git_update_title (data);
            return MC_PPR_OK;
        }

        if ((git_view_t) data->view == GIT_VIEW_BRANCHES_LOCAL
            || (git_view_t) data->view == GIT_VIEW_BRANCHES_REMOTE_ITEMS)
        {
            char *focus = NULL;

            if ((git_view_t) data->view == GIT_VIEW_BRANCHES_LOCAL)
                focus = g_strdup (GIT_BRANCHES_LOCAL_DIR_NAME);
            else
                focus = g_strdup_printf (
                    "remote/%s", data->selected_remote != NULL ? data->selected_remote : "");
            git_set_pending_focus (data, focus);
            g_free (focus);
            data->view = GIT_VIEW_BRANCHES;
            git_update_title (data);
            return MC_PPR_OK;
        }

        if ((git_view_t) data->view == GIT_VIEW_COMMIT_FILES)
        {
            char *return_focus = g_strdup (data->selected_commit_name);

            g_free (data->selected_commit_base);
            data->selected_commit_base = NULL;

            if (git_commit_stack_pop_to_current (data))
            {
                git_set_pending_focus (data, return_focus);
                git_update_title (data);
                g_free (return_focus);
                return MC_PPR_OK;
            }
            g_free (return_focus);

            git_set_pending_focus (data,
                                   data->selected_commit_list_name != NULL
                                       ? data->selected_commit_list_name
                                       : (data->selected_commit_name != NULL
                                              ? data->selected_commit_name
                                              : data->selected_commit));
            data->view = GIT_VIEW_COMMITS;
            git_update_title (data);
            return MC_PPR_OK;
        }

        if ((git_view_t) data->view == GIT_VIEW_COMMITS
            && data->selected_branch_scope != GIT_BRANCH_SCOPE_NONE)
        {
            if (data->selected_branch_scope == GIT_BRANCH_SCOPE_LOCAL)
            {
                git_set_pending_focus (data, data->selected_branch);
                data->view = GIT_VIEW_BRANCHES_LOCAL;
            }
            else
            {
                const char *focus = data->selected_branch;

                if (focus != NULL && data->selected_remote != NULL)
                {
                    size_t plen = strlen (data->selected_remote);

                    if (g_str_has_prefix (focus, data->selected_remote) && focus[plen] == '/')
                        focus += plen + 1;
                }
                git_set_pending_focus (data, focus);
                data->view = GIT_VIEW_BRANCHES_REMOTE_ITEMS;
            }
            g_free (data->selected_branch);
            data->selected_branch = NULL;
            data->selected_branch_scope = GIT_BRANCH_SCOPE_NONE;
            g_free (data->selected_commit);
            data->selected_commit = NULL;
            g_free (data->selected_commit_base);
            data->selected_commit_base = NULL;
            g_free (data->selected_commit_name);
            data->selected_commit_name = NULL;
            g_free (data->selected_commit_list_name);
            data->selected_commit_list_name = NULL;
            git_commit_stack_clear (data);
            git_update_title (data);
            return MC_PPR_OK;
        }

        git_set_pending_focus (data, GIT_COMMITS_DIR_NAME);
        data->view = GIT_VIEW_STATUS;
        g_free (data->selected_branch);
        data->selected_branch = NULL;
        data->selected_branch_scope = GIT_BRANCH_SCOPE_NONE;
        g_free (data->selected_commit);
        data->selected_commit = NULL;
        g_free (data->selected_commit_base);
        data->selected_commit_base = NULL;
        g_free (data->selected_commit_name);
        data->selected_commit_name = NULL;
        g_free (data->selected_commit_list_name);
        data->selected_commit_list_name = NULL;
        git_commit_stack_clear (data);
        g_free (data->selected_remote);
        data->selected_remote = NULL;
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_STATUS && strcmp (path, GIT_BRANCHES_DIR_NAME) == 0)
    {
        data->view = GIT_VIEW_BRANCHES;
        git_set_pending_focus (data, GIT_BRANCHES_DIR_NAME);
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_STATUS && strcmp (path, GIT_COMMITS_DIR_NAME) == 0)
    {
        g_free (data->selected_branch);
        data->selected_branch = NULL;
        data->selected_branch_scope = GIT_BRANCH_SCOPE_NONE;
        g_free (data->selected_commit_name);
        data->selected_commit_name = NULL;
        g_free (data->selected_commit_list_name);
        data->selected_commit_list_name = NULL;
        git_commit_stack_clear (data);
        git_set_pending_focus (data, GIT_COMMITS_DIR_NAME);
        data->view = GIT_VIEW_COMMITS;
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_BRANCHES
        && strcmp (path, GIT_BRANCHES_LOCAL_DIR_NAME) == 0)
    {
        data->view = GIT_VIEW_BRANCHES_LOCAL;
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_BRANCHES
        && g_str_has_prefix (path, GIT_BRANCHES_REMOTE_PREFIX))
    {
        info = git_lookup_info (data, path);
        if (info == NULL || info->kind != GIT_ITEM_REMOTE_DIR_ENTRY)
            return MC_PPR_FAILED;

        g_free (data->selected_remote);
        data->selected_remote = g_strdup (path + strlen (GIT_BRANCHES_REMOTE_PREFIX));
        git_set_pending_focus (data, path);
        data->view = GIT_VIEW_BRANCHES_REMOTE_ITEMS;
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_BRANCHES_LOCAL
        || (git_view_t) data->view == GIT_VIEW_BRANCHES_REMOTE_ITEMS)
    {
        info = git_lookup_info (data, path);
        if (info == NULL || info->kind != GIT_ITEM_BRANCH_ENTRY || info->repo_path == NULL)
            return MC_PPR_FAILED;

        g_free (data->selected_branch);
        data->selected_branch = g_strdup (info->repo_path);
        data->selected_branch_scope = ((git_view_t) data->view == GIT_VIEW_BRANCHES_LOCAL)
            ? GIT_BRANCH_SCOPE_LOCAL
            : GIT_BRANCH_SCOPE_REMOTE;
        git_set_pending_focus (data, info->repo_path);

        data->view = GIT_VIEW_COMMITS;
        git_commit_stack_clear (data);
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_COMMIT_FILES)
    {
        info = git_lookup_info (data, path);
        if (info == NULL || info->kind != GIT_ITEM_COMMIT_PARENT_ENTRY || info->commit_sha == NULL)
            return MC_PPR_FAILED;

        git_commit_stack_push_current (data);
        g_free (data->selected_commit);
        data->selected_commit = g_strdup (info->commit_sha);
        g_free (data->selected_commit_name);
        data->selected_commit_name = g_strdup (path);
        g_free (data->selected_commit_list_name);
        data->selected_commit_list_name = g_strdup (path);
        git_set_pending_focus (data, path);
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_COMMITS)
    {
        info = git_lookup_info (data, path);
        if (info == NULL || (info->kind != GIT_ITEM_COMMIT_DIR && info->kind != GIT_ITEM_DIFF_DIR)
            || info->commit_sha == NULL)
            return MC_PPR_FAILED;

        data->view = GIT_VIEW_COMMIT_FILES;
        g_free (data->selected_commit);
        data->selected_commit = g_strdup (info->commit_sha);
        g_free (data->selected_commit_base);
        data->selected_commit_base = g_strdup (info->base_sha);
        g_free (data->selected_commit_name);
        // a commit is named by its short sha, diff by itself
        data->selected_commit_name = g_strdup (
            info->kind == GIT_ITEM_COMMIT_DIR && info->state_text != NULL ? info->state_text
                                                                          : path);
        g_free (data->selected_commit_list_name);
        data->selected_commit_list_name = g_strdup (path);
        git_set_pending_focus (data, path);
        git_update_title (data);
        return MC_PPR_OK;
    }

    return MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_enter (void *plugin_data, const char *fname, const struct stat *st)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const git_entry_info_t *info;

    git_set_pending_focus (data, NULL);

    if (fname == NULL || st == NULL)
        return MC_PPR_FAILED;

    info = git_lookup_info (data, fname);
    git_debug_log ("git: enter name='%s' mode=0%o isdir=%d view=%s kind=%d", fname,
                   (unsigned) st->st_mode, S_ISDIR (st->st_mode) ? 1 : 0,
                   git_view_name (data->view), info != NULL ? info->kind : -1);

    if (info != NULL)
    {
        switch (info->kind)
        {
        case GIT_ITEM_STATUS_FILE:
            if (info->full_path != NULL && !info->is_deleted && data->host != NULL
                && data->host->navigate_other_panel != NULL)
            {
                char *dir = g_path_get_dirname (info->full_path);
                char *file = g_path_get_basename (info->full_path);

                data->host->navigate_other_panel (data->host, dir, file);
                g_free (dir);
                g_free (file);
                return MC_PPR_OK;
            }
            return MC_PPR_FAILED;
        case GIT_ITEM_BRANCHES_DIR:
        case GIT_ITEM_BRANCHES_LOCAL_DIR:
        case GIT_ITEM_REMOTE_DIR_ENTRY:
        case GIT_ITEM_BRANCH_ENTRY:
        case GIT_ITEM_COMMITS_DIR:
        case GIT_ITEM_COMMIT_DIR:
        case GIT_ITEM_COMMIT_PARENT_ENTRY:
        case GIT_ITEM_FAVORITE_REPO:
        case GIT_ITEM_DIFF_DIR:
            return git_chdir (plugin_data, fname);
        default:
            break;
        }
    }

    if (S_ISDIR (st->st_mode))
        return git_chdir (plugin_data, fname);

    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_view_item (void *plugin_data, const char *fname, const struct stat *st, gboolean plain_view)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const git_entry_info_t *info;

    (void) st;
    (void) plain_view;

    if (data == NULL || fname == NULL)
        return MC_PPR_FAILED;

    info = git_lookup_info (data, fname);
    if (info == NULL)
        return MC_PPR_NOT_SUPPORTED;

    if (info->kind == GIT_ITEM_DIFF_DIR)
    {
        char *tmp_path = NULL;

        if (git_diff_stat_to_temp (data, info, &tmp_path) && git_view_temp (tmp_path))
            return MC_PPR_OK;
        return MC_PPR_FAILED;
    }

    if ((git_view_t) data->view != GIT_VIEW_COMMITS
        && (git_view_t) data->view != GIT_VIEW_COMMIT_FILES)
        return MC_PPR_NOT_SUPPORTED;

    if (info->kind != GIT_ITEM_COMMIT_DIR && info->kind != GIT_ITEM_COMMIT_PARENT_ENTRY)
        return MC_PPR_NOT_SUPPORTED;

    if (git_show_commit_description_by_sha (data, info->commit_sha, fname))
        return MC_PPR_OK;

    return MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

/* A commit is a directory of the panel: Quick view shows what F3 does, and files go on to
   get_local_copy() */

static mc_pp_result_t
git_get_quick_view (void *plugin_data, const char *fname, const struct stat *st, char **local_path)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const git_entry_info_t *info;
    gboolean ok;

    (void) st;

    if (data == NULL || fname == NULL || local_path == NULL)
        return MC_PPR_FAILED;

    info = git_lookup_info (data, fname);
    if (info == NULL)
        return MC_PPR_NOT_SUPPORTED;

    if (info->kind == GIT_ITEM_DIFF_DIR)
        ok = git_diff_stat_to_temp (data, info, local_path);
    else if (info->kind == GIT_ITEM_COMMIT_DIR || info->kind == GIT_ITEM_COMMIT_PARENT_ENTRY)
        ok = git_commit_description_to_temp (data, info->commit_sha, local_path);
    else
        return MC_PPR_NOT_SUPPORTED;

    return ok ? MC_PPR_OK : MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_get_help_info (void *plugin_data, const char **filename, const char **node)
{
    git_data_t *data = (git_data_t *) plugin_data;

    if (filename != NULL && data != NULL && data->help_filename != NULL
        && exist_file (data->help_filename))
        *filename = data->help_filename;
    else if (filename != NULL)
        *filename = NULL;
    if (node != NULL)
        *node = "[Git panel plugin]";

    if (filename != NULL && *filename == NULL)
        return MC_PPR_NOT_SUPPORTED;

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_get_title (void *plugin_data)
{
    git_data_t *data = (git_data_t *) plugin_data;
    return data->title;
}

/* --------------------------------------------------------------------------------------------- */

/* The repository this panel shows, as "git:/path"; NULL in the favorites view. */
static char *
git_get_location (void *plugin_data)
{
    git_data_t *data = (git_data_t *) plugin_data;

    if (data->repo_root == NULL)
        return NULL;

    return g_strdup_printf ("%s%s", git_plugin.prefix, data->repo_root);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_create_item (void *plugin_data)
{
    git_data_t *data = (git_data_t *) plugin_data;
    char *path;
    char *repo_root;
    guint i;

    if ((git_view_t) data->view != GIT_VIEW_FAVORITES)
        return MC_PPR_NOT_SUPPORTED;

    path = input_expand_dialog (_ ("Add to favorites"), _ ("Repository path:"), "git-fav-path", "",
                                INPUT_COMPLETE_CD | INPUT_COMPLETE_FILENAMES);
    if (path == NULL || path[0] == '\0')
    {
        g_free (path);
        return MC_PPR_OK;
    }

    repo_root = git_detect_repo_root (path);
    g_free (path);

    if (repo_root == NULL)
    {
        if (data->host != NULL && data->host->message != NULL)
            data->host->message (data->host, D_ERROR, "Git panel", "Not a Git repository.");
        return MC_PPR_FAILED;
    }

    for (i = 0; i < data->fav_repos->len; i++)
        if (strcmp ((const char *) data->fav_repos->pdata[i], repo_root) == 0)
        {
            if (data->host != NULL && data->host->message != NULL)
                data->host->message (data->host, D_NORMAL, "Git panel",
                                     "Repository is already in favorites.");
            g_free (repo_root);
            return MC_PPR_OK;
        }

    g_ptr_array_add (data->fav_repos, repo_root);
    git_favorites_save (data->fav_repos);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_delete_items (void *plugin_data, const char **names, int count)
{
    git_data_t *data = (git_data_t *) plugin_data;
    int i;

    if ((git_view_t) data->view != GIT_VIEW_FAVORITES || data->fav_repos == NULL)
        return MC_PPR_NOT_SUPPORTED;

    for (i = 0; i < count; i++)
    {
        const git_entry_info_t *einfo;
        const char *full_path;
        guint j;

        einfo = (const git_entry_info_t *) g_hash_table_lookup (data->display_to_info, names[i]);
        full_path = (einfo != NULL && einfo->full_path != NULL) ? einfo->full_path : names[i];

        for (j = 0; j < data->fav_repos->len; j++)
            if (strcmp ((const char *) data->fav_repos->pdata[j], full_path) == 0)
            {
                g_ptr_array_remove_index (data->fav_repos, j);
                break;
            }
    }

    git_favorites_save (data->fav_repos);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_handle_key (void *plugin_data, int key)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const GString *current = NULL;
    const git_entry_info_t *info = NULL;
    const git_view_t view = (git_view_t) data->view;

    if (data->host != NULL && data->host->get_current != NULL)
        current = data->host->get_current (data->host);
    if (current != NULL && current->str != NULL)
        info = git_lookup_info (data, current->str);

    git_debug_log ("git: key=%d view=%s current='%s'", key, git_view_name (data->view),
                   current != NULL && current->str != NULL ? current->str : "(null)");

    if (key == CK_EditNew && (git_view_t) data->view == GIT_VIEW_FAVORITES)
        return git_create_item (data);

    if (data->repo_root != NULL && data->key_coole != 0 && key == data->key_coole)
        return git_launch_coole (data, GIT_COOLE_WINDOW, NULL);

    if (data->repo_root != NULL && data->key_commit != 0 && key == data->key_commit)
        return git_launch_coole (data, GIT_COOLE_COMMIT, NULL);

    if (data->repo_root != NULL && data->key_amend != 0 && key == data->key_amend)
        return git_launch_coole (data, GIT_COOLE_AMEND, NULL);

    if ((view == GIT_VIEW_COMMITS || view == GIT_VIEW_COMMIT_FILES)
        && ((data->key_show != 0 && key == data->key_show)
            || (data->key_reword != 0 && key == data->key_reword)
            || (view == GIT_VIEW_COMMITS && key == CK_Edit)))
    {
        // in diff selected_commit is the end of the list, not a chosen commit
        const char *sha = view == GIT_VIEW_COMMIT_FILES && data->selected_commit_base == NULL
            ? data->selected_commit
            : view == GIT_VIEW_COMMITS && info != NULL && info->kind == GIT_ITEM_COMMIT_DIR
            ? info->commit_sha
            : NULL;

        return git_launch_coole (data, key == data->key_show ? GIT_COOLE_SHOW : GIT_COOLE_REWORD,
                                 sha);
    }

    if ((data->key_diff != 0 && key == data->key_diff)
        || (data->key_diff_alt != 0 && key == data->key_diff_alt))
    {
        /* Only these two views list files; elsewhere the selection is a commit or a branch. */
        if ((view == GIT_VIEW_STATUS || view == GIT_VIEW_COMMIT_FILES)
            && git_show_diff_selected (data))
            return MC_PPR_OK;
        return MC_PPR_FAILED;
    }

    if (data->key_refresh != 0 && key == data->key_refresh)
        return MC_PPR_OK;

    /* The remaining hotkeys only make sense on the status list.  Report the key as unhandled
       rather than failed: panel_key() retries handle_key() with the raw terminal key only for
       MC_PPR_NOT_SUPPORTED, and plugin hotkeys such as Shift-F3 arrive as a command id first. */
    if ((git_view_t) data->view != GIT_VIEW_STATUS)
        return MC_PPR_NOT_SUPPORTED;

    if (data->key_fav_add != 0 && key == data->key_fav_add && data->repo_root != NULL)
    {
        gboolean added;

        added = git_favorites_add (data->repo_root);
        if (!added && data->host != NULL && data->host->message != NULL)
            data->host->message (data->host, D_NORMAL, "Git panel",
                                 "Repository is already in favorites.");
        else if (added && data->host != NULL && data->host->set_hint != NULL)
            data->host->set_hint (data->host, "Repository added to favorites.");
        return MC_PPR_OK;
    }

    if (data->key_stage != 0 && key == data->key_stage)
    {
        if (git_apply_selected (data, GIT_ACTION_STAGE))
            return MC_PPR_OK;
        return MC_PPR_FAILED;
    }

    if (data->key_unstage != 0 && key == data->key_unstage)
    {
        if (git_apply_selected (data, GIT_ACTION_UNSTAGE))
            return MC_PPR_OK;
        return MC_PPR_FAILED;
    }

    if (data->key_toggle != 0 && key == data->key_toggle)
    {
        if (git_apply_selected (data, GIT_ACTION_TOGGLE))
            return MC_PPR_OK;
        return MC_PPR_FAILED;
    }

    if (data->key_reset != 0 && key == data->key_reset)
    {
        if (git_apply_selected (data, GIT_ACTION_RESET))
            return MC_PPR_OK;
        return MC_PPR_FAILED;
    }

    return MC_PPR_NOT_SUPPORTED;  // not our key
}

/* --------------------------------------------------------------------------------------------- */

static const mc_panel_column_t *
git_get_columns (void *plugin_data, size_t *count)
{
    (void) plugin_data;

    if (count != NULL)
        *count = G_N_ELEMENTS (git_columns);

    return git_columns;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_get_column_value (void *plugin_data, const char *fname, const char *column_id)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const git_entry_info_t *info;

    if (fname == NULL || column_id == NULL)
        return NULL;

    info = (const git_entry_info_t *) g_hash_table_lookup (data->display_to_info, fname);
    if (info == NULL)
        return NULL;

    if (strcmp (column_id, "type") == 0)
        return info->state_mark_text;

    if (strcmp (column_id, "status") == 0 || strcmp (column_id, "git_status") == 0
        || strcmp (column_id, "git_state") == 0 || strcmp (column_id, "git_state_text") == 0)
        return info->state_text;

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_get_footer (void *plugin_data)
{
    git_data_t *data = (git_data_t *) plugin_data;

    if ((git_view_t) data->view == GIT_VIEW_FAVORITES)
        return NULL;

    return data->current_branch;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_get_focus_name (void *plugin_data)
{
    git_data_t *data = (git_data_t *) plugin_data;

    return data->pending_focus;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_get_default_format (void *plugin_data)
{
    git_data_t *data = (git_data_t *) plugin_data;

    return data->default_format;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &git_plugin;
}
