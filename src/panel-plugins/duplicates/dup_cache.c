/*
   Panel plugin duplicates for the M-Commander
   The hashes of files kept between searches

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

/** \file dup_cache.c
 *  \brief Source: the hashes of files kept between searches
 *
 *  A text file, a line per file: the id, the digests by dup_cache_kind_t ("-" when not known)
 *  and the day the entry was last used.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"

#include "dup_cache.h"

/*** file scope macro definitions ****************************************************************/

#define DAY (24 * 60 * 60)

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *digest[DUP_CACHE_KINDS];  // by dup_cache_kind_t, or NULL
    gint64 day;                     // the day it was last used
} cache_entry_t;

struct dup_cache_s
{
    char *path;
    GHashTable *entries;  // id -> cache_entry_t *
    GMutex lock;
    gint64 today;
    gboolean changed;
    gint refs;
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
entry_free (gpointer p)
{
    cache_entry_t *e = (cache_entry_t *) p;

    int k;

    for (k = 0; k < DUP_CACHE_KINDS; k++)
        g_free (e->digest[k]);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

static long
mtime_nsec (const struct stat *st)
{
#if defined(HAVE_STRUCT_STAT_ST_MTIM)
    return (long) st->st_mtim.tv_nsec;
#elif defined(HAVE_STRUCT_STAT_ST_MTIMESPEC)
    return (long) st->st_mtimespec.tv_nsec;
#elif defined(HAVE_STRUCT_STAT_ST_MTIMENSEC)
    return (long) st->st_mtimensec;
#else
    (void) st;
    return 0;
#endif
}

/* --------------------------------------------------------------------------------------------- */

static long
ctime_nsec (const struct stat *st)
{
#if defined(HAVE_STRUCT_STAT_ST_MTIM)
    return (long) st->st_ctim.tv_nsec;
#elif defined(HAVE_STRUCT_STAT_ST_MTIMESPEC)
    return (long) st->st_ctimespec.tv_nsec;
#elif defined(HAVE_STRUCT_STAT_ST_MTIMENSEC)
    return (long) st->st_ctimensec;
#else
    (void) st;
    return 0;
#endif
}

/* --------------------------------------------------------------------------------------------- */

static char *
digest_or_null (const char *text)
{
    return strcmp (text, "-") == 0 ? NULL : g_strdup (text);
}

/* --------------------------------------------------------------------------------------------- */

static void
load_lines (dup_cache_t *cache, const char *data)
{
    char **lines;
    int i;

    lines = g_strsplit (data, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        char **f;

        f = g_strsplit (lines[i], "\t", -1);
        if (g_strv_length (f) == DUP_CACHE_KINDS + 2 && *f[0] != '\0')
        {
            cache_entry_t *e;
            int k;

            e = g_new0 (cache_entry_t, 1);
            for (k = 0; k < DUP_CACHE_KINDS; k++)
                e->digest[k] = digest_or_null (f[k + 1]);
            e->day = g_ascii_strtoll (f[DUP_CACHE_KINDS + 1], NULL, 10);
            g_hash_table_replace (cache->entries, g_strdup (f[0]), e);
        }
        g_strfreev (f);
    }
    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

dup_cache_t *
dup_cache_load (const char *path, time_t now)
{
    dup_cache_t *cache;
    char *data = NULL;

    cache = g_new0 (dup_cache_t, 1);
    cache->path = g_strdup (path);
    cache->entries = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, entry_free);
    g_mutex_init (&cache->lock);
    cache->today = (gint64) now / DAY;
    cache->refs = 1;

    if (path != NULL && g_file_get_contents (path, &data, NULL, NULL))
        load_lines (cache, data);
    g_free (data);

    return cache;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dup_cache_save (dup_cache_t *cache)
{
    GString *out;
    GHashTableIter iter;
    gpointer key, value;
    gboolean ok = TRUE;

    if (cache == NULL || cache->path == NULL)
        return TRUE;

    g_mutex_lock (&cache->lock);
    if (!cache->changed)
    {
        g_mutex_unlock (&cache->lock);
        return TRUE;
    }
    // a store from now on sets it again, and is written by the next save
    cache->changed = FALSE;

    out = g_string_new (NULL);
    g_hash_table_iter_init (&iter, cache->entries);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        const cache_entry_t *e = (const cache_entry_t *) value;
        int k;

        if (cache->today - e->day > DUP_CACHE_KEEP_DAYS)
            continue;
        g_string_append (out, (const char *) key);
        for (k = 0; k < DUP_CACHE_KINDS; k++)
        {
            g_string_append_c (out, '\t');
            g_string_append (out, e->digest[k] != NULL ? e->digest[k] : "-");
        }
        g_string_append_printf (out, "\t%" G_GINT64_FORMAT "\n", e->day);
    }
    g_mutex_unlock (&cache->lock);

    {
        char *dir;

        dir = g_path_get_dirname (cache->path);
        (void) g_mkdir_with_parents (dir, 0700);
        g_free (dir);
    }

    ok = g_file_set_contents (cache->path, out->str, (gssize) out->len, NULL);
    g_string_free (out, TRUE);
    if (!ok)
    {
        g_mutex_lock (&cache->lock);
        cache->changed = TRUE;
        g_mutex_unlock (&cache->lock);
    }
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

dup_cache_t *
dup_cache_ref (dup_cache_t *cache)
{
    if (cache != NULL)
        g_atomic_int_inc (&cache->refs);
    return cache;
}

/* --------------------------------------------------------------------------------------------- */

void
dup_cache_free (dup_cache_t *cache)
{
    if (cache == NULL || !g_atomic_int_dec_and_test (&cache->refs))
        return;

    // what a worker stored after the owner had saved
    (void) dup_cache_save (cache);

    g_hash_table_destroy (cache->entries);
    g_mutex_clear (&cache->lock);
    g_free (cache->path);
    g_free (cache);
}

/* --------------------------------------------------------------------------------------------- */

char *
dup_cache_file_id (const struct stat *st)
{
    return g_strdup_printf ("%" G_GUINT64_FORMAT ":%" G_GUINT64_FORMAT ":%" G_GINT64_FORMAT
                            ":%" G_GINT64_FORMAT ".%09ld:%" G_GINT64_FORMAT ".%09ld",
                            (guint64) st->st_dev, (guint64) st->st_ino, (gint64) st->st_size,
                            (gint64) st->st_mtime, mtime_nsec (st), (gint64) st->st_ctime,
                            ctime_nsec (st));
}

/* --------------------------------------------------------------------------------------------- */

char *
dup_cache_lookup (dup_cache_t *cache, const char *id, dup_cache_kind_t kind)
{
    cache_entry_t *e;
    char *digest = NULL;

    if (cache == NULL || id == NULL)
        return NULL;

    g_mutex_lock (&cache->lock);
    e = (cache_entry_t *) g_hash_table_lookup (cache->entries, id);
    if (e != NULL && e->digest[kind] != NULL)
    {
        digest = g_strdup (e->digest[kind]);
        if (e->day != cache->today)
        {
            e->day = cache->today;
            cache->changed = TRUE;
        }
    }
    g_mutex_unlock (&cache->lock);

    return digest;
}

/* --------------------------------------------------------------------------------------------- */

void
dup_cache_store (dup_cache_t *cache, const char *id, dup_cache_kind_t kind, const char *digest)
{
    cache_entry_t *e;

    if (cache == NULL || id == NULL || digest == NULL)
        return;

    g_mutex_lock (&cache->lock);
    e = (cache_entry_t *) g_hash_table_lookup (cache->entries, id);
    if (e == NULL)
    {
        e = g_new0 (cache_entry_t, 1);
        g_hash_table_insert (cache->entries, g_strdup (id), e);
    }
    g_free (e->digest[kind]);
    e->digest[kind] = g_strdup (digest);
    e->day = cache->today;
    cache->changed = TRUE;
    g_mutex_unlock (&cache->lock);
}

/* --------------------------------------------------------------------------------------------- */

guint
dup_cache_size (dup_cache_t *cache)
{
    guint n;

    g_mutex_lock (&cache->lock);
    n = g_hash_table_size (cache->entries);
    g_mutex_unlock (&cache->lock);
    return n;
}

/* --------------------------------------------------------------------------------------------- */
