/*
   Panel plugin postgres for the M-Commander
   the columns of the panel

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

/*** file scope variables ************************************************************************/

static const mc_panel_column_t connection_columns[] = {
    { "description", "Description", 12, TRUE, J_LEFT_FIT, TRUE },
};

static const mc_panel_column_t database_columns[] = {
    { "owner", "Owner", 10, FALSE, J_LEFT_FIT, TRUE },
};

/*** public functions ****************************************************************************/

const mc_panel_column_t *
postgres_get_columns (void *plugin_data, size_t *count)
{
    postgres_data_t *data = (postgres_data_t *) plugin_data;

    switch (data->level)
    {
    case POSTGRES_LEVEL_CONNECTIONS:
        *count = G_N_ELEMENTS (connection_columns);
        return connection_columns;
    case POSTGRES_LEVEL_DATABASES:
        *count = G_N_ELEMENTS (database_columns);
        return database_columns;
    default:
        *count = 0;
        return NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

const char *
postgres_get_column_value (void *plugin_data, const char *fname, const char *column_id)
{
    postgres_data_t *data = (postgres_data_t *) plugin_data;
    const char *value = NULL;

    if (data->level == POSTGRES_LEVEL_CONNECTIONS)
    {
        const postgres_connection_t *c = postgres_config_find (data->cfg, fname);

        if (c != NULL && strcmp (column_id, "description") == 0)
            value = c->description;
    }
    else
    {
        const postgres_item_t *it = postgres_find_item (data, fname);

        if (it != NULL && strcmp (column_id, "owner") == 0)
            value = it->owner;
    }
    return value != NULL ? value : "";
}

/* --------------------------------------------------------------------------------------------- */

const char *
postgres_get_default_format (void *plugin_data)
{
    postgres_data_t *data = (postgres_data_t *) plugin_data;

    switch (data->level)
    {
    case POSTGRES_LEVEL_CONNECTIONS:
        return "half name | description";
    case POSTGRES_LEVEL_DATABASES:
        return "half name | size | owner";
    default:
        return NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
