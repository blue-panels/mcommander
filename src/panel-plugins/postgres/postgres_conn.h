/*
   Panel plugin postgres for the M-Commander
   the connection to a server and the queries of the panel

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

/** \file postgres_conn.h
 *  \brief PostgreSQL plugin: the connection to a server and the queries of the panel.
 */

#ifndef MC_PANEL_POSTGRES_CONN_H
#define MC_PANEL_POSTGRES_CONN_H

#include <libpq-fe.h>

#include "lib/global.h"
#include "lib/panel-plugin.h"  // mc_panel_host_t

#include "postgres_config.h"

/*** declarations of public functions ************************************************************/

/* Connect to @database of @c, or to the database of @c when NULL; a status window says what it
   waits for, Ctrl-C gives up. A server that asks for a password the connection has not got
   asks for it here, and it is kept in @c for the session; with save_password it goes to
   ~/.pgpass once it has worked. NULL with @err_out (the caller's to free) on failure; *@err_out
   NULL is the user who gave up. */
PGconn *postgres_conn_open (mc_panel_host_t *host, const postgres_config_t *cfg,
                            postgres_connection_t *c, const char *database, char **err_out);

/* Run @sql with @nparams text parameters, Ctrl-C cancelling it. The result of the last command,
   PGRES_TUPLES_OK or PGRES_COMMAND_OK, else NULL with @err_out. */
PGresult *postgres_conn_query (PGconn *conn, const char *sql, int nparams,
                               const char *const *params, char **err_out);

/* The URL of the fields of @c: what libpq is given in POSTGRES_BY_HOST, without the password */
char *postgres_conn_url (const postgres_connection_t *c);

/* What the server tells of itself: "PostgreSQL 16.4, database x as y on host:port" */
char *postgres_conn_describe (PGconn *conn);

#endif
