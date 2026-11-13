/** \file dup_engine_int.h
 *  \brief Header: what the search for similar files shares with the engine
 */

#ifndef MC__PANEL_PLUGIN_DUP_ENGINE_INT_H
#define MC__PANEL_PLUGIN_DUP_ENGINE_INT_H

#include "lib/global.h"

#include "dup_engine.h"

/*** structures declarations (and typedefs of structures) ****************************************/

struct duplicates_s
{
    duplicates_criteria_t criteria;
    off_t size_min;
    off_t size_max;

    GPtrArray *files;    // duplicates_file_t *, owned
    GHashTable *inodes;  // duplicates_file_t * by dev+ino, to keep hard links once
    GPtrArray *pending;  // GPtrArray of duplicates_file_t *, the groups the worker hashes
    GPtrArray *groups;   // duplicates_group_t *, the result
    gboolean sorted;

    GMutex lock;  // for progress, which the worker updates
    duplicates_progress_t progress;
    duplicates_stage_t end_stage;  // set by the worker before it says it ends

    dup_cache_t *cache;  // a reference of its own, or NULL
    dup_similar_method_t method;
    int percent;

    gint notify_fd;      // -1, or where the worker says it has news
    gint64 last_notify;  // worker only

    GThread *worker;
    GAsyncQueue *queue;  // duplicates_group_t *, then end_marker
    gint cancel;
    gint opened;
    gint refs;  // the caller, and the worker while it runs

    void (*hook) (duplicates_t *d, duplicates_stage_t stage, const char *path, gpointer user_data);
    gpointer hook_data;
};

/*** declarations of public functions ************************************************************/

/* The worker: @skipped are bytes counted in the total that are not going to be read */
void dup_engine_progress (duplicates_t *d, guint files, guint64 read, guint64 skipped,
                          guint errors);
void dup_engine_progress_file (duplicates_t *d, duplicates_stage_t stage, const char *path);
void dup_engine_push_group (duplicates_t *d, duplicates_group_t *g);
void dup_engine_end (duplicates_t *d, gboolean cancelled);
gboolean dup_engine_cancelled (duplicates_t *d);
void dup_engine_unref (duplicates_t *d);

gboolean dup_engine_file_changed (const duplicates_file_t *f);
char *dup_engine_hash_file (duplicates_t *d, duplicates_stage_t stage, const duplicates_file_t *f,
                            char *buf, gboolean *cancelled);
duplicates_group_t *dup_engine_group_new (GPtrArray *files, gboolean same_size);
void dup_engine_pending_free (gpointer p);

/* dup_engine_similar.c: the worker of a search for similar content */
gpointer dup_engine_worker_similar (duplicates_t *d);

/*** inline functions ****************************************************************************/

#endif
