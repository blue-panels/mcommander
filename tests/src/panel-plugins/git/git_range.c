/*
   src/panel-plugins/git - unit tests for the base of a branch and the range of its commits

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

#define TEST_SUITE_NAME "/src/panel-plugins/git"

#include "tests/mctest.h"

#include <stdarg.h>

#include "src/panel-plugins/git/git.c"

/* --------------------------------------------------------------------------------------------- */

static char *repo = NULL;
static git_data_t data;

/* --------------------------------------------------------------------------------------------- */

/* the listing and the viewer are the core's; nothing here lists or views */

gboolean
dir_list_append (dir_list *list, const char *fname, const struct stat *st, gboolean link_to_dir,
                 gboolean stale_link)
{
    (void) list;
    (void) fname;
    (void) st;
    (void) link_to_dir;
    (void) stale_link;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_viewer (const char *command, const vfs_path_t *file_vpath, int start_line,
               off_t search_start, off_t search_end)
{
    (void) command;
    (void) file_vpath;
    (void) start_line;
    (void) search_start;
    (void) search_end;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* git in the test repository; the output is returned with the trailing newline cut */

static char *run (const char *fmt, ...) G_GNUC_PRINTF (1, 2);

static char *
run (const char *fmt, ...)
{
    va_list ap;
    char *cmd, *line, *out = NULL;
    int status = -1;

    va_start (ap, fmt);
    cmd = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    line = g_strdup_printf ("git -C %s %s", repo, cmd);
    if (!g_spawn_command_line_sync (line, &out, NULL, &status, NULL) || status != 0)
        ck_abort_msg ("failed: %s", line);
    g_strchomp (out);
    g_free (line);
    g_free (cmd);
    return out;
}

/* --------------------------------------------------------------------------------------------- */

static void
add_origin (void)
{
    g_free (run ("config remote.origin.url /nonexistent"));
    g_free (run ("config remote.origin.fetch +refs/heads/*:refs/remotes/origin/*"));
}

/* --------------------------------------------------------------------------------------------- */

static void
commit (const char *file, const char *text)
{
    char *path;

    path = g_build_filename (repo, file, (char *) NULL);
    g_file_set_contents (path, text, -1, NULL);
    g_free (path);
    g_free (run ("add %s", file));
    g_free (run ("commit -q -m %s", file));
}

/* --------------------------------------------------------------------------------------------- */

/* the number of the listed commits; FROM gets where their changes start */

static int
listed (const char *base, const char *ref, char **from)
{
    char *log = NULL;
    int n = 0;
    const char *p;

    mctest_assert_true (git_run_branch_log (&data, base, ref, &log, from));
    for (p = log; *p != '\0'; p++)
        if (*p == '\n')
            n++;
    if (*log != '\0')
        n++;
    g_free (log);
    return n;
}

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    GError *error = NULL;

    // nothing of the user's git setup: no hooks, no signing, no default branch name
    g_setenv ("GIT_CONFIG_NOSYSTEM", "1", TRUE);
    g_setenv ("GIT_CONFIG_GLOBAL", "/dev/null", TRUE);
    g_setenv ("GIT_AUTHOR_NAME", "t", TRUE);
    g_setenv ("GIT_AUTHOR_EMAIL", "t@t", TRUE);
    g_setenv ("GIT_COMMITTER_NAME", "t", TRUE);
    g_setenv ("GIT_COMMITTER_EMAIL", "t@t", TRUE);

    repo = g_dir_make_tmp ("mc-git-range-XXXXXX", &error);
    ck_assert_ptr_nonnull (repo);
    memset (&data, 0, sizeof (data));
    data.repo_root = repo;

    g_free (run ("init -q"));
    g_free (run ("symbolic-ref HEAD refs/heads/master"));  // init -b is git 2.28
    commit ("base", "base\n");
    g_free (run ("checkout -q -b feat"));
    commit ("one", "one\n");
    commit ("two", "two\n");
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown (void)
{
    char *cmd;

    cmd = g_strdup_printf ("rm -rf %s", repo);
    (void) g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);
    g_free (cmd);
    g_free (repo);
    repo = NULL;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_branch_against_master)
{
    char *base, *from, *fork;

    base = git_branch_base (&data, NULL);
    ck_assert_str_eq (base, "master");
    fork = run ("rev-parse master");
    ck_assert_int_eq (listed (base, "HEAD", &from), 2);
    ck_assert_str_eq (from, fork);
    g_free (fork);
    g_free (from);
    g_free (base);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_master_has_no_base)
{
    char *from;

    g_free (run ("checkout -q master"));
    ck_assert_ptr_null (git_branch_base (&data, NULL));
    ck_assert_ptr_null (git_branch_base (&data, "master"));
    // the whole log: no range to diff
    ck_assert_int_eq (listed (NULL, "HEAD", &from), 1);
    ck_assert_ptr_null (from);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_pushed_branch_is_not_its_own_base)
{
    char *base;

    g_free (run ("update-ref refs/remotes/origin/feat feat"));
    add_origin ();
    g_free (run ("config branch.feat.remote origin"));
    g_free (run ("config branch.feat.merge refs/heads/feat"));
    base = run ("rev-parse --abbrev-ref @{upstream}");
    ck_assert_str_eq (base, "origin/feat");
    g_free (base);

    base = git_branch_base (&data, NULL);
    ck_assert_str_eq (base, "master");
    g_free (base);
    base = git_branch_base (&data, "feat");
    ck_assert_str_eq (base, "master");
    g_free (base);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_branch_pushed_under_another_path)
{
    char *base;

    g_free (run ("branch -q -m feat team/feat"));
    g_free (run ("update-ref refs/remotes/origin/feat team/feat"));
    add_origin ();
    g_free (run ("config branch.team/feat.remote origin"));
    g_free (run ("config branch.team/feat.merge refs/heads/feat"));
    base = run ("rev-parse --abbrev-ref @{upstream}");
    ck_assert_str_eq (base, "origin/feat");
    g_free (base);

    base = git_branch_base (&data, NULL);
    ck_assert_str_eq (base, "master");
    g_free (base);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_branch_tracking_another_branch)
{
    char *base;

    g_free (run ("update-ref refs/remotes/origin/develop master"));
    add_origin ();
    g_free (run ("config branch.feat.remote origin"));
    g_free (run ("config branch.feat.merge refs/heads/develop"));

    base = git_branch_base (&data, NULL);
    ck_assert_str_eq (base, "origin/develop");
    g_free (base);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_merged_branch)
{
    char *from, *fork;

    fork = run ("rev-parse master");
    g_free (run ("checkout -q master"));
    g_free (run ("merge -q --no-ff -m merge feat"));

    // what the merge brought in, and the changes from where the branch started
    ck_assert_int_eq (listed ("master", "feat", &from), 2);
    ck_assert_str_eq (from, fork);
    g_free (from);
    g_free (fork);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_squashed_branch)
{
    char *from, *fork;

    fork = run ("rev-parse master");
    g_free (run ("checkout -q master"));
    g_free (run ("merge -q --squash feat"));
    g_free (run ("commit -q -m squash"));

    // the commits of the branch are not in master: they still are the branch
    ck_assert_int_eq (listed ("master", "feat", &from), 2);
    ck_assert_str_eq (from, fork);
    g_free (from);
    g_free (fork);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_divergent_branches)
{
    char *from, *fork;

    fork = run ("rev-parse master");
    g_free (run ("checkout -q master"));
    commit ("later", "later\n");

    // the changes start where the branch left master, not at the end of master
    ck_assert_int_eq (listed ("master", "feat", &from), 2);
    ck_assert_str_eq (from, fork);
    g_free (from);
    g_free (fork);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;
    char *version = NULL;

    if (!g_spawn_command_line_sync ("git --version", &version, NULL, NULL, NULL))
        return 77;  // no git: skipped
    g_free (version);

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_branch_against_master);
    tcase_add_test (tc_core, test_master_has_no_base);
    tcase_add_test (tc_core, test_pushed_branch_is_not_its_own_base);
    tcase_add_test (tc_core, test_branch_pushed_under_another_path);
    tcase_add_test (tc_core, test_branch_tracking_another_branch);
    tcase_add_test (tc_core, test_merged_branch);
    tcase_add_test (tc_core, test_squashed_branch);
    tcase_add_test (tc_core, test_divergent_branches);

    return mctest_run_all (tc_core);
}
