/*
   Panel plugin gh for the M-Commander
   Issues: the list of them, and the files of one

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

#include "lib/global.h"

#include "src/panel-plugins/gh/gh-internal.h"

/*** file scope variables ************************************************************************/

static const mc_panel_column_t gh_issue_columns[] = {
    { "state", "State", 6, FALSE, J_LEFT_FIT, TRUE },
    { "author", "Author", 10, FALSE, J_LEFT_FIT, TRUE },
    { "assignees", "Assignees", 10, FALSE, J_LEFT_FIT, TRUE },
    { "milestone", "Milestone", 10, FALSE, J_LEFT_FIT, TRUE },
    { "labels", "Labels", 10, FALSE, J_LEFT_FIT, TRUE },
};

/* --------------------------------------------------------------------------------------------- */

#define GH_ISSUE_FIELDS                                                                            \
    "number,title,author,state,updatedAt,createdAt,labels,assignees,milestone,url,body"
#define GH_ISSUE_ROW                                                                               \
    "[(.number|tostring),.title,.author.login,.state,.updatedAt,.createdAt,"                       \
    "([.labels[].name]|join(\",\")),([.assignees[].login]|join(\",\")),(.milestone.title//\"\"),"  \
    ".url,(.body//\"\")]|@tsv"
#define GH_ISSUE_ROW_FIELDS 11

/* The comments, oldest first, a row each, as GH_PULL_REPLY_ROW gives them */
#define GH_ISSUE_REPLY_ROW ".comments[]|[.createdAt,.author.login,\"commented\",(.body//\"\")]|@tsv"

/*** file scope functions ************************************************************************/

static gh_item_t *
gh_issue_from_row (const char *line)
{
    char **f;
    guint n;
    gh_item_t *item = NULL;
    int number;

    f = gh_split_row (line, &n);
    number = n == GH_ISSUE_ROW_FIELDS ? atoi (f[0]) : 0;
    if (number > 0)
    {
        char *name;

        name = gh_entry_name (number, f[1]);
        item = gh_item_new (name, GH_ITEM_ISSUE, TRUE);
        g_free (name);
        item->number = number;
        item->title = g_strdup (f[1]);
        item->author = g_strdup (f[2]);
        item->state = g_ascii_strdown (f[3], -1);
        item->updated = gh_parse_time (f[4]);
        item->created = gh_parse_time (f[5]);
        item->labels = g_strdup (f[6]);
        item->branch = g_strdup (f[7]);
        item->review = g_strdup (f[8]);
        item->url = g_strdup (f[9]);
        item->body = g_strdup (f[10]);
    }
    g_strfreev (f);

    return item;
}

/* --------------------------------------------------------------------------------------------- */

/* gh issue view <number> -R <owner>/<repo> --json @fields --jq @jq */
static char *
gh_issue_view (gh_data_t *data, int number, const char *fields, const char *jq, char **err_text)
{
    char *repo;
    char *num;
    char *output = NULL;

    repo = g_strdup_printf ("%s/%s", data->owner, data->repo);
    num = g_strdup_printf ("%d", number);
    {
        const char *const args[] = { "issue",  "view", num,    "-R", repo,
                                     "--json", fields, "--jq", jq,   NULL };

        if (!gh_run (data, NULL, args, FALSE, &output, err_text))
            output = NULL;
    }
    g_free (num);
    g_free (repo);

    return output;
}

/*** public functions ****************************************************************************/

gboolean
gh_issues_reload (gh_data_t *data, char **err_text)
{
    char *repo;
    char *limit;
    char *output = NULL;
    char **lines;
    guint i;
    gboolean ok;

    repo = g_strdup_printf ("%s/%s", data->owner, data->repo);
    limit = g_strdup_printf ("%d", data->limit);
    {
        const char *const args[] = { "issue",   "list",
                                     "-R",      repo,
                                     "--state", gh_state_filter_name (data->issues_state),
                                     "--limit", limit,
                                     "--json",  GH_ISSUE_FIELDS,
                                     "--jq",    ".[]|" GH_ISSUE_ROW,
                                     NULL };

        ok = gh_run (data, NULL, args, FALSE, &output, err_text);
    }
    g_free (limit);
    g_free (repo);
    if (!ok)
        return FALSE;

    data->items = g_ptr_array_new_with_free_func (gh_item_free);
    lines = g_strsplit (output, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        gh_item_t *item;

        if (lines[i][0] == '\0')
            continue;
        item = gh_issue_from_row (lines[i]);
        if (item != NULL)
            g_ptr_array_add (data->items, item);
    }
    g_strfreev (lines);
    g_free (output);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gh_item_t *
gh_issue_fetch (gh_data_t *data, int number, char **err_text)
{
    char *output;
    gh_item_t *item;

    output = gh_issue_view (data, number, GH_ISSUE_FIELDS, GH_ISSUE_ROW, err_text);
    if (output == NULL)
        return NULL;

    g_strchomp (output);
    item = gh_issue_from_row (output);
    g_free (output);
    return item;
}

/* --------------------------------------------------------------------------------------------- */

void
gh_issue_reload (gh_data_t *data)
{
    data->items = g_ptr_array_new_with_free_func (gh_item_free);
    g_ptr_array_add (data->items, gh_item_new (gh_description_file, GH_ITEM_FILE, FALSE));
    g_ptr_array_add (data->items, gh_item_new (gh_conversation_entry, GH_ITEM_SECTION, TRUE));
}

/* --------------------------------------------------------------------------------------------- */

char *
gh_issue_text (gh_data_t *data, const char *fname, char **err_text)
{
    (void) err_text;

    if (strcmp (fname, gh_description_file) == 0)
        return data->topic != NULL ? gh_topic_description (data->topic, FALSE) : NULL;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
gh_issue_replies_reload (gh_data_t *data, char **err_text)
{
    char *output;

    output = gh_issue_view (data, data->number, "comments", GH_ISSUE_REPLY_ROW, err_text);
    if (output == NULL)
        return FALSE;
    gh_replies_fill (data, output);
    g_free (output);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

const mc_panel_column_t *
gh_issues_get_columns (size_t *count)
{
    *count = G_N_ELEMENTS (gh_issue_columns);
    return gh_issue_columns;
}

/* --------------------------------------------------------------------------------------------- */

const char *
gh_issues_get_column_value (const gh_item_t *item, const char *col)
{
    if (strcmp (col, "state") == 0)
        return item->state;
    if (strcmp (col, "author") == 0)
        return item->author;
    if (strcmp (col, "assignees") == 0)
        return item->branch;
    if (strcmp (col, "milestone") == 0)
        return item->review;
    if (strcmp (col, "labels") == 0)
        return item->labels;
    return NULL;
}
