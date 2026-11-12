/** \file dup_cache.h
 *  \brief Header: the hashes of files kept between searches
 */

#ifndef MC__PANEL_PLUGIN_DUP_CACHE_H
#define MC__PANEL_PLUGIN_DUP_CACHE_H

#include <sys/stat.h>
#include <time.h>

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* An entry not used for this long is dropped when the cache is saved. */
#define DUP_CACHE_KEEP_DAYS 90

typedef struct dup_cache_s dup_cache_t;

typedef enum
{
    DUP_CACHE_PARTIAL = 0,  // SHA-256 of the first and the last 4 KiB
    DUP_CACHE_FULL,         // SHA-256 of the file
    DUP_CACHE_TEXT,         // SHA-256 of the normalized text
    DUP_CACHE_PHASH,        // perceptual hash of the picture, 16 hex digits
    DUP_CACHE_KINDS
} dup_cache_kind_t;

/*** declarations of public functions ************************************************************/

/* A missing or damaged file is an empty cache; @path NULL keeps it in memory only. */
dup_cache_t *dup_cache_load (const char *path, time_t now);
gboolean dup_cache_save (dup_cache_t *cache);
/* The engine holds a reference while its worker may still read the cache.  The last reference
   saves what is not saved yet. */
dup_cache_t *dup_cache_ref (dup_cache_t *cache);
void dup_cache_free (dup_cache_t *cache);

/* What tells one file from another: device, inode, size, mtime and ctime. */
char *dup_cache_file_id (const struct stat *st);

/* These two may be called from any thread.  The caller frees the digest. */
char *dup_cache_lookup (dup_cache_t *cache, const char *id, dup_cache_kind_t kind);
void dup_cache_store (dup_cache_t *cache, const char *id, dup_cache_kind_t kind,
                      const char *digest);

guint dup_cache_size (dup_cache_t *cache);

/*** inline functions ****************************************************************************/

#endif
