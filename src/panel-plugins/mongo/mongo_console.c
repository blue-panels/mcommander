/*
   Panel plugin mongo for the M-Commander
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

/** \file mongo_console.c
 *  \brief Source: the query editor coole in a terminal screen of its own
 *
 *  Alt-F3 runs "coole -Q", the editor of queries for mongosh, for the cluster and the database
 *  under the cursor, in a screen beside the panels; Alt-F3 there comes back. Without coole,
 *  mongosh itself runs there, connected. One of them runs per cluster and database, for as
 *  long as its screen stays. The URI goes to it in its environment, never on its command line,
 *  where ps would show the password.
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"

#include "mongo_internal.h"
#include "mongo_ui.h"

/*** file scope macro definitions ****************************************************************/

/* What mongosh runs before it waits for input, when it stands in for coole: the connection,
   from the same variables coole reads; void 0, so that it prints no value of its own. */
#define MONGOSH_CONNECT                                                                            \
    "db = connect(process.env.MC_MONGO_URI); "                                                     \
    "if (process.env.MC_MONGO_DB) db = db.getSiblingDB(process.env.MC_MONGO_DB); void 0;"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *key;  // "<cluster>\n<database>": a name of a section of mongo.ini has no newline
    void *terminal;
    void (*forget) (mc_panel_host_t *host, void *terminal);
} mongo_console_t;

/*** file scope variables ************************************************************************/

/* key -> mongo_console_t * */
static GHashTable *consoles = NULL;

/*** file scope functions ************************************************************************/

static void
console_free (gpointer p)
{
    mongo_console_t *c = (mongo_console_t *) p;

    g_free (c->key);
    g_free (c);
}

/* --------------------------------------------------------------------------------------------- */

/* The screen has gone, and coole or mongosh with it. */
static void
console_closed (void *data)
{
    mongo_console_t *c = (mongo_console_t *) data;

    if (consoles != NULL)
        g_hash_table_remove (consoles, c->key);
}

/* --------------------------------------------------------------------------------------------- */

/* The cluster and the database the cursor is on; the database may be NULL. */
static void
console_target (const mongo_data_t *data, const mongo_cluster_t **cluster, const char **db)
{
    const GString *cur = NULL;
    const char *name = NULL;

    *cluster = data->cluster;
    *db = data->db_name;

    if (data->host != NULL && data->host->get_current != NULL)
        cur = data->host->get_current (data->host);
    if (cur != NULL && cur->len != 0 && strcmp (cur->str, "..") != 0)
        name = cur->str;

    switch (data->level)
    {
    case MONGO_LEVEL_CLUSTERS:
        *cluster = name != NULL ? mongo_config_find_cluster (data->cfg, name) : NULL;
        *db = NULL;
        break;

    case MONGO_LEVEL_DBS:
        *db = NULL;
        if (name != NULL && data->db_names != NULL)
            for (char **d = data->db_names; *d != NULL; d++)
                if (strcmp (*d, name) == 0)
                    *db = *d;
        break;

    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The setting, else the program found in PATH; NULL for neither. The setting is a path, ~/ the
   home directory, or a name looked for in PATH the way the terminal looks for it. A setting that
   names no program counts as none, so that the next choice is tried. */
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

static mongo_console_t *
console_start (mongo_data_t *data, const char *key, const mongo_cluster_t *cluster, const char *db)
{
    mc_panel_host_t *host = data->host;
    mongo_console_t *c;
    mc_pp_terminal_t spec;
    const char *argv[6];
    GPtrArray *env;
    char *program, *title, *mongosh;
    const char *what;

    program = console_program (data->cfg->coole_path, "coole");
    if (program != NULL)
    {
        what = "coole";
        argv[0] = program;
        argv[1] = "-Q";
        argv[2] = NULL;
    }
    else
    {
        // without the editor of queries, mongosh itself, connected
        program = console_program (data->cfg->mongosh_path, "mongosh");
        if (program == NULL)
        {
            mongo_show_message (host, TRUE,
                                _ ("Install coole for the query editor, or mongosh. Their paths "
                                   "can be given in the MongoDB settings."));
            return NULL;
        }
        what = "mongosh";
        argv[0] = program;
        argv[1] = "--nodb";
        argv[2] = "--shell";
        argv[3] = "--eval";
        argv[4] = MONGOSH_CONNECT;
        argv[5] = NULL;
    }

    // the URI has the password in it: the environment of the program, never its command line
    env = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add (env, g_strconcat ("MC_MONGO_URI=", cluster->uri, (char *) NULL));
    if (db != NULL)
        g_ptr_array_add (env, g_strconcat ("MC_MONGO_DB=", db, (char *) NULL));
    // the mongosh coole runs, as it is found here
    mongosh = data->cfg->mongosh_path != NULL && data->cfg->mongosh_path[0] != '\0'
        ? console_program (data->cfg->mongosh_path, "mongosh")
        : NULL;
    if (mongosh != NULL)
        g_ptr_array_add (env, g_strconcat ("MC_MONGOSH=", mongosh, (char *) NULL));
    g_free (mongosh);
    g_ptr_array_add (env, NULL);

    c = g_new0 (mongo_console_t, 1);
    c->key = g_strdup (key);
    c->forget = host->terminal_forget;

    title = db != NULL ? g_strdup_printf ("%s: %s / %s", what, cluster->name, db)
                       : g_strdup_printf ("%s: %s", what, cluster->name);

    memset (&spec, 0, sizeof (spec));
    spec.title = title;
    spec.argv = argv;
    spec.env = (const char *const *) env->pdata;
    spec.on_close = console_closed;
    spec.data = c;
    c->terminal = host->terminal_open (host, &spec);

    // the URI is wiped from the plugin's memory; the program keeps its copy
    memset (env->pdata[0], 0, strlen (env->pdata[0]));
    g_ptr_array_free (env, TRUE);
    g_free (title);
    g_free (program);

    if (c->terminal == NULL)
    {
        char *msg = g_strdup_printf (_ ("Cannot start %s."), what);

        mongo_show_message (host, TRUE, msg);
        g_free (msg);
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
mongo_console_open (mongo_data_t *data)
{
    mc_panel_host_t *host = data->host;
    const mongo_cluster_t *cluster;
    const char *db;
    mongo_console_t *c;
    char *key;

    /* MC_PPR_SKIPPED all through: nothing in the panel has changed, it is not read again
       from the server. */
    if (host == NULL || host->terminal_open == NULL)
    {
        mongo_show_message (host, TRUE, _ ("This build of mc has no terminal for coole."));
        return MC_PPR_SKIPPED;
    }

    console_target (data, &cluster, &db);
    if (cluster == NULL)
        return MC_PPR_SKIPPED;

    key = g_strconcat (cluster->name, "\n", db != NULL ? db : "", (char *) NULL);
    c = consoles != NULL ? g_hash_table_lookup (consoles, key) : NULL;

    /* Not shown: its program was quit while the screen was not in front, and the screen is
       gone; a new one comes. */
    if (c == NULL || !host->terminal_show (host, c->terminal))
    {
        c = console_start (data, key, cluster, db);
        if (c != NULL)
            host->terminal_show (host, c->terminal);
    }
    g_free (key);
    return MC_PPR_SKIPPED;
}

/* --------------------------------------------------------------------------------------------- */

void
mongo_console_shutdown (void)
{
    GHashTableIter it;
    gpointer value;

    if (consoles == NULL)
        return;

    // the screens may outlive the plugin's code: they must not call back into it
    g_hash_table_iter_init (&it, consoles);
    while (g_hash_table_iter_next (&it, NULL, &value))
    {
        mongo_console_t *c = (mongo_console_t *) value;

        if (c->forget != NULL)
            c->forget (NULL, c->terminal);
    }
    g_clear_pointer (&consoles, g_hash_table_destroy);
}
