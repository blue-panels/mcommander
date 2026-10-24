/*
   Panel plugin postgres for the M-Commander
   the loader and the writer of postgres.ini

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

/** \file postgres_config.c
 *  \brief Source: the loader and the writer of postgres.ini
 *
 *  ~/.config/mc6/postgres.ini has [General] and one [Connection.<name>] section per
 *  connection. A password is not kept there, but for one written in a URL: it is in ~/.pgpass or
 *  in the service file, where libpq finds it. The file is made at mode 0600 all the same, since
 *  it tells where the databases are.
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/mcconfig.h"

#include "postgres_config.h"

/*** file scope macro definitions ****************************************************************/

#define POSTGRES_INI_FILE         "postgres.ini"

#define SECT_GENERAL              "General"
#define SECT_CONNECTION_          "Connection."

#define DEFAULT_CONNECT_TIMEOUT   10
#define DEFAULT_STATEMENT_TIMEOUT 30
#define DEFAULT_PREVIEW_ROWS      100

/*** file scope variables ************************************************************************/

/* The string keys of a section, in the order they are written */
static const struct
{
    const char *key;
    gsize offset;
} connection_keys[] = {
    { "url", G_STRUCT_OFFSET (postgres_connection_t, url) },
    { "service", G_STRUCT_OFFSET (postgres_connection_t, service) },
    { "host", G_STRUCT_OFFSET (postgres_connection_t, host) },
    { "port", G_STRUCT_OFFSET (postgres_connection_t, port) },
    { "database", G_STRUCT_OFFSET (postgres_connection_t, database) },
    { "user", G_STRUCT_OFFSET (postgres_connection_t, user) },
    { "sslmode", G_STRUCT_OFFSET (postgres_connection_t, sslmode) },
    { "description", G_STRUCT_OFFSET (postgres_connection_t, description) },
};

static const char *const connect_by_names[] = { "host", "url", "service" };

/*** file scope functions ************************************************************************/

static char **
connection_field (postgres_connection_t *c, gsize i)
{
    return G_STRUCT_MEMBER_P (c, connection_keys[i].offset);
}

/* --------------------------------------------------------------------------------------------- */

/* The string of the file, NULL for none or an empty one */
static char *
config_get_string (mc_config_t *ini, const char *group, const char *key)
{
    char *s;

    s = mc_config_get_string (ini, group, key, "");
    if (s != NULL)
        g_strstrip (s);
    if (s != NULL && *s == '\0')
        g_clear_pointer (&s, g_free);
    return s;
}

/* --------------------------------------------------------------------------------------------- */

static void
config_set_string (mc_config_t *ini, const char *group, const char *key, const char *value)
{
    if (value != NULL && *value != '\0')
        mc_config_set_string (ini, group, key, value);
    else
        mc_config_del_key (ini, group, key);
}

/* --------------------------------------------------------------------------------------------- */

/* Open the file for a change: what is in it already is kept */
static mc_config_t *
config_open_rw (char **path, char **err_out)
{
    mc_config_t *ini;

    *path = postgres_config_path ();
    ini = mc_config_init (*path, FALSE);
    if (ini == NULL && err_out != NULL)
        *err_out = g_strdup_printf (_ ("Cannot open %s for writing"), *path);
    return ini;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
config_save (mc_config_t *ini, char *path, char **err_out)
{
    GError *gerr = NULL;
    gboolean ok;
    int fd;

    /* The file is there at 0600 before mc writes it: mc writes over a file that is there, keeping
       its mode, and makes one that is not with the mode of the umask */
    fd = open (path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    ok = fd != -1 && fchmod (fd, S_IRUSR | S_IWUSR) == 0;
    if (fd != -1)
        close (fd);
    if (!ok)
    {
        if (err_out != NULL)
            *err_out = g_strdup_printf (_ ("Cannot write %s: %s"), path, g_strerror (errno));
        mc_config_deinit (ini);
        g_free (path);
        return FALSE;
    }

    ok = mc_config_save_file (ini, &gerr);
    mc_config_deinit (ini);
    if (!ok && err_out != NULL)
        *err_out = g_strdup (gerr != NULL ? gerr->message : _ ("Cannot write postgres.ini"));
    g_clear_error (&gerr);
    g_free (path);
    return ok;
}

/*** public functions ****************************************************************************/

char *
postgres_config_path (void)
{
    return g_build_filename (mc_config_get_path (), POSTGRES_INI_FILE, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
postgres_connection_clear (postgres_connection_t *c)
{
    g_free (c->name);
    for (gsize i = 0; i < G_N_ELEMENTS (connection_keys); i++)
        g_free (*connection_field (c, i));
    if (c->password != NULL)
        memset (c->password, 0, strlen (c->password));
    g_free (c->password);
    memset (c, 0, sizeof (*c));
}

/* --------------------------------------------------------------------------------------------- */

void
postgres_connection_free (gpointer p)
{
    postgres_connection_t *c = (postgres_connection_t *) p;

    if (c == NULL)
        return;
    postgres_connection_clear (c);
    g_free (c);
}

/* --------------------------------------------------------------------------------------------- */

postgres_connection_t *
postgres_connection_copy (const postgres_connection_t *c)
{
    postgres_connection_t *copy;

    copy = g_new0 (postgres_connection_t, 1);
    copy->name = g_strdup (c->name);
    for (gsize i = 0; i < G_N_ELEMENTS (connection_keys); i++)
        *connection_field (copy, i) = g_strdup (*connection_field ((postgres_connection_t *) c, i));
    copy->connect_by = c->connect_by;
    copy->show_all_databases = c->show_all_databases;
    copy->save_password = c->save_password;
    copy->read_only = c->read_only;
    copy->password = g_strdup (c->password);
    return copy;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
postgres_connection_name_valid (const char *name)
{
    size_t len;

    if (name == NULL || *name == '\0' || strpbrk (name, "[]\r\n") != NULL)
        return FALSE;
    len = strlen (name);
    return !g_ascii_isspace (name[0]) && !g_ascii_isspace (name[len - 1]);
}

/* --------------------------------------------------------------------------------------------- */

postgres_config_t *
postgres_config_load (void)
{
    postgres_config_t *cfg;
    char *path;
    mc_config_t *ini;
    gchar **groups;

    cfg = g_new0 (postgres_config_t, 1);
    cfg->connect_timeout = DEFAULT_CONNECT_TIMEOUT;
    cfg->statement_timeout = DEFAULT_STATEMENT_TIMEOUT;
    cfg->preview_rows = DEFAULT_PREVIEW_ROWS;
    cfg->connections = g_ptr_array_new_with_free_func (postgres_connection_free);

    path = postgres_config_path ();
    ini = g_file_test (path, G_FILE_TEST_IS_REGULAR) ? mc_config_init (path, TRUE) : NULL;
    g_free (path);
    if (ini == NULL)
        return cfg;

    cfg->default_connection = config_get_string (ini, SECT_GENERAL, "default_connection");
    cfg->connect_timeout =
        MAX (0, mc_config_get_int (ini, SECT_GENERAL, "connect_timeout", DEFAULT_CONNECT_TIMEOUT));
    cfg->statement_timeout = MAX (
        0, mc_config_get_int (ini, SECT_GENERAL, "statement_timeout", DEFAULT_STATEMENT_TIMEOUT));
    cfg->show_system_schemas = mc_config_get_bool (ini, SECT_GENERAL, "show_system_schemas", FALSE);
    cfg->preview_rows =
        MAX (0, mc_config_get_int (ini, SECT_GENERAL, "preview_rows", DEFAULT_PREVIEW_ROWS));
    cfg->coole_path = config_get_string (ini, SECT_GENERAL, "coole");
    cfg->psql_path = config_get_string (ini, SECT_GENERAL, "psql");

    groups = mc_config_get_groups (ini, NULL);
    for (gchar **g = groups; g != NULL && *g != NULL; g++)
    {
        const char *name;
        postgres_connection_t *c;

        if (!g_str_has_prefix (*g, SECT_CONNECTION_))
            continue;
        name = *g + strlen (SECT_CONNECTION_);
        if (!postgres_connection_name_valid (name))
            continue;

        c = g_new0 (postgres_connection_t, 1);
        c->name = g_strdup (name);
        for (gsize i = 0; i < G_N_ELEMENTS (connection_keys); i++)
            *connection_field (c, i) = config_get_string (ini, *g, connection_keys[i].key);
        {
            char *by = config_get_string (ini, *g, "connect_by");

            for (gsize i = 0; i < G_N_ELEMENTS (connect_by_names); i++)
                if (g_strcmp0 (by, connect_by_names[i]) == 0)
                    c->connect_by = (postgres_connect_by_t) i;
            g_free (by);
        }
        c->show_all_databases = mc_config_get_bool (ini, *g, "show_all_databases", TRUE);
        c->save_password = mc_config_get_bool (ini, *g, "save_password", FALSE);
        c->read_only = mc_config_get_bool (ini, *g, "read_only", FALSE);
        g_ptr_array_add (cfg->connections, c);
    }
    g_strfreev (groups);
    mc_config_deinit (ini);
    return cfg;
}

/* --------------------------------------------------------------------------------------------- */

void
postgres_config_free (postgres_config_t *cfg)
{
    if (cfg == NULL)
        return;
    g_free (cfg->default_connection);
    g_free (cfg->coole_path);
    g_free (cfg->psql_path);
    g_ptr_array_free (cfg->connections, TRUE);
    g_free (cfg);
}

/* --------------------------------------------------------------------------------------------- */

postgres_connection_t *
postgres_config_find (const postgres_config_t *cfg, const char *name)
{
    if (cfg == NULL || name == NULL)
        return NULL;
    for (guint i = 0; i < cfg->connections->len; i++)
    {
        postgres_connection_t *c = g_ptr_array_index (cfg->connections, i);

        if (strcmp (c->name, name) == 0)
            return c;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
postgres_config_save_general (const postgres_config_t *cfg, char **err_out)
{
    mc_config_t *ini;
    char *path;

    ini = config_open_rw (&path, err_out);
    if (ini == NULL)
    {
        g_free (path);
        return FALSE;
    }

    config_set_string (ini, SECT_GENERAL, "default_connection", cfg->default_connection);
    mc_config_set_int (ini, SECT_GENERAL, "connect_timeout", cfg->connect_timeout);
    mc_config_set_int (ini, SECT_GENERAL, "statement_timeout", cfg->statement_timeout);
    mc_config_set_bool (ini, SECT_GENERAL, "show_system_schemas", cfg->show_system_schemas);
    mc_config_set_int (ini, SECT_GENERAL, "preview_rows", cfg->preview_rows);
    config_set_string (ini, SECT_GENERAL, "coole", cfg->coole_path);
    config_set_string (ini, SECT_GENERAL, "psql", cfg->psql_path);
    return config_save (ini, path, err_out);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
postgres_config_save_connection (const postgres_connection_t *c, const char *old_name,
                                 char **err_out)
{
    mc_config_t *ini;
    char *path, *sect;

    if (!postgres_connection_name_valid (c->name))
    {
        if (err_out != NULL)
            *err_out = g_strdup (_ ("Invalid connection name"));
        return FALSE;
    }

    ini = config_open_rw (&path, err_out);
    if (ini == NULL)
    {
        g_free (path);
        return FALSE;
    }

    if (old_name != NULL && strcmp (old_name, c->name) != 0)
    {
        sect = g_strconcat (SECT_CONNECTION_, old_name, (char *) NULL);
        mc_config_del_group (ini, sect);
        g_free (sect);
    }

    sect = g_strconcat (SECT_CONNECTION_, c->name, (char *) NULL);
    mc_config_set_string (ini, sect, "connect_by", connect_by_names[c->connect_by]);
    for (gsize i = 0; i < G_N_ELEMENTS (connection_keys); i++)
        config_set_string (ini, sect, connection_keys[i].key,
                           *connection_field ((postgres_connection_t *) c, i));
    mc_config_set_bool (ini, sect, "show_all_databases", c->show_all_databases);
    mc_config_set_bool (ini, sect, "save_password", c->save_password);
    mc_config_set_bool (ini, sect, "read_only", c->read_only);
    g_free (sect);
    return config_save (ini, path, err_out);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
postgres_config_delete_connection (const char *name, char **err_out)
{
    mc_config_t *ini;
    char *path, *sect;

    ini = config_open_rw (&path, err_out);
    if (ini == NULL)
    {
        g_free (path);
        return FALSE;
    }

    sect = g_strconcat (SECT_CONNECTION_, name, (char *) NULL);
    mc_config_del_group (ini, sect);
    g_free (sect);
    return config_save (ini, path, err_out);
}

/* --------------------------------------------------------------------------------------------- */
