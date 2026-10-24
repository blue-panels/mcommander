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

/* $1: the system schemas too. The temporary schemas of the sessions are never shown. */
#define SQL_SCHEMAS                                                                                \
    "SELECT n.nspname, pg_catalog.pg_get_userbyid (n.nspowner) FROM pg_catalog.pg_namespace n "    \
    "WHERE n.nspname !~ '^pg_(toast_)?temp_' "                                                     \
    "AND ($1::boolean OR (n.nspname !~ '^pg_' AND n.nspname <> 'information_schema')) ORDER BY 1"

/* The partitions of the partitioned table c.oid, at every level, as p */
#define SQL_PARTITIONS_OF_C                                                                        \
    "(WITH RECURSIVE t (oid) AS (SELECT inhrelid FROM pg_catalog.pg_inherits "                     \
    "WHERE inhparent = c.oid UNION ALL SELECT i.inhrelid FROM pg_catalog.pg_inherits i "           \
    "JOIN t ON i.inhparent = t.oid) SELECT oid FROM t) "

/* $1: the schema. A partition is left out: its rows are those of its parent, whose size and
   estimate of rows are the sums of the partitions under it, those partitioned in turn too. */
#define SQL_RELATIONS                                                                              \
    "SELECT c.relname, c.relkind, pg_catalog.pg_get_userbyid (c.relowner), "                       \
    "CASE WHEN c.relkind IN ('r', 'm', 'S') THEN pg_catalog.pg_total_relation_size (c.oid) "       \
    "WHEN c.relkind = 'p' THEN (SELECT sum (pg_catalog.pg_total_relation_size (p.oid)) "           \
    "FROM pg_catalog.pg_class p WHERE p.relkind <> 'p' AND p.oid IN " SQL_PARTITIONS_OF_C          \
    ") END, "                                                                                      \
    "CASE WHEN c.relkind IN ('r', 'm', 'f') AND c.reltuples >= 0 THEN c.reltuples::bigint "        \
    "WHEN c.relkind = 'p' THEN (SELECT sum (p.reltuples) FILTER (WHERE p.reltuples >= 0) "         \
    "FROM pg_catalog.pg_class p WHERE p.relkind <> 'p' AND p.oid IN " SQL_PARTITIONS_OF_C          \
    ")::bigint END "                                                                               \
    "FROM pg_catalog.pg_class c JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "         \
    "WHERE n.nspname = $1 AND c.relkind IN ('r', 'p', 'v', 'm', 'f', 'S') "                        \
    "AND NOT c.relispartition ORDER BY 1"

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

static gboolean
read_schemas (postgres_data_t *data)
{
    const char *params[1];
    GPtrArray *items;
    PGresult *res;

    params[0] = data->cfg->show_system_schemas ? "true" : "false";
    res = query (data, SQL_SCHEMAS, 1, params);
    if (res == NULL)
        return FALSE;

    items = g_ptr_array_new_with_free_func (item_free);
    for (int i = 0; i < PQntuples (res); i++)
    {
        postgres_item_t *it = item_new (PQgetvalue (res, i, 0));

        it->owner = g_strdup (field (res, i, 1));
        g_ptr_array_add (items, it);
    }
    PQclear (res);
    items_take (data, items);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
read_relations (postgres_data_t *data, const char *schema)
{
    const char *params[1] = { schema };
    GPtrArray *items;
    PGresult *res;

    res = query (data, SQL_RELATIONS, 1, params);
    if (res == NULL)
        return FALSE;

    items = g_ptr_array_new_with_free_func (item_free);
    for (int i = 0; i < PQntuples (res); i++)
    {
        postgres_item_t *it = item_new (PQgetvalue (res, i, 0));

        it->kind = PQgetvalue (res, i, 1)[0];
        it->owner = g_strdup (field (res, i, 2));
        it->size = field_int (res, i, 3);
        it->rows = field_int (res, i, 4);
        g_ptr_array_add (items, it);
    }
    PQclear (res);
    items_take (data, items);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The connection goes to @name, unless it is there already; the one it was on stays until the
   new one is open */
static mc_pp_result_t
enter_database (postgres_data_t *data, const char *name)
{
    if (strcmp (PQdb (data->conn), name) != 0)
    {
        PGconn *conn;
        char *err = NULL;

        conn = postgres_conn_open (data->host, data->cfg, data->connection, name, &err);
        if (conn == NULL)
        {
            if (err != NULL)
                postgres_message (data->host, TRUE, _ ("Cannot connect to \"%s\":\n%s"), name, err);
            g_free (err);
            return MC_PPR_FAILED;
        }
        PQfinish (data->conn);
        data->conn = conn;
    }

    if (!read_schemas (data))
        return MC_PPR_FAILED;
    data->level = POSTGRES_LEVEL_SCHEMAS;
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
enter_schema (postgres_data_t *data, const char *name)
{
    if (!read_relations (data, name))
        return MC_PPR_FAILED;
    g_free (data->schema);
    data->schema = g_strdup (name);
    data->level = POSTGRES_LEVEL_RELATIONS;
    return MC_PPR_OK;
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
    g_clear_pointer (&data->schema, g_free);
}

/* --------------------------------------------------------------------------------------------- */

const char *
postgres_relkind_name (char kind)
{
    switch (kind)
    {
    case 'r':
        return "table";
    case 'p':
        return "partitioned";
    case 'v':
        return "view";
    case 'm':
        return "matview";
    case 'f':
        return "foreign";
    case 'S':
        return "sequence";
    default:
        return "";
    }
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

            // a relation is a file: Enter on it opens the query editor, F3 shows it
            mc_pp_add_entry (list, it->name,
                             data->level == POSTGRES_LEVEL_RELATIONS ? S_IFREG | 0644
                                                                     : S_IFDIR | 0755,
                             it->size >= 0 ? (off_t) it->size : 0, now);
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
        case POSTGRES_LEVEL_SCHEMAS:
            // the list of databases is read again, from the database left
            if (!read_databases (data))
                return MC_PPR_FAILED;
            set_focus (data, PQdb (data->conn));
            data->level = POSTGRES_LEVEL_DATABASES;
            return MC_PPR_OK;
        case POSTGRES_LEVEL_RELATIONS:
            if (!read_schemas (data))
                return MC_PPR_FAILED;
            set_focus (data, data->schema);
            g_clear_pointer (&data->schema, g_free);
            data->level = POSTGRES_LEVEL_SCHEMAS;
            return MC_PPR_OK;
        default:
            return MC_PPR_FAILED;
        }

    switch (data->level)
    {
    case POSTGRES_LEVEL_CONNECTIONS:
        return enter_connection (data, path);
    case POSTGRES_LEVEL_DATABASES:
        return enter_database (data, path);
    case POSTGRES_LEVEL_SCHEMAS:
        return enter_schema (data, path);
    default:
        return MC_PPR_FAILED;
    }
}

/* --------------------------------------------------------------------------------------------- */
