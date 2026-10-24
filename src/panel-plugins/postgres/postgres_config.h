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

#define POSTGRES_MAINTENANCE_DB_DEFAULT "postgres"

/*** structures declarations (and typedefs of structures)*****************************************/

/* One [Connection.<name>] section. The keys but name, maintenance_db, description and read_only
   are the keywords of libpq; NULL is a keyword not set, which libpq then takes from the
   environment. */
typedef struct
{
    char *name;
    char *service;
    char *host;
    char *hostaddr;
    char *port;
    char *user;
    char *dbname;  // one database: the list of databases is left out
    char *sslmode;
    char *maintenance_db;  // where the list of databases is read; NULL: "postgres"
    char *description;
    gboolean read_only;
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
