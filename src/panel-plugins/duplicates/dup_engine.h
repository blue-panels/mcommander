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

#ifndef MC__PANEL_PLUGIN_DUP_ENGINE_H
#define MC__PANEL_PLUGIN_DUP_ENGINE_H

#include <sys/types.h>
#include <sys/stat.h>

#include "lib/global.h"

#include "dup_cache.h"
#include "dup_similar.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* The partial hash reads this much from the start and from the end of a file; a file of twice
   that size or less is read once, whole. */
#define DUPLICATES_EDGE_SIZE    (4 * 1024)
#define DUPLICATES_PARTIAL_SIZE (2 * DUPLICATES_EDGE_SIZE)

/* Bytes read at once for the full hash. */
#define DUPLICATES_CHUNK_SIZE (1024 * 1024)

/*** enums ***************************************************************************************/

typedef enum
{
    DUPLICATES_BY_SIZE = 1 << 0,
    DUPLICATES_BY_NAME = 1 << 1,
    DUPLICATES_NAME_CASE_SENS = 1 << 2,
    DUPLICATES_BY_CONTENT = 1 << 3,
    DUPLICATES_SIMILAR = 1 << 4  // the content compared by duplicates_set_similar ()
} duplicates_criteria_t;

typedef enum
{
    DUPLICATES_STAGE_COLLECT = 0,  // files are being added
    DUPLICATES_STAGE_PARTIAL,      // hashing the heads of the files
    DUPLICATES_STAGE_FULL,         // hashing the rest of the files
    DUPLICATES_STAGE_DONE,         // the result is complete
    DUPLICATES_STAGE_CANCELLED     // stopped; the result holds the groups finished before
} duplicates_stage_t;

/*** structures declarations (and typedefs of structures) ****************************************/

typedef struct
{
    char *path;
    off_t size;
    time_t mtime;
    mode_t mode;
    char *id;        // dup_cache_file_id ()
    int similarity;  // in a group of similar files: percent to the first one
    dev_t dev;
    ino_t ino;
} duplicates_file_t;

/* A group of two or more files that match by every criterion. */
typedef struct
{
    GPtrArray *files;  // duplicates_file_t *: by path; similar images: the largest picture first
    off_t size;        // size of one file, when the group is by size or content; else 0
} duplicates_group_t;

typedef struct
{
    duplicates_stage_t stage;
    guint files_done;     // files that need no more hashing
    guint files_total;    // files that may need hashing
    guint64 bytes_done;   // bytes read so far
    guint64 bytes_total;  // bytes of the files that may need hashing
    guint errors;         // files that could not be read
    char *current;        // file being hashed, or NULL
} duplicates_progress_t;

typedef struct duplicates_s duplicates_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

duplicates_t *duplicates_new (duplicates_criteria_t criteria, off_t size_min, off_t size_max);
void duplicates_free (duplicates_t *d);

gboolean duplicates_add (duplicates_t *d, const char *path, const struct stat *st);

/* With DUPLICATES_SIMILAR: how the content is compared, and how similar two images must be. */
void duplicates_set_similar (duplicates_t *d, dup_similar_method_t method, int percent);

/* The worker writes a byte to @fd when it finds a group, when it ends, and a few times a second
   while it reads; -1 for none.  The caller owns the descriptor. */
void duplicates_set_notify_fd (duplicates_t *d, int fd);

/* The hashes are taken from @cache and put into it; the caller keeps it, and saves it. */
void duplicates_set_cache (duplicates_t *d, dup_cache_t *cache);
void duplicates_start (duplicates_t *d);
duplicates_stage_t duplicates_poll (duplicates_t *d, duplicates_progress_t *progress,
                                    guint64 timeout_us);
duplicates_stage_t duplicates_wait (duplicates_t *d);
void duplicates_cancel (duplicates_t *d);

const GPtrArray *duplicates_get_groups (const duplicates_t *d);
guint64 duplicates_reclaimable (const duplicates_t *d);
guint duplicates_files_count (const duplicates_t *d);

/* For the tests: the number of files opened so far, and a hook called by the
   worker before it hashes each file, from the cache or not. */
guint duplicates_files_opened (const duplicates_t *d);
void duplicates_set_file_hook (duplicates_t *d,
                               void (*hook) (duplicates_t *d, duplicates_stage_t stage,
                                             const char *path, gpointer user_data),
                               gpointer user_data);

/*** inline functions ****************************************************************************/

#endif
