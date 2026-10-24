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

/** \file postgres_conn.c
 *  \brief Source: the connection to a server and the queries of the panel
 *
 *  The panel waits for the server, but not blindly: libpq is driven by PQconnectPoll and
 *  PQsendQueryParams, and the wait for its socket is cut in pieces of a tenth of a second, so that
 *  Ctrl-G gives up a connection and cancels a query. The connection reads the catalog only: it
 *  is opened with default_transaction_read_only, and statement_timeout and connect_timeout of
 *  [General] bound it.
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>  // realpath
#include <string.h>
#include <sys/socket.h>  // shutdown
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/tty/tty.h"  // tty_got_interrupt
#include "lib/widget.h"

#include "postgres_conn.h"
#include "postgres_internal.h"  // POSTGRES_HELP_NODE
#include "postgres_ui.h"

/*** file scope macro definitions ****************************************************************/

#define WAIT_SLICE_MS 100

/* how long the answer to a cancel is waited for before the connection is given up */
#define CANCEL_WAIT_S 5

/*** file scope type declarations ****************************************************************/

typedef enum
{
    WAIT_READY,
    WAIT_TIMEOUT,
    WAIT_INTERRUPTED,
    WAIT_ERROR,
} wait_result_t;

/*** file scope functions ************************************************************************/

static void
param_add (GPtrArray *keys, GPtrArray *values, const char *key, const char *value)
{
    if (value == NULL || *value == '\0')
        return;
    g_ptr_array_add (keys, (gpointer) key);
    g_ptr_array_add (values, (gpointer) value);
}

/* --------------------------------------------------------------------------------------------- */

/* Wait for @fd to take a write or to have something to read, until @deadline (monotonic
   microseconds, 0: none) or Ctrl-G */
static wait_result_t
wait_socket (int fd, gboolean for_write, gint64 deadline)
{
    struct pollfd p;

    if (fd < 0)
        return WAIT_ERROR;

    while (TRUE)
    {
        int timeout = WAIT_SLICE_MS;
        int r;

        if (tty_got_interrupt ())
            return WAIT_INTERRUPTED;
        if (deadline != 0)
        {
            gint64 left = deadline - g_get_monotonic_time ();

            if (left <= 0)
                return WAIT_TIMEOUT;
            timeout = (int) MIN ((gint64) WAIT_SLICE_MS, left / 1000 + 1);
        }

        p.fd = fd;
        p.events = for_write ? POLLOUT : POLLIN;
        p.revents = 0;
        r = poll (&p, 1, timeout);
        if (r > 0)
            return WAIT_READY;
        if (r < 0 && errno != EINTR)
            return WAIT_ERROR;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* PQcancel of libpq before 17 blocks until the server has read the request, with no limit: it
   runs beside, and the query waits for its answer CANCEL_WAIT_S at most */
static gpointer
cancel_thread (gpointer data)
{
    PGcancel *cancel = (PGcancel *) data;
    char buf[256];

    (void) PQcancel (cancel, buf, sizeof (buf));
    PQfreeCancel (cancel);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* The message of libpq without its trailing newline */
static char *
conn_error (PGconn *conn)
{
    char *msg;

    msg = g_strdup (conn != NULL ? PQerrorMessage (conn) : _ ("Out of memory"));
    g_strchomp (msg);
    if (*msg == '\0')
    {
        g_free (msg);
        msg = g_strdup (_ ("Unknown error"));
    }
    return msg;
}

/* --------------------------------------------------------------------------------------------- */

/* The message of a connection that failed, verbose, as the user reads it: without the SQLSTATE
   and the LOCATION of the server. @sqlstate, when not NULL, gets the SQLSTATE, or "" for none
   (a failure of libpq itself, before the server said anything). */
static char *
conn_failure (PGconn *conn, char sqlstate[6])
{
    static GRegex *code = NULL;
    char *msg, *text;
    GMatchInfo *m = NULL;
    GString *out;
    gchar **lines;

    if (sqlstate != NULL)
        sqlstate[0] = '\0';
    msg = conn_error (conn);

    if (code == NULL)
        code = g_regex_new ("(FATAL|ERROR|PANIC):  ([0-9A-Z]{5}): ", G_REGEX_OPTIMIZE, 0, NULL);
    if (g_regex_match (code, msg, 0, &m))
    {
        char *state = g_match_info_fetch (m, 2);

        if (sqlstate != NULL)
            g_strlcpy (sqlstate, state, 6);
        g_free (state);
    }
    g_match_info_free (m);

    // "FATAL:  28P01: text" is "FATAL:  text"
    text = g_regex_replace (code, msg, -1, 0, "\\1:  ", 0, NULL);
    g_free (msg);

    out = g_string_new (NULL);
    lines = g_strsplit (text != NULL ? text : "", "\n", -1);
    for (gchar **l = lines; *l != NULL; l++)
        if (!g_str_has_prefix (*l, "LOCATION:  "))
        {
            if (out->len != 0)
                g_string_append_c (out, '\n');
            g_string_append (out, *l);
        }
    g_strfreev (lines);
    g_free (text);
    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* One attempt to connect: the status window is the caller's */
static PGconn *
conn_try (const postgres_config_t *cfg, const postgres_connection_t *c, const char *database,
          char **err_out)
{
    GPtrArray *keys, *values;
    GString *options;
    PGconn *conn;
    gint64 deadline;
    gboolean for_write = TRUE;

    keys = g_ptr_array_new ();
    values = g_ptr_array_new ();
    // an empty search_path makes the server qualify every name it writes, as for pg_dump
    options = g_string_new ("-c default_transaction_read_only=on -c search_path=");
    if (cfg->statement_timeout > 0)
        g_string_append_printf (options, " -c statement_timeout=%ds", cfg->statement_timeout);

    switch (c->connect_by)
    {
    case POSTGRES_BY_URL:
        // the first dbname is expanded as a URL, the keywords after it win over the URL
        param_add (keys, values, "dbname", c->url != NULL ? c->url : "postgresql://");
        break;
    case POSTGRES_BY_SERVICE:
        param_add (keys, values, "service", c->service);
        param_add (keys, values, "user", c->user);
        param_add (keys, values, "dbname", c->database);
        break;
    default:
        param_add (keys, values, "host", c->host);
        param_add (keys, values, "port", c->port);
        param_add (keys, values, "user", c->user);
        param_add (keys, values, "dbname", c->database);
        param_add (keys, values, "sslmode", c->sslmode);
        break;
    }
    param_add (keys, values, "dbname", database);
    param_add (keys, values, "password", c->password);
    param_add (keys, values, "application_name", "mc-panel-postgres");
    param_add (keys, values, "client_encoding", "UTF8");
    param_add (keys, values, "options", options->str);
    g_ptr_array_add (keys, NULL);
    g_ptr_array_add (values, NULL);

    conn = PQconnectStartParams ((const char *const *) keys->pdata,
                                 (const char *const *) values->pdata,
                                 c->connect_by == POSTGRES_BY_URL ? 1 : 0);
    g_ptr_array_free (keys, TRUE);
    g_ptr_array_free (values, TRUE);
    g_string_free (options, TRUE);
    // the SQLSTATE of a refusal is in the message only when it is verbose: see conn_failure
    if (conn != NULL)
        PQsetErrorVerbosity (conn, PQERRORS_VERBOSE);

    deadline = cfg->connect_timeout > 0
        ? g_get_monotonic_time () + (gint64) cfg->connect_timeout * G_USEC_PER_SEC
        : 0;

    while (conn != NULL && PQstatus (conn) != CONNECTION_BAD)
    {
        PostgresPollingStatusType st;
        wait_result_t w;

        w = wait_socket (PQsocket (conn), for_write, deadline);
        if (w == WAIT_INTERRUPTED)
        {
            PQfinish (conn);
            *err_out = NULL;
            return NULL;
        }
        if (w == WAIT_TIMEOUT)
        {
            PQfinish (conn);
            *err_out =
                g_strdup_printf (_ ("No answer from the server in %d s."), cfg->connect_timeout);
            return NULL;
        }

        st = PQconnectPoll (conn);
        if (st == PGRES_POLLING_OK)
        {
            PQsetErrorVerbosity (conn, PQERRORS_DEFAULT);
            return conn;
        }
        if (st == PGRES_POLLING_FAILED)
            break;
        for_write = st == PGRES_POLLING_WRITING;
    }

    *err_out = conn_failure (conn, NULL);
    return conn;  // the caller asks it whether a password was wanted, and finishes it
}

/* --------------------------------------------------------------------------------------------- */

static char *
ask_password (const postgres_connection_t *c)
{
    char *password = NULL;
    char *label;
    int rc;

    label = g_strdup_printf (_ ("Password for \"%s\":"), c->name);
    {
        quick_widget_t quick_widgets[] = {
            QUICK_LABELED_INPUT (label, input_label_above, "", NULL, &password, NULL, TRUE, TRUE,
                                 INPUT_COMPLETE_NONE),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END,
        };
        WRect r = { -1, -1, 0, 50 };
        quick_dialog_t qdlg = {
            .rect = r,
            .title = N_ ("PostgreSQL"),
            .help = POSTGRES_HELP_NODE,
            .help_file = POSTGRES_HELP_FILE,
            .widgets = quick_widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        rc = quick_dialog (&qdlg);
    }
    g_free (label);

    if (rc != B_ENTER)
    {
        g_free (password);
        return NULL;
    }
    return password;
}

/* --------------------------------------------------------------------------------------------- */

/* @path made anew with @data, at mode 0600 from the start: libpq ignores a file of passwords
   the group or the others can read */
static gboolean
write_private (const char *path, const char *data, gsize len, char **err_out)
{
    char *tmp;
    int fd;
    gboolean ok = FALSE;

    tmp = g_strconcat (path, ".mc-XXXXXX", (char *) NULL);
    fd = g_mkstemp_full (tmp, O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd >= 0)
    {
        gsize done = 0;

        while (done < len)
        {
            ssize_t n = write (fd, data + done, len - done);

            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                break;
            done += (gsize) n;
        }
        ok = close (fd) == 0 && done == len && rename (tmp, path) == 0;
    }
    if (!ok)
    {
        *err_out = g_strdup_printf ("%s: %s", path, g_strerror (errno));
        if (fd >= 0)
            unlink (tmp);
    }
    g_free (tmp);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* A field of a line of ~/.pgpass: ':' and '\' escaped */
static void
pgpass_append_field (GString *line, const char *field)
{
    for (const char *p = field; *p != '\0'; p++)
    {
        if (*p == ':' || *p == '\\')
            g_string_append_c (line, '\\');
        g_string_append_c (line, *p);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The password of a connection that has worked to ~/.pgpass (or $PGPASSFILE), for its host,
   port and user and any database, in place of a line of the same ones */
static gboolean
pgpass_save (PGconn *conn, const char *password, char **err_out)
{
    const char *env, *host;
    char *path, *real, *contents = NULL;
    GString *key, *out;
    GError *gerr = NULL;
    gboolean ok;

    env = g_getenv ("PGPASSFILE");
    path = env != NULL && *env != '\0' ? g_strdup (env)
                                       : g_build_filename (g_get_home_dir (), ".pgpass", NULL);
    // a link stays a link: the file it points to is the one written anew
    real = realpath (path, NULL);
    if (real != NULL)
    {
        g_free (path);
        path = g_strdup (real);
        free (real);
    }

    // libpq calls the default socket directory localhost there
    host = PQhost (conn);
    if (host == NULL || *host == '\0' || *host == '/')
        host = "localhost";

    key = g_string_new (NULL);
    pgpass_append_field (key, host);
    g_string_append_c (key, ':');
    pgpass_append_field (key, PQport (conn));
    g_string_append (key, ":*:");
    pgpass_append_field (key, PQuser (conn));
    g_string_append_c (key, ':');

    // a file there that cannot be read is not written over: the other passwords are in it
    if (!g_file_get_contents (path, &contents, NULL, &gerr)
        && !g_error_matches (gerr, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
        *err_out = g_strdup (gerr->message);
        g_error_free (gerr);
        g_string_free (key, TRUE);
        g_free (path);
        return FALSE;
    }
    g_clear_error (&gerr);

    out = g_string_new (NULL);
    if (contents != NULL)
    {
        gchar **lines = g_strsplit (contents, "\n", -1);

        for (gchar **l = lines; *l != NULL; l++)
            if (**l != '\0' && !g_str_has_prefix (*l, key->str))
                g_string_append_printf (out, "%s\n", *l);
        g_strfreev (lines);
        memset (contents, 0, strlen (contents));
        g_free (contents);
    }
    g_string_append (out, key->str);
    pgpass_append_field (out, password);
    g_string_append_c (out, '\n');

    ok = write_private (path, out->str, out->len, err_out);

    memset (out->str, 0, out->len);
    g_string_free (out, TRUE);
    g_string_free (key, TRUE);
    g_free (path);
    return ok;
}

/*** public functions ****************************************************************************/

PGconn *
postgres_conn_open (mc_panel_host_t *host, const postgres_config_t *cfg, postgres_connection_t *c,
                    const char *database, char **err_out)
{
    postgres_status_t st;
    PGconn *conn = NULL;
    gboolean asked = FALSE;  // a password is asked for once
    char *text;

    *err_out = NULL;

    while (TRUE)
    {
        text = g_strdup_printf (_ ("Connecting to \"%s\"..."), c->name);
        postgres_status_open (&st, _ ("PostgreSQL"));
        postgres_status_set (&st, text);
        g_free (text);
        tty_enable_interrupt_key ();
        conn = conn_try (cfg, c, database, err_out);
        tty_disable_interrupt_key ();
        postgres_status_close (&st);

        if (conn == NULL || PQstatus (conn) == CONNECTION_OK)
            break;

        if (!asked && PQconnectionNeedsPassword (conn) && c->password == NULL)
        {
            char *password;

            PQfinish (conn);
            conn = NULL;
            password = ask_password (c);
            if (password == NULL)
            {
                g_clear_pointer (err_out, g_free);
                break;
            }
            g_clear_pointer (err_out, g_free);
            c->password = password;
            c->password_saved = FALSE;
            asked = TRUE;
            continue;
        }

        // a password the server refused is not kept; one that got past it to a database that is
        // not there, or not open to the user, is
        if (c->password != NULL && PQconnectionUsedPassword (conn))
        {
            char sqlstate[6];

            g_free (conn_failure (conn, sqlstate));
            if (strncmp (sqlstate, "28", 2) == 0)
            {
                memset (c->password, 0, strlen (c->password));
                g_clear_pointer (&c->password, g_free);
            }
        }
        PQfinish (conn);
        conn = NULL;
        break;
    }

    // the catalog the panel reads is that of 10: relispartition, attidentity, pg_sequence
    if (conn != NULL && PQserverVersion (conn) < 100000)
    {
        const char *version = PQparameterStatus (conn, "server_version");

        *err_out = g_strdup_printf (_ ("The server is PostgreSQL %s: the panel needs 10 or newer."),
                                    version != NULL ? version : "?");
        PQfinish (conn);
        return NULL;
    }

    // once a session: the line of ~/.pgpass is for any database of the server
    if (conn != NULL && c->save_password && c->password != NULL && !c->password_saved
        && PQconnectionUsedPassword (conn))
    {
        char *err = NULL;

        if (pgpass_save (conn, c->password, &err))
            c->password_saved = TRUE;
        else
            postgres_message (host, TRUE, _ ("Cannot save the password to ~/.pgpass: %s"), err);
        g_free (err);
    }
    return conn;
}

/* --------------------------------------------------------------------------------------------- */

PGresult *
postgres_conn_query (PGconn *conn, const char *sql, int nparams, const char *const *params,
                     char **err_out, gboolean *cancelled_out)
{
    PGresult *res, *last = NULL;
    char *err = NULL;
    gboolean cancelled = FALSE, abandoned = FALSE;
    gint64 deadline = 0;

    *err_out = NULL;
    if (cancelled_out != NULL)
        *cancelled_out = FALSE;

    if (PQsendQueryParams (conn, sql, nparams, NULL, params, NULL, NULL, 0) == 0)
    {
        *err_out = conn_error (conn);
        return NULL;
    }

    // the connection blocks: the query, a few hundred bytes, has gone out whole
    tty_enable_interrupt_key ();

    while (TRUE)
    {
        while (PQisBusy (conn))
        {
            wait_result_t w;

            w = wait_socket (PQsocket (conn), FALSE, deadline);
            if (w == WAIT_INTERRUPTED && !cancelled)
            {
                PGcancel *cancel = PQgetCancel (conn);

                // the server answers it with an error of the query, which is read as any other
                if (cancel != NULL)
                    g_thread_unref (g_thread_new ("postgres-cancel", cancel_thread, cancel));
                cancelled = TRUE;
                deadline = g_get_monotonic_time () + (gint64) CANCEL_WAIT_S * G_USEC_PER_SEC;
                continue;
            }
            if (w == WAIT_INTERRUPTED || w == WAIT_TIMEOUT)
            {
                // no answer to the cancel, or the user gave up waiting for it: the socket is shut,
                // libpq finds the connection broken
                (void) shutdown (PQsocket (conn), SHUT_RDWR);
                abandoned = TRUE;
            }
            else if (w == WAIT_ERROR)
            {
                // the socket cannot be waited for: shut too, or PQgetResult would wait on it in
                // libpq, with no limit
                (void) shutdown (PQsocket (conn), SHUT_RDWR);
            }
            if (PQconsumeInput (conn) == 0)
            {
                g_free (err);
                err = conn_error (conn);
                break;
            }
        }
        if (err != NULL && PQstatus (conn) == CONNECTION_BAD)
            break;

        res = PQgetResult (conn);
        if (res == NULL)
            break;

        switch (PQresultStatus (res))
        {
        case PGRES_TUPLES_OK:
        case PGRES_COMMAND_OK:
            if (last != NULL)
                PQclear (last);
            last = res;
            break;
        default:
            if (err == NULL)
            {
                err = g_strdup (PQresultErrorMessage (res));
                g_strchomp (err);
            }
            PQclear (res);
            break;
        }
    }

    tty_disable_interrupt_key ();

    if (err != NULL || cancelled)
    {
        if (last != NULL)
            PQclear (last);
        if (cancelled)
        {
            g_free (err);
            err = g_strdup (
                abandoned ? _ ("Cancelled. The server did not answer: the connection is closed.")
                          : _ ("Cancelled."));
        }
        if (cancelled_out != NULL)
            *cancelled_out = cancelled;
        *err_out = err;
        return NULL;
    }
    if (last == NULL)
        *err_out = g_strdup (_ ("The server returned nothing."));
    return last;
}

/* --------------------------------------------------------------------------------------------- */

char *
postgres_conn_url (const postgres_connection_t *c)
{
    GString *url;
    const char *sep = "?";
    gboolean socket_dir = c->host != NULL && *c->host == '/';
    char *s;

    url = g_string_new ("postgresql://");
    if (c->user != NULL)
    {
        s = g_uri_escape_string (c->user, NULL, FALSE);
        g_string_append_printf (url, "%s@", s);
        g_free (s);
    }
    // an address of IPv6 is in brackets; a directory of sockets is a parameter
    if (c->host != NULL && !socket_dir)
        g_string_append_printf (url, strchr (c->host, ':') != NULL ? "[%s]" : "%s", c->host);
    if (c->port != NULL)
        g_string_append_printf (url, ":%s", c->port);
    g_string_append_c (url, '/');
    if (c->database != NULL)
    {
        s = g_uri_escape_string (c->database, NULL, FALSE);
        g_string_append (url, s);
        g_free (s);
    }
    if (socket_dir)
    {
        s = g_uri_escape_string (c->host, NULL, FALSE);
        g_string_append_printf (url, "%shost=%s", sep, s);
        g_free (s);
        sep = "&";
    }
    if (c->sslmode != NULL)
        g_string_append_printf (url, "%ssslmode=%s", sep, c->sslmode);
    return g_string_free (url, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

char *
postgres_conn_describe (PGconn *conn)
{
    const char *version = PQparameterStatus (conn, "server_version");
    const char *host = PQhost (conn);

    return g_strdup_printf (_ ("PostgreSQL %s, database \"%s\" as \"%s\" on %s:%s"),
                            version != NULL ? version : "?", PQdb (conn), PQuser (conn),
                            host != NULL && *host != '\0' ? host : "localhost", PQport (conn));
}

/* --------------------------------------------------------------------------------------------- */
