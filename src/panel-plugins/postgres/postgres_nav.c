/*
   Panel plugin postgres for the M-Commander
   the levels of the panel and the way between them

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

/** \file postgres_nav.c
 *  \brief Source: the levels of the panel and the way between them
 *
 *  Each level is read by one query of the catalog when it is entered, and kept until it is left;
 *  nothing is counted. A connection lost on the way is opened again once, quietly.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "lib/global.h"

#include "postgres_internal.h"
#include "postgres_ui.h"

/*** file scope macro definitions ****************************************************************/

#define SQL_DATABASES                                                                              \
    "SELECT d.datname, pg_catalog.pg_get_userbyid (d.datdba), pg_catalog.pg_database_size "        \
    "(d.oid) "                                                                                     \
    "FROM pg_catalog.pg_database d "                                                               \
    "WHERE d.datallowconn AND NOT d.datistemplate "                                                \
    "AND pg_catalog.has_database_privilege (d.oid, 'CONNECT') ORDER BY 1"

/*** file scope functions ************************************************************************/

static void
item_free (gpointer p)
{
    postgres_item_t *it = (postgres_item_t *) p;

    g_free (it->name);
    g_free (it->owner);
    g_free (it);
}

/* --------------------------------------------------------------------------------------------- */

static postgres_item_t *
item_new (const char *name)
{
    postgres_item_t *it;

    it = g_new0 (postgres_item_t, 1);
    it->name = g_strdup (name);
    it->size = -1;
    return it;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
field (const PGresult *res, int row, int col)
{
    return PQgetisnull (res, row, col) ? NULL : PQgetvalue (res, row, col);
}

/* --------------------------------------------------------------------------------------------- */

static gint64
field_int (const PGresult *res, int row, int col)
{
    const char *s = field (res, row, col);

    return s != NULL ? g_ascii_strtoll (s, NULL, 10) : -1;
}

/* --------------------------------------------------------------------------------------------- */

static void
set_focus (postgres_data_t *data, const char *name)
{
    g_free (data->focus_after_up);
    data->focus_after_up = g_strdup (name);
}

/* --------------------------------------------------------------------------------------------- */

/* A query of the panel: a connection found lost is opened again, to the same database, and the
   query run once more. NULL with a message shown on failure. */
static PGresult *
query (postgres_data_t *data, const char *sql, int nparams, const char *const *params)
{
    PGresult *res;
    char *err = NULL;

    res = postgres_conn_query (data->conn, sql, nparams, params, &err);
    if (res == NULL && PQstatus (data->conn) == CONNECTION_BAD)
    {
        char *db = g_strdup (PQdb (data->conn));
        PGconn *conn;

        g_free (err);
        conn = postgres_conn_open (data->host, data->cfg, data->connection, db, &err);
        g_free (db);
        if (conn != NULL)
        {
            PQfinish (data->conn);
            data->conn = conn;
            res = postgres_conn_query (data->conn, sql, nparams, params, &err);
        }
    }
    if (res == NULL && err != NULL)
        postgres_message (data->host, TRUE, "%s", err);
    g_free (err);
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static void
items_take (postgres_data_t *data, GPtrArray *items)
{
    if (data->items != NULL)
        g_ptr_array_free (data->items, TRUE);
    data->items = items;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
read_databases (postgres_data_t *data)
{
    GPtrArray *items;
    PGresult *res;

    items = g_ptr_array_new_with_free_func (item_free);

    if (!data->connection->show_all_databases)
    {
        // the database of the connection only: the one it has opened
        g_ptr_array_add (items, item_new (PQdb (data->conn)));
        items_take (data, items);
        return TRUE;
    }

    res = query (data, SQL_DATABASES, 0, NULL);
    if (res == NULL)
    {
        g_ptr_array_free (items, TRUE);
        return FALSE;
    }
    for (int i = 0; i < PQntuples (res); i++)
    {
        postgres_item_t *it = item_new (PQgetvalue (res, i, 0));

        it->owner = g_strdup (field (res, i, 1));
        it->size = field_int (res, i, 2);
        g_ptr_array_add (items, it);
    }
    PQclear (res);
    items_take (data, items);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
enter_connection (postgres_data_t *data, const char *name)
{
    postgres_connection_t *c;
    char *err = NULL;

    c = postgres_config_find (data->cfg, name);
    if (c == NULL)
        return MC_PPR_FAILED;

    data->connection = c;
    data->conn = postgres_conn_open (data->host, data->cfg, c, NULL, &err);
    if (data->conn == NULL)
    {
        if (err != NULL)
            postgres_message (data->host, TRUE, _ ("Cannot connect to \"%s\":\n%s"), name, err);
        g_free (err);
        data->connection = NULL;
        return MC_PPR_FAILED;
    }

    if (!read_databases (data))
    {
        postgres_session_close (data);
        return MC_PPR_FAILED;
    }
    data->level = POSTGRES_LEVEL_DATABASES;
    return MC_PPR_OK;
}

/*** public functions ****************************************************************************/

void
postgres_session_close (postgres_data_t *data)
{
    items_take (data, NULL);
    if (data->conn != NULL)
        PQfinish (data->conn);
    data->conn = NULL;
    data->connection = NULL;
}

/* --------------------------------------------------------------------------------------------- */

const postgres_item_t *
postgres_find_item (const postgres_data_t *data, const char *name)
{
    if (data->items == NULL || name == NULL)
        return NULL;
    for (guint i = 0; i < data->items->len; i++)
    {
        const postgres_item_t *it = g_ptr_array_index (data->items, i);

        if (strcmp (it->name, name) == 0)
            return it;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

mc_pp_result_t
postgres_get_items (void *plugin_data, void *list)
{
    postgres_data_t *data = (postgres_data_t *) plugin_data;
    time_t now = time (NULL);

    if (data->level == POSTGRES_LEVEL_CONNECTIONS)
    {
        for (guint i = 0; i < data->cfg->connections->len; i++)
        {
            const postgres_connection_t *c = g_ptr_array_index (data->cfg->connections, i);

            mc_pp_add_entry (list, c->name, S_IFDIR | 0755, 0, now);
        }
        return MC_PPR_OK;
    }

    if (data->items != NULL)
        for (guint i = 0; i < data->items->len; i++)
        {
            const postgres_item_t *it = g_ptr_array_index (data->items, i);

            mc_pp_add_entry (list, it->name, S_IFDIR | 0755, it->size >= 0 ? (off_t) it->size : 0,
                             now);
        }
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

mc_pp_result_t
postgres_chdir (void *plugin_data, const char *path)
{
    postgres_data_t *data = (postgres_data_t *) plugin_data;

    if (strcmp (path, "..") == 0)
        switch (data->level)
        {
        case POSTGRES_LEVEL_CONNECTIONS:
            return MC_PPR_CLOSE;
        case POSTGRES_LEVEL_DATABASES:
            set_focus (data, data->connection->name);
            postgres_session_close (data);
            data->level = POSTGRES_LEVEL_CONNECTIONS;
            return MC_PPR_OK;
        default:
            return MC_PPR_FAILED;
        }

    switch (data->level)
    {
    case POSTGRES_LEVEL_CONNECTIONS:
        return enter_connection (data, path);
    default:
        return MC_PPR_FAILED;
    }
}

/* --------------------------------------------------------------------------------------------- */
