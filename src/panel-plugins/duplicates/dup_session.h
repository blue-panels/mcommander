/** \file dup_session.h
 *  \brief Header: a search for duplicates that goes on without a dialog
 */

#ifndef MC__PANEL_PLUGIN_DUP_SESSION_H
#define MC__PANEL_PLUGIN_DUP_SESSION_H

#include "lib/global.h"

#include "dup_cache.h"
#include "dup_dialog.h"
#include "dup_engine.h"

/*** typedefs(not structures) and defined constants **********************************************/

typedef enum
{
    DUP_SESSION_WALKING = 0,
    DUP_SESSION_HASHING,
    DUP_SESSION_DONE,
    DUP_SESSION_STOPPED  // the groups complete when it was stopped
} dup_session_state_t;

/*** structures declarations (and typedefs of structures) ****************************************/

typedef struct dup_session_s
{
    dup_search_t search;
    char *base;  // the directory the names in the panel are relative to
    dup_session_state_t state;
    duplicates_progress_t progress;  // of the hashing, read in the main thread
    GHashTable *deleted;             // paths the user deleted: not shown again

    /* below: the walk, in a thread of its own */
    dup_cache_t *cache;
    duplicates_t *engine;
    GThread *walker;
    gint walk_done;
    gint cancel;
    gint walk_files;
    gint walk_errors;  // directories that could not be read
    gint walk_taken;
    GMutex lock;           // for walk_dir, walk_bytes and physical
    GHashTable *physical;  // path -> where it is on the disk, for the files reached by a link
    char *walk_dir;
    guint64 walk_bytes;
    int notify_fd;
    gint refs;  // the owner, and the walker while it runs
} dup_session_t;

/*** declarations of public functions ************************************************************/

/* Takes @search.  The threads write a byte to @notify_fd when there is news. */
dup_session_t *dup_session_start (dup_search_t *search, int notify_fd);

/* In the main thread: takes what the threads did; starts the hashing when the walk is over. */
void dup_session_update (dup_session_t *s);

/* Stops the search; the groups complete so far stay */
void dup_session_stop (dup_session_t *s);

/* Does not wait for a walker stuck on a dead mount: it frees the rest when it returns */
void dup_session_free (dup_session_t *s);

gboolean dup_session_running (const dup_session_t *s);

/* What the walk has done: the directory it reads, files seen and taken, their bytes */
void dup_session_walk_state (dup_session_t *s, char **dir, guint *files, guint *taken,
                             guint64 *bytes);

const GPtrArray *dup_session_groups (dup_session_t *s);

/* A file of the reference directory, by where it is on the disk: a link from the start
   directory into the reference one does not make its files deletable */
gboolean dup_session_is_reference (dup_session_t *s, const char *path);

/* What deleting the extra copies frees: in one group, and in all of them */
guint64 dup_session_group_freed (dup_session_t *s, const duplicates_group_t *g);
guint64 dup_session_reclaimable (dup_session_t *s);

/*** inline functions ****************************************************************************/

#endif
