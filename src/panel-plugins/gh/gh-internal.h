/*
   Panel plugin gh for the M-Commander
   Types and functions shared by the files of the plugin

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

#ifndef MC__PANEL_PLUGINS_GH_INTERNAL_H
#define MC__PANEL_PLUGINS_GH_INTERNAL_H

#include <sys/stat.h>
#include <time.h>

#include "lib/global.h"
#include "lib/panel-plugin.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define GH_PANEL_CONFIG_FILE         "panels.gh.ini"
#define GH_PANEL_CONFIG_GROUP        "gh-panel"
#define GH_FAVORITES_CONFIG_GROUP    "gh-favorites"
#define GH_FAVORITES_CONFIG_KEY      "locations"
#define GH_CLONES_CONFIG_GROUP       "gh-clones"
#define GH_PANEL_CONFIG_GH           "gh"
#define GH_PANEL_CONFIG_LIMIT        "limit"
#define GH_PANEL_KEY_STATE           "hotkey_state"
#define GH_PANEL_KEY_WEB             "hotkey_web"
#define GH_PANEL_KEY_FAV_ADD         "hotkey_fav_add"
#define GH_PANEL_KEY_GIT             "hotkey_git"
#define GH_PANEL_KEY_DIFF            "hotkey_diff"
#define GH_PANEL_KEY_STATE_DEFAULT   "alt-f"
#define GH_PANEL_KEY_WEB_DEFAULT     "alt-w"
#define GH_PANEL_KEY_FAV_ADD_DEFAULT "ctrl-b"
#define GH_PANEL_KEY_GIT_DEFAULT     "alt-g"
#define GH_PANEL_KEY_DIFF_DEFAULT    "shift-f3"
#define GH_LIST_LIMIT_DEFAULT        100

/*** enums ***************************************************************************************/

typedef enum
{
    GH_VIEW_ROOT = 0,  /* gh:/                      the owners */
    GH_VIEW_OWNER,     /* gh:/<owner>               its repositories */
    GH_VIEW_REPO,      /* gh:/<owner>/<repo>        pulls, issues, repo.md */
    GH_VIEW_PULLS,     /* .../pulls                 the pull requests */
    GH_VIEW_PULL,      /* .../pulls/<number>        the files of one */
    GH_VIEW_ISSUES,    /* .../issues                the issues */
    GH_VIEW_ISSUE,     /* .../issues/<number>       the files of one */
    GH_VIEW_COMMITS,   /* .../pulls/<number>/commits       the commits of a pull request */
    GH_VIEW_FILES,     /* .../pulls/<number>/files         the files it changes */
    GH_VIEW_PULL_CONV, /* .../pulls/<number>/conversation  its comments and reviews */
    GH_VIEW_ISSUE_CONV /* .../issues/<number>/conversation the comments of an issue */
} gh_view_t;

typedef enum
{
    GH_ITEM_OWNER = 0,
    GH_ITEM_REPO,
    GH_ITEM_SECTION, /* pulls/ and issues/ of a repository */
    GH_ITEM_PULL,
    GH_ITEM_ISSUE,
    GH_ITEM_FILE,    /* a text the plugin makes or fetches */
    GH_ITEM_COMMIT,  /* a commit of a pull request; url holds its hash */
    GH_ITEM_REPLY,   /* a comment or a review; title says which */
    GH_ITEM_CHANGE,  /* a file a pull request changes; url holds its old name */
    GH_ITEM_FAVORITE /* a place below an owner the user saved; url holds it */
} gh_item_kind_t;

/* Which pull requests or issues a list shows; issues have no MERGED */
typedef enum
{
    GH_STATE_OPEN = 0,
    GH_STATE_CLOSED,
    GH_STATE_MERGED,
    GH_STATE_ALL
} gh_state_filter_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    char *name; /* what the panel shows; unique in a list */
    gh_item_kind_t kind;
    gboolean is_dir;
    int number;   /* pulls, issues */
    char *title;  /* pulls, issues; repos: the name */
    char *author; /* pulls, issues */
    char *state;  /* pulls, issues: open, draft, merged, closed; repos: visibility;
                     owners: why the owner is listed */
    char *labels; /* pulls, issues; repos: fork, archived */
    char *branch; /* pulls: head -> base; issues: assignees; repos: stars */
    char *review; /* pulls: the review decision; issues: the milestone */
    char *url;    /* pulls, issues */
    char *body;   /* pulls, issues: the description; repos: the description */
    time_t created;
    time_t updated;
} gh_item_t;

typedef struct
{
    mc_panel_host_t *host;
    gh_view_t view;
    char *owner;
    char *repo;
    int number;       /* the pull request or issue whose directory is open */
    gh_item_t *topic; /* that pull request or issue; NULL until it is fetched */

    char *here_owner; /* the repository of the directory the panel came from */
    char *here_repo;
    char *here_dir;
    char *start_dir; /* the directory the panel was opened from */

    gh_state_filter_t pulls_state;
    gh_state_filter_t issues_state;

    GPtrArray *items;       /* gh_item_t*, the current view; NULL: to be listed */
    GHashTable *item_index; /* item->name -> gh_item_t* (borrowed) */
    gpointer index_for;     /* the items array item_index was built for */
    GHashTable *texts;      /* file name -> fetched text, for the open topic */
    GPtrArray *favorites;   /* char*, places the user saved: owner[/repo[/...]] */

    char *pending_focus;
    char *title_buf;
    char *footer_buf;
    char *help_filename;
    char *gh_cmd;
    int limit;
    int key_state;
    int key_web;
    int key_fav_add;
    int key_git;
    int key_diff;

    /* files/ of the open pull request */
    char *base_sha;           /* where its branch left the base */
    char *head_sha;           /* its head */
    gboolean files_truncated; /* GitHub listed no more than it can */
    GHashTable *blobs;        /* "<sha>:<path>" -> local copy of that version */
} gh_data_t;

/*** global variables defined in .c file *********************************************************/

extern const char gh_pulls_entry[];
extern const char gh_issues_entry[];
extern const char gh_repo_file[];
extern const char gh_description_file[];
extern const char gh_conversation_entry[];
extern const char gh_files_entry[];
extern const char gh_diff_file[];
extern const char gh_commits_entry[];
extern const char gh_checks_file[];

/*** declarations of public functions ************************************************************/

/* gh.c */
void gh_item_free (gpointer p);
gh_item_t *gh_item_new (const char *name, gh_item_kind_t kind, gboolean is_dir);
void gh_clear_items (gh_data_t *data);
const gh_item_t *gh_find_item (gh_data_t *data, const char *name);
void gh_error (const gh_data_t *data, const char *text);

/* gh-cmd.c */
gboolean gh_run (const gh_data_t *data, const char *cwd, const char *const *args,
                 gboolean any_status, char **output, char **err_text);
gboolean gh_run_to_fd (const gh_data_t *data, const char *const *args, int fd);
char *gh_run_line (const gh_data_t *data, const char *cwd, const char *const *args);
char **gh_split_row (const char *line, guint *count);
time_t gh_parse_time (const char *iso);
char *gh_format_time (time_t t);
char *gh_entry_label (const char *head, const char *title);
char *gh_entry_name (int number, const char *title);
char *gh_read_config_string (const char *key, const char *fallback);
int gh_read_config_int (const char *key, int fallback);
int gh_load_hotkey (const char *key, const char *fallback_text, int fallback_key);
char *gh_normalize_location (const char *text);
GPtrArray *gh_favorites_load (void);
void gh_favorites_save (GPtrArray *favs);
char *gh_clone_load (const char *owner, const char *repo);
void gh_clone_save (const char *owner, const char *repo, const char *dir);
char *gh_git_toplevel (const char *dir);
const char *gh_state_filter_name (gh_state_filter_t state);
gboolean gh_open_web (const gh_data_t *data, const char *const *args);

/* repos.c */
gboolean gh_owners_reload (gh_data_t *data, char **err_text);
gboolean gh_repos_reload (gh_data_t *data, char **err_text);
void gh_repo_reload (gh_data_t *data);
char *gh_repo_summary (const gh_item_t *item, const char *owner);
const mc_panel_column_t *gh_owners_get_columns (size_t *count);
const mc_panel_column_t *gh_repos_get_columns (size_t *count);
const char *gh_repos_get_column_value (const gh_item_t *item, const char *col);
gboolean gh_repo_of_dir (const gh_data_t *data, const char *dir, char **owner, char **repo);
gboolean gh_detect_repo (gh_data_t *data, const char *dir);

/* pulls.c */
gboolean gh_pulls_reload (gh_data_t *data, char **err_text);
gh_item_t *gh_pull_fetch (gh_data_t *data, int number, char **err_text);
void gh_pull_reload (gh_data_t *data);
char *gh_pull_text (gh_data_t *data, const char *fname, char **err_text);
gboolean gh_commits_reload (gh_data_t *data, char **err_text);
void gh_replies_fill (gh_data_t *data, const char *output);
gboolean gh_pull_replies_reload (gh_data_t *data, char **err_text);
char *gh_reply_text (const gh_item_t *item);
char *gh_commit_text (gh_data_t *data, const gh_item_t *item, char **err_text);
const mc_panel_column_t *gh_commits_get_columns (size_t *count);
const mc_panel_column_t *gh_pulls_get_columns (size_t *count);
const char *gh_pulls_get_column_value (const gh_item_t *item, const char *col);

/* issues.c */
gboolean gh_issues_reload (gh_data_t *data, char **err_text);
gh_item_t *gh_issue_fetch (gh_data_t *data, int number, char **err_text);
void gh_issue_reload (gh_data_t *data);
gboolean gh_issue_replies_reload (gh_data_t *data, char **err_text);
char *gh_issue_text (gh_data_t *data, const char *fname, char **err_text);
const mc_panel_column_t *gh_issues_get_columns (size_t *count);
const char *gh_issues_get_column_value (const gh_item_t *item, const char *col);

/* files.c */
void gh_blob_free (gpointer p);
gboolean gh_files_reload (gh_data_t *data, char **err_text);
char *gh_files_patch_text (const gh_item_t *item);
mc_pp_result_t gh_files_diff (gh_data_t *data, const gh_item_t *item);
mc_pp_result_t gh_files_local_copy (gh_data_t *data, const gh_item_t *item, char **local_path);
const mc_panel_column_t *gh_files_get_columns (size_t *count);
const char *gh_files_get_column_value (const gh_item_t *item, const char *col);

/* shared by pulls.c and issues.c */
char *gh_topic_description (const gh_item_t *item, gboolean is_pull);

#endif /* MC__PANEL_PLUGINS_GH_INTERNAL_H */
