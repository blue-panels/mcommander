/*
   Tests for the M-Commander
   The search engine of the duplicates panel plugin

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

#define TEST_SUITE_NAME "/src/panel-plugins/duplicates"

#include "tests/mctest.h"

#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <glib/gstdio.h>  // g_mkdir

#include "lib/strutil.h"
#include "lib/util.h"  // MC_PTR_FREE

#ifdef DUP_HAVE_JPEG
#include <jpeglib.h>
#endif
#ifdef DUP_HAVE_PNG
#include <png.h>
#endif

#include "src/panel-plugins/duplicates/dup_engine.h"
#include "src/panel-plugins/duplicates/dup_filter.h"

/* --------------------------------------------------------------------------------------------- */

#define BIG (3 * DUPLICATES_PARTIAL_SIZE)

static char *tmp_dir = NULL;

/* --------------------------------------------------------------------------------------------- */

static void
rm_rf (const char *path)
{
    GDir *dir;
    const char *name;

    dir = g_dir_open (path, 0, NULL);
    if (dir != NULL)
    {
        while ((name = g_dir_read_name (dir)) != NULL)
        {
            char *p;

            p = g_build_filename (path, name, (char *) NULL);
            (void) chmod (p, 0700);
            if (g_file_test (p, G_FILE_TEST_IS_DIR) && !g_file_test (p, G_FILE_TEST_IS_SYMLINK))
                rm_rf (p);
            else
                (void) unlink (p);
            g_free (p);
        }
        g_dir_close (dir);
    }
    (void) rmdir (path);
}

/* --------------------------------------------------------------------------------------------- */

static void
my_setup (void)
{
    tmp_dir = g_dir_make_tmp ("mc-duplicates-XXXXXX", NULL);
    ck_assert_ptr_nonnull (tmp_dir);
    str_init_strings (NULL);  // the name masks report their errors through it
}

/* --------------------------------------------------------------------------------------------- */

static void
my_teardown (void)
{
    str_uninit_strings ();
    rm_rf (tmp_dir);
    MC_PTR_FREE (tmp_dir);
}

/* --------------------------------------------------------------------------------------------- */

static char *
path_of (const char *name)
{
    return g_build_filename (tmp_dir, name, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* A file of `size` bytes of `fill`, with byte `at` (if >= 0) set to `mark`. */
static void
make_file (const char *name, size_t size, char fill, gssize at, char mark)
{
    char *path, *dir, *buf;

    path = path_of (name);
    dir = g_path_get_dirname (path);
    (void) g_mkdir_with_parents (dir, 0700);

    buf = g_malloc (size + 1);
    memset (buf, fill, size);
    if (at >= 0)
        buf[at] = mark;
    ck_assert (g_file_set_contents (path, buf, (gssize) size, NULL));

    g_free (buf);
    g_free (dir);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
add (duplicates_t *d, const char *name)
{
    struct stat st;
    char *path;
    gboolean taken;

    path = path_of (name);
    ck_assert_int_eq (lstat (path, &st), 0);
    taken = duplicates_add (d, path, &st);
    g_free (path);
    return taken;
}

/* --------------------------------------------------------------------------------------------- */

static duplicates_t *
run (duplicates_criteria_t criteria, const char *const *names)
{
    duplicates_t *d;

    d = duplicates_new (criteria, 0, 0);
    for (; *names != NULL; names++)
        (void) add (d, *names);
    duplicates_start (d);
    ck_assert_int_eq (duplicates_wait (d), DUPLICATES_STAGE_DONE);
    return d;
}

/* --------------------------------------------------------------------------------------------- */

static const duplicates_group_t *
group_at (const duplicates_t *d, guint i)
{
    return (const duplicates_group_t *) g_ptr_array_index (duplicates_get_groups (d), i);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
name_at (const duplicates_t *d, guint group, guint file)
{
    const duplicates_file_t *f;

    f = (const duplicates_file_t *) g_ptr_array_index (group_at (d, group)->files, file);
    return f->path + strlen (tmp_dir) + 1;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_by_size)
{
    static const char *const names[] = { "a", "b", "c", "d", NULL };
    duplicates_t *d;

    make_file ("a", 10, 'x', -1, 0);
    make_file ("b", 10, 'y', -1, 0);
    make_file ("c", 10, 'x', -1, 0);
    make_file ("d", 11, 'x', -1, 0);

    d = run (DUPLICATES_BY_SIZE, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_uint_eq (group_at (d, 0)->files->len, 3);
    ck_assert_int_eq (group_at (d, 0)->size, 10);
    ck_assert_str_eq (name_at (d, 0, 0), "a");
    ck_assert_str_eq (name_at (d, 0, 1), "b");
    ck_assert_str_eq (name_at (d, 0, 2), "c");
    ck_assert_uint_eq (duplicates_files_opened (d), 0);
    ck_assert_uint_eq (duplicates_reclaimable (d), 20);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_by_content)
{
    static const char *const names[] = { "a", "b", "c", "d", NULL };
    duplicates_t *d;

    make_file ("a", 10, 'x', -1, 0);
    make_file ("b", 10, 'y', -1, 0);
    make_file ("c", 10, 'x', -1, 0);
    make_file ("d", 11, 'x', -1, 0);

    d = run (DUPLICATES_BY_CONTENT, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_uint_eq (group_at (d, 0)->files->len, 2);
    ck_assert_str_eq (name_at (d, 0, 0), "a");
    ck_assert_str_eq (name_at (d, 0, 1), "c");
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_by_name_case)
{
    static const char *const names[] = { "1/Foo.txt", "2/foo.TXT", "3/foo.txt", "4/bar", NULL };
    duplicates_t *d;

    make_file ("1/Foo.txt", 1, 'x', -1, 0);
    make_file ("2/foo.TXT", 2, 'x', -1, 0);
    make_file ("3/foo.txt", 3, 'x', -1, 0);
    make_file ("4/bar", 3, 'x', -1, 0);

    d = run (DUPLICATES_BY_NAME, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_uint_eq (group_at (d, 0)->files->len, 3);
    ck_assert_int_eq (group_at (d, 0)->size, 0);
    /* sizes 1, 2, 3: keeping the largest frees 1 + 2 */
    ck_assert_uint_eq (duplicates_reclaimable (d), 3);
    duplicates_free (d);

    d = run (DUPLICATES_BY_NAME | DUPLICATES_NAME_CASE_SENS, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 0);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_by_name_and_size)
{
    static const char *const names[] = { "1/f", "2/f", "3/f", "4/g", NULL };
    duplicates_t *d;

    make_file ("1/f", 5, 'x', -1, 0);
    make_file ("2/f", 5, 'y', -1, 0);
    make_file ("3/f", 6, 'x', -1, 0);
    make_file ("4/g", 5, 'x', -1, 0);

    d = run (DUPLICATES_BY_NAME | DUPLICATES_BY_SIZE, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_str_eq (name_at (d, 0, 0), "1/f");
    ck_assert_str_eq (name_at (d, 0, 1), "2/f");
    duplicates_free (d);

    d = run (DUPLICATES_BY_NAME | DUPLICATES_BY_CONTENT, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 0);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_big_files)
{
    static const char *const names[] = { "same1", "same2", "last", "head", NULL };
    duplicates_t *d;

    make_file ("same1", BIG, 'x', -1, 0);
    make_file ("same2", BIG, 'x', -1, 0);
    make_file ("last", BIG, 'x', BIG - 1, 'z');
    make_file ("head", BIG, 'x', 10, 'z');

    d = run (DUPLICATES_BY_CONTENT, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_uint_eq (group_at (d, 0)->files->len, 2);
    ck_assert_str_eq (name_at (d, 0, 0), "same1");
    ck_assert_str_eq (name_at (d, 0, 1), "same2");
    ck_assert_uint_eq (duplicates_reclaimable (d), BIG);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_hard_links)
{
    duplicates_t *d;
    char *a, *b;

    make_file ("a", 10, 'x', -1, 0);
    make_file ("c", 10, 'x', -1, 0);
    a = path_of ("a");
    b = path_of ("b");
    ck_assert_int_eq (link (a, b), 0);

    d = duplicates_new (DUPLICATES_BY_CONTENT, 0, 0);
    ck_assert (add (d, "a"));
    ck_assert (!add (d, "b"));
    ck_assert (add (d, "c"));
    duplicates_start (d);
    ck_assert_int_eq (duplicates_wait (d), DUPLICATES_STAGE_DONE);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_uint_eq (group_at (d, 0)->files->len, 2);
    ck_assert_str_eq (name_at (d, 0, 0), "a");
    ck_assert_str_eq (name_at (d, 0, 1), "c");
    duplicates_free (d);

    g_free (a);
    g_free (b);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_empty_and_special)
{
    duplicates_t *d;
    char *dir;

    make_file ("e1", 0, 'x', -1, 0);
    make_file ("e2", 0, 'x', -1, 0);
    dir = path_of ("sub");
    ck_assert_int_eq (g_mkdir (dir, 0700), 0);

    d = duplicates_new (DUPLICATES_BY_SIZE, 0, 0);
    ck_assert (!add (d, "e1"));
    ck_assert (!add (d, "e2"));
    ck_assert (!add (d, "sub"));
    duplicates_start (d);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 0);
    duplicates_free (d);

    g_free (dir);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_unreadable)
{
    static const char *const names[] = { "a", "b", "c", NULL };
    duplicates_progress_t progress = { 0 };
    duplicates_t *d;
    char *c;

    if (geteuid () == 0)
        return;  // root reads anything

    make_file ("a", 10, 'x', -1, 0);
    make_file ("b", 10, 'x', -1, 0);
    make_file ("c", 10, 'x', -1, 0);
    c = path_of ("c");
    ck_assert_int_eq (chmod (c, 0), 0);

    d = run (DUPLICATES_BY_CONTENT, names);
    (void) duplicates_poll (d, &progress, 0);
    ck_assert_uint_eq (progress.errors, 1);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_uint_eq (group_at (d, 0)->files->len, 2);
    ck_assert_str_eq (name_at (d, 0, 0), "a");
    ck_assert_str_eq (name_at (d, 0, 1), "b");
    g_free (progress.current);
    duplicates_free (d);

    g_free (c);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_unique_size_not_opened)
{
    static const char *const names[] = { "a", "b", "c", "d", "e", NULL };
    duplicates_t *d;

    make_file ("a", 1, 'x', -1, 0);
    make_file ("b", 2, 'x', -1, 0);
    make_file ("c", 3, 'x', -1, 0);
    make_file ("d", 4, 'x', -1, 0);
    make_file ("e", 4, 'y', -1, 0);

    d = run (DUPLICATES_BY_CONTENT, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 0);
    ck_assert_uint_eq (duplicates_files_opened (d), 2);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_order)
{
    static const char *const names[] = { "s1", "s2", "m1", "m2", "m3", "l1", "l2", NULL };
    duplicates_t *d;

    make_file ("s1", 10, 'x', -1, 0);
    make_file ("s2", 10, 'x', -1, 0);
    make_file ("m1", 20, 'x', -1, 0);
    make_file ("m2", 20, 'x', -1, 0);
    make_file ("m3", 20, 'x', -1, 0);
    make_file ("l1", 30, 'x', -1, 0);
    make_file ("l2", 30, 'x', -1, 0);

    /* reclaimable: m 40, l 30, s 10 */
    d = run (DUPLICATES_BY_CONTENT, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 3);
    ck_assert_str_eq (name_at (d, 0, 0), "m1");
    ck_assert_str_eq (name_at (d, 1, 0), "l1");
    ck_assert_str_eq (name_at (d, 2, 0), "s1");
    ck_assert_uint_eq (duplicates_files_count (d), 7);
    ck_assert_uint_eq (duplicates_reclaimable (d), 80);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_size_limits)
{
    duplicates_t *d;

    make_file ("a", 10, 'x', -1, 0);
    make_file ("b", 20, 'x', -1, 0);
    make_file ("c", 30, 'x', -1, 0);

    d = duplicates_new (DUPLICATES_BY_SIZE, 15, 25);
    ck_assert (!add (d, "a"));
    ck_assert (add (d, "b"));
    ck_assert (!add (d, "c"));
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    duplicates_stage_t stage;
    const char *name;
} cancel_at_t;

static void
cancel_hook (duplicates_t *d, duplicates_stage_t stage, const char *path, gpointer user_data)
{
    const cancel_at_t *at = (const cancel_at_t *) user_data;

    if (stage == at->stage && g_str_has_suffix (path, at->name))
        duplicates_cancel (d);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cancel_collect)
{
    duplicates_t *d;

    make_file ("a", 10, 'x', -1, 0);

    d = duplicates_new (DUPLICATES_BY_CONTENT, 0, 0);
    (void) add (d, "a");
    duplicates_cancel (d);
    ck_assert_int_eq (duplicates_poll (d, NULL, 0), DUPLICATES_STAGE_CANCELLED);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 0);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cancel_partial)
{
    static const cancel_at_t at = { DUPLICATES_STAGE_PARTIAL, "/b2" };
    duplicates_t *d;

    /* group a reads less, and is done before b is started */
    make_file ("a1", 10, 'x', -1, 0);
    make_file ("a2", 10, 'x', -1, 0);
    make_file ("b1", 20, 'x', -1, 0);
    make_file ("b2", 20, 'x', -1, 0);

    d = duplicates_new (DUPLICATES_BY_CONTENT, 0, 0);
    (void) add (d, "a1");
    (void) add (d, "a2");
    (void) add (d, "b1");
    (void) add (d, "b2");
    duplicates_set_file_hook (d, cancel_hook, (gpointer) &at);
    duplicates_start (d);
    ck_assert_int_eq (duplicates_wait (d), DUPLICATES_STAGE_CANCELLED);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_str_eq (name_at (d, 0, 0), "a1");
    ck_assert_uint_eq (duplicates_files_opened (d), 3);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_small_groups_first)
{
    static const cancel_at_t at = { DUPLICATES_STAGE_PARTIAL, "/big1" };
    duplicates_t *d;

    make_file ("big1", BIG, 'x', -1, 0);
    make_file ("big2", BIG, 'x', -1, 0);
    make_file ("small1", 10, 'x', -1, 0);
    make_file ("small2", 10, 'x', -1, 0);

    d = duplicates_new (DUPLICATES_BY_CONTENT, 0, 0);
    (void) add (d, "big1");
    (void) add (d, "big2");
    (void) add (d, "small1");
    (void) add (d, "small2");
    duplicates_set_file_hook (d, cancel_hook, (gpointer) &at);
    duplicates_start (d);
    ck_assert_int_eq (duplicates_wait (d), DUPLICATES_STAGE_CANCELLED);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_str_eq (name_at (d, 0, 0), "small1");
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cancel_full)
{
    static const cancel_at_t at = { DUPLICATES_STAGE_FULL, "/b1" };
    duplicates_t *d;

    make_file ("a1", BIG, 'x', -1, 0);
    make_file ("a2", BIG, 'x', -1, 0);
    make_file ("b1", BIG + 1, 'x', -1, 0);
    make_file ("b2", BIG + 1, 'x', -1, 0);

    d = duplicates_new (DUPLICATES_BY_CONTENT, 0, 0);
    (void) add (d, "a1");
    (void) add (d, "a2");
    (void) add (d, "b1");
    (void) add (d, "b2");
    duplicates_set_file_hook (d, cancel_hook, (gpointer) &at);
    duplicates_start (d);
    ck_assert_int_eq (duplicates_wait (d), DUPLICATES_STAGE_CANCELLED);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_str_eq (name_at (d, 0, 0), "a1");
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_free_while_running)
{
    static const char *const names[] = { "a", "b", NULL };
    duplicates_t *d;
    const char *const *n;

    make_file ("a", BIG, 'x', -1, 0);
    make_file ("b", BIG, 'x', -1, 0);

    d = duplicates_new (DUPLICATES_BY_CONTENT, 0, 0);
    for (n = names; *n != NULL; n++)
        (void) add (d, *n);
    duplicates_start (d);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_progress)
{
    static const char *const names[] = { "a", "b", "c", NULL };
    duplicates_progress_t progress = { 0 };
    duplicates_t *d;

    make_file ("a", BIG, 'x', -1, 0);
    make_file ("b", BIG, 'x', -1, 0);
    make_file ("c", 5, 'x', -1, 0);

    d = run (DUPLICATES_BY_CONTENT, names);
    ck_assert_int_eq (duplicates_poll (d, &progress, 0), DUPLICATES_STAGE_DONE);
    ck_assert_uint_eq (progress.files_total, 2);
    ck_assert_uint_eq (progress.files_done, 2);
    // the partial hash, then the whole file
    ck_assert_uint_eq (progress.bytes_total, 2 * (DUPLICATES_PARTIAL_SIZE + BIG));
    ck_assert_uint_eq (progress.bytes_done, 2 * (DUPLICATES_PARTIAL_SIZE + BIG));
    ck_assert_uint_eq (progress.errors, 0);
    ck_assert_ptr_null (progress.current);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Files that differ only at the end are told apart by the partial hash. */
START_TEST (test_tail_differs)
{
    static const char *const names[] = { "a", "b", NULL };
    duplicates_t *d;

    make_file ("a", BIG, 'x', BIG - 1, 'a');
    make_file ("b", BIG, 'x', BIG - 1, 'b');

    d = run (DUPLICATES_BY_CONTENT, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 0);
    ck_assert_uint_eq (duplicates_files_opened (d), 2);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static duplicates_t *
run_cached (dup_cache_t *cache, const char *const *names)
{
    duplicates_t *d;

    d = duplicates_new (DUPLICATES_BY_CONTENT, 0, 0);
    duplicates_set_cache (d, cache);
    for (; *names != NULL; names++)
        (void) add (d, *names);
    duplicates_start (d);
    ck_assert_int_eq (duplicates_wait (d), DUPLICATES_STAGE_DONE);
    return d;
}

/* --------------------------------------------------------------------------------------------- */

/* A second search with the cache of the first reads no file. */
START_TEST (test_cache_second_search)
{
    static const char *const names[] = { "a", "b", "c", "d", NULL };
    dup_cache_t *cache;
    duplicates_t *d;
    char *cache_path;

    make_file ("a", BIG, 'x', -1, 0);
    make_file ("b", BIG, 'x', -1, 0);
    make_file ("c", BIG, 'x', 0, 'c');
    make_file ("d", 10, 'x', -1, 0);
    cache_path = path_of ("hashes.cache");

    cache = dup_cache_load (cache_path, time (NULL));
    d = run_cached (cache, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_uint_eq (duplicates_files_opened (d), 5);  // 3 partial, 2 full
    duplicates_free (d);
    ck_assert (dup_cache_save (cache));
    dup_cache_free (cache);

    cache = dup_cache_load (cache_path, time (NULL));
    ck_assert_uint_eq (dup_cache_size (cache), 3);
    d = run_cached (cache, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_str_eq (name_at (d, 0, 0), "a");
    ck_assert_str_eq (name_at (d, 0, 1), "b");
    ck_assert_uint_eq (duplicates_files_opened (d), 0);
    duplicates_free (d);
    dup_cache_free (cache);

    g_free (cache_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A file written again is hashed again: its entry is for the old one. */
START_TEST (test_cache_changed_file)
{
    static const char *const names[] = { "a", "b", NULL };
    dup_cache_t *cache;
    duplicates_t *d;

    make_file ("a", BIG, 'x', -1, 0);
    make_file ("b", BIG, 'x', -1, 0);

    cache = dup_cache_load (NULL, time (NULL));
    d = run_cached (cache, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    duplicates_free (d);

    make_file ("b", BIG, 'x', BIG / 2, 'b');

    d = run_cached (cache, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 0);
    ck_assert_uint_eq (duplicates_files_opened (d), 2);  // b, partial and full
    duplicates_free (d);
    dup_cache_free (cache);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A file written between the walk and the hashing is not taken by the hash the cache has for
   the old one. */
START_TEST (test_cache_changed_after_walk)
{
    dup_cache_t *cache;
    duplicates_t *d;

    make_file ("a", BIG, 'x', -1, 0);
    make_file ("b", BIG, 'x', -1, 0);

    cache = dup_cache_load (NULL, time (NULL));
    d = run_cached (cache, (const char *const[]) { "a", "b", NULL });
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    duplicates_free (d);

    d = duplicates_new (DUPLICATES_BY_CONTENT, 0, 0);
    duplicates_set_cache (d, cache);
    (void) add (d, "a");
    (void) add (d, "b");
    sleep (1);  // a new mtime even where it has whole seconds only
    make_file ("b", BIG, 'x', BIG / 2, 'b');
    duplicates_start (d);
    ck_assert_int_eq (duplicates_wait (d), DUPLICATES_STAGE_DONE);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 0);
    duplicates_free (d);
    dup_cache_free (cache);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A stop is seen also when every hash comes from the cache: the worker reads no file then. */
START_TEST (test_cancel_with_cached_hashes)
{
    static const cancel_at_t at = { DUPLICATES_STAGE_PARTIAL, "/a1" };
    static const char *const names[] = { "a1", "a2", "b1", "b2", NULL };
    dup_cache_t *cache;
    duplicates_t *d;
    const char *const *n;

    make_file ("a1", 10, 'x', -1, 0);
    make_file ("a2", 10, 'x', -1, 0);
    make_file ("b1", 20, 'x', -1, 0);
    make_file ("b2", 20, 'x', -1, 0);

    cache = dup_cache_load (NULL, time (NULL));
    duplicates_free (run_cached (cache, names));

    d = duplicates_new (DUPLICATES_BY_CONTENT, 0, 0);
    duplicates_set_cache (d, cache);
    for (n = names; *n != NULL; n++)
        (void) add (d, *n);
    duplicates_set_file_hook (d, cancel_hook, (gpointer) &at);
    duplicates_start (d);
    ck_assert_int_eq (duplicates_wait (d), DUPLICATES_STAGE_CANCELLED);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 0);
    ck_assert_uint_eq (duplicates_files_opened (d), 0);

    duplicates_free (d);
    dup_cache_free (cache);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static void
make_text (const char *name, const char *text)
{
    char *path;

    path = path_of (name);
    ck_assert (g_file_set_contents (path, text, -1, NULL));
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

static duplicates_t *
run_similar (dup_similar_method_t method, int percent, const char *const *names)
{
    duplicates_t *d;

    d = duplicates_new (DUPLICATES_BY_CONTENT, 0, 0);
    duplicates_set_similar (d, method, percent);
    for (; *names != NULL; names++)
        (void) add (d, *names);
    duplicates_start (d);
    ck_assert_int_eq (duplicates_wait (d), DUPLICATES_STAGE_DONE);
    return d;
}

/* --------------------------------------------------------------------------------------------- */

/* A stop is seen between the blocks of a text, not at its end */
START_TEST (test_text_hash_cancel)
{
    char *path, *digest;
    guint64 bytes;
    gint cancel = 0;
    GString *text;
    int i;

    text = g_string_new (NULL);
    for (i = 0; i < 100000; i++)
        g_string_append (text, "a line of text\n");
    path = path_of ("big.txt");
    ck_assert (g_file_set_contents (path, text->str, (gssize) text->len, NULL));

    digest = dup_text_hash (path, &bytes, &cancel);
    ck_assert_ptr_nonnull (digest);
    ck_assert_uint_eq (bytes, text->len);
    g_free (digest);

    cancel = 1;
    ck_assert_ptr_null (dup_text_hash (path, &bytes, &cancel));
    ck_assert_uint_lt (bytes, text->len);

    g_string_free (text, TRUE);
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_similar_text)
{
    static const char *const names[] = {
        "a.txt", "b.txt", "c.txt", "d.txt", "e.bin", "f.bin", NULL
    };
    duplicates_t *d;

    make_text ("a.txt", "one\ntwo\n");
    make_text ("b.txt", "one  \r\ntwo\t\r\n\r\n");  // CRLF, spaces at the ends, a blank line
    make_text ("c.txt", "one\ntwo");                // no line end at the end
    make_text ("d.txt", "one\nthree\n");
    make_file ("e.bin", 10, '\0', 1, 'x');
    make_file ("f.bin", 10, '\0', 1, 'x');

    d = run_similar (DUP_SIMILAR_TEXT, 90, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 2);
    ck_assert_uint_eq (group_at (d, 0)->files->len, 3);
    ck_assert_str_eq (name_at (d, 0, 0), "a.txt");
    ck_assert_str_eq (name_at (d, 0, 1), "b.txt");
    ck_assert_str_eq (name_at (d, 0, 2), "c.txt");
    // the files that are not text, byte by byte
    ck_assert_str_eq (name_at (d, 1, 0), "e.bin");
    ck_assert_str_eq (name_at (d, 1, 1), "f.bin");
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* the pictures are made only where the test that reads them is built */
#if defined(DUP_HAVE_PNG) && defined(DUP_HAVE_JPEG)
/* A picture with a shape that does not depend on its size: a diagonal gradient and a disc. */
static unsigned char
pattern (int x, int y, int w, int h, gboolean other)
{
    const double fx = (double) x / w, fy = (double) y / h;
    const double dx = fx - (other ? 0.25 : 0.6), dy = fy - (other ? 0.7 : 0.4);
    int v;

    v = (int) (200 * (other ? fy : fx));
    if (dx * dx + dy * dy < 0.04)
        v = other ? 20 : 250;
    return (unsigned char) CLAMP (v, 0, 255);
}

/* --------------------------------------------------------------------------------------------- */

static void
make_png (const char *name, int w, int h, gboolean other)
{
    png_image image;
    unsigned char *buf;
    char *path;
    int x, y;

    buf = g_malloc ((size_t) w * (size_t) h);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            buf[y * w + x] = pattern (x, y, w, h, other);

    memset (&image, 0, sizeof (image));
    image.version = PNG_IMAGE_VERSION;
    image.width = (png_uint_32) w;
    image.height = (png_uint_32) h;
    image.format = PNG_FORMAT_GRAY;
    path = path_of (name);
    ck_assert (png_image_write_to_file (&image, path, 0, buf, 0, NULL));
    g_free (path);
    g_free (buf);
}

/* --------------------------------------------------------------------------------------------- */

static void
make_jpeg (const char *name, int w, int h, int quality, gboolean other)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    unsigned char *row;
    FILE *f;
    char *path;
    int x;

    path = path_of (name);
    f = fopen (path, "wb");
    ck_assert_ptr_nonnull (f);

    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_compress (&cinfo);
    jpeg_stdio_dest (&cinfo, f);
    cinfo.image_width = (JDIMENSION) w;
    cinfo.image_height = (JDIMENSION) h;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;
    jpeg_set_defaults (&cinfo);
    jpeg_set_quality (&cinfo, quality, TRUE);
    jpeg_start_compress (&cinfo, TRUE);

    row = g_malloc ((size_t) w);
    while (cinfo.next_scanline < cinfo.image_height)
    {
        for (x = 0; x < w; x++)
            row[x] = pattern (x, (int) cinfo.next_scanline, w, h, other);
        jpeg_write_scanlines (&cinfo, &row, 1);
    }
    jpeg_finish_compress (&cinfo);
    jpeg_destroy_compress (&cinfo);
    fclose (f);
    g_free (row);
    g_free (path);
}
#endif

/* --------------------------------------------------------------------------------------------- */

#if defined(DUP_HAVE_PNG) && defined(DUP_HAVE_JPEG)
/* The same picture, smaller, and saved again as JPEG at a low quality, is found; another
   picture is not. */
START_TEST (test_similar_images)
{
    static const char *const names[] = { "big.png", "small.png", "low.jpg", "other.png", NULL };
    duplicates_t *d;
    const duplicates_group_t *g;

    make_png ("big.png", 640, 480, FALSE);
    make_png ("small.png", 160, 120, FALSE);
    make_jpeg ("low.jpg", 400, 300, 30, FALSE);
    make_png ("other.png", 640, 480, TRUE);

    d = run_similar (DUP_SIMILAR_IMAGES, 90, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    g = group_at (d, 0);
    ck_assert_uint_eq (g->files->len, 3);
    ck_assert_str_eq (name_at (d, 0, 0), "big.png");
    ck_assert_int_eq (((const duplicates_file_t *) g_ptr_array_index (g->files, 0))->similarity,
                      100);
    ck_assert_int_ge (((const duplicates_file_t *) g_ptr_array_index (g->files, 1))->similarity,
                      90);
    duplicates_free (d);
}
END_TEST
#endif

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_phash_distance)
{
    ck_assert_int_eq (dup_phash_distance (0, 0), 0);
    ck_assert_int_eq (dup_phash_distance (0, G_MAXUINT64), 64);
    ck_assert_int_eq (dup_phash_similarity (0, 0), 100);
    ck_assert_int_eq (dup_phash_max_distance (100), 0);
    ck_assert_int_eq (dup_phash_max_distance (90), 6);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A file that cannot be read in the middle of a group: the hashes after it are freed too. */
START_TEST (test_unreadable_in_the_middle)
{
    static const char *const names[] = { "a", "b", "c", NULL };
    duplicates_t *d;
    char *b;

    if (geteuid () == 0)
        return;  // root reads anything

    make_file ("a", 10, 'x', -1, 0);
    make_file ("b", 10, 'x', -1, 0);
    make_file ("c", 10, 'x', -1, 0);
    b = path_of ("b");
    ck_assert_int_eq (chmod (b, 0), 0);

    d = run (DUPLICATES_BY_CONTENT, names);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    ck_assert_str_eq (name_at (d, 0, 0), "a");
    ck_assert_str_eq (name_at (d, 0, 1), "c");
    duplicates_free (d);
    g_free (b);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The files no other one matches by its head are not read on: the total says so. */
START_TEST (test_progress_heads_differ)
{
    static const char *const names[] = { "a", "b", NULL };
    duplicates_progress_t progress = { 0 };
    duplicates_t *d;

    make_file ("a", BIG, 'x', 0, 'a');
    make_file ("b", BIG, 'x', 0, 'b');

    d = run (DUPLICATES_BY_CONTENT, names);
    ck_assert_int_eq (duplicates_poll (d, &progress, 0), DUPLICATES_STAGE_DONE);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 0);
    ck_assert_uint_eq (progress.bytes_done, 2 * DUPLICATES_PARTIAL_SIZE);
    ck_assert_uint_eq (progress.bytes_total, progress.bytes_done);
    ck_assert_uint_eq (progress.files_done, progress.files_total);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_progress_unreadable)
{
    static const char *const names[] = { "a", "b", "c", NULL };
    duplicates_progress_t progress = { 0 };
    duplicates_t *d;
    char *c;

    if (geteuid () == 0)
        return;  // root reads anything

    make_file ("a", BIG, 'x', -1, 0);
    make_file ("b", BIG, 'x', -1, 0);
    make_file ("c", BIG, 'x', -1, 0);
    c = path_of ("c");
    ck_assert_int_eq (chmod (c, 0), 0);

    d = run (DUPLICATES_BY_CONTENT, names);
    ck_assert_int_eq (duplicates_poll (d, &progress, 0), DUPLICATES_STAGE_DONE);
    ck_assert_uint_eq (progress.errors, 1);
    ck_assert_uint_eq (progress.bytes_done, 2 * (DUPLICATES_PARTIAL_SIZE + BIG));
    ck_assert_uint_eq (progress.bytes_total, progress.bytes_done);
    duplicates_free (d);
    g_free (c);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static GMutex stuck_lock;
static GCond stuck_cond;
static gboolean stuck_in_read = FALSE;
static gboolean stuck_released = FALSE;

/* the worker before its first read, held as if the file were on a dead mount */
static void
stuck_hook (duplicates_t *d, duplicates_stage_t stage, const char *path, gpointer user_data)
{
    (void) d;
    (void) stage;
    (void) path;
    (void) user_data;

    g_mutex_lock (&stuck_lock);
    stuck_in_read = TRUE;
    g_cond_broadcast (&stuck_cond);
    while (!stuck_released)
        g_cond_wait (&stuck_cond, &stuck_lock);
    g_mutex_unlock (&stuck_lock);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_free_does_not_wait_for_the_worker)
{
    static const char *const names[] = { "a", "b", NULL };
    duplicates_t *d;
    dup_cache_t *cache;
    const char *const *n;
    gint64 deadline;

    make_file ("a", BIG, 'x', -1, 0);
    make_file ("b", BIG, 'x', -1, 0);

    d = duplicates_new (DUPLICATES_BY_CONTENT, 0, 0);
    cache = dup_cache_load (NULL, time (NULL));
    duplicates_set_cache (d, cache);
    duplicates_set_file_hook (d, stuck_hook, NULL);
    for (n = names; *n != NULL; n++)
        (void) add (d, *n);
    duplicates_start (d);

    g_mutex_lock (&stuck_lock);
    while (!stuck_in_read)
        g_cond_wait (&stuck_cond, &stuck_lock);
    g_mutex_unlock (&stuck_lock);

    /* returns while the worker is still held; the worker keeps the cache it writes to */
    duplicates_free (d);
    dup_cache_free (cache);

    g_mutex_lock (&stuck_lock);
    ck_assert (!stuck_released);
    stuck_released = TRUE;
    g_cond_broadcast (&stuck_cond);
    g_mutex_unlock (&stuck_lock);

    /* the worker frees the rest on its own; give it the time before the files go */
    deadline = g_get_monotonic_time () + G_USEC_PER_SEC / 5;
    while (g_get_monotonic_time () < deadline)
        g_usleep (10000);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Masks split by ';' and ',', but not inside braces, brackets or after a backslash */
START_TEST (test_name_masks)
{
    static const struct
    {
        const char *masks;
        const char *name;
        gboolean match;
    } cases[] = {
        { "*.jpg;*.png", "x.png", TRUE },
        { "*.jpg, *.png", "x.png", TRUE },
        { "*.jpg,*.png", "x.gif", FALSE },
        { "*.{jpg,png}", "x.jpg", TRUE },
        { "[a,b].txt", "a.txt", TRUE },
        { "[a,b].txt", ",.txt", TRUE },
        { "[a,b].txt", "c.txt", FALSE },
        { "[]a].txt", "].txt", TRUE },
        { "[[:alpha:],].txt", "a.txt", TRUE },
        { "[[:alpha:],].txt", ",.txt", TRUE },
        { "[[:alpha:],].txt", "1.txt", FALSE },
        { "[\\],].txt", "].txt", TRUE },
        { "", "anything", TRUE },
    };
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
        GError *error = NULL;
        dup_name_filter_t *filter;

        filter = dup_name_filter_new (cases[i].masks, TRUE, TRUE, &error);
        ck_assert_msg (filter != NULL, "%s: %s", cases[i].masks,
                       error != NULL ? error->message : "");
        ck_assert_msg (dup_name_filter_match (filter, cases[i].name) == cases[i].match, "%s ~ %s",
                       cases[i].masks, cases[i].name);
        dup_name_filter_free (filter);
    }

    // a malformed expression is an error, not a crash
    {
        GError *error = NULL;

        ck_assert_ptr_null (dup_name_filter_new ("(", FALSE, TRUE, &error));
        ck_assert_ptr_nonnull (error);
        g_error_free (error);
    }
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_dir_ignored)
{
    char **abs = g_strsplit ("/tmp/cache/", ":", -1);
    char **rel = g_strsplit (".git/:node_modules", ":", -1);

    ck_assert (dup_dir_ignored (abs, "/tmp/cache", "cache"));
    ck_assert (!dup_dir_ignored (abs, "/tmp/cache2", "cache2"));
    ck_assert (!dup_dir_ignored (abs, "/tmp", "tmp"));
    ck_assert (dup_dir_ignored (rel, "/src/.git", ".git"));
    ck_assert (dup_dir_ignored (rel, "/src/node_modules", "node_modules"));
    ck_assert (!dup_dir_ignored (rel, "/src/.github", ".github"));
    g_strfreev (rel);
    g_strfreev (abs);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A file that starts with text and has a NUL further on is not text: a TAR archive */
START_TEST (test_file_kind_probe)
{
    unsigned char head[DUP_KIND_PROBE];

    memset (head, 'a', sizeof (head));
    ck_assert_int_eq (dup_file_kind (head, sizeof (head)), DUP_KIND_TEXT);
    head[100] = '\0';
    ck_assert_int_eq (dup_file_kind (head, sizeof (head)), DUP_KIND_OTHER);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_parse_size)
{
    static const struct
    {
        const char *text;
        gboolean ok;
        off_t size;
    } cases[] = {
        { "", TRUE, 0 },
        { "  ", TRUE, 0 },
        { "0", TRUE, 0 },
        { "123", TRUE, 123 },
        { " 10k ", TRUE, 10240 },
        { "10K", TRUE, 10240 },
        { "10KiB", TRUE, 10240 },
        { "10KB", TRUE, 10000 },
        { "1500K", TRUE, 1536000 },
        { "2G", TRUE, 2147483648LL },
        { "1T", TRUE, 1099511627776LL },
        { "-1", FALSE, 0 },
        { "k", FALSE, 0 },
        { "1.5M", FALSE, 0 },
        { "10 K", FALSE, 0 },
        { "12b", FALSE, 0 },
        { "10x2", FALSE, 0 },
        { "1e3", FALSE, 0 },
        { "0x10", FALSE, 0 },
        { "inf", FALSE, 0 },
        { "99999999999999999999", FALSE, 0 },
        { "9E", FALSE, 0 },
    };
    size_t i;

    ck_assert (dup_parse_size (NULL, &(off_t) { 1 }));

    for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
        off_t size = -1;

        ck_assert_msg (dup_parse_size (cases[i].text, &size) == cases[i].ok, "\"%s\"",
                       cases[i].text);
        if (cases[i].ok)
            ck_assert_msg (size == cases[i].size, "\"%s\": %" G_GINT64_FORMAT, cases[i].text,
                           (gint64) size);
    }
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_poll_timeout)
{
    static const char *const names[] = { "a", "b", NULL };
    duplicates_t *d;
    const char *const *n;
    duplicates_stage_t stage;
    int rounds = 0;

    make_file ("a", BIG, 'x', -1, 0);
    make_file ("b", BIG, 'x', -1, 0);

    d = duplicates_new (DUPLICATES_BY_CONTENT, 0, 0);
    for (n = names; *n != NULL; n++)
        (void) add (d, *n);
    duplicates_start (d);
    do
        stage = duplicates_poll (d, NULL, 20 * 1000);
    while (stage != DUPLICATES_STAGE_DONE && ++rounds < 1000);
    ck_assert_int_eq (stage, DUPLICATES_STAGE_DONE);
    ck_assert_uint_eq (duplicates_get_groups (d)->len, 1);
    duplicates_free (d);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, my_setup, my_teardown);

    tcase_add_test (tc_core, test_by_size);
    tcase_add_test (tc_core, test_by_content);
    tcase_add_test (tc_core, test_by_name_case);
    tcase_add_test (tc_core, test_by_name_and_size);
    tcase_add_test (tc_core, test_big_files);
    tcase_add_test (tc_core, test_hard_links);
    tcase_add_test (tc_core, test_empty_and_special);
    tcase_add_test (tc_core, test_unreadable);
    tcase_add_test (tc_core, test_unique_size_not_opened);
    tcase_add_test (tc_core, test_order);
    tcase_add_test (tc_core, test_size_limits);
    tcase_add_test (tc_core, test_cancel_collect);
    tcase_add_test (tc_core, test_cancel_partial);
    tcase_add_test (tc_core, test_small_groups_first);
    tcase_add_test (tc_core, test_cancel_full);
    tcase_add_test (tc_core, test_free_while_running);
    tcase_add_test (tc_core, test_progress);
    tcase_add_test (tc_core, test_unreadable_in_the_middle);
    tcase_add_test (tc_core, test_tail_differs);
    tcase_add_test (tc_core, test_cache_second_search);
    tcase_add_test (tc_core, test_cache_changed_file);
    tcase_add_test (tc_core, test_cache_changed_after_walk);
    tcase_add_test (tc_core, test_cancel_with_cached_hashes);
    tcase_add_test (tc_core, test_similar_text);
    tcase_add_test (tc_core, test_text_hash_cancel);
#if defined(DUP_HAVE_PNG) && defined(DUP_HAVE_JPEG)
    tcase_add_test (tc_core, test_similar_images);
#endif
    tcase_add_test (tc_core, test_phash_distance);
    tcase_add_test (tc_core, test_progress_heads_differ);
    tcase_add_test (tc_core, test_progress_unreadable);
    tcase_add_test (tc_core, test_free_does_not_wait_for_the_worker);
    tcase_add_test (tc_core, test_parse_size);
    tcase_add_test (tc_core, test_name_masks);
    tcase_add_test (tc_core, test_dir_ignored);
    tcase_add_test (tc_core, test_file_kind_probe);
    tcase_add_test (tc_core, test_poll_timeout);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
