/*
   Panel plugin duplicates for the M-Commander
   A search for duplicates that goes on without a dialog

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

/** \file dup_session.c
 *  \brief Source: a search for duplicates that goes on without a dialog
 *
 *  The walk of the directories runs in a thread; when it is over, the main thread starts the
 *  engine, whose worker hashes the files.  Both say they have news through a pipe, so the panel
 *  that shows the search is redrawn as the groups come.
 */

#include <config.h>

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/mcconfig.h"
#include "lib/util.h"

#include "dup_session.h"

/*** file scope macro definitions ****************************************************************/

#define CACHE_FILE "duplicates-v1.cache"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *path;
    char *phys;  // where it is on the disk when a link led to it, else NULL
} walk_item_t;

typedef struct
{
    dup_session_t *s;
    GQueue dirs;          // walk_item_t *, the directories still to read
    GHashTable *visited;  // "dev:ino" of the directories read or queued
    gint64 last_notify;
} walk_t;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
notify (int fd)
{
    ssize_t n;

    if (fd < 0)
        return;
    do
        n = write (fd, "", 1);
    while (n == -1 && errno == EINTR);
}

/* --------------------------------------------------------------------------------------------- */

static void
walk_item_free (gpointer p)
{
    walk_item_t *item = (walk_item_t *) p;

    g_free (item->path);
    g_free (item->phys);
    g_free (item);
}

/* --------------------------------------------------------------------------------------------- */

static char *
real_path (const char *path)
{
    char *real, *copy;

    real = realpath (path, NULL);
    copy = g_strdup (real);
    free (real);
    return copy;
}

/* --------------------------------------------------------------------------------------------- */

static char *
dir_key (const struct stat *st)
{
    return g_strdup_printf ("%" G_GUINT64_FORMAT ":%" G_GUINT64_FORMAT, (guint64) st->st_dev,
                            (guint64) st->st_ino);
}

/* --------------------------------------------------------------------------------------------- */

/* Queues @path once: a link to a directory read before, or the other panel inside the start
   directory, is not read twice */
static void
walk_push (walk_t *w, const char *path, const char *phys, const struct stat *st)
{
    walk_item_t *item;
    char *key;

    key = dir_key (st);
    if (g_hash_table_contains (w->visited, key))
    {
        g_free (key);
        return;
    }
    g_hash_table_add (w->visited, key);
    item = g_new (walk_item_t, 1);
    item->path = g_strdup (path);
    item->phys = g_strdup (phys);
    g_queue_push_tail (&w->dirs, item);
}

/* --------------------------------------------------------------------------------------------- */

/* @phys: where @path is on the disk when a link led to it, else NULL */
static void
walk_entry (walk_t *w, const char *path, char *phys, const char *name)
{
    dup_session_t *s = w->s;
    const dup_search_t *search = &s->search;
    struct stat st;

    if (lstat (path, &st) != 0)
        goto done;
    if (S_ISLNK (st.st_mode))
    {
        if (!search->follow_symlinks || stat (path, &st) != 0)
            goto done;
        g_free (phys);
        phys = real_path (path);
    }

    if (S_ISDIR (st.st_mode))
    {
        if (search->recursive && !dup_dir_ignored (search->ignore_dirs, path, name))
            walk_push (w, path, phys, &st);
        goto done;
    }

    if (!S_ISREG (st.st_mode))
        goto done;

    g_atomic_int_inc (&s->walk_files);
    if (!dup_name_filter_match (search->names, name))
        return;
    if (search->mtime_from != 0 && st.st_mtime < search->mtime_from)
        return;
    if (search->mtime_to != 0 && st.st_mtime > search->mtime_to)
        return;

    if (duplicates_add (s->engine, path, &st))
    {
        g_atomic_int_inc (&s->walk_taken);
        g_mutex_lock (&s->lock);
        s->walk_bytes += (guint64) st.st_size;
        if (phys != NULL)
        {
            g_hash_table_insert (s->physical, g_strdup (path), phys);
            phys = NULL;
        }
        g_mutex_unlock (&s->lock);
    }

done:
    g_free (phys);
}

/* --------------------------------------------------------------------------------------------- */

static void
walk_dir (walk_t *w, const walk_item_t *dir)
{
    DIR *d;
    struct dirent *de;

    d = opendir (dir->path);
    if (d == NULL)
    {
        g_atomic_int_inc (&w->s->walk_errors);
        return;
    }

    while ((de = readdir (d)) != NULL && g_atomic_int_get (&w->s->cancel) == 0)
    {
        char *path;

        if (DIR_IS_DOT (de->d_name) || DIR_IS_DOTDOT (de->d_name))
            continue;
        if (w->s->search.skip_hidden && de->d_name[0] == '.')
            continue;

        path = g_build_filename (dir->path, de->d_name, (char *) NULL);
        walk_entry (w, path,
                    dir->phys != NULL ? g_build_filename (dir->phys, de->d_name, (char *) NULL)
                                      : NULL,
                    de->d_name);
        g_free (path);
    }

    closedir (d);
}

/* --------------------------------------------------------------------------------------------- */

static void
walk_start_dir (walk_t *w, const char *dir)
{
    struct stat st;

    if (dir != NULL && stat (dir, &st) == 0 && S_ISDIR (st.st_mode))
        walk_push (w, dir, NULL, &st);
}

/* --------------------------------------------------------------------------------------------- */

static void
session_unref (dup_session_t *s)
{
    if (!g_atomic_int_dec_and_test (&s->refs))
        return;

    duplicates_set_notify_fd (s->engine, -1);
    duplicates_free (s->engine);  // a worker still reading frees the engine when it returns
    (void) dup_cache_save (s->cache);
    dup_cache_free (s->cache);

    g_mutex_clear (&s->lock);
    g_free (s->walk_dir);
    g_free (s->progress.current);
    g_hash_table_destroy (s->deleted);
    g_hash_table_destroy (s->physical);
    g_free (s->base);
    dup_search_clear (&s->search);
    g_free (s);
}

/* --------------------------------------------------------------------------------------------- */

static gpointer
walker_main (gpointer data)
{
    dup_session_t *s = (dup_session_t *) data;
    walk_t w;

    memset (&w, 0, sizeof (w));
    w.s = s;
    g_queue_init (&w.dirs);
    w.visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    walk_start_dir (&w, s->search.start_dir);
    walk_start_dir (&w, s->search.other_dir);

    while (!g_queue_is_empty (&w.dirs) && g_atomic_int_get (&s->cancel) == 0)
    {
        walk_item_t *dir;
        gint64 now;

        dir = (walk_item_t *) g_queue_pop_head (&w.dirs);
        g_mutex_lock (&s->lock);
        g_free (s->walk_dir);
        s->walk_dir = g_strdup (dir->path);
        g_mutex_unlock (&s->lock);

        walk_dir (&w, dir);
        walk_item_free (dir);

        now = g_get_monotonic_time ();
        if (now - w.last_notify >= G_USEC_PER_SEC / 5)
        {
            w.last_notify = now;
            notify (g_atomic_int_get (&s->notify_fd));
        }
    }

    g_queue_clear_full (&w.dirs, walk_item_free);
    g_hash_table_destroy (w.visited);

    g_atomic_int_set (&s->walk_done, 1);
    notify (g_atomic_int_get (&s->notify_fd));
    session_unref (s);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static char *
cache_path (void)
{
    const char *dir;

    dir = mc_config_get_cache_path ();
    return dir != NULL ? g_build_filename (dir, CACHE_FILE, (char *) NULL) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* The deepest directory both the start directory and the other one are below */
static char *
common_base (const dup_search_t *search)
{
    char *base;

    base = g_strdup (search->start_dir);
    while (search->other_dir != NULL && !dup_path_is_below (search->other_dir, base))
    {
        char *up;

        up = g_path_get_dirname (base);
        g_free (base);
        base = up;
    }
    return base;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

dup_session_t *
dup_session_start (dup_search_t *search, int notify_fd)
{
    dup_session_t *s;
    char *path;

    s = g_new0 (dup_session_t, 1);
    s->search = *search;
    memset (search, 0, sizeof (*search));
    s->base = common_base (&s->search);
    s->state = DUP_SESSION_WALKING;
    s->deleted = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    s->physical = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    s->notify_fd = notify_fd;
    s->refs = 2;
    g_mutex_init (&s->lock);

    path = cache_path ();
    s->cache = dup_cache_load (path, time (NULL));
    g_free (path);

    s->engine = duplicates_new (s->search.criteria, s->search.size_min, s->search.size_max);
    duplicates_set_cache (s->engine, s->cache);
    duplicates_set_notify_fd (s->engine, notify_fd);
    if ((s->search.criteria & DUPLICATES_SIMILAR) != 0)
        duplicates_set_similar (s->engine, s->search.method, s->search.similarity);

    s->walker = g_thread_new ("duplicates-walk", walker_main, s);
    return s;
}

/* --------------------------------------------------------------------------------------------- */

void
dup_session_update (dup_session_t *s)
{
    if (s->state == DUP_SESSION_WALKING && g_atomic_int_get (&s->walk_done) != 0)
    {
        g_thread_join (s->walker);
        s->walker = NULL;

        if (g_atomic_int_get (&s->cancel) != 0)
            s->state = DUP_SESSION_STOPPED;
        else
        {
            duplicates_start (s->engine);
            s->state = DUP_SESSION_HASHING;
        }
    }

    if (s->state == DUP_SESSION_HASHING)
    {
        switch (duplicates_poll (s->engine, &s->progress, 0))
        {
        case DUPLICATES_STAGE_DONE:
            s->state = DUP_SESSION_DONE;
            break;
        case DUPLICATES_STAGE_CANCELLED:
            s->state = DUP_SESSION_STOPPED;
            break;
        default:
            break;
        }

        if (s->state != DUP_SESSION_HASHING)
            (void) dup_cache_save (s->cache);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
dup_session_stop (dup_session_t *s)
{
    g_atomic_int_set (&s->cancel, 1);
    if (s->state == DUP_SESSION_HASHING)
        duplicates_cancel (s->engine);
}

/* --------------------------------------------------------------------------------------------- */

void
dup_session_free (dup_session_t *s)
{
    if (s == NULL)
        return;

    g_atomic_int_set (&s->cancel, 1);
    g_atomic_int_set (&s->notify_fd, -1);
    if (s->walker != NULL)
        g_thread_unref (s->walker);
    session_unref (s);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dup_session_running (const dup_session_t *s)
{
    return s != NULL && (s->state == DUP_SESSION_WALKING || s->state == DUP_SESSION_HASHING);
}

/* --------------------------------------------------------------------------------------------- */

void
dup_session_walk_state (dup_session_t *s, char **dir, guint *files, guint *taken, guint64 *bytes)
{
    g_mutex_lock (&s->lock);
    *dir = g_strdup (s->walk_dir);
    *bytes = s->walk_bytes;
    g_mutex_unlock (&s->lock);
    *files = (guint) g_atomic_int_get (&s->walk_files);
    *taken = (guint) g_atomic_int_get (&s->walk_taken);
}

/* --------------------------------------------------------------------------------------------- */

const GPtrArray *
dup_session_groups (dup_session_t *s)
{
    return duplicates_get_groups (s->engine);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dup_session_is_reference (dup_session_t *s, const char *path)
{
    const char *phys;

    g_mutex_lock (&s->lock);
    phys = (const char *) g_hash_table_lookup (s->physical, path);
    g_mutex_unlock (&s->lock);
    // the table does not change once the walk is over, when the groups come
    return dup_search_is_reference (&s->search, phys != NULL ? phys : path);
}

/* --------------------------------------------------------------------------------------------- */

/* The bytes a group frees: all the files but one, or all those not of the reference directory
   when it has a copy there.  The files deleted already are not counted. */
guint64
dup_session_group_freed (dup_session_t *s, const duplicates_group_t *g)
{
    guint64 sum = 0, largest = 0;
    gboolean reference = FALSE;
    guint i, left = 0;

    for (i = 0; i < g->files->len; i++)
    {
        const duplicates_file_t *f = (const duplicates_file_t *) g_ptr_array_index (g->files, i);

        if (g_hash_table_contains (s->deleted, f->path))
            continue;
        left++;
        if (dup_session_is_reference (s, f->path))
            reference = TRUE;
        else
        {
            sum += (guint64) f->size;
            largest = MAX (largest, (guint64) f->size);
        }
    }

    if (left < 2)
        return 0;
    return reference ? sum : sum - largest;
}

/* --------------------------------------------------------------------------------------------- */

guint64
dup_session_reclaimable (dup_session_t *s)
{
    const GPtrArray *groups = dup_session_groups (s);
    guint64 total = 0;
    guint i;

    for (i = 0; i < groups->len; i++)
        total += dup_session_group_freed (s, g_ptr_array_index (groups, i));
    return total;
}

/* --------------------------------------------------------------------------------------------- */
