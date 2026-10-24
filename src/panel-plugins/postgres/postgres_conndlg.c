/*
   Panel plugin postgres for the M-Commander
   the dialog of a connection

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

/** \file postgres_conndlg.c
 *  \brief Source: the dialog of a connection
 *
 *  The server is named by its host and port, by a URL, or by a service of pg_service.conf: the
 *  fields the choice does not use are disabled. With the host the URL is what the fields make,
 *  and is read-only; switching from the URL back to the host fills the fields from it. The
 *  password is kept for the session; with "Save password" it goes to ~/.pgpass once it has
 *  connected. "Test connection" connects with what is in the fields.
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"
#include "lib/strutil.h"  // str_term_width1
#include "lib/widget.h"

#include "postgres_conn.h"
#include "postgres_internal.h"  // POSTGRES_HELP_NODE
#include "postgres_settings.h"
#include "postgres_ui.h"

/*** file scope macro definitions ****************************************************************/

#define B_TEST    B_USER
#define URL_WIDTH 56

/*** file scope type declarations ****************************************************************/

typedef struct
{
    mc_panel_host_t *host;
    const postgres_config_t *cfg;
    const char *name;  // of the connection being edited, for the password prompt

    WInput *conn_name;
    WRadio *by;
    WInput *url, *service, *host_in, *port, *database, *sslmode, *user, *password;
    WCheck *all_databases, *save_password;
    int last_by;
} conndlg_t;

/*** file scope variables ************************************************************************/

static conndlg_t dlg;

/*** file scope functions ************************************************************************/

/* The text of an input, stripped; NULL for an empty one */
static char *
input_value (const WInput *in)
{
    char *s;

    s = g_strstrip (g_strdup (input_get_ctext (in)));
    if (*s == '\0')
        g_clear_pointer (&s, g_free);
    return s;
}

/* --------------------------------------------------------------------------------------------- */

/* A password as it was typed, its spaces too; empty is NULL */
static char *
password_value (const WInput *in)
{
    const char *s = input_get_ctext (in);

    return *s != '\0' ? g_strdup (s) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* The server part of the fields into @c */
static void
conndlg_read_server (postgres_connection_t *c)
{
    c->connect_by = (postgres_connect_by_t) dlg.by->sel;
    g_free (c->url);
    c->url = input_value (dlg.url);
    g_free (c->service);
    c->service = input_value (dlg.service);
    g_free (c->host);
    c->host = input_value (dlg.host_in);
    g_free (c->port);
    c->port = input_value (dlg.port);
    g_free (c->database);
    c->database = input_value (dlg.database);
    g_free (c->sslmode);
    c->sslmode = input_value (dlg.sslmode);
    g_free (c->user);
    c->user = input_value (dlg.user);
    c->show_all_databases = dlg.all_databases->state;
}

/* --------------------------------------------------------------------------------------------- */

static void
conndlg_assign (WInput *in, const char *text)
{
    input_assign_text (in, text != NULL ? text : "");
}

/* --------------------------------------------------------------------------------------------- */

/* The fields of the host from the URL, when it can be read */
static void
conndlg_fields_from_url (void)
{
    PQconninfoOption *opts;
    char *url;

    url = input_value (dlg.url);
    opts = url != NULL ? PQconninfoParse (url, NULL) : NULL;
    g_free (url);
    if (opts == NULL)
        return;

    for (PQconninfoOption *o = opts; o->keyword != NULL; o++)
    {
        if (o->val == NULL)
            continue;
        if (strcmp (o->keyword, "host") == 0)
            conndlg_assign (dlg.host_in, o->val);
        else if (strcmp (o->keyword, "port") == 0)
            conndlg_assign (dlg.port, o->val);
        else if (strcmp (o->keyword, "dbname") == 0)
            conndlg_assign (dlg.database, o->val);
        else if (strcmp (o->keyword, "user") == 0)
            conndlg_assign (dlg.user, o->val);
        else if (strcmp (o->keyword, "sslmode") == 0)
            conndlg_assign (dlg.sslmode, o->val);
    }
    PQconninfoFree (opts);
}

/* --------------------------------------------------------------------------------------------- */

/* The fields the choice of the server uses enabled, the URL of the host's fields */
static void
conndlg_update (void)
{
    int by = dlg.by->sel;

    if (dlg.last_by == POSTGRES_BY_URL && by == POSTGRES_BY_HOST)
        conndlg_fields_from_url ();
    dlg.last_by = by;

    if (by == POSTGRES_BY_HOST)
    {
        postgres_connection_t c;
        char *url;

        memset (&c, 0, sizeof (c));
        conndlg_read_server (&c);
        url = postgres_conn_url (&c);
        input_assign_text (dlg.url, url);
        g_free (url);
        postgres_connection_clear (&c);
    }

    widget_disable (WIDGET (dlg.url), by != POSTGRES_BY_URL);
    widget_disable (WIDGET (dlg.service), by != POSTGRES_BY_SERVICE);
    widget_disable (WIDGET (dlg.host_in), by != POSTGRES_BY_HOST);
    widget_disable (WIDGET (dlg.port), by != POSTGRES_BY_HOST);
    widget_disable (WIDGET (dlg.sslmode), by != POSTGRES_BY_HOST);
    widget_disable (WIDGET (dlg.database), by == POSTGRES_BY_URL);
    widget_disable (WIDGET (dlg.user), by == POSTGRES_BY_URL);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
conndlg_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_INIT:
        conndlg_update ();
        return MSG_HANDLED;

    case MSG_NOTIFY:
        if (sender == WIDGET (dlg.by))
        {
            conndlg_update ();
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case MSG_VALIDATE:
        if (DIALOG (w)->ret_value == B_ENTER)
        {
            char *name = input_value (dlg.conn_name);

            if (!postgres_connection_name_valid (name))
            {
                // the dialog stays
                widget_set_state (w, WST_ACTIVE, TRUE);
                message (D_ERROR, _ ("PostgreSQL connection"), "%s",
                         _ ("The name of a connection must not be empty nor contain '[' or ']'."));
                widget_select (WIDGET (dlg.conn_name));
            }
            g_free (name);
        }
        return MSG_HANDLED;

    case MSG_POST_KEY:
        if (dlg.by->sel == POSTGRES_BY_HOST)
            conndlg_update ();
        return MSG_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* "Test connection": a connection of the fields, closed at once */
static int
conndlg_test (WButton *button, int action)
{
    postgres_connection_t c;
    PGconn *conn;
    char *err = NULL;

    (void) button;
    (void) action;

    memset (&c, 0, sizeof (c));
    c.name = (char *) dlg.name;
    conndlg_read_server (&c);
    c.password = password_value (dlg.password);

    conn = postgres_conn_open (dlg.host, dlg.cfg, &c, NULL, &err);
    if (conn != NULL)
    {
        char *text = postgres_conn_describe (conn);

        postgres_message (dlg.host, FALSE, _ ("Connected: %s"), text);
        g_free (text);
        PQfinish (conn);
        // a password asked for on the way is the one of the field
        if (c.password != NULL)
            input_assign_text (dlg.password, c.password);
    }
    else if (err != NULL)
        postgres_message (dlg.host, TRUE, "%s", err);

    g_free (err);
    c.name = NULL;
    postgres_connection_clear (&c);
    return 0;  // the dialog stays
}

/* --------------------------------------------------------------------------------------------- */

static WInput *
conndlg_input (WGroup *g, int y, int x, int width, const char *text, const char *histname)
{
    WInput *in;

    in = input_new (y, x, input_colors, width, text != NULL ? text : "", histname,
                    INPUT_COMPLETE_NONE);
    group_add_widget (g, in);
    return in;
}

/* --------------------------------------------------------------------------------------------- */

static void
conndlg_label (WGroup *g, int y, int x, const char *text)
{
    group_add_widget (g, label_new (y, x, text));
}

/*** public functions ****************************************************************************/

gboolean
postgres_connection_dialog_run (mc_panel_host_t *host, const postgres_config_t *cfg,
                                postgres_connection_t *c, gboolean is_new)
{
    const char *by_items[] = { N_ ("&Host"), N_ ("&URL"), N_ ("&Service") };
    const char *labels[] = {
        N_ ("Name:"),     N_ ("Connect by:"), N_ ("URL:"),      N_ ("Service:"),  N_ ("Host:"),
        N_ ("Database:"), N_ ("SSL mode:"),   N_ ("Username:"), N_ ("Password:"),
    };
    const char *port_label = _ ("Port:");
    const char *desc_label = _ ("Description:");
    const char *all_label = _ ("Show all d&atabases");
    const char *save_label = _ ("Sa&ve password");
    WDialog *d;
    WGroup *g;
    WInput *desc;
    WCheck *read_only;
    WButton *ok_button, *cancel_button;
    int lw = 0, x, cols, lines = 20, y, half;
    gboolean done;

    for (gsize i = 0; i < G_N_ELEMENTS (labels); i++)
    {
        labels[i] = _ (labels[i]);
        lw = MAX (lw, str_term_width1 (labels[i]));
    }
    for (gsize i = 0; i < G_N_ELEMENTS (by_items); i++)
        by_items[i] = _ (by_items[i]);

    x = 3 + lw + 1;  // where the fields start
    cols = x + URL_WIDTH + 3;
    half = (URL_WIDTH - 2) / 2;

    memset (&dlg, 0, sizeof (dlg));
    dlg.host = host;
    dlg.cfg = cfg;
    dlg.name = c->name != NULL ? c->name : "";
    dlg.last_by = c->connect_by;

    d = dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER | WPOS_TRYUP, FALSE, dialog_colors,
                    conndlg_callback, NULL, POSTGRES_HELP_NODE,
                    is_new ? _ ("New PostgreSQL connection") : _ ("Edit PostgreSQL connection"));
    d->help_file = POSTGRES_HELP_FILE;
    g = GROUP (d);

    y = 2;
    conndlg_label (g, y, 3, labels[0]);
    dlg.conn_name = conndlg_input (g, y, x, half, c->name, "postgres-conn-name");
    conndlg_label (g, y, x + half + 2, desc_label);
    desc = conndlg_input (g, y, x + half + 3 + str_term_width1 (desc_label),
                          URL_WIDTH - half - 3 - str_term_width1 (desc_label), c->description,
                          "postgres-conn-desc");

    group_add_widget (g, hline_new (++y, -1, -1));
    conndlg_label (g, ++y, 3, labels[1]);
    dlg.by = radio_new (y, x, G_N_ELEMENTS (by_items), by_items);
    dlg.by->sel = dlg.by->pos = (int) c->connect_by;
    group_add_widget (g, dlg.by);
    y += G_N_ELEMENTS (by_items);

    conndlg_label (g, y, 3, labels[2]);
    // no history: a URL can carry a password, and the history file is no place for it
    dlg.url = conndlg_input (g, y, x, URL_WIDTH, c->url, NULL);
    conndlg_label (g, ++y, 3, labels[3]);
    dlg.service = conndlg_input (g, y, x, half, c->service, "postgres-conn-service");

    conndlg_label (g, ++y, 3, labels[4]);
    dlg.host_in = conndlg_input (g, y, x, URL_WIDTH - 9 - str_term_width1 (port_label), c->host,
                                 "postgres-conn-host");
    conndlg_label (g, y, x + URL_WIDTH - 8 - str_term_width1 (port_label), port_label);
    dlg.port = conndlg_input (g, y, x + URL_WIDTH - 7, 7, c->port, "postgres-conn-port");

    conndlg_label (g, ++y, 3, labels[5]);
    dlg.database = conndlg_input (g, y, x, half, c->database, "postgres-conn-database");
    dlg.all_databases = check_new (y, x + half + 2, c->show_all_databases, all_label);
    group_add_widget (g, dlg.all_databases);

    conndlg_label (g, ++y, 3, labels[6]);
    dlg.sslmode = conndlg_input (g, y, x, half, c->sslmode, "postgres-conn-sslmode");

    {
        WHLine *hl = hline_new (++y, -1, -1);

        hline_set_text (hl, _ (" Authentication "));
        group_add_widget (g, hl);
    }
    conndlg_label (g, ++y, 3, labels[7]);
    dlg.user = conndlg_input (g, y, x, half, c->user, "postgres-conn-user");
    conndlg_label (g, ++y, 3, labels[8]);
    dlg.password = conndlg_input (g, y, x, half, c->password, NULL);
    dlg.password->is_password = TRUE;
    dlg.save_password = check_new (y, x + half + 2, c->save_password, save_label);
    group_add_widget (g, dlg.save_password);

    read_only = check_new (++y, 3, c->read_only,
                           _ ("&Read-only: the query editor opens read-only transactions"));
    group_add_widget (g, read_only);

    group_add_widget (g, hline_new (++y, -1, -1));
    y++;
    group_add_widget (
        g, button_new (y, 3, B_TEST, NORMAL_BUTTON, _ ("&Test connection"), conndlg_test));
    cancel_button = button_new (y, 0, B_CANCEL, NORMAL_BUTTON, _ ("&Cancel"), NULL);
    ok_button = button_new (y, 0, B_ENTER, DEFPUSH_BUTTON, _ ("&OK"), NULL);
    WIDGET (cancel_button)->rect.x = cols - 3 - button_get_width (cancel_button);
    WIDGET (ok_button)->rect.x = WIDGET (cancel_button)->rect.x - 1 - button_get_width (ok_button);
    group_add_widget (g, ok_button);
    group_add_widget (g, cancel_button);

    group_select_widget_by_id (g, WIDGET (dlg.conn_name)->id);

    done = dlg_run (d) == B_ENTER;
    if (done)
    {
        g_free (c->name);
        c->name = input_value (dlg.conn_name);
        g_free (c->description);
        c->description = input_value (desc);
        conndlg_read_server (c);
        if (c->password != NULL)
            memset (c->password, 0, strlen (c->password));
        g_free (c->password);
        c->password = password_value (dlg.password);
        c->password_saved = FALSE;
        c->save_password = dlg.save_password->state;
        c->read_only = read_only->state;
    }

    widget_destroy (WIDGET (d));
    memset (&dlg, 0, sizeof (dlg));
    return done;
}

/* --------------------------------------------------------------------------------------------- */
