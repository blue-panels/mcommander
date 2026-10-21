/*
   Panel plugin gh for the M-Commander
   Pull requests: the list of them, and the files of one

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

static const mc_panel_column_t gh_commit_columns[] = {
    { "author", "Author", 10, FALSE, J_LEFT_FIT, TRUE },
};

static const mc_panel_column_t gh_pull_columns[] = {
    { "state", "State", 6, FALSE, J_LEFT_FIT, TRUE },
    { "author", "Author", 10, FALSE, J_LEFT_FIT, TRUE },
    { "review", "Review", 8, FALSE, J_LEFT_FIT, TRUE },
    { "branch", "Branch", 14, FALSE, J_LEFT_FIT, TRUE },
    { "labels", "Labels", 10, FALSE, J_LEFT_FIT, TRUE },
};

/* --------------------------------------------------------------------------------------------- */

/* One list request carries everything Quick View and description.md show,
   so moving the cursor costs no request. */
#define GH_PULL_FIELDS                                                                             \
    "number,title,author,state,isDraft,updatedAt,createdAt,headRefName,baseRefName,"               \
    "reviewDecision,labels,url,body"
#define GH_PULL_ROW                                                                                \
    "[(.number|tostring),.title,.author.login,.state,(.isDraft|tostring),.updatedAt,.createdAt,"   \
    ".headRefName,.baseRefName,(.reviewDecision//\"\"),([.labels[].name]|join(\",\")),.url,"       \
    "(.body//\"\")]|@tsv"
#define GH_PULL_ROW_FIELDS 13

/* The comments and the reviews, oldest first, a row each */
#define GH_PULL_REPLY_ROW                                                                          \
    "[(.comments[]|{at:.createdAt,who:.author.login,what:\"commented\",body}),"                    \
    "(.reviews[]|{at:.submittedAt,who:.author.login,what:(.state|ascii_downcase),body})]"          \
    "|sort_by(.at)[]|[.at,.who,.what,(.body//\"\")]|@tsv"

#define GH_PULL_COMMIT_ROW                                                                         \
    ".commits[]|[.oid,.messageHeadline,(.authors[0].login // .authors[0].name),.authoredDate]"     \
    "|@tsv"

/*** file scope functions ************************************************************************/

static const char *
gh_pull_review_name (const char *decision)
{
    if (strcmp (decision, "APPROVED") == 0)
        return "approved";
    if (strcmp (decision, "CHANGES_REQUESTED") == 0)
        return "changes";
    if (strcmp (decision, "REVIEW_REQUIRED") == 0)
        return "required";
    return "";
}

/* --------------------------------------------------------------------------------------------- */

static gh_item_t *
gh_pull_from_row (const char *line)
{
    char **f;
    guint n;
    gh_item_t *item = NULL;
    int number;

    f = gh_split_row (line, &n);
    number = n == GH_PULL_ROW_FIELDS ? atoi (f[0]) : 0;
    if (number > 0)
    {
        char *name;

        name = gh_entry_name (number, f[1]);
        item = gh_item_new (name, GH_ITEM_PULL, TRUE);
        g_free (name);
        item->number = number;
        item->title = g_strdup (f[1]);
        item->author = g_strdup (f[2]);
        if (strcmp (f[3], "OPEN") == 0 && strcmp (f[4], "true") == 0)
            item->state = g_strdup ("draft");
        else
            item->state = g_ascii_strdown (f[3], -1);
        item->updated = gh_parse_time (f[5]);
        item->created = gh_parse_time (f[6]);
        item->branch = g_strdup_printf ("%s -> %s", f[7], f[8]);
        item->review = g_strdup (gh_pull_review_name (f[9]));
        item->labels = g_strdup (f[10]);
        item->url = g_strdup (f[11]);
        item->body = g_strdup (f[12]);
    }
    g_strfreev (f);

    return item;
}

/* --------------------------------------------------------------------------------------------- */

/* gh pr view <number> -R <owner>/<repo> --json @fields --jq @jq */
static char *
gh_pull_view (gh_data_t *data, int number, const char *fields, const char *jq, char **err_text)
{
    char *repo;
    char *num;
    char *output = NULL;

    repo = g_strdup_printf ("%s/%s", data->owner, data->repo);
    num = g_strdup_printf ("%d", number);
    {
        const char *const args[] = { "pr",     "view", num,    "-R", repo,
                                     "--json", fields, "--jq", jq,   NULL };

        if (!gh_run (data, NULL, args, FALSE, &output, err_text))
            output = NULL;
    }
    g_free (num);
    g_free (repo);

    return output;
}

/*** public functions ****************************************************************************/

/* The description of a pull request or an issue, from what its list row holds. */
char *
gh_topic_description (const gh_item_t *item, gboolean is_pull)
{
    GString *s;
    char *created;
    char *updated;

    created = gh_format_time (item->created);
    updated = gh_format_time (item->updated);

    s = g_string_new (NULL);
    g_string_append_printf (s, "# %s (#%d)\n\n", item->title != NULL ? item->title : "",
                            item->number);
    g_string_append_printf (s, "State:   %s\n", item->state != NULL ? item->state : "");
    g_string_append_printf (s, "Author:  %s\n", item->author != NULL ? item->author : "");
    if (is_pull)
    {
        g_string_append_printf (s, "Branch:  %s\n", item->branch != NULL ? item->branch : "");
        if (item->review != NULL && item->review[0] != '\0')
            g_string_append_printf (s, "Review:  %s\n", item->review);
    }
    else
    {
        if (item->branch != NULL && item->branch[0] != '\0')
            g_string_append_printf (s, "Assigned: %s\n", item->branch);
        if (item->review != NULL && item->review[0] != '\0')
            g_string_append_printf (s, "Milestone: %s\n", item->review);
    }
    if (item->labels != NULL && item->labels[0] != '\0')
        g_string_append_printf (s, "Labels:  %s\n", item->labels);
    g_string_append_printf (s, "Created: %s\n", created);
    g_string_append_printf (s, "Updated: %s\n", updated);
    g_string_append_printf (s, "URL:     %s\n", item->url != NULL ? item->url : "");
    g_string_append (s, "\n---\n\n");
    if (item->body != NULL && item->body[0] != '\0')
        g_string_append_printf (s, "%s\n", item->body);
    else
        g_string_append (s, "No description provided.\n");

    g_free (created);
    g_free (updated);
    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
gh_pulls_reload (gh_data_t *data, char **err_text)
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
        const char *const args[] = { "pr",      "list",
                                     "-R",      repo,
                                     "--state", gh_state_filter_name (data->pulls_state),
                                     "--limit", limit,
                                     "--json",  GH_PULL_FIELDS,
                                     "--jq",    ".[]|" GH_PULL_ROW,
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
        item = gh_pull_from_row (lines[i]);
        if (item != NULL)
            g_ptr_array_add (data->items, item);
    }
    g_strfreev (lines);
    g_free (output);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* A pull request opened by its address, with no list to take it from. */
gh_item_t *
gh_pull_fetch (gh_data_t *data, int number, char **err_text)
{
    char *output;
    gh_item_t *item;

    output = gh_pull_view (data, number, GH_PULL_FIELDS, GH_PULL_ROW, err_text);
    if (output == NULL)
        return NULL;

    g_strchomp (output);
    item = gh_pull_from_row (output);
    g_free (output);
    return item;
}

/* --------------------------------------------------------------------------------------------- */

void
gh_pull_reload (gh_data_t *data)
{
    static const char *const files[] = {
        gh_description_file,
        gh_diff_file,
        gh_checks_file,
    };
    size_t i;

    data->items = g_ptr_array_new_with_free_func (gh_item_free);
    g_ptr_array_add (data->items, gh_item_new (gh_files_entry, GH_ITEM_SECTION, TRUE));
    g_ptr_array_add (data->items, gh_item_new (gh_commits_entry, GH_ITEM_SECTION, TRUE));
    g_ptr_array_add (data->items, gh_item_new (gh_conversation_entry, GH_ITEM_SECTION, TRUE));
    for (i = 0; i < G_N_ELEMENTS (files); i++)
        g_ptr_array_add (data->items, gh_item_new (files[i], GH_ITEM_FILE, FALSE));
}

/* --------------------------------------------------------------------------------------------- */

/* commits/ of the open pull request: "<hash> <headline>", by the time it was
   written, so the newest is on top as in the list of pull requests. */
gboolean
gh_commits_reload (gh_data_t *data, char **err_text)
{
    char *output;
    char **lines;
    guint i;

    output = gh_pull_view (data, data->number, "commits", GH_PULL_COMMIT_ROW, err_text);
    if (output == NULL)
        return FALSE;

    data->items = g_ptr_array_new_with_free_func (gh_item_free);
    lines = g_strsplit (output, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        char **f;
        guint n;

        f = gh_split_row (lines[i], &n);
        if (n == 4 && strlen (f[0]) >= 10)
        {
            char head[11];
            char *name;
            gh_item_t *item;

            g_strlcpy (head, f[0], sizeof (head));
            name = gh_entry_label (head, f[1]);
            item = gh_item_new (name, GH_ITEM_COMMIT, FALSE);
            g_free (name);
            item->url = g_strdup (f[0]);
            item->title = g_strdup (f[1]);
            item->author = g_strdup (f[2]);
            item->updated = gh_parse_time (f[3]);
            g_ptr_array_add (data->items, item);
        }
        g_strfreev (f);
    }
    g_strfreev (lines);
    g_free (output);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The commit as a patch: its message and what it changes. GitHub has the
   commits of a pull request from a fork too. */
char *
gh_commit_text (gh_data_t *data, const gh_item_t *item, char **err_text)
{
    char *path;
    char *output = NULL;

    path = g_strdup_printf ("repos/%s/%s/commits/%s", data->owner, data->repo, item->url);
    {
        const char *const args[] = { "api", "-H", "Accept: application/vnd.github.patch", path,
                                     NULL };

        (void) gh_run (data, NULL, args, FALSE, &output, err_text);
    }
    g_free (path);

    return output;
}

/* --------------------------------------------------------------------------------------------- */

/* conversation/ from the rows of GH_PULL_REPLY_ROW or GH_ISSUE_REPLY_ROW, oldest
   first: "001 zyv commented.md". The rows carry the texts, so no reply asks
   GitHub again. */
void
gh_replies_fill (gh_data_t *data, const char *output)
{
    char **lines;
    guint i;
    guint count = 0;

    data->items = g_ptr_array_new_with_free_func (gh_item_free);
    lines = g_strsplit (output, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        char **f;
        guint n;

        f = gh_split_row (lines[i], &n);
        if (n == 4 && f[0][0] != '\0')
        {
            char head[16];
            char *who_what;
            char *label;
            char *name;
            gh_item_t *item;

            g_snprintf (head, sizeof (head), "%03u", ++count);
            who_what = g_strdup_printf ("%s %s", f[1][0] != '\0' ? f[1] : "ghost", f[2]);
            label = gh_entry_label (head, who_what);
            name = g_strdup_printf ("%s.md", label);
            item = gh_item_new (name, GH_ITEM_REPLY, FALSE);
            item->updated = gh_parse_time (f[0]);
            item->author = g_strdup (f[1]);
            item->title = g_strdup (f[2]);
            item->body = g_strdup (f[3]);
            g_ptr_array_add (data->items, item);
            g_free (name);
            g_free (label);
            g_free (who_what);
        }
        g_strfreev (f);
    }
    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
gh_pull_replies_reload (gh_data_t *data, char **err_text)
{
    char *output;

    output = gh_pull_view (data, data->number, "comments,reviews", GH_PULL_REPLY_ROW, err_text);
    if (output == NULL)
        return FALSE;
    gh_replies_fill (data, output);
    g_free (output);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

char *
gh_reply_text (const gh_item_t *item)
{
    char *when;
    char *text;

    when = gh_format_time (item->updated);
    /* an approval often says nothing but that */
    text =
        g_strdup_printf ("### %s %s, %s\n\n%s\n", item->author != NULL ? item->author : "",
                         item->title != NULL ? item->title : "", when,
                         item->body != NULL && item->body[0] != '\0' ? item->body : "*No text.*");
    g_free (when);
    return text;
}

/* --------------------------------------------------------------------------------------------- */

const mc_panel_column_t *
gh_commits_get_columns (size_t *count)
{
    *count = G_N_ELEMENTS (gh_commit_columns);
    return gh_commit_columns;
}

/* --------------------------------------------------------------------------------------------- */

/* The text of one file of the open pull request; NULL with @err_text set
   when gh cannot give it. */
char *
gh_pull_text (gh_data_t *data, const char *fname, char **err_text)
{
    char *repo;
    char *number;
    char *output = NULL;

    if (strcmp (fname, gh_description_file) == 0)
        return data->topic != NULL ? gh_topic_description (data->topic, TRUE) : NULL;

    repo = g_strdup_printf ("%s/%s", data->owner, data->repo);
    number = g_strdup_printf ("%d", data->number);
    if (strcmp (fname, gh_diff_file) == 0)
    {
        const char *const args[] = { "pr", "diff", number, "-R", repo, NULL };

        (void) gh_run (data, NULL, args, FALSE, &output, err_text);
    }
    else if (strcmp (fname, gh_checks_file) == 0)
    {
        const char *const args[] = { "pr", "checks", number, "-R", repo, NULL };

        /* a failed or a running check makes it exit with an error, and so
           does a pull request with no checks at all */
        gboolean ok;

        ok = gh_run (data, NULL, args, TRUE, &output, err_text);
        if (!ok && *err_text != NULL && strstr (*err_text, "no checks") != NULL)
            ok = TRUE;
        if (ok)
        {
            g_clear_pointer (err_text, g_free);
            if (output == NULL || output[0] == '\0')
            {
                g_free (output);
                output = g_strdup ("No checks.\n");
            }
        }
    }
    g_free (number);
    g_free (repo);

    return output;
}

/* --------------------------------------------------------------------------------------------- */

const mc_panel_column_t *
gh_pulls_get_columns (size_t *count)
{
    *count = G_N_ELEMENTS (gh_pull_columns);
    return gh_pull_columns;
}

/* --------------------------------------------------------------------------------------------- */

const char *
gh_pulls_get_column_value (const gh_item_t *item, const char *col)
{
    if (strcmp (col, "state") == 0)
        return item->state;
    if (strcmp (col, "author") == 0)
        return item->author;
    if (strcmp (col, "review") == 0)
        return item->review;
    if (strcmp (col, "branch") == 0)
        return item->branch;
    if (strcmp (col, "labels") == 0)
        return item->labels;
    return NULL;
}
