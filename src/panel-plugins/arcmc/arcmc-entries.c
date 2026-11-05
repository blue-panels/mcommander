/*
   Panel plugin arcmc for the M-Commander
   The entries of an archive: lookup, sizes, links, release

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

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"

#include "arcmc-types.h"
#include "archive-io.h"

/* --------------------------------------------------------------------------------------------- */

void
arcmc_entry_free (gpointer p)
{
    arcmc_entry_t *e = (arcmc_entry_t *) p;

    g_free (e->full_path);
    g_free (e->name);
    g_free (e->linkname);
    g_free (e->link_path);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

const arcmc_entry_t *
arcmc_find_entry (GPtrArray *entries, const char *full_path)
{
    guint i;

    if (entries == NULL)
        return NULL;

    for (i = 0; i < entries->len; i++)
    {
        const arcmc_entry_t *e = (const arcmc_entry_t *) g_ptr_array_index (entries, i);

        if (strcmp (e->full_path, full_path) == 0)
            return e;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* The entry of @data at @full_path, looked up in an index of all_entries made at the first
   call after the list changed. The first entry of a path wins, as in arcmc_find_entry(). */
const arcmc_entry_t *
arcmc_data_find_entry (arcmc_data_t *data, const char *full_path)
{
    if (data->all_entries == NULL)
        return NULL;

    if (data->entry_index == NULL)
    {
        guint i;

        data->entry_index = g_hash_table_new (g_str_hash, g_str_equal);
        for (i = data->all_entries->len; i > 0; i--)
        {
            arcmc_entry_t *e = (arcmc_entry_t *) g_ptr_array_index (data->all_entries, i - 1);

            g_hash_table_insert (data->entry_index, e->full_path, e);
        }
    }

    return (const arcmc_entry_t *) g_hash_table_lookup (data->entry_index, full_path);
}

/* --------------------------------------------------------------------------------------------- */

/* Remove a directory tree recursively. */
void
arcmc_rm_rf (const char *path)
{
    GDir *dir;
    const char *entry;

    dir = g_dir_open (path, 0, NULL);
    if (dir != NULL)
    {
        while ((entry = g_dir_read_name (dir)) != NULL)
        {
            char *child = g_build_filename (path, entry, NULL);

            if (g_file_test (child, G_FILE_TEST_IS_DIR))
                arcmc_rm_rf (child);
            else
                unlink (child);
            g_free (child);
        }
        g_dir_close (dir);
    }
    rmdir (path);
}

/* --------------------------------------------------------------------------------------------- */

/* Drop the files unpacked before and kept in the cache of @data. */
void
arcmc_data_drop_cache (arcmc_data_t *data)
{
    if (data->bulk_cache != NULL)
    {
        g_hash_table_destroy (data->bulk_cache);
        data->bulk_cache = NULL;
    }
    if (data->bulk_temp_dir != NULL)
    {
        arcmc_rm_rf (data->bulk_temp_dir);
        g_free (data->bulk_temp_dir);
        data->bulk_temp_dir = NULL;
    }
    data->bulk_cache_size = 0;
    data->bulk_cache_seq = 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Drop the index and the cache of @data: all_entries is another list now. */
void
arcmc_data_entries_changed (arcmc_data_t *data)
{
    if (data->entry_index != NULL)
    {
        g_hash_table_destroy (data->entry_index);
        data->entry_index = NULL;
    }
    arcmc_data_drop_cache (data);
}

/* --------------------------------------------------------------------------------------------- */

/* Free all_entries of @data and its index. */
void
arcmc_data_free_entries (arcmc_data_t *data)
{
    arcmc_data_entries_changed (data);
    if (data->all_entries != NULL)
    {
        g_ptr_array_free (data->all_entries, TRUE);
        data->all_entries = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The total size of the files below @full_path, at any depth, kept on the
   directory entry itself. FALSE when @full_path is not a directory here. */
gboolean
arcmc_compute_dir_size (GPtrArray *entries, const char *full_path)
{
    arcmc_entry_t *dir;
    size_t path_len;
    off_t total = 0;
    guint i;

    if (entries == NULL || full_path == NULL)
        return FALSE;

    dir = (arcmc_entry_t *) arcmc_find_entry (entries, full_path);
    if (dir == NULL || !S_ISDIR (dir->mode))
        return FALSE;

    path_len = strlen (full_path);
    for (i = 0; i < entries->len; i++)
    {
        const arcmc_entry_t *entry = (const arcmc_entry_t *) g_ptr_array_index (entries, i);

        if (strncmp (entry->full_path, full_path, path_len) == 0
            && entry->full_path[path_len] == '/' && !S_ISDIR (entry->mode))
            total += entry->size;
    }

    dir->dir_size = total;
    dir->dir_size_computed = TRUE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Check if `entry_path` lies anywhere below `dir`. An empty `dir` is the root,
   which holds everything. */
gboolean
is_under_dir (const char *entry_path, const char *dir)
{
    size_t dir_len;

    if (dir == NULL || dir[0] == '\0')
        return TRUE;

    dir_len = strlen (dir);

    return strncmp (entry_path, dir, dir_len) == 0 && entry_path[dir_len] == '/';
}

/* --------------------------------------------------------------------------------------------- */

/* Check if `entry_path` is a direct child of `dir`.
   If so, return the child name component; otherwise NULL. */
const char *
is_direct_child (const char *entry_path, const char *dir)
{
    size_t dir_len;
    const char *rest;

    if (dir == NULL || dir[0] == '\0')
    {
        /* root: direct child if no '/' in path */
        if (strchr (entry_path, '/') == NULL)
            return entry_path;
        return NULL;
    }

    dir_len = strlen (dir);

    if (strncmp (entry_path, dir, dir_len) != 0)
        return NULL;

    if (entry_path[dir_len] != '/')
        return NULL;

    rest = entry_path + dir_len + 1;

    /* must not contain further '/' (i.e., must be direct child) */
    if (rest[0] == '\0' || strchr (rest, '/') != NULL)
        return NULL;

    return rest;
}

/* --------------------------------------------------------------------------------------------- */

/* The name of the directory of @dir that @path lies in, or NULL when @path is
   not below @dir or is at that level itself. */
static char *
arcmc_level_name (const char *path, const char *dir)
{
    const char *rest;
    const char *slash;

    if (!is_under_dir (path, dir))
        return NULL;

    rest = (dir == NULL || dir[0] == '\0') ? path : path + strlen (dir) + 1;
    slash = strchr (rest, '/');

    return slash == NULL ? NULL : g_strndup (rest, (gsize) (slash - rest));
}

/* --------------------------------------------------------------------------------------------- */

/* The sizes of the directories of @dir named in @names, or of all of them when @names is NULL,
   at once: one pass over the entries, so that a whole panel level costs what a single directory
   costs. Returns how many directories were counted. */
guint
arcmc_compute_named_dir_sizes (GPtrArray *entries, const char *dir, GHashTable *names)
{
    GHashTable *dirs;
    guint found;
    guint i;

    if (entries == NULL)
        return 0;

    /* the name of each directory of this level, and the entry to count into */
    dirs = g_hash_table_new (g_str_hash, g_str_equal);

    for (i = 0; i < entries->len; i++)
    {
        arcmc_entry_t *e = (arcmc_entry_t *) g_ptr_array_index (entries, i);
        const char *name;

        if (!S_ISDIR (e->mode))
            continue;

        name = is_direct_child (e->full_path, dir);
        if (name == NULL || (names != NULL && !g_hash_table_contains (names, name)))
            continue;

        e->dir_size = 0;
        e->dir_size_computed = TRUE;
        g_hash_table_insert (dirs, (gpointer) name, e);
    }

    found = g_hash_table_size (dirs);
    if (found != 0)
        for (i = 0; i < entries->len; i++)
        {
            const arcmc_entry_t *e = (const arcmc_entry_t *) g_ptr_array_index (entries, i);
            arcmc_entry_t *level;
            char *name;

            if (S_ISDIR (e->mode))
                continue;

            name = arcmc_level_name (e->full_path, dir);
            if (name == NULL)
                continue;

            level = (arcmc_entry_t *) g_hash_table_lookup (dirs, name);
            if (level != NULL)
                level->dir_size += e->size;

            g_free (name);
        }

    g_hash_table_destroy (dirs);
    return found;
}

/* --------------------------------------------------------------------------------------------- */

void
arcmc_compute_level_dir_sizes (GPtrArray *entries, const char *dir)
{
    (void) arcmc_compute_named_dir_sizes (entries, dir, NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* The path inside the archive, without a leading slash, with "." and ".." folded;
   "" is the root. */
static char *
arcmc_path_normalize (const char *path)
{
    char **parts;
    GPtrArray *kept;
    GString *out;
    guint i;

    parts = g_strsplit (path, "/", -1);
    kept = g_ptr_array_new ();

    for (i = 0; parts[i] != NULL; i++)
    {
        if (parts[i][0] == '\0' || strcmp (parts[i], ".") == 0)
            continue;
        if (strcmp (parts[i], "..") == 0)
        {
            if (kept->len > 0)
                g_ptr_array_remove_index (kept, kept->len - 1);
            continue;
        }
        g_ptr_array_add (kept, parts[i]);
    }

    out = g_string_new ("");
    for (i = 0; i < kept->len; i++)
    {
        if (i > 0)
            g_string_append_c (out, '/');
        g_string_append (out, (const char *) g_ptr_array_index (kept, i));
    }

    g_ptr_array_free (kept, TRUE);
    g_strfreev (parts);

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* Every symbolic link among @entries learns what it leads to inside the archive:
   a link to a directory, a link to a file (link_path names it), or nothing. */
void
arcmc_resolve_links (GPtrArray *entries)
{
    GHashTable *by_path;
    guint i;

    if (entries == NULL)
        return;

    by_path = g_hash_table_new (g_str_hash, g_str_equal);
    for (i = 0; i < entries->len; i++)
    {
        arcmc_entry_t *e = (arcmc_entry_t *) g_ptr_array_index (entries, i);

        g_hash_table_insert (by_path, e->full_path, e);
    }

    for (i = 0; i < entries->len; i++)
    {
        arcmc_entry_t *e = (arcmc_entry_t *) g_ptr_array_index (entries, i);
        const arcmc_entry_t *cur = e;
        char *path = NULL;
        int hops;

        if (!S_ISLNK (e->mode))
            continue;

        g_free (e->link_path);
        e->link_path = NULL;
        e->link_to_dir = FALSE;
        e->stale_link = FALSE;

        for (hops = 0; hops < 32 && cur != NULL && S_ISLNK (cur->mode); hops++)
        {
            const char *target = cur->linkname;
            char *joined;

            if (target == NULL || target[0] == '\0')
            {
                cur = NULL;
                break;
            }

            if (target[0] == '/')
                joined = g_strdup (target);
            else
            {
                const char *slash = strrchr (cur->full_path, '/');

                if (slash == NULL)
                    joined = g_strdup (target);
                else
                    joined = g_strdup_printf ("%.*s/%s", (int) (slash - cur->full_path),
                                              cur->full_path, target);
            }

            g_free (path);
            path = arcmc_path_normalize (joined);
            g_free (joined);

            cur = path[0] == '\0' ? NULL
                                  : (const arcmc_entry_t *) g_hash_table_lookup (by_path, path);
        }

        if (cur == NULL || S_ISLNK (cur->mode))
        {
            e->stale_link = TRUE;
            g_free (path);
            continue;
        }

        e->link_to_dir = S_ISDIR (cur->mode);
        e->link_path = path;
    }

    g_hash_table_destroy (by_path);
}

/* --------------------------------------------------------------------------------------------- */
