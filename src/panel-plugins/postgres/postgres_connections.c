/*
   Panel plugin postgres for the M-Commander
   the list of connections: create, edit, delete

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

#include "lib/global.h"

#include "postgres_internal.h"
#include "postgres_settings.h"
#include "postgres_ui.h"

/*** public functions ****************************************************************************/

/* F7 or Shift-F4 on the list of connections */
mc_pp_result_t
postgres_create_connection (postgres_data_t *data)
{
    postgres_connection_t *c;
    char *err = NULL;

    c = g_new0 (postgres_connection_t, 1);
    if (!postgres_connection_dialog_run (data->host, data->cfg, c, TRUE))
    {
        postgres_connection_free (c);
        return MC_PPR_SKIPPED;
    }

    if (postgres_config_find (data->cfg, c->name) != NULL)
    {
        postgres_message (data->host, TRUE, _ ("A connection named \"%s\" already exists."),
                          c->name);
        postgres_connection_free (c);
        return MC_PPR_SKIPPED;
    }

    if (!postgres_config_save_connection (c, NULL, &err))
    {
        postgres_message (data->host, TRUE, _ ("Cannot save the connection: %s"), err);
        g_free (err);
        postgres_connection_free (c);
        return MC_PPR_FAILED;
    }

    g_ptr_array_add (data->cfg->connections, c);
    g_free (data->focus_after_up);
    data->focus_after_up = g_strdup (c->name);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* F4 on the list of connections: the one under the cursor. It is edited in a copy, so that a
   name already taken or a failed save leaves it as it was. */
mc_pp_result_t
postgres_edit_connection (postgres_data_t *data)
{
    const GString *cur = NULL;
    postgres_connection_t *orig, *edited, *clash;
    char *err = NULL;

    if (data->host->get_current != NULL)
        cur = data->host->get_current (data->host);
    orig = cur != NULL ? postgres_config_find (data->cfg, cur->str) : NULL;
    if (orig == NULL)
        return MC_PPR_SKIPPED;

    edited = postgres_connection_copy (orig);
    if (!postgres_connection_dialog_run (data->host, data->cfg, edited, FALSE))
    {
        postgres_connection_free (edited);
        return MC_PPR_SKIPPED;
    }

    clash = postgres_config_find (data->cfg, edited->name);
    if (clash != NULL && clash != orig)
    {
        postgres_message (data->host, TRUE, _ ("A connection named \"%s\" already exists."),
                          edited->name);
        postgres_connection_free (edited);
        return MC_PPR_SKIPPED;
    }

    if (!postgres_config_save_connection (edited, orig->name, &err))
    {
        postgres_message (data->host, TRUE, _ ("Cannot save the connection: %s"), err);
        g_free (err);
        postgres_connection_free (edited);
        return MC_PPR_FAILED;
    }

    for (guint i = 0; i < data->cfg->connections->len; i++)
        if (g_ptr_array_index (data->cfg->connections, i) == orig)
        {
            g_ptr_array_index (data->cfg->connections, i) = edited;
            postgres_connection_free (orig);
            break;
        }

    g_free (data->focus_after_up);
    data->focus_after_up = g_strdup (edited->name);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* F8 on the list of connections: the sections go from postgres.ini */
mc_pp_result_t
postgres_delete_connections (postgres_data_t *data, const char **names, int count)
{
    for (int i = 0; i < count; i++)
    {
        postgres_connection_t *c = postgres_config_find (data->cfg, names[i]);
        char *err = NULL;

        if (c == NULL)
            continue;
        if (!postgres_config_delete_connection (c->name, &err))
        {
            postgres_message (data->host, TRUE, _ ("Cannot delete the connection: %s"), err);
            g_free (err);
            return MC_PPR_FAILED;
        }
        g_ptr_array_remove (data->cfg->connections, c);
    }
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* The general options, from the Manage Plugins dialog: on their own, without a panel */
void
postgres_configure (void)
{
    postgres_config_t *cfg;

    cfg = postgres_config_load ();
    (void) postgres_settings_dialog_run (cfg);
    postgres_config_free (cfg);
}

/* --------------------------------------------------------------------------------------------- */
