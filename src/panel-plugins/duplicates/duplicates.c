/*
   Panel plugin duplicates for the M-Commander
   The command, and the panel that shows the duplicates found

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

/** \file duplicates.c
 *  \brief Source: the command, and the panel that shows the duplicates found
 *
 *  A search goes on after "Show found" and after its panel is left; the panel shows the groups
 *  as they come, and the drive menu opens it again.
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/panel-plugin.h"
#include "lib/tty/key.h"  // add_select_channel()
#include "lib/vfs/vfs.h"
#include "lib/widget.h"
#include "lib/widget/dialog-switch.h"  // filemanager, top_dlg

#include "src/filemanager/dir.h"
#include "src/filemanager/filemanager.h"  // the_menubar
#include "src/filemanager/layout.h"       // get_other_panel()
#include "src/filemanager/panel.h"

#include "dup_dialog.h"
#include "dup_engine.h"
#include "dup_search.h"
#include "dup_session.h"

/*** file scope macro definitions ****************************************************************/

#define TICK_MS     300
#define TICK_MAX_MS 5000

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *name;  // as the panel lists it
    char *path;  // absolute
    char *id;    // what the file was when it was hashed: dup_cache_file_id ()
    guint group;
    off_t size;
    int similarity;  // percent to the first file of the group, or 0
    gboolean reference;
} dup_entry_t;

typedef struct
{
    mc_panel_host_t *host;
    dup_session_t *session;  // NULL when a new search has replaced it
    char *base;              // the directory of the panel
    char *title;
    GPtrArray *entries;   // dup_entry_t *, by group
    GHashTable *by_name;  // name -> dup_entry_t *
    gboolean has_reference;
    gboolean similar;
    char footer[128];
    guint shown;       // groups listed
    guint64 freed;     // what deleting all but the largest file of each would free
    char *listed_sig;  // what the list was built from: see session_sig ()
} dup_panel_t;

/*** forward declarations (file scope functions) *************************************************/

static void *dup_open (mc_panel_host_t *host, const char *open_path);
static void *dup_action_find (mc_panel_host_t *host, const char *open_path);
static void *dup_action_stop (mc_panel_host_t *host, const char *open_path);
static void dup_close (void *plugin_data);
static mc_pp_result_t dup_get_items (void *plugin_data, void *list_ptr);
static const char *dup_get_title (void *plugin_data);
static const char *dup_get_footer (void *plugin_data);
static const char *dup_get_default_format (void *plugin_data);
static const mc_panel_column_t *dup_get_columns (void *plugin_data, size_t *count);
static const char *dup_get_column_value (void *plugin_data, const char *fname,
                                         const char *column_id);
static mc_pp_result_t dup_delete_items (void *plugin_data, const char **names, int count);
static mc_pp_result_t dup_reload (void *plugin_data);
static mc_pp_result_t dup_handle_key (void *plugin_data, int key);
static mc_pp_result_t dup_get_help_info (void *plugin_data, const char **filename,
                                         const char **node);
static void dup_shutdown (void);
static void forget_entry (dup_panel_t *data, dup_entry_t *e);

/*** file scope variables ************************************************************************/

static const mc_pp_action_t dup_actions[] = {
    { N_ ("Find &duplicates..."), dup_action_find },
    { N_ ("Stop the search for duplicates"), dup_action_stop },
};

static const mc_pp_cmd_menu_entry_t dup_menu[] = {
    { N_ ("Find &duplicates..."), 0, NULL, 0, MC_PP_MENU_COMMAND },
    { N_ ("Stop the search for duplicates"), 1, NULL, 0, MC_PP_MENU_COMMAND },
};

static const mc_panel_column_t dup_columns[] = {
    { "group", N_ ("Grp"), 4, FALSE, J_RIGHT, TRUE },
    { "reference", N_ ("Ref"), 3, FALSE, J_LEFT, TRUE },
    { "similarity", N_ ("Sim"), 4, FALSE, J_RIGHT, TRUE },
};

static const mc_panel_plugin_t dup_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "duplicates",
    .display_name = N_ ("Duplicates"),
    .proto = "duplicates",
    .prefix = "duplicates:",
    .flags = MC_PPF_LOCAL_FILES | MC_PPF_DELETE | MC_PPF_CUSTOM_TITLE | MC_PPF_SHOW_IN_DRIVE_MENU,

    .open = dup_open,
    .close = dup_close,
    .get_items = dup_get_items,
    .delete_items = dup_delete_items,
    .get_title = dup_get_title,
    .get_footer = dup_get_footer,
    .get_default_format = dup_get_default_format,
    .get_columns = dup_get_columns,
    .get_column_value = dup_get_column_value,
    .reload = dup_reload,
    .handle_key = dup_handle_key,
    .get_help_info = dup_get_help_info,
    .shutdown = dup_shutdown,
    .default_sort_id = "unsorted",

    .actions = dup_actions,
    .action_count = G_N_ELEMENTS (dup_actions),
    .cmd_menu_entries = dup_menu,
    .cmd_menu_entry_count = G_N_ELEMENTS (dup_menu),
};

static dup_session_t *session = NULL;
static GSList *panels = NULL;  // dup_panel_t *, the open panels
static int notify_pipe[2] = { -1, -1 };

/* asks on_notify() again while the panels cannot be redrawn */
static GThread *ticker = NULL;
static GMutex ticker_lock;
static GCond ticker_cond;
static gboolean ticker_wanted = FALSE;
static gboolean ticker_quit = FALSE;
static int ticker_ms = TICK_MS;  // grows while the panels stay hidden

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
entry_free (gpointer p)
{
    dup_entry_t *e = (dup_entry_t *) p;

    g_free (e->name);
    g_free (e->path);
    g_free (e->id);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

/* The directory of the other panel, when it shows a local one */
static char *
other_panel_dir (void)
{
    WPanel *other;

    other = get_other_panel ();
    if (other == NULL || other->is_plugin_panel || !vfs_file_is_local (other->cwd_vpath))
        return NULL;

    return g_strdup (vfs_path_as_str (other->cwd_vpath));
}

/* --------------------------------------------------------------------------------------------- */

static char *
format_size (guint64 bytes)
{
    return g_format_size_full (bytes, G_FORMAT_SIZE_IEC_UNITS);
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_footer (dup_panel_t *data, guint groups, guint64 freed)
{
    const dup_session_t *s = data->session;
    char *size;

    size = format_size (freed);

    if (s != NULL && s->state == DUP_SESSION_WALKING)
        g_snprintf (data->footer, sizeof (data->footer), _ ("walking, %u files"),
                    (guint) g_atomic_int_get (&((dup_session_t *) s)->walk_taken));
    else if (s != NULL && s->state == DUP_SESSION_HASHING)
        g_snprintf (data->footer, sizeof (data->footer),
                    ngettext ("%u/%u hashed, %u group", "%u/%u hashed, %u groups", groups),
                    s->progress.files_done, s->progress.files_total, groups);
    else if (s != NULL && s->state == DUP_SESSION_STOPPED)
        g_snprintf (data->footer, sizeof (data->footer),
                    ngettext ("stopped, %u group, %s", "stopped, %u groups, %s", groups), groups,
                    size);
    else
        g_snprintf (data->footer, sizeof (data->footer),
                    ngettext ("%u group, %s", "%u groups, %s", groups), groups, size);

    // the files and the directories not read may hold duplicates too
    if (s != NULL)
    {
        const guint unread =
            s->progress.errors + (guint) g_atomic_int_get (&((dup_session_t *) s)->walk_errors);

        if (unread != 0)
        {
            const size_t len = strlen (data->footer);

            g_snprintf (data->footer + len, sizeof (data->footer) - len, _ (", %u unread"), unread);
        }
    }

    g_free (size);
}

/* --------------------------------------------------------------------------------------------- */

/* Changes when the groups found, or the files deleted, change */
static char *
session_sig (dup_session_t *s)
{
    return g_strdup_printf ("%u:%u:%u", dup_session_groups (s)->len,
                            duplicates_files_count (s->engine), g_hash_table_size (s->deleted));
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
session_changed (const dup_panel_t *data)
{
    char *sig;
    gboolean changed;

    sig = session_sig (data->session);
    changed = g_strcmp0 (sig, data->listed_sig) != 0;
    g_free (sig);
    return changed;
}

/* --------------------------------------------------------------------------------------------- */

/* The groups of the session without the files deleted since, and those left with one file */
static void
panel_rebuild (dup_panel_t *data)
{
    dup_session_t *s = data->session;
    const GPtrArray *groups;
    guint i, j, shown = 0;
    guint64 freed = 0;

    if (s == NULL)
        return;

    g_hash_table_remove_all (data->by_name);
    g_ptr_array_set_size (data->entries, 0);
    data->similar = (s->search.criteria & DUPLICATES_SIMILAR) != 0;

    groups = dup_session_groups (s);
    for (i = 0; i < groups->len; i++)
    {
        const duplicates_group_t *g = (const duplicates_group_t *) g_ptr_array_index (groups, i);
        guint left = 0;

        for (j = 0; j < g->files->len; j++)
            if (!g_hash_table_contains (
                    s->deleted,
                    ((const duplicates_file_t *) g_ptr_array_index (g->files, j))->path))
                left++;
        if (left < 2)
            continue;

        shown++;
        for (j = 0; j < g->files->len; j++)
        {
            const duplicates_file_t *f =
                (const duplicates_file_t *) g_ptr_array_index (g->files, j);
            dup_entry_t *e;

            if (g_hash_table_contains (s->deleted, f->path))
                continue;

            e = g_new0 (dup_entry_t, 1);
            e->path = g_strdup (f->path);
            e->id = g_strdup (f->id);
            e->name = strcmp (data->base, PATH_SEP_STR) == 0
                ? g_strdup (f->path + 1)
                : g_strdup (f->path + strlen (data->base) + 1);
            e->group = shown;
            e->size = f->size;
            e->similarity = f->similarity;
            e->reference = dup_session_is_reference (s, f->path);
            data->has_reference = data->has_reference || e->reference;

            g_ptr_array_add (data->entries, e);
            g_hash_table_insert (data->by_name, e->name, e);
        }
        freed += dup_session_group_freed (s, g);
    }

    data->shown = shown;
    data->freed = freed;
    g_free (data->listed_sig);
    data->listed_sig = session_sig (s);
    panel_footer (data, shown, freed);
}

/* --------------------------------------------------------------------------------------------- */

/* Shows again what the panel lists; the core keeps the cursor, the marks are kept here */
static void
panel_redraw (dup_panel_t *data)
{
    WPanel *panel = (WPanel *) data->host->host_data;
    GHashTable *marked = NULL;
    int i;

    if (panel == NULL || !panel->is_plugin_panel || panel->plugin_data != data)
        return;

    if (panel->marked != 0)
    {
        marked = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
        for (i = 0; i < panel->dir.len; i++)
            if (panel->dir.list[i].f.marked != 0)
                g_hash_table_add (marked, g_strdup (panel->dir.list[i].fname->str));
    }

    panel_plugin_refresh (panel);

    if (marked != NULL)
    {
        for (i = 0; i < panel->dir.len; i++)
            if (g_hash_table_contains (marked, panel->dir.list[i].fname->str))
                do_file_mark (panel, i, 1);
        g_hash_table_destroy (marked);
    }

    widget_draw (WIDGET (panel));
}

/* --------------------------------------------------------------------------------------------- */

/* A dialog or a dropped menu is over the panels: a redraw would show through it */
static gboolean
can_redraw (void)
{
    return filemanager != NULL && top_dlg != NULL && DIALOG (top_dlg->data) == filemanager
        && (the_menubar == NULL || !the_menubar->is_dropped);
}

/* --------------------------------------------------------------------------------------------- */

static void
ticker_want (gboolean want)
{
    g_mutex_lock (&ticker_lock);
    if (want && !ticker_wanted)
        ticker_ms = TICK_MS;
    ticker_wanted = want;
    g_cond_signal (&ticker_cond);
    g_mutex_unlock (&ticker_lock);
}

/* --------------------------------------------------------------------------------------------- */

static gpointer
ticker_main (gpointer data)
{
    ssize_t n;

    (void) data;

    g_mutex_lock (&ticker_lock);
    while (!ticker_quit)
    {
        gint64 until;

        if (!ticker_wanted)
        {
            g_cond_wait (&ticker_cond, &ticker_lock);
            continue;
        }
        until = g_get_monotonic_time () + ticker_ms * G_TIME_SPAN_MILLISECOND;
        if (g_cond_wait_until (&ticker_cond, &ticker_lock, until) || !ticker_wanted)
            continue;  // woken by ticker_want () or shutdown
        ticker_ms = MIN (ticker_ms * 2, TICK_MAX_MS);
        g_mutex_unlock (&ticker_lock);
        n = write (notify_pipe[1], "", 1);  // a full pipe is fine: on_notify () reads it all
        (void) n;
        g_mutex_lock (&ticker_lock);
    }
    g_mutex_unlock (&ticker_lock);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static int
on_notify (int fd, void *info)
{
    GSList *l;
    char *sig;

    (void) info;

    dup_drain (fd);
    if (session == NULL)
    {
        ticker_want (FALSE);
        return 0;
    }

    dup_session_update (session);

    if (!can_redraw ())
    {
        ticker_want (panels != NULL);
        return 0;
    }
    ticker_want (FALSE);

    sig = session_sig (session);
    for (l = panels; l != NULL; l = g_slist_next (l))
    {
        dup_panel_t *data = (dup_panel_t *) l->data;

        if (data->session == NULL)
            continue;
        if (g_strcmp0 (sig, data->listed_sig) != 0)
        {
            panel_rebuild (data);
            panel_redraw (data);
        }
        else
        {
            // the progress in the footer only: the list and its stat () calls stay
            panel_footer (data, data->shown, data->freed);
            widget_draw (WIDGET ((WPanel *) data->host->host_data));
        }
    }
    g_free (sig);
    mc_refresh ();
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ensure_pipe (void)
{
    if (notify_pipe[0] != -1)
        return TRUE;

    if (pipe (notify_pipe) != 0)
        return FALSE;

    (void) fcntl (notify_pipe[0], F_SETFL, O_NONBLOCK);
    (void) fcntl (notify_pipe[1], F_SETFL, O_NONBLOCK);
    (void) fcntl (notify_pipe[0], F_SETFD, FD_CLOEXEC);
    (void) fcntl (notify_pipe[1], F_SETFD, FD_CLOEXEC);
    add_select_channel (notify_pipe[0], on_notify, NULL);
    ticker = g_thread_new ("duplicates-tick", ticker_main, NULL);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
session_end (void)
{
    GSList *l;

    for (l = panels; l != NULL; l = g_slist_next (l))
        ((dup_panel_t *) l->data)->session = NULL;

    dup_session_free (session);
    session = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static dup_panel_t *
open_session_panel (mc_panel_host_t *host)
{
    dup_panel_t *data;

    data = g_new0 (dup_panel_t, 1);
    data->host = host;
    data->session = session;
    data->base = g_strdup (session->base);
    data->title = g_strdup (session->search.start_dir);
    data->entries = g_ptr_array_new_with_free_func (entry_free);
    data->by_name = g_hash_table_new (g_str_hash, g_str_equal);
    panel_rebuild (data);

    panels = g_slist_prepend (panels, data);
    if (host->set_cwd != NULL)
        host->set_cwd (host, data->base);
    return data;
}

/* --------------------------------------------------------------------------------------------- */

/* The drive menu and the history open the panel of the search there is */
static void *
dup_open (mc_panel_host_t *host, const char *open_path)
{
    if (session != NULL)
        return open_session_panel (host);

    // an entry of the directory history: "duplicates:" and the start directory
    if (open_path != NULL && g_str_has_prefix (open_path, dup_plugin.prefix))
    {
        open_path += strlen (dup_plugin.prefix);
        if (*open_path == '\0')
            open_path = NULL;
    }
    return dup_action_find (host, open_path);
}

/* --------------------------------------------------------------------------------------------- */

static void *
dup_action_find (mc_panel_host_t *host, const char *open_path)
{
    dup_search_t search;
    char *other;
    guint groups;

    if (dup_session_running (session))
    {
        if (query_dialog (_ ("Find duplicates"),
                          _ ("A search for duplicates is running.\nStop it and start a new one?"),
                          D_NORMAL, 2, _ ("&Yes"), _ ("&No"))
            != 0)
            return open_session_panel (host);
    }

    other = other_panel_dir ();
    if (!dup_dialog_run (open_path != NULL ? open_path : ".", other, &search))
    {
        g_free (other);
        return NULL;
    }
    g_free (other);

    if (!ensure_pipe ())
    {
        message (D_ERROR, MSG_ERROR, _ ("Cannot create a pipe:\n%s"), g_strerror (errno));
        dup_search_clear (&search);
        return NULL;
    }

    session_end ();
    session = dup_session_start (&search, notify_pipe[1]);

    switch (dup_progress_run (session, notify_pipe[0]))
    {
    case DUP_PROGRESS_CANCEL:
        session_end ();
        return NULL;
    case DUP_PROGRESS_SHOW:
        return open_session_panel (host);
    default:
        break;
    }

    groups = dup_session_groups (session)->len;
    if (groups == 0)
    {
        const guint taken = (guint) g_atomic_int_get (&session->walk_taken);
        const guint unread = session->progress.errors;
        const guint unread_dirs = (guint) g_atomic_int_get (&session->walk_errors);
        char *text;

        if (session->state != DUP_SESSION_DONE)
            text = g_strdup (_ ("No group was complete when stopped."));
        else
            text = g_strdup_printf (ngettext ("No duplicates found among %u file.",
                                              "No duplicates found among %u files.", taken),
                                    taken);
        if (unread != 0)
        {
            char *more;

            more = g_strdup_printf (
                ngettext ("%s\n%u file could not be read, it was not compared.",
                          "%s\n%u files could not be read, they were not compared.", unread),
                text, unread);
            g_free (text);
            text = more;
        }
        if (unread_dirs != 0)
        {
            char *more;

            more = g_strdup_printf (
                ngettext ("%s\n%u directory could not be read, its files were not seen.",
                          "%s\n%u directories could not be read, their files were not seen.",
                          unread_dirs),
                text, unread_dirs);
            g_free (text);
            text = more;
        }
        message (unread + unread_dirs != 0 ? D_ERROR : D_NORMAL, _ ("Find duplicates"), "%s", text);
        g_free (text);
        session_end ();
        return NULL;
    }

    return open_session_panel (host);
}

/* --------------------------------------------------------------------------------------------- */

static void *
dup_action_stop (mc_panel_host_t *host, const char *open_path)
{
    (void) host;
    (void) open_path;

    if (!dup_session_running (session))
        message (D_NORMAL, _ ("Find duplicates"), "%s", _ ("No search for duplicates is running."));
    else
        dup_session_stop (session);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
dup_close (void *plugin_data)
{
    dup_panel_t *data = (dup_panel_t *) plugin_data;

    if (data == NULL)
        return;

    panels = g_slist_remove (panels, data);
    g_hash_table_destroy (data->by_name);
    g_ptr_array_free (data->entries, TRUE);
    g_free (data->base);
    g_free (data->title);
    g_free (data->listed_sig);
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
dup_get_items (void *plugin_data, void *list_ptr)
{
    dup_panel_t *data = (dup_panel_t *) plugin_data;
    dir_list *list = (dir_list *) list_ptr;
    gboolean gone = FALSE;
    guint i;

    /* a file moved or deleted outside the plugin, or in the other panel of the same search:
       the groups are made again, without it */
    for (i = data->entries->len; i > 0; i--)
    {
        dup_entry_t *e = (dup_entry_t *) g_ptr_array_index (data->entries, i - 1);
        struct stat st;

        if (lstat (e->path, &st) != 0)
        {
            forget_entry (data, e);
            gone = TRUE;
        }
    }
    if (gone || (data->session != NULL && session_changed (data)))
        panel_rebuild (data);

    for (i = 0; i < data->entries->len; i++)
    {
        const dup_entry_t *e = (const dup_entry_t *) g_ptr_array_index (data->entries, i);
        struct stat st;

        if (stat (e->path, &st) == 0)
            dir_list_append (list, e->name, &st, FALSE, FALSE);
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
dup_get_title (void *plugin_data)
{
    return ((dup_panel_t *) plugin_data)->title;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
dup_get_footer (void *plugin_data)
{
    return ((dup_panel_t *) plugin_data)->footer;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
dup_get_default_format (void *plugin_data)
{
    const dup_panel_t *data = (const dup_panel_t *) plugin_data;

    if (data->similar)
        return data->has_reference ? "half group | reference | similarity | name | size | mtime"
                                   : "half group | similarity | name | size | mtime";
    return data->has_reference ? "half group | reference | name | size | mtime"
                               : "half group | name | size | mtime";
}

/* --------------------------------------------------------------------------------------------- */

static const mc_panel_column_t *
dup_get_columns (void *plugin_data, size_t *count)
{
    (void) plugin_data;
    *count = G_N_ELEMENTS (dup_columns);
    return dup_columns;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
dup_get_column_value (void *plugin_data, const char *fname, const char *column_id)
{
    static char buf[16];
    dup_panel_t *data = (dup_panel_t *) plugin_data;
    const dup_entry_t *e;

    e = (const dup_entry_t *) g_hash_table_lookup (data->by_name, fname);
    if (e == NULL)
        return "";

    if (strcmp (column_id, "group") == 0)
    {
        g_snprintf (buf, sizeof (buf), "%u", e->group);
        return buf;
    }
    if (strcmp (column_id, "reference") == 0)
        return e->reference ? _ ("ref") : "";
    if (strcmp (column_id, "similarity") == 0 && e->similarity != 0)
    {
        g_snprintf (buf, sizeof (buf), "%d%%", e->similarity);
        return buf;
    }
    return "";
}

/* --------------------------------------------------------------------------------------------- */

/* The file is gone from the panel, and from the panels opened later on the same search */
static void
forget_entry (dup_panel_t *data, dup_entry_t *e)
{
    if (data->session != NULL)
        g_hash_table_add (data->session->deleted, g_strdup (e->path));
    g_hash_table_remove (data->by_name, e->name);
    g_ptr_array_remove (data->entries, e);
}

/* --------------------------------------------------------------------------------------------- */

/* The file is still the one that was hashed */
static gboolean
entry_unchanged (const dup_entry_t *e)
{
    struct stat st;
    char *id;
    gboolean same;

    if (stat (e->path, &st) != 0)
        return FALSE;
    id = dup_cache_file_id (&st);
    same = g_strcmp0 (id, e->id) == 0;
    g_free (id);
    return same;
}

/* --------------------------------------------------------------------------------------------- */

/* The groups that @doomed would leave with no copy on the disk */
static guint
groups_left_empty (const dup_panel_t *data, GHashTable *doomed)
{
    GHashTable *kept;  // groups with a copy that stays
    GHashTable *hit;   // groups something is deleted from
    guint i, empty = 0;
    GHashTableIter iter;
    gpointer group;

    kept = g_hash_table_new (g_direct_hash, g_direct_equal);
    hit = g_hash_table_new (g_direct_hash, g_direct_equal);

    for (i = 0; i < data->entries->len; i++)
    {
        const dup_entry_t *e = (const dup_entry_t *) g_ptr_array_index (data->entries, i);
        gpointer key = GUINT_TO_POINTER (e->group);

        if (g_hash_table_contains (doomed, e))
            g_hash_table_add (hit, key);
        else if (!g_hash_table_contains (kept, key) && entry_unchanged (e))
            g_hash_table_add (kept, key);
    }

    g_hash_table_iter_init (&iter, hit);
    while (g_hash_table_iter_next (&iter, &group, NULL))
        if (!g_hash_table_contains (kept, group))
            empty++;

    g_hash_table_destroy (hit);
    g_hash_table_destroy (kept);
    return empty;
}

/* --------------------------------------------------------------------------------------------- */

/* Deletes the files from the disk, but never those of the reference directory, nor a file
   changed since it was hashed: it may be no duplicate now.  It reports what it kept itself. */
static mc_pp_result_t
dup_delete_items (void *plugin_data, const char **names, int count)
{
    dup_panel_t *data = (dup_panel_t *) plugin_data;
    GHashTable *doomed;  // dup_entry_t * to unlink
    GPtrArray *order;
    guint reference = 0, changed = 0, empty;
    char *failed = NULL;
    int i;
    guint j;

    doomed = g_hash_table_new (g_direct_hash, g_direct_equal);
    order = g_ptr_array_new ();

    for (i = 0; i < count; i++)
    {
        dup_entry_t *e;

        e = (dup_entry_t *) g_hash_table_lookup (data->by_name, names[i]);
        if (e == NULL || g_hash_table_contains (doomed, e))
            continue;
        if (e->reference)
            reference++;
        else if (!entry_unchanged (e))
            changed++;
        else
        {
            g_hash_table_add (doomed, e);
            g_ptr_array_add (order, e);
        }
    }

    empty = groups_left_empty (data, doomed);
    if (empty != 0)
    {
        char *text;
        int answer;

        text = g_strdup_printf (
            ngettext ("No copy of %u group is left after this.\nDelete the files anyway?",
                      "No copy of %u groups is left after this.\nDelete the files anyway?", empty),
            empty);
        query_set_sel (1);  // Enter keeps the files
        answer = query_dialog (_ ("Find duplicates"), text, D_ERROR, 2, _ ("&Delete"), _ ("&No"));
        g_free (text);
        if (answer != 0)
        {
            g_ptr_array_free (order, TRUE);
            g_hash_table_destroy (doomed);
            return MC_PPR_OK;  // nothing to delete: not a failure for the core to report
        }
    }

    for (j = 0; j < order->len; j++)
    {
        dup_entry_t *e = (dup_entry_t *) g_ptr_array_index (order, j);

        // again: it may have changed while the question was on the screen
        if (!entry_unchanged (e))
        {
            changed++;
            continue;
        }
        if (unlink (e->path) != 0 && errno != ENOENT)
        {
            if (failed == NULL)
                failed = g_strdup_printf ("%s: %s", e->path, g_strerror (errno));
            continue;
        }
        forget_entry (data, e);
    }
    g_ptr_array_free (order, TRUE);
    g_hash_table_destroy (doomed);

    panel_rebuild (data);

    if (reference != 0)
        message (D_NORMAL, _ ("Find duplicates"),
                 ngettext ("%u file of the reference directory is kept.",
                           "%u files of the reference directory are kept.", reference),
                 reference);
    if (changed != 0)
        message (D_NORMAL, _ ("Find duplicates"),
                 ngettext ("%u file changed since the search and is kept.",
                           "%u files changed since the search and are kept.", changed),
                 changed);
    if (failed != NULL)
    {
        message (D_ERROR, MSG_ERROR, _ ("Cannot delete\n%s"), failed);
        g_free (failed);
    }
    // what went wrong is said above; the core would only add "Delete failed"
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
is_reference (const dup_panel_t *data, const char *name)
{
    const dup_entry_t *e;

    e = (const dup_entry_t *) g_hash_table_lookup (data->by_name, name);
    return e != NULL && e->reference;
}

/* --------------------------------------------------------------------------------------------- */

/* F6 is done by the core, which knows nothing of the reference directory: it is refused when
   a file to move is there */
static mc_pp_result_t
dup_handle_key (void *plugin_data, int key)
{
    dup_panel_t *data = (dup_panel_t *) plugin_data;
    const WPanel *panel = (const WPanel *) data->host->host_data;
    gboolean refused = FALSE;

    if ((key != CK_Move && key != CK_MoveSingle) || !data->has_reference || panel == NULL)
        return MC_PPR_NOT_SUPPORTED;

    if (key == CK_Move && panel->marked != 0)
    {
        int i;

        for (i = 0; i < panel->dir.len && !refused; i++)
            refused = panel->dir.list[i].f.marked != 0
                && is_reference (data, panel->dir.list[i].fname->str);
    }
    else
    {
        const file_entry_t *fe = panel_current_entry (panel);

        refused = fe != NULL && is_reference (data, fe->fname->str);
    }

    if (!refused)
        return MC_PPR_NOT_SUPPORTED;

    message (D_ERROR, _ ("Find duplicates"), "%s",
             _ ("The files of the reference directory are not moved."));
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* Ctrl-R: the files gone since are dropped */
static mc_pp_result_t
dup_reload (void *plugin_data)
{
    dup_panel_t *data = (dup_panel_t *) plugin_data;
    guint i;

    for (i = data->entries->len; i > 0; i--)
    {
        dup_entry_t *e = (dup_entry_t *) g_ptr_array_index (data->entries, i - 1);
        struct stat st;

        if (lstat (e->path, &st) != 0)
            forget_entry (data, e);
    }

    panel_rebuild (data);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
dup_get_help_info (void *plugin_data, const char **filename, const char **node)
{
    (void) plugin_data;

    if (node != NULL)
        *node = "[Duplicates panel]";
    if (filename != NULL)
    {
        *filename = dup_help_file ();
        if (*filename == NULL)
            return MC_PPR_NOT_SUPPORTED;
    }
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static void
dup_shutdown (void)
{
    session_end ();

    if (ticker != NULL)
    {
        g_mutex_lock (&ticker_lock);
        ticker_quit = TRUE;
        g_cond_signal (&ticker_cond);
        g_mutex_unlock (&ticker_lock);
        g_thread_join (ticker);
        ticker = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &dup_plugin;
}

/* --------------------------------------------------------------------------------------------- */
