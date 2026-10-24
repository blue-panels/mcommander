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

#include "lib/global.h"
#include "lib/keybind.h"  // CK_Edit
#include "lib/panel-plugin.h"

#include "postgres_internal.h"
#include "postgres_ui.h"

/*** forward declarations ************************************************************************/

static void *postgres_open (mc_panel_host_t *host, const char *open_path);
static void postgres_close (void *plugin_data);
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
    .get_columns = postgres_get_columns,
    .get_column_value = postgres_get_column_value,
    .get_default_format = postgres_get_default_format,
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

    postgres_session_close (data);
    postgres_config_free (data->cfg);
    g_free (data->focus_after_up);
    g_free (data);
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
        return MC_PPR_NOT_SUPPORTED;
    }

    // below the connections the panel only reads: its objects are changed by SQL in coole
    if (key == CK_Edit || key == CK_EditNew || key == CK_Delete || key == CK_MakeDir
        || key == CK_Move)
    {
        postgres_message (data->host, FALSE,
                          _ ("The panel only shows the objects of the database: change them with "
                             "SQL in the query editor."));
        return MC_PPR_OK;
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
    static char title[256];
    postgres_data_t *data = (postgres_data_t *) plugin_data;

    switch (data->level)
    {
    case POSTGRES_LEVEL_DATABASES:
        g_snprintf (title, sizeof (title), _ ("%s: databases"), data->connection->name);
        return title;
    case POSTGRES_LEVEL_SCHEMAS:
        g_snprintf (title, sizeof (title), _ ("%s/%s: schemas"), data->connection->name,
                    PQdb (data->conn));
        return title;
    case POSTGRES_LEVEL_RELATIONS:
        g_snprintf (title, sizeof (title), _ ("%s/%s/%s: relations"), data->connection->name,
                    PQdb (data->conn), data->schema);
        return title;
    default:
        return _ ("PostgreSQL connections");
    }
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
