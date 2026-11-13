/*
   Panel plugin duplicates for the M-Commander
   Search engine of duplicate files, with no UI

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

/** \file dup_engine.c
 *  \brief Source: search engine of duplicate files
 *
 *  The caller adds the files it found; duplicates_start() groups them by the
 *  key made of the criteria. When the content is compared, the groups go to a
 *  worker thread that hashes the head of each file, regroups, then hashes the
 *  rest of the files that are larger than the head. The worker takes the
 *  groups one at a time, those that read less first, so a stop keeps the
 *  groups already finished whole.
 *
 *  The worker keeps the progress under a mutex, and hands the finished groups
 *  over a GAsyncQueue that duplicates_poll() drains in the main thread.
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/util.h"  // MC_PTR_FREE

#include "dup_engine.h"
#include "dup_engine_int.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static duplicates_group_t end_marker;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
file_free (gpointer p)
{
    duplicates_file_t *f = (duplicates_file_t *) p;

    g_free (f->path);
    g_free (f->id);
    g_free (f);
}

/* --------------------------------------------------------------------------------------------- */

static guint
inode_hash (gconstpointer p)
{
    const duplicates_file_t *f = (const duplicates_file_t *) p;

    return (guint) f->ino ^ ((guint) f->dev * 31U);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
inode_equal (gconstpointer a, gconstpointer b)
{
    const duplicates_file_t *fa = (const duplicates_file_t *) a;
    const duplicates_file_t *fb = (const duplicates_file_t *) b;

    return fa->dev == fb->dev && fa->ino == fb->ino;
}

/* --------------------------------------------------------------------------------------------- */

static void
group_free (gpointer p)
{
    duplicates_group_t *g = (duplicates_group_t *) p;

    if (g == NULL || g == &end_marker)
        return;
    g_ptr_array_free (g->files, TRUE);
    g_free (g);
}

/* --------------------------------------------------------------------------------------------- */

void
dup_engine_pending_free (gpointer p)
{
    g_ptr_array_free ((GPtrArray *) p, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static int
file_cmp_path (gconstpointer a, gconstpointer b)
{
    const duplicates_file_t *fa = *(const duplicates_file_t *const *) a;
    const duplicates_file_t *fb = *(const duplicates_file_t *const *) b;

    return strcmp (fa->path, fb->path);
}

/* --------------------------------------------------------------------------------------------- */

static guint64
group_reclaimable (const duplicates_group_t *g)
{
    guint64 sum = 0, max = 0;
    guint i;

    for (i = 0; i < g->files->len; i++)
    {
        const duplicates_file_t *f = (const duplicates_file_t *) g_ptr_array_index (g->files, i);

        sum += (guint64) f->size;
        max = MAX (max, (guint64) f->size);
    }
    return sum - max;
}

/* --------------------------------------------------------------------------------------------- */

static int
group_cmp (gconstpointer a, gconstpointer b)
{
    const duplicates_group_t *ga = *(const duplicates_group_t *const *) a;
    const duplicates_group_t *gb = *(const duplicates_group_t *const *) b;
    guint64 ra, rb;

    ra = group_reclaimable (ga);
    rb = group_reclaimable (gb);
    if (ra != rb)
        return ra > rb ? -1 : 1;

    return strcmp (((const duplicates_file_t *) g_ptr_array_index (ga->files, 0))->path,
                   ((const duplicates_file_t *) g_ptr_array_index (gb->files, 0))->path);
}

/* --------------------------------------------------------------------------------------------- */

/* A group made of the files of `files`, which it does not own. */
duplicates_group_t *
dup_engine_group_new (GPtrArray *files, gboolean same_size)
{
    duplicates_group_t *g;
    guint i;

    g = g_new0 (duplicates_group_t, 1);
    g->files = g_ptr_array_sized_new (files->len);
    for (i = 0; i < files->len; i++)
        g_ptr_array_add (g->files, g_ptr_array_index (files, i));
    g_ptr_array_sort (g->files, file_cmp_path);
    if (same_size)
        g->size = ((const duplicates_file_t *) g_ptr_array_index (g->files, 0))->size;
    return g;
}

/* --------------------------------------------------------------------------------------------- */

static char *
make_key (const duplicates_t *d, const duplicates_file_t *f)
{
    GString *key;

    key = g_string_new (NULL);

    if ((d->criteria & (DUPLICATES_BY_SIZE | DUPLICATES_BY_CONTENT)) != 0)
        g_string_append_printf (key, "%" G_GUINT64_FORMAT, (guint64) f->size);
    g_string_append_c (key, '\n');

    if ((d->criteria & DUPLICATES_BY_NAME) != 0)
    {
        const char *base;

        base = strrchr (f->path, PATH_SEP);
        base = base == NULL ? f->path : base + 1;

        if ((d->criteria & DUPLICATES_NAME_CASE_SENS) != 0)
            g_string_append (key, base);
        else if (g_utf8_validate (base, -1, NULL))
        {
            char *folded;

            folded = g_utf8_casefold (base, -1);
            g_string_append (key, folded);
            g_free (folded);
        }
        else
        {
            char *folded;

            folded = g_ascii_strdown (base, -1);
            g_string_append (key, folded);
            g_free (folded);
        }
    }

    return g_string_free (key, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* Splits `files` by their `keys` into the arrays of `out`, keeping the ones with
   two files or more. */
static void
split_by_key (GPtrArray *files, char **keys, GPtrArray *out)
{
    GHashTable *by_key;
    GPtrArray *order;
    guint i;

    by_key = g_hash_table_new (g_str_hash, g_str_equal);
    order = g_ptr_array_new ();

    for (i = 0; i < files->len; i++)
    {
        GPtrArray *bucket;

        if (keys[i] == NULL)
            continue;

        bucket = (GPtrArray *) g_hash_table_lookup (by_key, keys[i]);
        if (bucket == NULL)
        {
            bucket = g_ptr_array_new ();
            g_hash_table_insert (by_key, keys[i], bucket);
            g_ptr_array_add (order, bucket);
        }
        g_ptr_array_add (bucket, g_ptr_array_index (files, i));
    }

    for (i = 0; i < order->len; i++)
    {
        GPtrArray *bucket = (GPtrArray *) g_ptr_array_index (order, i);

        if (bucket->len > 1)
            g_ptr_array_add (out, bucket);
        else
            g_ptr_array_free (bucket, TRUE);
    }

    g_ptr_array_free (order, TRUE);
    g_hash_table_destroy (by_key);
}

/* --------------------------------------------------------------------------------------------- */

static guint64
pending_bytes (const GPtrArray *files)
{
    return (guint64) ((const duplicates_file_t *) g_ptr_array_index (files, 0))->size * files->len;
}

/* --------------------------------------------------------------------------------------------- */

/* The groups that read less go first, so that a stop finds the most groups done. */
static int
pending_cmp (gconstpointer a, gconstpointer b)
{
    const guint64 ba = pending_bytes (*(const GPtrArray *const *) a);
    const guint64 bb = pending_bytes (*(const GPtrArray *const *) b);

    return ba < bb ? -1 : (ba > bb ? 1 : 0);
}

/* --------------------------------------------------------------------------------------------- */

/* A byte to the caller's pipe: at once for a group or the end, else a few times a second */
static void
notify (duplicates_t *d, gboolean now)
{
    const int fd = g_atomic_int_get (&d->notify_fd);
    const gint64 t = g_get_monotonic_time ();
    ssize_t n;

    if (fd < 0 || (!now && t - d->last_notify < G_USEC_PER_SEC / 5))
        return;
    d->last_notify = t;
    do
        n = write (fd, "", 1);
    while (n == -1 && errno == EINTR);
}

/* --------------------------------------------------------------------------------------------- */

/* @skipped: bytes counted in the total that are not going to be read */
void
dup_engine_progress (duplicates_t *d, guint files, guint64 read, guint64 skipped, guint errors)
{
    g_mutex_lock (&d->lock);
    d->progress.files_done += files;
    d->progress.bytes_done += read;
    d->progress.bytes_total -= skipped;
    d->progress.errors += errors;
    g_mutex_unlock (&d->lock);
    notify (d, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
dup_engine_progress_file (duplicates_t *d, duplicates_stage_t stage, const char *path)
{
    g_mutex_lock (&d->lock);
    d->progress.stage = stage;
    g_free (d->progress.current);
    d->progress.current = g_strdup (path);
    g_mutex_unlock (&d->lock);
    notify (d, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
dup_engine_push_group (duplicates_t *d, duplicates_group_t *g)
{
    g_async_queue_push (d->queue, g);
    notify (d, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
dup_engine_end (duplicates_t *d, gboolean cancelled)
{
    d->end_stage = cancelled ? DUPLICATES_STAGE_CANCELLED : DUPLICATES_STAGE_DONE;
    g_async_queue_push (d->queue, &end_marker);
    notify (d, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dup_engine_cancelled (duplicates_t *d)
{
    return g_atomic_int_get (&d->cancel) != 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Reads @len bytes from @offset into @sum.  The bytes not read are left in @left. */
static gboolean
read_range (duplicates_t *d, int fd, off_t offset, off_t len, GChecksum *sum, char *buf,
            off_t *left, gboolean *cancelled)
{
    off_t todo = len;

    if (lseek (fd, offset, SEEK_SET) != offset)
        return FALSE;

    while (todo > 0)
    {
        ssize_t n;

        if (dup_engine_cancelled (d))
        {
            *cancelled = TRUE;
            return FALSE;
        }

        n = read (fd, buf, (size_t) MIN (todo, (off_t) DUPLICATES_CHUNK_SIZE));
        if (n == -1 && errno == EINTR)
            continue;
        if (n <= 0)
            return FALSE;

        g_checksum_update (sum, (const guchar *) buf, (gssize) n);
        todo -= n;
        *left -= n;
        dup_engine_progress (d, 0, (guint64) n, 0, 0);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static off_t
partial_len (const duplicates_file_t *f)
{
    return MIN (f->size, (off_t) DUPLICATES_PARTIAL_SIZE);
}

/* --------------------------------------------------------------------------------------------- */

/* The file is not the one the walk saw: its hashes in the cache, or the size the panel shows,
   would be wrong */
gboolean
dup_engine_file_changed (const duplicates_file_t *f)
{
    struct stat st;
    char *id;
    gboolean changed;

    if (stat (f->path, &st) != 0)
        return TRUE;
    id = dup_cache_file_id (&st);
    changed = strcmp (id, f->id) != 0;
    g_free (id);
    return changed;
}

/* --------------------------------------------------------------------------------------------- */

/* The hex SHA-256 of `f` in `stage`: the first and the last DUPLICATES_EDGE_SIZE bytes for the
   partial hash, the whole file for the full one.  NULL when the file cannot be read, has
   changed size, or the search is stopped. */
char *
dup_engine_hash_file (duplicates_t *d, duplicates_stage_t stage, const duplicates_file_t *f,
                      char *buf, gboolean *cancelled)
{
    const dup_cache_kind_t kind =
        stage == DUPLICATES_STAGE_PARTIAL ? DUP_CACHE_PARTIAL : DUP_CACHE_FULL;
    const off_t len = stage == DUPLICATES_STAGE_PARTIAL ? partial_len (f) : f->size;
    GChecksum *sum;
    char *digest;
    off_t left = len;
    gboolean ok;
    int fd;

    if (d->hook != NULL)
        d->hook (d, stage, f->path, d->hook_data);

    *cancelled = dup_engine_cancelled (d);
    if (*cancelled)
        return NULL;

    if (dup_engine_file_changed (f))
    {
        dup_engine_progress (d, 0, 0, (guint64) len, 1);
        return NULL;
    }

    digest = dup_cache_lookup (d->cache, f->id, kind);
    if (digest != NULL)
    {
        dup_engine_progress (d, 0, 0, (guint64) len, 0);
        return digest;
    }

    dup_engine_progress_file (d, stage, f->path);

    g_atomic_int_inc (&d->opened);
    fd = open (f->path, O_RDONLY | O_CLOEXEC);
    if (fd == -1)
        goto fail;

    sum = g_checksum_new (G_CHECKSUM_SHA256);

    if (stage == DUPLICATES_STAGE_FULL || f->size <= DUPLICATES_PARTIAL_SIZE)
        ok = read_range (d, fd, 0, f->size, sum, buf, &left, cancelled);
    else
        ok = read_range (d, fd, 0, DUPLICATES_EDGE_SIZE, sum, buf, &left, cancelled)
            && read_range (d, fd, f->size - DUPLICATES_EDGE_SIZE, DUPLICATES_EDGE_SIZE, sum, buf,
                           &left, cancelled);

    /* a file that grew is caught by the byte that should not be there */
    if (ok)
    {
        char extra;
        ssize_t n;

        do
            n = read (fd, &extra, 1);
        while (n == -1 && errno == EINTR);
        ok = n == 0;
    }

    close (fd);

    digest = ok ? g_strdup (g_checksum_get_string (sum)) : NULL;
    g_checksum_free (sum);

    if (*cancelled)
    {
        g_free (digest);
        return NULL;
    }
    if (digest == NULL)
        goto fail;

    dup_cache_store (d->cache, f->id, kind, digest);
    return digest;

fail:
    dup_engine_progress (d, 0, 0, (guint64) MAX (left, 0), 1);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static guint64
rest_bytes (const GPtrArray *files)
{
    guint64 rest = 0;
    guint i;

    for (i = 0; i < files->len; i++)
    {
        const duplicates_file_t *f = (const duplicates_file_t *) g_ptr_array_index (files, i);

        if (f->size > DUPLICATES_PARTIAL_SIZE)
            rest += (guint64) f->size;
    }
    return rest;
}

/* --------------------------------------------------------------------------------------------- */

/* Hashes `files` (one size) in `stage`, and splits them by the digest.
   FALSE when the search is stopped. */
static gboolean
hash_split (duplicates_t *d, duplicates_stage_t stage, GPtrArray *files, char *buf, GPtrArray *out)
{
    char **keys;
    gboolean cancelled = FALSE;
    guint i;

    keys = g_new0 (char *, files->len + 1);

    for (i = 0; i < files->len && !cancelled; i++)
    {
        const duplicates_file_t *f = (const duplicates_file_t *) g_ptr_array_index (files, i);

        keys[i] = dup_engine_hash_file (d, stage, f, buf, &cancelled);
    }

    if (!cancelled)
        split_by_key (files, keys, out);

    // a file that could not be hashed leaves a NULL among the keys
    for (i = 0; i < files->len; i++)
        g_free (keys[i]);
    g_free (keys);
    return !cancelled;
}

/* --------------------------------------------------------------------------------------------- */

void
dup_engine_unref (duplicates_t *d)
{
    if (!g_atomic_int_dec_and_test (&d->refs))
        return;

    g_async_queue_unref (d->queue);
    g_mutex_clear (&d->lock);
    g_ptr_array_free (d->groups, TRUE);
    g_ptr_array_free (d->pending, TRUE);
    g_hash_table_destroy (d->inodes);
    g_ptr_array_free (d->files, TRUE);
    dup_cache_free (d->cache);
    g_free (d->progress.current);
    g_free (d);
}

/* --------------------------------------------------------------------------------------------- */

static gpointer
worker_main (gpointer data)
{
    duplicates_t *d = (duplicates_t *) data;
    char *buf;
    gboolean cancelled = FALSE;
    guint i;

    if ((d->criteria & DUPLICATES_SIMILAR) != 0)
        return dup_engine_worker_similar (d);

    buf = g_malloc (DUPLICATES_CHUNK_SIZE);

    for (i = 0; i < d->pending->len && !cancelled; i++)
    {
        GPtrArray *files = (GPtrArray *) g_ptr_array_index (d->pending, i);
        GPtrArray *heads;
        guint j;

        heads = g_ptr_array_new_with_free_func (dup_engine_pending_free);
        if (!hash_split (d, DUPLICATES_STAGE_PARTIAL, files, buf, heads))
            cancelled = TRUE;
        else
        {
            guint left = 0;
            guint64 rest;

            // the files of one size: either all of the heads are hashed on, or none
            rest = rest_bytes (files);
            if (((const duplicates_file_t *) g_ptr_array_index (files, 0))->size
                > DUPLICATES_PARTIAL_SIZE)
                for (j = 0; j < heads->len; j++)
                {
                    GPtrArray *head = (GPtrArray *) g_ptr_array_index (heads, j);

                    left += head->len;
                    rest -= rest_bytes (head);
                }
            // the files no other one matches by its head are not read any further
            dup_engine_progress (d, files->len - left, 0, rest, 0);
        }

        for (j = 0; j < heads->len && !cancelled; j++)
        {
            GPtrArray *head = (GPtrArray *) g_ptr_array_index (heads, j);
            const duplicates_file_t *f0 = (const duplicates_file_t *) g_ptr_array_index (head, 0);
            GPtrArray *fulls;
            guint k;

            if (f0->size <= DUPLICATES_PARTIAL_SIZE)
            {
                dup_engine_push_group (d, dup_engine_group_new (head, TRUE));
                continue;
            }

            fulls = g_ptr_array_new_with_free_func (dup_engine_pending_free);
            if (!hash_split (d, DUPLICATES_STAGE_FULL, head, buf, fulls))
                cancelled = TRUE;
            else
            {
                dup_engine_progress (d, head->len, 0, 0, 0);
                for (k = 0; k < fulls->len; k++)
                    dup_engine_push_group (
                        d, dup_engine_group_new ((GPtrArray *) g_ptr_array_index (fulls, k), TRUE));
            }
            g_ptr_array_free (fulls, TRUE);
        }

        g_ptr_array_free (heads, TRUE);
    }

    g_free (buf);

    dup_engine_end (d, cancelled);
    dup_engine_unref (d);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* A group from the worker, or its end: FALSE then */
static gboolean
take (duplicates_t *d, duplicates_group_t *g)
{
    if (g != &end_marker)
    {
        g_ptr_array_add (d->groups, g);
        d->sorted = FALSE;
        return TRUE;
    }

    g_thread_join (d->worker);
    d->worker = NULL;
    d->progress.stage = d->end_stage;
    MC_PTR_FREE (d->progress.current);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
sort_groups (duplicates_t *d)
{
    if (!d->sorted)
    {
        g_ptr_array_sort (d->groups, group_cmp);
        d->sorted = TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
fill_progress (duplicates_t *d, duplicates_progress_t *progress)
{
    if (progress == NULL)
        return;

    g_free (progress->current);
    g_mutex_lock (&d->lock);
    *progress = d->progress;
    progress->current = g_strdup (d->progress.current);
    g_mutex_unlock (&d->lock);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * The search for files that match by `criteria`. "Same content" implies
 * "same size"; no criterion at all means "same size". A limit of 0 is no
 * limit.
 */
duplicates_t *
duplicates_new (duplicates_criteria_t criteria, off_t size_min, off_t size_max)
{
    duplicates_t *d;

    if ((criteria & (DUPLICATES_BY_SIZE | DUPLICATES_BY_NAME | DUPLICATES_BY_CONTENT)) == 0)
        criteria |= DUPLICATES_BY_SIZE;
    if ((criteria & DUPLICATES_BY_CONTENT) != 0)
        criteria |= DUPLICATES_BY_SIZE;

    d = g_new0 (duplicates_t, 1);
    d->criteria = criteria;
    d->size_min = size_min;
    d->size_max = size_max;
    d->files = g_ptr_array_new_with_free_func (file_free);
    d->inodes = g_hash_table_new (inode_hash, inode_equal);
    d->pending = g_ptr_array_new_with_free_func (dup_engine_pending_free);
    d->groups = g_ptr_array_new_with_free_func (group_free);
    d->sorted = TRUE;
    d->queue = g_async_queue_new_full (group_free);
    g_mutex_init (&d->lock);
    d->refs = 1;
    d->notify_fd = -1;
    d->progress.stage = DUPLICATES_STAGE_COLLECT;
    return d;
}

/* --------------------------------------------------------------------------------------------- */

void
duplicates_free (duplicates_t *d)
{
    if (d == NULL)
        return;

    /* a worker stuck in read () on a dead mount must not hold the caller: it frees what is
       left when it returns */
    if (d->worker != NULL)
    {
        duplicates_cancel (d);
        g_thread_unref (d->worker);
        d->worker = NULL;
    }

    dup_engine_unref (d);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Adds a file found by the walk. Only non-empty regular files within the
 * size limits are taken, and a file whose dev+ino is already in is a hard
 * link to it and is taken once.
 *
 * @return TRUE if the file was taken
 */
gboolean
duplicates_add (duplicates_t *d, const char *path, const struct stat *st)
{
    duplicates_file_t *f;

    g_return_val_if_fail (d->progress.stage == DUPLICATES_STAGE_COLLECT, FALSE);

    if (!S_ISREG (st->st_mode) || st->st_size == 0)
        return FALSE;
    if (d->size_min > 0 && st->st_size < d->size_min)
        return FALSE;
    if (d->size_max > 0 && st->st_size > d->size_max)
        return FALSE;

    f = g_new0 (duplicates_file_t, 1);
    f->dev = st->st_dev;
    f->ino = st->st_ino;

    if (g_hash_table_contains (d->inodes, f))
    {
        g_free (f);
        return FALSE;
    }

    f->path = g_strdup (path);
    f->size = st->st_size;
    f->mtime = st->st_mtime;
    f->mode = st->st_mode;
    f->id = dup_cache_file_id (st);
    g_ptr_array_add (d->files, f);
    g_hash_table_add (d->inodes, f);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
duplicates_set_notify_fd (duplicates_t *d, int fd)
{
    g_atomic_int_set (&d->notify_fd, fd);
}

/* --------------------------------------------------------------------------------------------- */

void
duplicates_set_similar (duplicates_t *d, dup_similar_method_t method, int percent)
{
    d->criteria |= DUPLICATES_SIMILAR;
    d->method = method;
    d->percent = percent;
}

/* --------------------------------------------------------------------------------------------- */

void
duplicates_set_cache (duplicates_t *d, dup_cache_t *cache)
{
    dup_cache_free (d->cache);
    d->cache = dup_cache_ref (cache);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Groups the files added by the key of the criteria. Without "same content"
 * the result is complete at once; with it the groups are hashed in a worker
 * thread, and duplicates_poll() tells when it is done.
 */
void
duplicates_start (duplicates_t *d)
{
    char **keys;
    guint i;

    g_return_if_fail (d->progress.stage == DUPLICATES_STAGE_COLLECT);

    if ((d->criteria & DUPLICATES_SIMILAR) != 0)
    {
        GPtrArray *all;

        all = g_ptr_array_new ();
        for (i = 0; i < d->files->len; i++)
        {
            const duplicates_file_t *f =
                (const duplicates_file_t *) g_ptr_array_index (d->files, i);

            g_ptr_array_add (all, (gpointer) f);
            d->progress.bytes_total += (guint64) f->size;
        }
        g_ptr_array_sort (all, file_cmp_path);
        g_ptr_array_add (d->pending, all);
        d->progress.files_total = all->len;
        d->progress.stage = DUPLICATES_STAGE_FULL;
        g_atomic_int_inc (&d->refs);
        d->worker = g_thread_new ("duplicates", worker_main, d);
        return;
    }

    keys = g_new0 (char *, d->files->len + 1);
    for (i = 0; i < d->files->len; i++)
        keys[i] = make_key (d, (const duplicates_file_t *) g_ptr_array_index (d->files, i));
    split_by_key (d->files, keys, d->pending);
    g_strfreev (keys);

    if ((d->criteria & DUPLICATES_BY_CONTENT) == 0)
    {
        gboolean same_size = (d->criteria & DUPLICATES_BY_SIZE) != 0;

        for (i = 0; i < d->pending->len; i++)
            g_ptr_array_add (
                d->groups,
                dup_engine_group_new ((GPtrArray *) g_ptr_array_index (d->pending, i), same_size));
        g_ptr_array_set_size (d->pending, 0);
        d->sorted = FALSE;
        d->progress.stage = DUPLICATES_STAGE_DONE;
        return;
    }

    g_ptr_array_sort (d->pending, pending_cmp);

    for (i = 0; i < d->pending->len; i++)
    {
        GPtrArray *files = (GPtrArray *) g_ptr_array_index (d->pending, i);
        guint j;

        d->progress.files_total += files->len;
        for (j = 0; j < files->len; j++)
            d->progress.bytes_total +=
                (guint64) partial_len ((const duplicates_file_t *) g_ptr_array_index (files, j));
        d->progress.bytes_total += rest_bytes (files);
    }

    d->progress.stage = DUPLICATES_STAGE_PARTIAL;
    g_atomic_int_inc (&d->refs);
    d->worker = g_thread_new ("duplicates", worker_main, d);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Takes the groups the worker finished so far.
 *
 * @param progress if not NULL, filled with a copy of the progress; its
 *        `current` is the caller's to free, and is freed by the next call
 * @param timeout_us how long to wait for a group or the end, 0 for no wait
 * @return the stage of the search
 */
duplicates_stage_t
duplicates_poll (duplicates_t *d, duplicates_progress_t *progress, guint64 timeout_us)
{
    duplicates_group_t *g;
    duplicates_stage_t stage;

    if (d->worker != NULL && timeout_us != 0)
    {
        g = (duplicates_group_t *) g_async_queue_timeout_pop (d->queue, timeout_us);
        if (g != NULL)
            (void) take (d, g);
    }

    while (d->worker != NULL
           && (g = (duplicates_group_t *) g_async_queue_try_pop (d->queue)) != NULL)
        (void) take (d, g);

    fill_progress (d, progress);
    g_mutex_lock (&d->lock);
    stage = d->progress.stage;
    g_mutex_unlock (&d->lock);
    return stage;
}

/* --------------------------------------------------------------------------------------------- */

/** Blocks until the worker exits. */
duplicates_stage_t
duplicates_wait (duplicates_t *d)
{
    while (d->worker != NULL)
        (void) take (d, (duplicates_group_t *) g_async_queue_pop (d->queue));

    return d->progress.stage;
}

/* --------------------------------------------------------------------------------------------- */

/** Asks the worker to stop after the chunk it reads. */
void
duplicates_cancel (duplicates_t *d)
{
    g_atomic_int_set (&d->cancel, 1);

    if (d->worker == NULL && d->progress.stage != DUPLICATES_STAGE_DONE)
        d->progress.stage = DUPLICATES_STAGE_CANCELLED;
}

/* --------------------------------------------------------------------------------------------- */

/** The groups found so far: by the bytes they can free, largest first. */
const GPtrArray *
duplicates_get_groups (const duplicates_t *d)
{
    sort_groups ((duplicates_t *) d);
    return d->groups;
}

/* --------------------------------------------------------------------------------------------- */

/** The bytes freed by keeping one file, the largest, in each group. */
guint64
duplicates_reclaimable (const duplicates_t *d)
{
    guint64 total = 0;
    guint i;

    for (i = 0; i < d->groups->len; i++)
        total += group_reclaimable ((const duplicates_group_t *) g_ptr_array_index (d->groups, i));
    return total;
}

/* --------------------------------------------------------------------------------------------- */

guint
duplicates_files_count (const duplicates_t *d)
{
    guint total = 0;
    guint i;

    for (i = 0; i < d->groups->len; i++)
        total += ((const duplicates_group_t *) g_ptr_array_index (d->groups, i))->files->len;
    return total;
}

/* --------------------------------------------------------------------------------------------- */

guint
duplicates_files_opened (const duplicates_t *d)
{
    return (guint) g_atomic_int_get (&((duplicates_t *) d)->opened);
}

/* --------------------------------------------------------------------------------------------- */

void
duplicates_set_file_hook (duplicates_t *d,
                          void (*hook) (duplicates_t *d, duplicates_stage_t stage, const char *path,
                                        gpointer user_data),
                          gpointer user_data)
{
    d->hook = hook;
    d->hook_data = user_data;
}

/* --------------------------------------------------------------------------------------------- */
