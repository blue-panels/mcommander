/*
   Panel plugin postgres for the M-Commander
   connections, databases, schemas and relations of PostgreSQL

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

/** \file postgres.c
 *  \brief Source: the table of the plugin
 *
 *  The panel goes down from the connections of postgres.ini to the databases of a server, their
 *  schemas and the relations of a schema. The rows of a relation are not in the panel: they are
 *  the business of the query editor coole.
 */

#include <config.h>

#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "lib/global.h"
#include "lib/keybind.h"  // CK_Edit
#include "lib/panel-plugin.h"

#include "postgres_internal.h"

/*** forward declarations ************************************************************************/

static void *postgres_open (mc_panel_host_t *host, const char *open_path);
static void postgres_close (void *plugin_data);
static mc_pp_result_t postgres_get_items (void *plugin_data, void *list);
static mc_pp_result_t postgres_chdir (void *plugin_data, const char *path);
static mc_pp_result_t postgres_handle_key (void *plugin_data, int key);
static mc_pp_result_t postgres_create_item (void *plugin_data);
static mc_pp_result_t postgres_delete_items (void *plugin_data, const char **names, int count);
static const char *postgres_get_title (void *plugin_data);
static const char *postgres_get_focus_name (void *plugin_data);
static mc_pp_result_t postgres_get_help_info (void *plugin_data, const char **filename,
                                              const char **node);

const mc_panel_plugin_t *mc_panel_plugin_register (void);

/*** file scope variables ************************************************************************/

static const mc_panel_plugin_t postgres_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "postgres",
    .display_name = N_ ("PostgreSQL"),
    .proto = "postgres",
    .prefix = "postgres:",
    .flags = MC_PPF_NAVIGATE | MC_PPF_DELETE | MC_PPF_CREATE | MC_PPF_CUSTOM_TITLE
        | MC_PPF_SHOW_IN_DRIVE_MENU,

    .open = postgres_open,
    .close = postgres_close,
    .get_items = postgres_get_items,
    .chdir = postgres_chdir,
    .handle_key = postgres_handle_key,
    .create_item = postgres_create_item,
    .delete_items = postgres_delete_items,
    .get_title = postgres_get_title,
    .get_focus_name = postgres_get_focus_name,
    .configure = postgres_configure,
    .get_help_info = postgres_get_help_info,

    .default_sort_id = "name",
};

/*** file scope functions ************************************************************************/

static void *
postgres_open (mc_panel_host_t *host, const char *open_path)
{
    postgres_data_t *data;

    (void) open_path;

    if (host == NULL)
        return NULL;

    data = g_new0 (postgres_data_t, 1);
    data->host = host;
    data->cfg = postgres_config_load ();
    data->level = POSTGRES_LEVEL_CONNECTIONS;
    // the cursor starts on the default connection
    if (postgres_config_find (data->cfg, data->cfg->default_connection) != NULL)
        data->focus_after_up = g_strdup (data->cfg->default_connection);
    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
postgres_close (void *plugin_data)
{
    postgres_data_t *data = (postgres_data_t *) plugin_data;

    postgres_config_free (data->cfg);
    g_free (data->focus_after_up);
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
postgres_get_items (void *plugin_data, void *list)
{
    postgres_data_t *data = (postgres_data_t *) plugin_data;
    time_t now = time (NULL);

    switch (data->level)
    {
    case POSTGRES_LEVEL_CONNECTIONS:
        for (guint i = 0; i < data->cfg->connections->len; i++)
        {
            const postgres_connection_t *c = g_ptr_array_index (data->cfg->connections, i);

            mc_pp_add_entry (list, c->name, S_IFDIR | 0755, 0, now);
        }
        return MC_PPR_OK;

    default:
        return MC_PPR_FAILED;
    }
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
postgres_chdir (void *plugin_data, const char *path)
{
    postgres_data_t *data = (postgres_data_t *) plugin_data;

    if (strcmp (path, "..") == 0)
        return data->level == POSTGRES_LEVEL_CONNECTIONS ? MC_PPR_CLOSE : MC_PPR_FAILED;

    return MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
postgres_handle_key (void *plugin_data, int key)
{
    postgres_data_t *data = (postgres_data_t *) plugin_data;

    if (data->level == POSTGRES_LEVEL_CONNECTIONS)
    {
        if (key == CK_Edit)
            return postgres_edit_connection (data);
        if (key == CK_EditNew)
            return postgres_create_connection (data);
    }
    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
postgres_create_item (void *plugin_data)
{
    postgres_data_t *data = (postgres_data_t *) plugin_data;

    if (data->level == POSTGRES_LEVEL_CONNECTIONS)
        return postgres_create_connection (data);
    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
postgres_delete_items (void *plugin_data, const char **names, int count)
{
    postgres_data_t *data = (postgres_data_t *) plugin_data;

    if (data->level == POSTGRES_LEVEL_CONNECTIONS)
        return postgres_delete_connections (data, names, count);
    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
postgres_get_title (void *plugin_data)
{
    (void) plugin_data;

    return _ ("PostgreSQL connections");
}

/* --------------------------------------------------------------------------------------------- */

static const char *
postgres_get_focus_name (void *plugin_data)
{
    static char buf[256];
    postgres_data_t *data = (postgres_data_t *) plugin_data;

    if (data->focus_after_up == NULL)
        return NULL;
    g_strlcpy (buf, data->focus_after_up, sizeof (buf));
    g_clear_pointer (&data->focus_after_up, g_free);
    return buf;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
postgres_get_help_info (void *plugin_data, const char **filename, const char **node)
{
    (void) plugin_data;

    if (filename != NULL)
        *filename =
            g_file_test (POSTGRES_HELP_FILE, G_FILE_TEST_IS_REGULAR) ? POSTGRES_HELP_FILE : NULL;
    if (node != NULL)
        *node = POSTGRES_HELP_NODE;
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &postgres_plugin;
}

/* --------------------------------------------------------------------------------------------- */
