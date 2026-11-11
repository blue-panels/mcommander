/*
   Dynamic panel plugin loader.

   Copyright (C) 2025-2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2025-2026.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file panel-plugin-loader.c
 *  \brief Source: dynamic panel plugin loader
 *
 *  Scans MC_PANEL_PLUGINS_DIR for shared objects exporting
 *  mc_panel_plugin_register(), loads them, and registers the returned
 *  mc_panel_plugin_t descriptor via mc_panel_plugin_add().
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <time.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/editor-plugin.h"
#include "lib/mcconfig.h"
#include "lib/util.h"  // mc_realpath()
#include "lib/panel-plugin.h"
#include "lib/panel_plugin_cache.h"
#include "lib/plugin-prefs.h"
#include "lib/widget/hexstrip.h"  // named in mc_plugin_widgets below

#ifdef HAVE_GMODULE

#include <gmodule.h>

/*** global variables ****************************************************************************/

/* The widgets a plugin puts on a dialog and mc itself never does.
 *
 * lib/ is linked as a static archive, and a linker takes out of an archive only what the program
 * refers to.  A widget no part of mc uses is left behind, and the plugin that wanted it then fails
 * to load with an undefined symbol.  Naming one here is what keeps it in.
 */
void (*const mc_plugin_widgets[]) (void) = {
    (void (*) (void)) hexstrip_new,
    NULL,
};

/*** file scope macro definitions ****************************************************************/

#define PANEL_PLUGINS_CACHE_PREFIX "panel-plugins-"
#define PANEL_PLUGINS_CACHE_SUFFIX ".cache"
/* the cache of another install of mc, not run for this long, is removed */
#define PANEL_PLUGINS_CACHE_DAYS 30

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *so_path;
    char *error;
    gboolean loaded;
    int offers;                  /* mc_pp_offers_t */
    int *op_offers;              /* mc_pp_op_offers_t per operation of the stub */
    char *description;           /* of the module, from the cache */
    char *file_id;               /* of the module file at start */
    mc_panel_plugin_t described; /* what the stub held before the load: its arrays stay valid */
} lazy_module_t;

/*** file scope variables ************************************************************************/

static GHashTable *lazy_modules = NULL; /* stub -> lazy_module_t */
static mc_pp_cache_t *panel_plugin_cache = NULL;
static char *panel_plugin_cache_file = NULL;
static GPtrArray *panel_plugin_modules = NULL;
static gboolean editor_plugins_loaded = FALSE;
static GPtrArray *editor_plugin_modules = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
module_filename_has_native_suffix (const gchar *filename)
{
    if (filename == NULL)
        return FALSE;

    return g_str_has_suffix (filename, ".so") || g_str_has_suffix (filename, ".dylib")
        || g_str_has_suffix (filename, ".bundle") || g_str_has_suffix (filename, ".dll");
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_panel_plugin_valid (const mc_panel_plugin_t *plugin)
{
    return plugin->api_version == MC_PANEL_PLUGIN_API_VERSION && plugin->name != NULL
        && (plugin->open != NULL || plugin->open_with_plugin != NULL) && plugin->close != NULL
        && plugin->get_items != NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Try to load a single .so from the given directory.
 */
static void
mc_panel_plugin_try_load (const gchar *so_path, const gchar *filename, const struct stat *st)
{
    GModule *module;
    mc_panel_plugin_register_fn register_fn;
    const mc_panel_plugin_t *plugin;

    /* Eager binding: a plugin from an older install may reference a symbol this
       build no longer exports; lazy binding would crash mc later instead. */
    module = g_module_open (so_path, G_MODULE_BIND_LOCAL);
    if (module == NULL)
    {
        fprintf (stderr, "Panel plugin %s not loaded: %s\n", filename, g_module_error ());
        return;
    }

    if (!g_module_symbol (module, MC_PANEL_PLUGIN_ENTRY, (gpointer *) &register_fn))
    {
        fprintf (stderr, "Panel plugin %s: symbol %s not found\n", filename, MC_PANEL_PLUGIN_ENTRY);
        g_module_close (module);
        return;
    }

    plugin = register_fn ();
    if (plugin == NULL || !mc_panel_plugin_add (plugin))
    {
        /* a module switched off in Manage Plugins is described, so the next start does not
           open it only to drop it */
        if (st != NULL && plugin != NULL && mc_panel_plugin_valid (plugin)
            && (plugin->flags & MC_PPF_EAGER_LOAD) == 0
            && mc_plugin_prefs_is_disabled (MC_PLUGIN_KIND_PANEL, plugin->name)
            && mc_panel_plugin_find_by_name (plugin->name) == NULL)
            mc_pp_cache_store (panel_plugin_cache, so_path, st, plugin);

        /* register() may allocate module-global state even when preferences or
           validation reject the descriptor. Only the current ABI can safely
           expose the shutdown field. A duplicate name is left alone: the same
           .so reached by another path shares its globals with the live plugin. */
        if (plugin != NULL && plugin->api_version == MC_PANEL_PLUGIN_API_VERSION
            && plugin->shutdown != NULL && plugin->name != NULL
            && mc_panel_plugin_find_by_name (plugin->name) == NULL)
            plugin->shutdown ();
        /* duplicate name from user dir overriding system dir is expected, stay silent */
        g_module_close (module);
        return;
    }

    if (st != NULL && (plugin->flags & MC_PPF_EAGER_LOAD) == 0)
        mc_pp_cache_store (panel_plugin_cache, so_path, st, plugin);

    g_module_make_resident (module); /* prevent unload - plugin descriptor lives in .so */
    g_ptr_array_add (panel_plugin_modules, module);
}

/* --------------------------------------------------------------------------------------------- */

/* TRUE when the cache described this very file and the module need not be opened. */
static gboolean
mc_panel_plugin_try_lazy (const gchar *so_path, const struct stat *st)
{
    mc_panel_plugin_t *stub;
    lazy_module_t *lazy;
    int offers, *op_offers;
    char *description;

    stub =
        mc_pp_cache_make_stub (panel_plugin_cache, so_path, st, &offers, &op_offers, &description);
    if (stub == NULL)
        return FALSE;

    if (!mc_panel_plugin_add_lazy (stub))
    {
        g_free (op_offers);
        g_free (description);
        mc_pp_cache_free_stub (stub);
        return TRUE;
    }

    if (lazy_modules == NULL)
        lazy_modules = g_hash_table_new (g_direct_hash, g_direct_equal);

    lazy = g_new0 (lazy_module_t, 1);
    lazy->so_path = g_strdup (so_path);
    lazy->offers = offers;
    lazy->op_offers = op_offers;
    lazy->description = description;
    lazy->file_id = mc_pp_cache_file_id (st);
    g_hash_table_insert (lazy_modules, stub, lazy);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_panel_plugin_usable_for (const mc_panel_plugin_t *real, const mc_panel_plugin_t *stub)
{
    return mc_panel_plugin_valid (real) && strcmp (real->name, stub->name) == 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_panel_plugin_described_by (const mc_panel_plugin_t *real, const lazy_module_t *lazy)
{
    char *description;
    gboolean same;

    description = mc_pp_cache_describe (real);
    same = g_strcmp0 (description, lazy->description) == 0;
    g_free (description);
    return same;
}

/* --------------------------------------------------------------------------------------------- */

/* The menus and the lists of the session were built from the stub, by the place of an action
   or an operation: a module that has them in another order cannot take its place. */
static gboolean
mc_panel_plugin_fits_session (const mc_panel_plugin_t *real, const mc_panel_plugin_t *stub)
{
    int i;

    if (real->action_count != stub->action_count
        || real->file_operation_count != stub->file_operation_count)
        return FALSE;

    for (i = 0; i < real->action_count; i++)
        if (g_strcmp0 (real->actions[i].label, stub->actions[i].label) != 0)
            return FALSE;

    for (i = 0; i < real->file_operation_count; i++)
        if (g_strcmp0 (real->file_operations[i].name, stub->file_operations[i].name) != 0
            || real->file_operations[i].kind != stub->file_operations[i].kind)
            return FALSE;

    if (real->cmd_menu_entry_count != stub->cmd_menu_entry_count)
        return FALSE;

    for (i = 0; i < real->cmd_menu_entry_count; i++)
        if (real->cmd_menu_entries[i].action_index != stub->cmd_menu_entries[i].action_index)
            return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_panel_plugin_module_is_open (const GModule *module)
{
    guint i;

    for (i = 0; i < panel_plugin_modules->len; i++)
        if (g_ptr_array_index (panel_plugin_modules, i) == module)
            return TRUE;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* The next start opens every module again and describes it anew. */
static void
mc_panel_plugins_forget_cache (void)
{
    if (panel_plugin_cache_file != NULL)
        (void) unlink (panel_plugin_cache_file);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_panel_plugin_stub_offer_nothing (mc_panel_plugin_t *stub)
{
    stub->open_input_stream = NULL;
    stub->open_file_list = NULL;
    stub->configure = NULL;
    /* actions and menu entries stay: using one shows why the module is not there */
    stub->file_operation_count = 0;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Scan plugins_dir for subdirectories containing .so files.
 * Each plugin lives in its own subdirectory (e.g. panel-plugins/s3/mc-panel-s3.so).
 */
/* one package directory: every native module in it */
static void
mc_panel_plugins_load_package (const gchar *entry_path, const gchar *entry_name, gboolean lazy_ok)
{
    GDir *subdir;
    const gchar *sub_name;

    if (!g_file_test (entry_path, G_FILE_TEST_IS_DIR))
        return;

    /* A user package shadows the system package with the same ID.  Do not
       even open the duplicate: its register() callback may have process-wide
       side effects before mc_panel_plugin_add() can reject it. */
    if (mc_panel_plugin_find_by_name (entry_name) != NULL)
        return;

    subdir = g_dir_open (entry_path, 0, NULL);
    if (subdir == NULL)
        return;
    while ((sub_name = g_dir_read_name (subdir)) != NULL)
    {
        if (module_filename_has_native_suffix (sub_name))
        {
            gchar *so_path;
            struct stat st;
            gboolean have_stat;

            so_path = g_build_filename (entry_path, sub_name, (char *) NULL);
            have_stat = lazy_ok && panel_plugin_cache != NULL && stat (so_path, &st) == 0;
            if (!have_stat || !mc_panel_plugin_try_lazy (so_path, &st))
                mc_panel_plugin_try_load (so_path, sub_name, have_stat ? &st : NULL);
            g_free (so_path);
        }
    }
    g_dir_close (subdir);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_panel_plugins_load_from_dir (const gchar *plugins_dir, gboolean lazy_ok)
{
    GDir *dir;
    const gchar *entry_name;

    dir = g_dir_open (plugins_dir, 0, NULL);
    if (dir == NULL)
        return;

    while ((entry_name = g_dir_read_name (dir)) != NULL)
    {
        gchar *entry_path;

        entry_path = g_build_filename (plugins_dir, entry_name, (char *) NULL);
        mc_panel_plugins_load_package (entry_path, entry_name, lazy_ok);
        g_free (entry_path);
    }

    g_dir_close (dir);
}

/* --------------------------------------------------------------------------------------------- */

static gchar *
mc_panel_plugins_user_dir (void)
{
    return g_build_filename (g_get_home_dir (), ".local", "lib", MC_USERCONF_DIR, "panel-plugins",
                             (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* A build with its system plugin directory in the user's one */
static gboolean
mc_panel_plugins_same_dir (const char *a, const char *b)
{
    char ra[PATH_MAX], rb[PATH_MAX];

    return mc_realpath (a, ra) != NULL && mc_realpath (b, rb) != NULL && strcmp (ra, rb) == 0;
}

/* --------------------------------------------------------------------------------------------- */

/* One cache per install of mc: its version and its plugin directory. */
static char *
mc_panel_plugins_cache_file (void)
{
    const char *cache_dir;
    char *key, *sum, *name, *path;

    cache_dir = mc_config_get_cache_path ();
    if (cache_dir == NULL)
        return NULL;

    key = g_strconcat (mc_global.mc_version, "\n", MC_PANEL_PLUGINS_DIR, (char *) NULL);
    sum = g_compute_checksum_for_string (G_CHECKSUM_SHA256, key, -1);
    name = g_strdup_printf (PANEL_PLUGINS_CACHE_PREFIX "%.16s" PANEL_PLUGINS_CACHE_SUFFIX, sum);
    path = g_build_filename (cache_dir, name, (char *) NULL);
    g_free (name);
    g_free (sum);
    g_free (key);
    return path;
}

/* --------------------------------------------------------------------------------------------- */

/* The time of a cache file is when its install last ran: the caches of installs gone for long
   are removed, the others are kept. */
static void
mc_panel_plugins_tidy_caches (const char *own)
{
    char *dir_path;
    GDir *dir;
    const char *name;
    time_t now;

    (void) utime (own, NULL);

    dir_path = g_path_get_dirname (own);
    dir = g_dir_open (dir_path, 0, NULL);
    now = time (NULL);
    while (dir != NULL && (name = g_dir_read_name (dir)) != NULL)
        if (g_str_has_prefix (name, PANEL_PLUGINS_CACHE_PREFIX)
            && g_str_has_suffix (name, PANEL_PLUGINS_CACHE_SUFFIX))
        {
            char *path;
            struct stat st;

            path = g_build_filename (dir_path, name, (char *) NULL);
            if (strcmp (path, own) != 0 && stat (path, &st) == 0
                && now - st.st_mtime > (time_t) PANEL_PLUGINS_CACHE_DAYS * 24 * 60 * 60)
                (void) unlink (path);
            g_free (path);
        }
    if (dir != NULL)
        g_dir_close (dir);
    g_free (dir_path);
}

/* --------------------------------------------------------------------------------------------- */

void
mc_panel_plugins_load (void)
{
    gchar *system_dir;
    gchar *user_dir;
    gboolean same_dir;

    if (panel_plugin_modules == NULL)
        panel_plugin_modules = g_ptr_array_new ();

    g_free (panel_plugin_cache_file);
    panel_plugin_cache_file = mc_panel_plugins_cache_file ();
    panel_plugin_cache = mc_pp_cache_open (panel_plugin_cache_file);

    system_dir = g_build_filename (MC_PANEL_PLUGINS_DIR, (char *) NULL);
    user_dir = mc_panel_plugins_user_dir ();
    same_dir = mc_panel_plugins_same_dir (user_dir, system_dir);

    /* Load user packages first so they can shadow system packages.  They are opened at start:
       a copy that cannot be opened leaves its place to the system package of the same plugin,
       whatever the names of their directories.  A build whose system directory is the user's
       one reads it once, as the system directory. */
    mc_panel_plugins_load_from_dir (user_dir, same_dir);
    if (!same_dir)
        mc_panel_plugins_load_from_dir (system_dir, TRUE);
    g_free (user_dir);
    g_free (system_dir);

    mc_pp_cache_close (panel_plugin_cache);
    panel_plugin_cache = NULL;
    if (panel_plugin_cache_file != NULL)
        mc_panel_plugins_tidy_caches (panel_plugin_cache_file);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_panel_plugin_ensure_loaded (const mc_panel_plugin_t *plugin, char **error)
{
    mc_panel_plugin_t *stub = (mc_panel_plugin_t *) plugin;
    lazy_module_t *lazy;
    GModule *module;
    mc_panel_plugin_register_fn register_fn;
    const mc_panel_plugin_t *real = NULL;

    if (lazy_modules == NULL || plugin == NULL)
        return TRUE;

    lazy = g_hash_table_lookup (lazy_modules, plugin);
    if (lazy == NULL || lazy->loaded)
        return TRUE;

    if (lazy->error == NULL)
    {
        struct stat st;
        char *id = NULL;
        gboolean replaced;

        /* a file put in place since the start, a package update say, is taken only when it says
           all that its entry said: the menus and lists of the run were built from that */
        if (stat (lazy->so_path, &st) == 0)
            id = mc_pp_cache_file_id (&st);
        replaced = g_strcmp0 (id, lazy->file_id) != 0;
        g_free (id);

        module = g_module_open (lazy->so_path, G_MODULE_BIND_LOCAL);
        if (module == NULL)
            lazy->error = g_strdup (g_module_error ());
        else if (mc_panel_plugin_module_is_open (module))
        {
            /* the file is the module of another plugin: its register() must not run again */
            lazy->error = g_strdup (_ ("the module is the one of another plugin"));
            g_module_close (module);
        }
        else if (!g_module_symbol (module, MC_PANEL_PLUGIN_ENTRY, (gpointer *) &register_fn))
        {
            lazy->error = g_strdup_printf (_ ("symbol %s not found"), MC_PANEL_PLUGIN_ENTRY);
            g_module_close (module);
        }
        else
        {
            gboolean usable;

            real = register_fn ();
            usable = real != NULL && mc_panel_plugin_usable_for (real, stub);
            if (usable
                && (replaced ? mc_panel_plugin_described_by (real, lazy)
                             : mc_panel_plugin_fits_session (real, stub)))
            {
                /* other details, a menu hotkey say, are taken as the module has them; the
                   next start describes a module that differs or is another file */
                if (replaced || !mc_panel_plugin_described_by (real, lazy))
                    mc_panel_plugins_forget_cache ();
                lazy->described = *stub;
                *stub = *real;
                lazy->loaded = TRUE;
                g_module_make_resident (module);
                g_ptr_array_add (panel_plugin_modules, module);
                return TRUE;
            }

            if (usable)
                lazy->error = g_strdup (_ ("the module was replaced after mc started; restart mc"));
            else
                lazy->error = g_strdup (_ ("the module does not match what is known about it"));

            if (real != NULL && real->api_version == MC_PANEL_PLUGIN_API_VERSION
                && real->shutdown != NULL)
                real->shutdown ();
            g_module_close (module);
        }

        mc_panel_plugin_stub_offer_nothing (stub);
        mc_panel_plugins_forget_cache ();
    }

    if (error != NULL)
        *error = g_strdup (lazy->error);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static const lazy_module_t *
mc_panel_plugin_lazy_entry (const mc_panel_plugin_t *plugin)
{
    const lazy_module_t *lazy;

    if (lazy_modules == NULL)
        return NULL;

    lazy = g_hash_table_lookup (lazy_modules, plugin);
    return lazy != NULL && !lazy->loaded ? lazy : NULL;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_panel_plugin_offers (const mc_panel_plugin_t *plugin, int what)
{
    const lazy_module_t *lazy;
    int offers;

    if (plugin == NULL)
        return FALSE;

    lazy = mc_panel_plugin_lazy_entry (plugin);
    if (lazy == NULL)
        offers = mc_pp_plugin_offers (plugin);
    else
        offers = lazy->error != NULL ? 0 : lazy->offers;

    return (offers & what) == what;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_panel_plugin_operation_offers (const mc_panel_plugin_t *plugin, int index, int what)
{
    const lazy_module_t *lazy;
    int offers;

    if (plugin == NULL || index < 0 || index >= plugin->file_operation_count)
        return FALSE;

    lazy = mc_panel_plugin_lazy_entry (plugin);
    if (lazy == NULL)
        offers = mc_pp_operation_offers (&plugin->file_operations[index]);
    else
        offers = lazy->error != NULL ? 0 : lazy->op_offers[index];

    return (offers & what) == what;
}

/* --------------------------------------------------------------------------------------------- */

/* Load one package by name, the user copy first; the rest stay unloaded. */
const mc_panel_plugin_t *
mc_panel_plugin_load_named (const char *name, char **error)
{
    gchar *dir, *path;
    const mc_panel_plugin_t *plugin;

    plugin = mc_panel_plugin_find_by_name (name);
    if (plugin != NULL)
        return mc_panel_plugin_ensure_loaded (plugin, error) ? plugin : NULL;

    if (panel_plugin_modules == NULL)
        panel_plugin_modules = g_ptr_array_new ();

    dir = mc_panel_plugins_user_dir ();
    path = g_build_filename (dir, name, (char *) NULL);
    mc_panel_plugins_load_package (path, name, TRUE);
    g_free (path);
    g_free (dir);

    if (mc_panel_plugin_find_by_name (name) == NULL)
    {
        path = g_build_filename (MC_PANEL_PLUGINS_DIR, name, (char *) NULL);
        mc_panel_plugins_load_package (path, name, TRUE);
        g_free (path);
    }

    return mc_panel_plugin_find_by_name (name);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_editor_plugins_load_from_dir (const gchar *plugins_dir)
{
    GDir *dir;
    const gchar *filename;

    dir = g_dir_open (plugins_dir, 0, NULL);
    if (dir == NULL)
        return;

    while ((filename = g_dir_read_name (dir)) != NULL)
    {
        GModule *module;
        mc_editor_plugin_register_fn register_fn;
        const mc_editor_plugin_t *plugin;
        gchar *path;

        if (!module_filename_has_native_suffix (filename))
            continue;

        path = g_build_filename (plugins_dir, filename, (char *) NULL);
        module = g_module_open (path, 0);
        if (module == NULL)
        {
            fprintf (stderr, "Editor plugin %s not loaded: %s\n", filename, g_module_error ());
            g_free (path);
            continue;
        }

        if (!g_module_symbol (module, MC_EDITOR_PLUGIN_ENTRY, (gpointer *) &register_fn))
        {
            fprintf (stderr, "Editor plugin %s: symbol %s not found\n", filename,
                     MC_EDITOR_PLUGIN_ENTRY);
            g_module_close (module);
            g_free (path);
            continue;
        }

        plugin = register_fn ();
        if (plugin == NULL || !mc_editor_plugin_add (plugin))
        {
            g_module_close (module);
            g_free (path);
            continue;
        }

        g_module_make_resident (module);
        g_ptr_array_add (editor_plugin_modules, module);
        g_free (path);
    }

    g_dir_close (dir);
}

/* --------------------------------------------------------------------------------------------- */

void
mc_editor_plugins_load (void)
{
    gchar *system_dir;
    gchar *user_dir;

    if (editor_plugins_loaded)
        return;

    editor_plugins_loaded = TRUE;
    editor_plugin_modules = g_ptr_array_new ();

    /* load from system plugin directory */
    system_dir = g_build_filename (MC_EDITOR_PLUGINS_DIR, (char *) NULL);
    mc_editor_plugins_load_from_dir (system_dir);
    g_free (system_dir);

    /* load from user plugin directory (~/.local/lib/mc6/editor-plugins) */
    user_dir = g_build_filename (g_get_home_dir (), ".local", "lib", MC_USERCONF_DIR,
                                 "editor-plugins", (char *) NULL);
    mc_editor_plugins_load_from_dir (user_dir);
    g_free (user_dir);
}

/* --------------------------------------------------------------------------------------------- */

#else /* !HAVE_GMODULE */

void
mc_panel_plugins_load (void)
{
    // GModule not available - dynamic panel plugins disabled
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_panel_plugin_ensure_loaded (const mc_panel_plugin_t *plugin, char **error)
{
    (void) plugin;
    (void) error;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_panel_plugin_offers (const mc_panel_plugin_t *plugin, int what)
{
    return plugin != NULL && (mc_pp_plugin_offers (plugin) & what) == what;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_panel_plugin_operation_offers (const mc_panel_plugin_t *plugin, int index, int what)
{
    if (plugin == NULL || index < 0 || index >= plugin->file_operation_count)
        return FALSE;

    return (mc_pp_operation_offers (&plugin->file_operations[index]) & what) == what;
}

/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *
mc_panel_plugin_load_named (const char *name, char **error)
{
    (void) name;
    (void) error;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_editor_plugins_load (void)
{
    /* GModule not available - dynamic editor plugins disabled */
}

/* --------------------------------------------------------------------------------------------- */

#endif /* HAVE_GMODULE */
