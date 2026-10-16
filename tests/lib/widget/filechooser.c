/*
   lib/widget - tests for the directory list of the file chooser

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

#define TEST_SUITE_NAME "/lib/widget"

#include "tests/mctest.h"

#include <glib/gstdio.h>  // g_mkdir(), g_remove(), g_rmdir()
#include <unistd.h>

#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "src/vfs/local/local.c"

#include "lib/widget/filechooser.h"

/* --------------------------------------------------------------------------------------------- */

static char *dir;

/* --------------------------------------------------------------------------------------------- */

static void
put_file (const char *name)
{
    char *path = g_build_filename (dir, name, (char *) NULL);

    ck_assert (g_file_set_contents (path, "x", -1, NULL));
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

static void
put_dir (const char *name)
{
    char *path = g_build_filename (dir, name, (char *) NULL);

    ck_assert_int_eq (g_mkdir (path, 0700), 0);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    // the order of Cyrillic names should not hang on the locale of the test
    str_init_strings ("UTF-8");
    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    dir = g_dir_make_tmp ("mc-filechooser-XXXXXX", NULL);
    ck_assert_ptr_nonnull (dir);
    put_file ("b.txt");
    put_file ("A.txt");
    put_file (".hidden");
    put_dir ("zdir");
    put_dir ("Adir");
    put_file ("\xd0\xaf\xd1\x89\xd0\xb8\xd0\xba");  // upper case Ya, before "a" in bytes
    put_file ("\xd0\xb0\xd1\x80\xd0\xb1\xd1\x83\xd0\xb7");
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    static const char *names[] = {
        "b.txt",
        "A.txt",
        ".hidden",
        "zdir",
        "Adir",
        "\xd0\xaf\xd1\x89\xd0\xb8\xd0\xba",
        "\xd0\xb0\xd1\x80\xd0\xb1\xd1\x83\xd0\xb7",
    };
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (names); i++)
    {
        char *path = g_build_filename (dir, names[i], (char *) NULL);

        (void) g_remove (path);
        g_free (path);
    }
    (void) g_rmdir (dir);
    g_free (dir);

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

static char *
names_of_keep (gboolean show_hidden, gboolean dirs_only, const char *keep)
{
    vfs_path_t *vpath = vfs_path_from_str (dir);
    GPtrArray *entries = file_chooser_read_dir (vpath, show_hidden, dirs_only, keep);
    GString *out = g_string_new (NULL);
    guint i;

    ck_assert_ptr_nonnull (entries);
    for (i = 0; i < entries->len; i++)
    {
        const file_chooser_entry_t *e = g_ptr_array_index (entries, i);

        if (i != 0)
            g_string_append_c (out, ' ');
        g_string_append (out, e->name);
        if (e->is_dir)
            g_string_append_c (out, '/');
    }

    g_ptr_array_free (entries, TRUE);
    vfs_path_free (vpath, TRUE);
    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static char *
names_of (gboolean show_hidden, gboolean dirs_only)
{
    return names_of_keep (show_hidden, dirs_only, NULL);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_order)
{
    char *got;

    /* directories first, then by name whatever the case, Cyrillic too */
    got = names_of (FALSE, FALSE);
    ck_assert_str_eq (got,
                      "Adir/ zdir/ A.txt b.txt \xd0\xb0\xd1\x80\xd0\xb1\xd1\x83\xd0\xb7 "
                      "\xd0\xaf\xd1\x89\xd0\xb8\xd0\xba");
    g_free (got);

    /* the names that start with a dot come first, as in a panel */
    got = names_of (TRUE, FALSE);
    ck_assert_str_eq (got,
                      "Adir/ zdir/ .hidden A.txt b.txt "
                      "\xd0\xb0\xd1\x80\xd0\xb1\xd1\x83\xd0\xb7 "
                      "\xd0\xaf\xd1\x89\xd0\xb8\xd0\xba");
    g_free (got);

    got = names_of (FALSE, TRUE);
    ck_assert_str_eq (got, "Adir/ zdir/");
    g_free (got);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keep_hidden)
{
    char *got;

    /* the file the dialog starts on is listed though hidden */
    got = names_of_keep (FALSE, FALSE, ".hidden");
    ck_assert_str_eq (got,
                      "Adir/ zdir/ .hidden A.txt b.txt "
                      "\xd0\xb0\xd1\x80\xd0\xb1\xd1\x83\xd0\xb7 "
                      "\xd0\xaf\xd1\x89\xd0\xb8\xd0\xba");
    g_free (got);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_parent_of_file)
{
    /* the dialog goes up and to the directory of a file by appending "..": the path is made
       canonical, so the name and ".." go away */
    vfs_path_t *file = vfs_path_from_str ("/a/b/file.txt");
    vfs_path_t *parent = vfs_path_append_new (file, "..", (char *) NULL);

    ck_assert_str_eq (vfs_path_as_str (parent), "/a/b");
    vfs_path_free (parent, TRUE);
    vfs_path_free (file, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_missing_dir)
{
    vfs_path_t *vpath = vfs_path_from_str ("/nonexistent/mc-filechooser");

    ck_assert_ptr_null (file_chooser_read_dir (vpath, TRUE, FALSE, NULL));
    vfs_path_free (vpath, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_order);
    tcase_add_test (tc_core, test_keep_hidden);
    tcase_add_test (tc_core, test_parent_of_file);
    tcase_add_test (tc_core, test_missing_dir);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
