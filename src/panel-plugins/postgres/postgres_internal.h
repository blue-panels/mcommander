/*
   Panel plugin postgres for the M-Commander
   the state of a panel and the functions its modules share

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

/** \file postgres_internal.h
 *  \brief PostgreSQL plugin: the state of a panel and the functions its modules share.
 */

#ifndef MC_PANEL_POSTGRES_INTERNAL_H
#define MC_PANEL_POSTGRES_INTERNAL_H

#include "lib/global.h"
#include "lib/panel-plugin.h"  // mc_panel_host_t, mc_pp_result_t

#include "postgres_config.h"
#include "postgres_conn.h"  // PGconn

/*** typedefs(not structures) and defined constants **********************************************/

#define POSTGRES_HELP_NODE "[PostgreSQL Plugin]"
#define POSTGRES_HELP_FILE MC_PLUGIN_DIR "/postgres_panel.md"

/*** enums ***************************************************************************************/

typedef enum
{
    POSTGRES_LEVEL_CONNECTIONS = 0,
    POSTGRES_LEVEL_DATABASES,
    POSTGRES_LEVEL_SCHEMAS,
    POSTGRES_LEVEL_RELATIONS,
} postgres_level_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/* An entry of the panel below the connections */
typedef struct
{
    char *name;
    char *owner;
    gint64 size;  // bytes, -1: not known
    char kind;    // of a relation: relkind of pg_class
    gint64 rows;  // of a relation: the estimate of the statistics, -1: none
} postgres_item_t;

typedef struct
{
    mc_panel_host_t *host;
    postgres_config_t *cfg;
    postgres_level_t level;

    postgres_connection_t *connection;  // the open one, in cfg
    PGconn *conn;
    char *schema;      // the open one
    GPtrArray *items;  // postgres_item_t *, of the level

    char *focus_after_up;  // the entry the cursor goes to when the list is read next
} postgres_data_t;

/*** declarations of public functions ************************************************************/

/* postgres_nav.c */

mc_pp_result_t postgres_get_items (void *plugin_data, void *list);
mc_pp_result_t postgres_chdir (void *plugin_data, const char *path);
const postgres_item_t *postgres_find_item (const postgres_data_t *data, const char *name);
void postgres_session_close (postgres_data_t *data);
/* A query of the panel: a connection found lost is opened again, to the same database, and the
   query run once more. NULL with a message shown on failure. */
PGresult *postgres_query (postgres_data_t *data, const char *sql, int nparams,
                          const char *const *params);
/* The name of the kind of a relation: "table", "view"... */
const char *postgres_relkind_name (char kind);

/* postgres_ddl.c */

/* The SQL that makes the relation @name of the open schema, rebuilt from the catalog; NULL with
   a message shown on failure */
char *postgres_ddl (postgres_data_t *data, const char *name);

/* postgres_columns.c */

const mc_panel_column_t *postgres_get_columns (void *plugin_data, size_t *count);
const char *postgres_get_column_value (void *plugin_data, const char *fname, const char *column_id);
const char *postgres_get_default_format (void *plugin_data);

/* postgres_console.c */

/* Show the query editor coole, or psql without it, for the connection and the database under
   the cursor, or the open ones, started the first time; @relation, when not NULL, is the one
   the editor is told of, else the one under the cursor. */
mc_pp_result_t postgres_console_open (postgres_data_t *data, const char *relation);
/* Before the plugin is unloaded: the screens stay, but no longer call back */
void postgres_console_shutdown (void);

/* postgres_connections.c */

mc_pp_result_t postgres_create_connection (postgres_data_t *data);
mc_pp_result_t postgres_edit_connection (postgres_data_t *data);
mc_pp_result_t postgres_delete_connections (postgres_data_t *data, const char **names, int count);
void postgres_configure (void);

#endif
