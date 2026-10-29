/*
   Panel plugin postgres for the M-Commander
   the query editor coole in a terminal screen of its own

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

/** \file postgres_console.c
 *  \brief Source: the query editor coole in a terminal screen of its own
 *
 *  Alt-F3, and Enter on a relation, run "coole -Q", the editor of queries, for the connection
 *  and the database under the cursor, or the open ones, in a screen beside the panels; without
 *  coole, psql. One of them runs per connection and database, for as long as its screen stays.
 *
 *  What to connect to goes in the environment, as the PG* variables of libpq: those of the
 *  connection of the panel, as libpq has resolved them (PQconninfo), so that the editor goes
 *  where the panel went, whether the server was named by its host, a URL or a service. A
 *  password typed in the session or written in the URL goes as PGPASSWORD, never on the command
 *  line. MC_PG_CONNECTION, MC_PG_SCHEMA and MC_PG_RELATION tell coole where the cursor was.
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"

#include "postgres_internal.h"
#include "postgres_ui.h"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *key;  // "<connection>\n<database>": a name of a section of postgres.ini has no newline
    void *terminal;
    void (*forget) (mc_panel_host_t *host, void *terminal);
} postgres_console_t;

/* Where the cursor is: the connection, the database, the schema and the relation, or NULL */
typedef struct
{
    postgres_connection_t *connection;
    const char *database;
    const char *schema;
    const char *relation;
} console_target_t;

/*** file scope variables ************************************************************************/

/* key -> postgres_console_t * */
static GHashTable *consoles = NULL;

/* The keywords of libpq that are the panel's own, not the server's; and the service, whose file
   libpq reads before the environment: its dbname would win over PGDATABASE, its options over
   PGOPTIONS. What the service gave is in the others, resolved. */
static const char *const own_keywords[] = {
    "password",        "passfile", "options", "application_name", "fallback_application_name",
    "client_encoding", "service",
};

/* Those that go always, for the editor to tell where it connects */
static const char *const server_keywords[] = { "host", "port", "user" };

/*** file scope functions ************************************************************************/

static void
console_free (gpointer p)
{
    postgres_console_t *c = (postgres_console_t *) p;

    g_free (c->key);
    g_free (c);
}

/* --------------------------------------------------------------------------------------------- */

/* The screen has gone, and coole or psql with it */
static void
console_closed (void *data)
{
    postgres_console_t *c = (postgres_console_t *) data;

    if (consoles != NULL)
        g_hash_table_remove (consoles, c->key);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
cursor_name (const postgres_data_t *data)
{
    const GString *cur = NULL;

    if (data->host->get_current != NULL)
        cur = data->host->get_current (data->host);
    if (cur == NULL || cur->len == 0 || strcmp (cur->str, "..") == 0)
        return NULL;
    return cur->str;
}

/* --------------------------------------------------------------------------------------------- */

static console_target_t
console_target (const postgres_data_t *data, const char *relation)
{
    console_target_t t;
    const char *name = cursor_name (data);

    memset (&t, 0, sizeof (t));
    t.connection = data->connection;
    if (data->conn != NULL)
        t.database = PQdb (data->conn);

    switch (data->level)
    {
    case POSTGRES_LEVEL_CONNECTIONS:
        t.connection = name != NULL ? postgres_config_find (data->cfg, name) : NULL;
        break;
    case POSTGRES_LEVEL_DATABASES:
        if (name != NULL && postgres_find_item (data, name) != NULL)
            t.database = name;
        break;
    case POSTGRES_LEVEL_SCHEMAS:
        if (name != NULL && postgres_find_item (data, name) != NULL)
            t.schema = name;
        break;
    case POSTGRES_LEVEL_RELATIONS:
        t.schema = data->schema;
        if (relation == NULL && name != NULL && postgres_find_item (data, name) != NULL)
            relation = name;
        t.relation = relation;
        break;
    default:
        break;
    }
    return t;
}

/* --------------------------------------------------------------------------------------------- */

/* The setting, else the program found in PATH; NULL for neither. The setting is a path, ~/ the
   home directory, or a name looked for in PATH. */
static char *
console_program (const char *setting, const char *name)
{
    char *home_path, *found;

    if (setting == NULL || setting[0] == '\0')
        return g_find_program_in_path (name);

    if (strncmp (setting, "~/", 2) != 0)
        return g_find_program_in_path (setting);

    home_path = g_build_filename (g_get_home_dir (), setting + 2, (char *) NULL);
    found = g_find_program_in_path (home_path);
    g_free (home_path);
    return found;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
keyword_in (const char *keyword, const char *const *list, gsize n)
{
    for (gsize i = 0; i < n; i++)
        if (strcmp (keyword, list[i]) == 0)
            return TRUE;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* The value libpq takes for @keyword when it is not given: the environment, else its own */
static const char *
default_value (const PQconninfoOption *defaults, const char *keyword)
{
    for (const PQconninfoOption *o = defaults; o != NULL && o->keyword != NULL; o++)
        if (strcmp (o->keyword, keyword) == 0)
            return o->val;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* The PG* variables of @conn, but its database: @database. The host, the port and the user go
   always, the others where they are not what libpq would take anyway: the environment of mc
   goes to the editor too. */
static void
console_env_pg (GPtrArray *env, PGconn *conn, const char *database)
{
    PQconninfoOption *opts, *defaults;

    opts = PQconninfo (conn);
    defaults = PQconndefaults ();
    for (PQconninfoOption *o = opts; o != NULL && o->keyword != NULL; o++)
    {
        if (o->envvar == NULL || o->val == NULL || *o->val == '\0'
            || strcmp (o->keyword, "dbname") == 0
            || keyword_in (o->keyword, own_keywords, G_N_ELEMENTS (own_keywords)))
            continue;
        if (!keyword_in (o->keyword, server_keywords, G_N_ELEMENTS (server_keywords))
            && g_strcmp0 (o->val, default_value (defaults, o->keyword)) == 0)
            continue;
        g_ptr_array_add (env, g_strconcat (o->envvar, "=", o->val, (char *) NULL));
    }
    PQconninfoFree (defaults);
    PQconninfoFree (opts);
    g_ptr_array_add (env, g_strconcat ("PGDATABASE=", database, (char *) NULL));
}

/* --------------------------------------------------------------------------------------------- */

/* The password @conn was given, typed in the session or written in its URL; NULL for none, as
   for one of ~/.pgpass, which the editor reads itself */
static char *
console_password (PGconn *conn)
{
    PQconninfoOption *opts;
    char *password = NULL;

    opts = PQconninfo (conn);
    for (PQconninfoOption *o = opts; o != NULL && o->keyword != NULL; o++)
        if (strcmp (o->keyword, "password") == 0 && o->val != NULL)
        {
            if (*o->val != '\0')
                password = g_strdup (o->val);
            memset (o->val, 0, strlen (o->val));
        }
    PQconninfoFree (opts);
    return password;
}

/* --------------------------------------------------------------------------------------------- */

static postgres_console_t *
console_start (postgres_data_t *data, const char *key, const console_target_t *t, PGconn *conn)
{
    mc_panel_host_t *host = data->host;
    postgres_connection_t *pc = t->connection;
    postgres_console_t *c;
    mc_pp_terminal_t spec;
    const char *argv[3];
    GPtrArray *env;
    char *program, *title, *password;
    const char *what;
    int password_at = -1;

    program = console_program (data->cfg->coole_path, "coole");
    what = "coole";
    if (program == NULL)
    {
        // without the editor of queries, psql, connected
        program = console_program (data->cfg->psql_path, "psql");
        what = "psql";
        if (program == NULL)
        {
            postgres_message (host, TRUE,
                              _ ("Install coole for the query editor, or psql. Their paths can be "
                                 "given in the PostgreSQL settings."));
            return NULL;
        }
    }
    argv[0] = program;
    argv[1] = strcmp (what, "coole") == 0 ? "-Q" : NULL;
    argv[2] = NULL;

    env = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add (env, g_strdup ("MC_QUERY_KIND=postgres"));
    console_env_pg (env, conn, t->database);
    password = console_password (conn);
    if (password != NULL)
    {
        password_at = (int) env->len;
        g_ptr_array_add (env, g_strconcat ("PGPASSWORD=", password, (char *) NULL));
        memset (password, 0, strlen (password));
        g_free (password);
    }
    g_ptr_array_add (env, g_strconcat ("MC_PG_CONNECTION=", pc->name, (char *) NULL));
    if (t->schema != NULL)
        g_ptr_array_add (env, g_strconcat ("MC_PG_SCHEMA=", t->schema, (char *) NULL));
    if (t->relation != NULL)
        g_ptr_array_add (env, g_strconcat ("MC_PG_RELATION=", t->relation, (char *) NULL));
    if (pc->read_only)
    {
        g_ptr_array_add (env, g_strdup ("MC_QUERY_READ_ONLY=1"));
        // what coole does with MC_QUERY_READ_ONLY, for psql
        if (strcmp (what, "psql") == 0)
            g_ptr_array_add (env, g_strdup ("PGOPTIONS=-c default_transaction_read_only=on"));
    }
    g_ptr_array_add (env, NULL);

    c = g_new0 (postgres_console_t, 1);
    c->key = g_strdup (key);
    c->forget = host->terminal_forget;

    title = g_strdup_printf ("%s: %s / %s", what, pc->name, t->database);

    memset (&spec, 0, sizeof (spec));
    spec.title = title;
    spec.argv = argv;
    spec.env = (const char *const *) env->pdata;
    spec.on_close = console_closed;
    spec.data = c;
    c->terminal = host->terminal_open (host, &spec);

    // the password is wiped from the plugin's copy; the program keeps its own
    if (password_at >= 0)
    {
        char *p = g_ptr_array_index (env, password_at);

        memset (p, 0, strlen (p));
    }
    g_ptr_array_free (env, TRUE);
    g_free (title);
    g_free (program);

    if (c->terminal == NULL)
    {
        postgres_message (host, TRUE, _ ("Cannot start %s."), what);
        console_free (c);
        return NULL;
    }

    if (consoles == NULL)
        consoles = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, console_free);
    g_hash_table_insert (consoles, c->key, c);
    return c;
}

/*** public functions ****************************************************************************/

mc_pp_result_t
postgres_console_open (postgres_data_t *data, const char *relation)
{
    mc_panel_host_t *host = data->host;
    console_target_t t;
    postgres_console_t *c;
    PGconn *conn = data->conn, *own = NULL;
    char *key;

    // MC_PPR_SKIPPED all through: nothing in the panel has changed
    if (host->terminal_open == NULL)
    {
        postgres_message (host, TRUE, _ ("This build of mc has no terminal for coole."));
        return MC_PPR_SKIPPED;
    }

    t = console_target (data, relation);
    if (t.connection == NULL)
        return MC_PPR_SKIPPED;

    // on the list of connections: a connection is made, for what libpq resolves and the password
    if (conn == NULL)
    {
        char *err = NULL;

        own = conn = postgres_conn_open (host, data->cfg, t.connection, NULL, &err);
        if (conn == NULL)
        {
            if (err != NULL)
                postgres_message (host, TRUE, _ ("Cannot connect to \"%s\":\n%s"),
                                  t.connection->name, err);
            g_free (err);
            return MC_PPR_SKIPPED;
        }
        t.database = PQdb (conn);
    }

    key = g_strconcat (t.connection->name, "\n", t.database, (char *) NULL);
    c = consoles != NULL ? g_hash_table_lookup (consoles, key) : NULL;

    // not shown: its program was quit while the screen was not in front, and the screen is gone
    if (c == NULL || !host->terminal_show (host, c->terminal))
    {
        c = console_start (data, key, &t, conn);

        if (c != NULL)
            host->terminal_show (host, c->terminal);
    }
    g_free (key);
    if (own != NULL)
        PQfinish (own);
    return MC_PPR_SKIPPED;
}

/* --------------------------------------------------------------------------------------------- */

void
postgres_console_shutdown (void)
{
    GHashTableIter it;
    gpointer value;

    if (consoles == NULL)
        return;

    // the screens may outlive the plugin's code: they must not call back into it
    g_hash_table_iter_init (&it, consoles);
    while (g_hash_table_iter_next (&it, NULL, &value))
    {
        postgres_console_t *c = (postgres_console_t *) value;

        if (c->forget != NULL)
            c->forget (NULL, c->terminal);
    }
    g_clear_pointer (&consoles, g_hash_table_destroy);
}

/* --------------------------------------------------------------------------------------------- */
