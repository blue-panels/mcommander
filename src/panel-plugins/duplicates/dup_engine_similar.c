/*
   Panel plugin duplicates for the M-Commander
   The worker of a search for similar content

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

/** \file dup_engine_similar.c
 *  \brief Source: the worker of a search for similar content
 *
 *  Each file gets a fingerprint by its kind: the perceptual hash of an image, the hash of a
 *  text, or the hash of the bytes.  Equal fingerprints make a group; images make groups by the
 *  distance of their hashes.
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib/global.h"

#include "dup_engine_int.h"

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static char *
prefixed (const char *prefix, char *digest)
{
    char *key;

    key = g_strconcat (prefix, digest, (char *) NULL);
    g_free (digest);
    return key;
}

/* --------------------------------------------------------------------------------------------- */

/* The fingerprint of a file for a search of similar content: "x:" and the SHA-256 of the bytes,
   "t:" and that of the text, "i:" and the perceptual hash; NULL when it is not compared. */
static char *
similar_key (duplicates_t *d, const duplicates_file_t *f, char *buf, gboolean *cancelled)
{
    unsigned char head[DUP_KIND_PROBE];
    ssize_t len;
    dup_file_kind_t kind;
    char *digest;
    guint64 bytes = 0;
    int fd;

    *cancelled = FALSE;

    fd = dup_engine_file_changed (f) ? -1 : open (f->path, O_RDONLY | O_CLOEXEC);
    if (fd == -1)
    {
        dup_engine_progress (d, 0, 0, (guint64) f->size, 1);
        return NULL;
    }
    do
        len = read (fd, head, sizeof (head));
    while (len == -1 && errno == EINTR);
    close (fd);
    kind = dup_file_kind (head, len > 0 ? (size_t) len : 0);

    if (kind == DUP_KIND_IMAGE && d->method != DUP_SIMILAR_TEXT
        && dup_image_supported (head, (size_t) len))
    {
        guint64 hash, pixels;

        /* the hash, and the number of pixels: the image with most is the one to compare with */
        digest = dup_cache_lookup (d->cache, f->id, DUP_CACHE_PHASH);
        if (digest == NULL)
        {
            dup_engine_progress_file (d, DUPLICATES_STAGE_FULL, f->path);
            g_atomic_int_inc (&d->opened);
            if (!dup_image_phash (f->path, &hash, &pixels, &bytes))
            {
                dup_engine_progress (d, 0, 0, (guint64) f->size, 1);
                return NULL;
            }
            digest =
                g_strdup_printf ("%016" G_GINT64_MODIFIER "x:%" G_GUINT64_FORMAT, hash, pixels);
            dup_cache_store (d->cache, f->id, DUP_CACHE_PHASH, digest);
            dup_engine_progress (d, 0, (guint64) f->size, 0, 0);
        }
        else
            dup_engine_progress (d, 0, 0, (guint64) f->size, 0);
        return prefixed ("i:", digest);
    }

    /* the images alone are compared by "Images" */
    if (d->method == DUP_SIMILAR_IMAGES)
    {
        // an image this build cannot decode is not compared: said as a file not read
        dup_engine_progress (d, 0, 0, (guint64) f->size, kind == DUP_KIND_IMAGE ? 1 : 0);
        return NULL;
    }

    if (kind == DUP_KIND_TEXT)
    {
        digest = dup_cache_lookup (d->cache, f->id, DUP_CACHE_TEXT);
        if (digest == NULL)
        {
            dup_engine_progress_file (d, DUPLICATES_STAGE_FULL, f->path);
            g_atomic_int_inc (&d->opened);
            digest = dup_text_hash (f->path, &bytes, &d->cancel);
            *cancelled = digest == NULL && dup_engine_cancelled (d);
            dup_engine_progress (d, 0, bytes, 0, digest == NULL && !*cancelled ? 1 : 0);
            if (digest == NULL)
                return NULL;
            dup_cache_store (d->cache, f->id, DUP_CACHE_TEXT, digest);
        }
        else
            dup_engine_progress (d, 0, 0, (guint64) f->size, 0);
        return prefixed ("t:", digest);
    }

    /* the rest is compared byte by byte */
    digest = dup_engine_hash_file (d, DUPLICATES_STAGE_FULL, f, buf, cancelled);
    return digest != NULL ? prefixed ("x:", digest) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    duplicates_file_t *f;
    guint64 hash;
    guint64 pixels;
} image_t;

/* --------------------------------------------------------------------------------------------- */

/* The image with the most pixels, then the largest file, comes first: the others of its cluster
   are compared with it */
static int
image_cmp (gconstpointer a, gconstpointer b)
{
    const image_t *ia = (const image_t *) a;
    const image_t *ib = (const image_t *) b;

    if (ia->pixels != ib->pixels)
        return ia->pixels > ib->pixels ? -1 : 1;
    if (ia->f->size != ib->f->size)
        return ia->f->size > ib->f->size ? -1 : 1;
    return strcmp (ia->f->path, ib->f->path);
}

/* --------------------------------------------------------------------------------------------- */

/* Groups the images whose hashes differ in no more bits than the threshold: a file joins the
   group of the first file it is similar to, so a chain of small changes does not join files
   that are not alike. */
static void
cluster_images (duplicates_t *d, GArray *images)
{
    const int max = dup_phash_max_distance (d->percent);
    gboolean *taken;
    guint i, j;

    g_array_sort (images, image_cmp);

    taken = g_new0 (gboolean, images->len);
    for (i = 0; i < images->len && !dup_engine_cancelled (d); i++)
    {
        const image_t *first = &g_array_index (images, image_t, i);
        GPtrArray *cluster;

        if (taken[i])
            continue;

        cluster = g_ptr_array_new ();
        g_ptr_array_add (cluster, first->f);
        first->f->similarity = 100;

        for (j = i + 1; j < images->len; j++)
        {
            const image_t *im = &g_array_index (images, image_t, j);

            if (!taken[j] && dup_phash_distance (first->hash, im->hash) <= max)
            {
                taken[j] = TRUE;
                im->f->similarity = dup_phash_similarity (first->hash, im->hash);
                g_ptr_array_add (cluster, im->f);
            }
        }

        if (cluster->len > 1)
        {
            duplicates_group_t *g;

            g = g_new0 (duplicates_group_t, 1);
            g->files = cluster;  // the one the others are compared with first
            dup_engine_push_group (d, g);
        }
        else
            g_ptr_array_free (cluster, TRUE);
    }
    g_free (taken);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gpointer
dup_engine_worker_similar (duplicates_t *d)
{
    GPtrArray *files = (GPtrArray *) g_ptr_array_index (d->pending, 0);
    GHashTable *by_key;
    GPtrArray *order;
    GArray *images;
    char *buf;
    gboolean cancelled = FALSE;
    guint i;

    buf = g_malloc (DUPLICATES_CHUNK_SIZE);
    by_key = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    order = g_ptr_array_new_with_free_func (dup_engine_pending_free);
    images = g_array_new (FALSE, FALSE, sizeof (image_t));

    for (i = 0; i < files->len && !cancelled; i++)
    {
        duplicates_file_t *f = (duplicates_file_t *) g_ptr_array_index (files, i);
        char *key;

        if (dup_engine_cancelled (d))
            break;

        key = similar_key (d, f, buf, &cancelled);
        dup_engine_progress (d, 1, 0, 0, 0);
        if (key == NULL)
            continue;

        if (key[0] == 'i')
        {
            image_t im = { f, 0, 0 };
            char *end;

            im.hash = g_ascii_strtoull (key + 2, &end, 16);
            if (*end == ':')
                im.pixels = g_ascii_strtoull (end + 1, NULL, 10);
            g_array_append_val (images, im);
            g_free (key);
        }
        else
        {
            GPtrArray *bucket = (GPtrArray *) g_hash_table_lookup (by_key, key);

            if (bucket == NULL)
            {
                bucket = g_ptr_array_new ();
                g_ptr_array_add (order, bucket);
                g_hash_table_insert (by_key, key, bucket);
            }
            else
                g_free (key);
            g_ptr_array_add (bucket, f);
            f->similarity = 100;
        }
    }

    for (i = 0; i < order->len; i++)
    {
        GPtrArray *bucket = (GPtrArray *) g_ptr_array_index (order, i);

        if (bucket->len > 1)
            dup_engine_push_group (d, dup_engine_group_new (bucket, FALSE));
    }
    cluster_images (d, images);

    g_array_free (images, TRUE);
    g_ptr_array_free (order, TRUE);
    g_hash_table_destroy (by_key);
    g_free (buf);

    dup_engine_end (d, cancelled || dup_engine_cancelled (d));
    dup_engine_unref (d);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
