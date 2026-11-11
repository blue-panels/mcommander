/*
   Panel plugins for the M-Commander
   What is known about the modules without opening them

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

/** \file panel_plugin_cache.c
 *  \brief Source: what is known about panel plugin modules without opening them
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"
#include "lib/panel_plugin_cache.h"

/*** file scope macro definitions ****************************************************************/

#define CACHE_VERSION     2
#define CACHE_MAX_ENTRIES 256

#define GROUP_HEADER      "Cache"
#define DESCRIPTION_GROUP "Plugin"

/*** file scope type declarations ****************************************************************/

struct mc_pp_cache_t
{
    char *path;
    GKeyFile *in;
    GKeyFile *out;
    GHashTable *groups; /* so path -> group in @in */
    guint in_count;
    guint out_count;
    gboolean changed;
};

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static long
stat_mtime_nsec (const struct stat *st)
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
stat_ctime_nsec (const struct stat *st)
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

/* ctime cannot be set from user space: a file put in place again never keeps it */
char *
mc_pp_cache_file_id (const struct stat *st)
{
    return g_strdup_printf ("%" G_GUINT64_FORMAT ":%" G_GUINT64_FORMAT ":%" G_GINT64_FORMAT
                            ":%" G_GINT64_FORMAT ".%09ld:%" G_GINT64_FORMAT ".%09ld",
                            (guint64) st->st_dev, (guint64) st->st_ino, (gint64) st->st_size,
                            (gint64) st->st_mtime, stat_mtime_nsec (st), (gint64) st->st_ctime,
                            stat_ctime_nsec (st));
}

/* --------------------------------------------------------------------------------------------- */

static char *
indexed_key (const char *kind, int index, const char *field)
{
    return g_strdup_printf ("%s%d_%s", kind, index, field);
}

/* --------------------------------------------------------------------------------------------- */

static void
set_string (GKeyFile *kf, const char *group, const char *key, const char *value)
{
    if (value != NULL)
        g_key_file_set_string (kf, group, key, value);
}

/* --------------------------------------------------------------------------------------------- */

static void
set_indexed_string (GKeyFile *kf, const char *group, const char *kind, int index, const char *field,
                    const char *value)
{
    char *key;

    key = indexed_key (kind, index, field);
    set_string (kf, group, key, value);
    g_free (key);
}

/* --------------------------------------------------------------------------------------------- */

static void
set_indexed_integer (GKeyFile *kf, const char *group, const char *kind, int index,
                     const char *field, int value)
{
    char *key;

    key = indexed_key (kind, index, field);
    g_key_file_set_integer (kf, group, key, value);
    g_free (key);
}

/* --------------------------------------------------------------------------------------------- */

/* NULL when the key is absent, which is how a NULL field was stored */
static char *
get_string (GKeyFile *kf, const char *group, const char *key)
{
    return g_key_file_get_string (kf, group, key, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static char *
get_indexed_string (GKeyFile *kf, const char *group, const char *kind, int index, const char *field)
{
    char *key, *value;

    key = indexed_key (kind, index, field);
    value = get_string (kf, group, key);
    g_free (key);
    return value;
}

/* --------------------------------------------------------------------------------------------- */

/* FALSE when the key is absent or not a number: the entry is damaged */
static gboolean
get_integer (GKeyFile *kf, const char *group, const char *key, int *value)
{
    GError *error = NULL;

    *value = g_key_file_get_integer (kf, group, key, &error);
    if (error == NULL)
        return TRUE;

    g_error_free (error);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
get_indexed_integer (GKeyFile *kf, const char *group, const char *kind, int index,
                     const char *field, int *value)
{
    char *key;
    gboolean ok;

    key = indexed_key (kind, index, field);
    ok = get_integer (kf, group, key, value);
    g_free (key);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
get_count (GKeyFile *kf, const char *group, const char *key, int *count)
{
    return get_integer (kf, group, key, count) && *count >= 0 && *count <= CACHE_MAX_ENTRIES;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
get_boolean (GKeyFile *kf, const char *group, const char *key, gboolean *value)
{
    GError *error = NULL;

    *value = g_key_file_get_boolean (kf, group, key, &error);
    if (error == NULL)
        return TRUE;

    g_error_free (error);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static char *
next_out_group (mc_pp_cache_t *cache)
{
    return g_strdup_printf ("Module%u", cache->out_count++);
}

/* --------------------------------------------------------------------------------------------- */

static void
copy_entry_to_out (mc_pp_cache_t *cache, const char *group)
{
    char **keys;
    char *out_group;
    int i;

    keys = g_key_file_get_keys (cache->in, group, NULL, NULL);
    if (keys == NULL)
        return;

    out_group = next_out_group (cache);
    for (i = 0; keys[i] != NULL; i++)
    {
        char *value;

        value = g_key_file_get_value (cache->in, group, keys[i], NULL);
        if (value != NULL)
            g_key_file_set_value (cache->out, out_group, keys[i], value);
        g_free (value);
    }
    g_free (out_group);
    g_strfreev (keys);
}

/* --------------------------------------------------------------------------------------------- */

/* The keys of the description, as mc_pp_cache_describe() writes them */
static char *
entry_description (GKeyFile *in, const char *group)
{
    GKeyFile *kf;
    char **keys;
    char *data;
    int i;

    kf = g_key_file_new ();
    keys = g_key_file_get_keys (in, group, NULL, NULL);
    for (i = 0; keys != NULL && keys[i] != NULL; i++)
        if (strcmp (keys[i], "path") != 0 && strcmp (keys[i], "file") != 0)
        {
            char *value;

            value = g_key_file_get_value (in, group, keys[i], NULL);
            if (value != NULL)
                g_key_file_set_value (kf, DESCRIPTION_GROUP, keys[i], value);
            g_free (value);
        }
    g_strfreev (keys);

    data = g_key_file_to_data (kf, NULL, NULL);
    g_key_file_free (kf);
    return data;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
entry_is_for (GKeyFile *kf, const char *group, const struct stat *st)
{
    char *stored, *current;
    gboolean same;

    if (g_key_file_get_integer (kf, group, "api", NULL) != MC_PANEL_PLUGIN_API_VERSION)
        return FALSE;

    stored = get_string (kf, group, "file");
    current = mc_pp_cache_file_id (st);
    same = stored != NULL && strcmp (stored, current) == 0;
    g_free (current);
    g_free (stored);
    return same;
}

/* --------------------------------------------------------------------------------------------- */

static int *
stub_set_operations (mc_panel_plugin_t *stub, GKeyFile *kf, const char *group, int count,
                     gboolean *ok)
{
    mc_pp_file_operation_t *ops;
    int *offers;
    int i;

    if (count == 0)
        return NULL;

    ops = g_new0 (mc_pp_file_operation_t, count);
    offers = g_new0 (int, count);
    stub->file_operations = ops;
    stub->file_operation_count = count;
    for (i = 0; i < count; i++)
    {
        int kind = 0;

        ops[i].name = get_indexed_string (kf, group, "op", i, "name");
        if (!get_indexed_integer (kf, group, "op", i, "kind", &kind)
            || !get_indexed_integer (kf, group, "op", i, "offers", &offers[i])
            || kind < MC_PP_FILE_OPERATION_OPEN || kind > MC_PP_FILE_OPERATION_SHOW)
            *ok = FALSE;
        ops[i].kind = (mc_pp_file_operation_kind_t) kind;
    }
    return offers;
}

/* --------------------------------------------------------------------------------------------- */

static void
stub_set_actions (mc_panel_plugin_t *stub, GKeyFile *kf, const char *group, int count)
{
    mc_pp_action_t *actions;
    int i;

    if (count == 0)
        return;

    actions = g_new0 (mc_pp_action_t, count);
    for (i = 0; i < count; i++)
        actions[i].label = get_indexed_string (kf, group, "action", i, "label");
    stub->actions = actions;
    stub->action_count = count;
}

/* --------------------------------------------------------------------------------------------- */

static void
stub_set_menu (mc_panel_plugin_t *stub, GKeyFile *kf, const char *group, int count, gboolean *ok)
{
    mc_pp_cmd_menu_entry_t *entries;
    int i;

    if (count == 0)
        return;

    entries = g_new0 (mc_pp_cmd_menu_entry_t, count);
    stub->cmd_menu_entries = entries;
    stub->cmd_menu_entry_count = count;
    for (i = 0; i < count; i++)
    {
        entries[i].label = get_indexed_string (kf, group, "menu", i, "label");
        entries[i].shortcut = get_indexed_string (kf, group, "menu", i, "shortcut");
        entries[i].menu_name = get_indexed_string (kf, group, "menu", i, "menu_name");
        if (!get_indexed_integer (kf, group, "menu", i, "action", &entries[i].action_index)
            || !get_indexed_integer (kf, group, "menu", i, "key", &entries[i].key))
            *ok = FALSE;
        else if ((entries[i].label != NULL || entries[i].key != 0)
                 && (entries[i].action_index < 0 || entries[i].action_index >= stub->action_count))
            *ok = FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
write_description (GKeyFile *kf, const char *group, const mc_panel_plugin_t *plugin)
{
    int i;

    g_key_file_set_integer (kf, group, "api", plugin->api_version);
    g_key_file_set_string (kf, group, "name", plugin->name);
    set_string (kf, group, "display_name", plugin->display_name);
    set_string (kf, group, "proto", plugin->proto);
    set_string (kf, group, "prefix", plugin->prefix);
    g_key_file_set_integer (kf, group, "flags", (int) plugin->flags);
    set_string (kf, group, "sort", plugin->default_sort_id);
    g_key_file_set_boolean (kf, group, "sort_reverse", plugin->default_sort_reverse);
    g_key_file_set_integer (kf, group, "offers", mc_pp_plugin_offers (plugin));

    g_key_file_set_integer (kf, group, "op_count", plugin->file_operation_count);
    for (i = 0; i < plugin->file_operation_count; i++)
    {
        const mc_pp_file_operation_t *op = &plugin->file_operations[i];

        set_indexed_string (kf, group, "op", i, "name", op->name);
        set_indexed_integer (kf, group, "op", i, "kind", (int) op->kind);
        set_indexed_integer (kf, group, "op", i, "offers", mc_pp_operation_offers (op));
    }

    g_key_file_set_integer (kf, group, "action_count", plugin->action_count);
    for (i = 0; i < plugin->action_count; i++)
        set_indexed_string (kf, group, "action", i, "label", plugin->actions[i].label);

    g_key_file_set_integer (kf, group, "menu_count", plugin->cmd_menu_entry_count);
    for (i = 0; i < plugin->cmd_menu_entry_count; i++)
    {
        const mc_pp_cmd_menu_entry_t *e = &plugin->cmd_menu_entries[i];

        set_indexed_string (kf, group, "menu", i, "label", e->label);
        set_indexed_integer (kf, group, "menu", i, "action", e->action_index);
        set_indexed_string (kf, group, "menu", i, "shortcut", e->shortcut);
        set_indexed_integer (kf, group, "menu", i, "key", e->key);
        set_indexed_string (kf, group, "menu", i, "menu_name", e->menu_name);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
mc_pp_plugin_offers (const mc_panel_plugin_t *plugin)
{
    int offers = 0;

    if (plugin->open_input_stream != NULL)
        offers |= MC_PP_OFFERS_OPEN_INPUT_STREAM;
    if (plugin->open_file_list != NULL)
        offers |= MC_PP_OFFERS_OPEN_FILE_LIST;
    if (plugin->configure != NULL)
        offers |= MC_PP_OFFERS_CONFIGURE;
    return offers;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_pp_operation_offers (const mc_pp_file_operation_t *operation)
{
    int offers = 0;

    if (operation->may_open_name != NULL)
        offers |= MC_PP_OP_OFFERS_MAY_OPEN_NAME;
    if (operation->open_input_stream != NULL)
        offers |= MC_PP_OP_OFFERS_OPEN_INPUT_STREAM;
    if (operation->view_input_stream != NULL)
        offers |= MC_PP_OP_OFFERS_VIEW_INPUT_STREAM;
    if (operation->show != NULL)
        offers |= MC_PP_OP_OFFERS_SHOW;
    return offers;
}

/* --------------------------------------------------------------------------------------------- */

mc_pp_cache_t *
mc_pp_cache_open (const char *path)
{
    mc_pp_cache_t *cache;
    char *program;

    if (path == NULL)
        return NULL;

    cache = g_new0 (mc_pp_cache_t, 1);
    cache->path = g_strdup (path);
    cache->in = g_key_file_new ();
    cache->out = g_key_file_new ();
    cache->groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    g_key_file_set_integer (cache->out, GROUP_HEADER, "version", CACHE_VERSION);
    g_key_file_set_string (cache->out, GROUP_HEADER, "program", mc_global.mc_version);

    if (!g_key_file_load_from_file (cache->in, path, G_KEY_FILE_NONE, NULL)
        || g_key_file_get_integer (cache->in, GROUP_HEADER, "version", NULL) != CACHE_VERSION)
        return cache;

    /* another build of mc may expect what an entry does not say */
    program = get_string (cache->in, GROUP_HEADER, "program");
    if (program != NULL && strcmp (program, mc_global.mc_version) == 0)
    {
        char **groups;
        int i;

        groups = g_key_file_get_groups (cache->in, NULL);
        for (i = 0; groups[i] != NULL; i++)
        {
            char *so_path;

            if (strcmp (groups[i], GROUP_HEADER) == 0)
                continue;

            so_path = get_string (cache->in, groups[i], "path");
            if (so_path != NULL)
            {
                g_hash_table_insert (cache->groups, so_path, g_strdup (groups[i]));
                cache->in_count++;
            }
        }
        g_strfreev (groups);
    }
    else
        cache->changed = TRUE;
    g_free (program);

    return cache;
}

/* --------------------------------------------------------------------------------------------- */

mc_panel_plugin_t *
mc_pp_cache_make_stub (mc_pp_cache_t *cache, const char *so_path, const struct stat *st,
                       int *offers, int **op_offers, char **description)
{
    const char *group;
    GKeyFile *kf;
    mc_panel_plugin_t *stub;
    int actions, menu, ops, flags;
    gboolean sort_reverse;
    gboolean ok = TRUE;
    char *name;

    *offers = 0;
    *op_offers = NULL;
    if (description != NULL)
        *description = NULL;

    if (cache == NULL || so_path == NULL || st == NULL)
        return NULL;

    group = g_hash_table_lookup (cache->groups, so_path);
    if (group == NULL)
        return NULL;

    kf = cache->in;
    if (!entry_is_for (kf, group, st))
        return NULL;

    /* a damaged entry, or one that must not be used, is dropped: the module is opened again */
    if (!get_count (kf, group, "action_count", &actions)
        || !get_count (kf, group, "menu_count", &menu) || !get_count (kf, group, "op_count", &ops)
        || !get_integer (kf, group, "flags", &flags) || (flags & MC_PPF_EAGER_LOAD) != 0
        || !get_integer (kf, group, "offers", offers)
        || !get_boolean (kf, group, "sort_reverse", &sort_reverse))
        return NULL;

    name = get_string (kf, group, "name");
    if (name == NULL)
        return NULL;

    stub = g_new0 (mc_panel_plugin_t, 1);
    stub->api_version = MC_PANEL_PLUGIN_API_VERSION;
    stub->name = name;
    stub->display_name = get_string (kf, group, "display_name");
    stub->proto = get_string (kf, group, "proto");
    stub->prefix = get_string (kf, group, "prefix");
    stub->flags = (mc_pp_flags_t) flags;
    stub->default_sort_id = get_string (kf, group, "sort");
    stub->default_sort_reverse = sort_reverse;

    *op_offers = stub_set_operations (stub, kf, group, ops, &ok);
    stub_set_actions (stub, kf, group, actions);
    stub_set_menu (stub, kf, group, menu, &ok);

    if (!ok)
    {
        g_free (*op_offers);
        *op_offers = NULL;
        *offers = 0;
        mc_pp_cache_free_stub (stub);
        return NULL;
    }

    if (description != NULL)
        *description = entry_description (kf, group);

    copy_entry_to_out (cache, group);
    return stub;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_pp_cache_free_stub (mc_panel_plugin_t *stub)
{
    int i;

    if (stub == NULL)
        return;

    for (i = 0; i < stub->file_operation_count; i++)
        g_free ((char *) stub->file_operations[i].name);
    for (i = 0; i < stub->action_count; i++)
        g_free ((char *) stub->actions[i].label);
    for (i = 0; i < stub->cmd_menu_entry_count; i++)
    {
        g_free ((char *) stub->cmd_menu_entries[i].label);
        g_free ((char *) stub->cmd_menu_entries[i].shortcut);
        g_free ((char *) stub->cmd_menu_entries[i].menu_name);
    }
    g_free ((gpointer) stub->file_operations);
    g_free ((gpointer) stub->actions);
    g_free ((gpointer) stub->cmd_menu_entries);
    g_free ((char *) stub->name);
    g_free ((char *) stub->display_name);
    g_free ((char *) stub->proto);
    g_free ((char *) stub->prefix);
    g_free ((char *) stub->default_sort_id);
    g_free (stub);
}

/* --------------------------------------------------------------------------------------------- */

char *
mc_pp_cache_describe (const mc_panel_plugin_t *plugin)
{
    GKeyFile *kf;
    char *data;

    kf = g_key_file_new ();
    write_description (kf, DESCRIPTION_GROUP, plugin);
    data = g_key_file_to_data (kf, NULL, NULL);
    g_key_file_free (kf);
    return data;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_pp_cache_store (mc_pp_cache_t *cache, const char *so_path, const struct stat *st,
                   const mc_panel_plugin_t *plugin)
{
    char *group, *id;

    if (cache == NULL || so_path == NULL || st == NULL || plugin == NULL)
        return;

    if (plugin->action_count > CACHE_MAX_ENTRIES || plugin->cmd_menu_entry_count > CACHE_MAX_ENTRIES
        || plugin->file_operation_count > CACHE_MAX_ENTRIES)
        return;

    group = next_out_group (cache);
    id = mc_pp_cache_file_id (st);
    g_key_file_set_string (cache->out, group, "path", so_path);
    g_key_file_set_string (cache->out, group, "file", id);
    write_description (cache->out, group, plugin);
    g_free (id);
    g_free (group);
    cache->changed = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_pp_cache_close (mc_pp_cache_t *cache)
{
    if (cache == NULL)
        return;

    if (cache->changed || cache->in_count != cache->out_count)
    {
        char *dir, *data;
        gsize length = 0;

        dir = g_path_get_dirname (cache->path);
        (void) g_mkdir_with_parents (dir, 0700);
        g_free (dir);

        data = g_key_file_to_data (cache->out, &length, NULL);
        if (data != NULL)
            (void) g_file_set_contents (cache->path, data, (gssize) length, NULL);
        g_free (data);
    }

    g_hash_table_destroy (cache->groups);
    g_key_file_free (cache->in);
    g_key_file_free (cache->out);
    g_free (cache->path);
    g_free (cache);
}

/* --------------------------------------------------------------------------------------------- */
