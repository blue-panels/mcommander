/*
   Panel plugin postgres for the M-Commander
   postgres.ini, the general options and the connections

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

/** \file postgres_config.h
 *  \brief PostgreSQL plugin: postgres.ini, the general options and the connections.
 */

#ifndef MC_PANEL_POSTGRES_CONFIG_H
#define MC_PANEL_POSTGRES_CONFIG_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/* What names the server: the fields of the dialog, a URL, or a service of pg_service.conf */
typedef enum
{
    POSTGRES_BY_HOST = 0,
    POSTGRES_BY_URL,
    POSTGRES_BY_SERVICE,
} postgres_connect_by_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/* One [Connection.<name>] section. A field NULL is not given to libpq, which then takes it from
   the service file or the environment, the way psql does. */
typedef struct
{
    char *name;
    postgres_connect_by_t connect_by;
    char *url;      // postgresql://...; POSTGRES_BY_URL only
    char *service;  // POSTGRES_BY_SERVICE only
    char *host;     // POSTGRES_BY_HOST only, with port
    char *port;
    char *database;  // with show_all_databases, where the list of databases is read
    char *user;
    char *sslmode;
    char *description;
    gboolean show_all_databases;  // FALSE: the panel has only the database
    gboolean save_password;       // the password goes to ~/.pgpass once it has connected
    gboolean read_only;           // the query editor opens read-only transactions

    char *password;           // typed in this session, never written to postgres.ini
    gboolean password_saved;  // that password has gone to ~/.pgpass already
} postgres_connection_t;

typedef struct
{
    char *default_connection;
    int connect_timeout;    // seconds, 0: none
    int statement_timeout;  // seconds, 0: none
    gboolean show_system_schemas;

    /* coole, the editor of queries, and psql for Alt-F3; NULL or empty: looked up in PATH */
    char *coole_path;
    char *psql_path;

    GPtrArray *connections;  // postgres_connection_t *
} postgres_config_t;

/*** declarations of public functions ************************************************************/

/* The file, or the defaults with no connection when there is none. */
postgres_config_t *postgres_config_load (void);
void postgres_config_free (postgres_config_t *cfg);

postgres_connection_t *postgres_connection_copy (const postgres_connection_t *c);
void postgres_connection_free (gpointer p);
/* The fields of @c freed and zeroed, the password wiped */
void postgres_connection_clear (postgres_connection_t *c);

postgres_connection_t *postgres_config_find (const postgres_config_t *cfg, const char *name);

/* A name of a connection becomes the name of a section: no '[', ']', line breaks, nor blanks
   around it. */
gboolean postgres_connection_name_valid (const char *name);

char *postgres_config_path (void);

/* Write [General] back, keeping the connections. FALSE with @err_out (the caller's to free) on
   failure. */
gboolean postgres_config_save_general (const postgres_config_t *cfg, char **err_out);

/* Write the section of @c; @old_name, when not NULL and not the name of @c, is removed in the
   same save. The file is created at mode 0600. */
gboolean postgres_config_save_connection (const postgres_connection_t *c, const char *old_name,
                                          char **err_out);

gboolean postgres_config_delete_connection (const char *name, char **err_out);

#endif
