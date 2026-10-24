/*
   Panel plugin postgres for the M-Commander
   the dialogs of a connection and of the general options

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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/widget.h"

#include "postgres_internal.h"  // POSTGRES_HELP_NODE
#include "postgres_settings.h"

/*** file scope functions ************************************************************************/

/* @text as a number of seconds, 0 or more */
static gboolean
parse_seconds (const char *text, int *out)
{
    char *end = NULL;
    long v;

    if (text == NULL)
        return FALSE;
    v = strtol (text, &end, 10);
    if (end == text || *end != '\0' || v < 0 || v > G_MAXINT / 1000)
        return FALSE;
    *out = (int) v;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The text of an input, stripped, in place of @field; empty is NULL. Takes @text. */
static void
take_field (char **field, char *text)
{
    g_free (*field);
    if (text != NULL)
        g_strstrip (text);
    if (text != NULL && *text == '\0')
        g_clear_pointer (&text, g_free);
    *field = text;
}

/*** public functions ****************************************************************************/

gboolean
postgres_settings_dialog_run (postgres_config_t *cfg)
{
    char *r_default = NULL, *r_connect = NULL, *r_statement = NULL, *r_coole = NULL;
    char *r_psql = NULL;
    char buf_connect[16], buf_statement[16];
    gboolean show_system = cfg->show_system_schemas;
    int connect_timeout, statement_timeout;
    gboolean ok;
    int rc;

    g_snprintf (buf_connect, sizeof (buf_connect), "%d", cfg->connect_timeout);
    g_snprintf (buf_statement, sizeof (buf_statement), "%d", cfg->statement_timeout);

    {
        quick_widget_t quick_widgets[] = {
            // clang-format off
            QUICK_LABELED_INPUT (N_ ("Default connection:"), input_label_left,
                                 cfg->default_connection != NULL ? cfg->default_connection : "",
                                 "postgres-default-conn", &r_default, NULL, FALSE, FALSE,
                                 INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (N_ ("Connect timeout (s, 0: none):"), input_label_left,
                                 buf_connect, "postgres-connect-timeout", &r_connect, NULL, FALSE,
                                 FALSE, INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (N_ ("Statement timeout (s, 0: none):"), input_label_left,
                                 buf_statement, "postgres-statement-timeout", &r_statement, NULL,
                                 FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_CHECKBOX (N_ ("Show &system schemas"), &show_system, NULL),
            QUICK_SEPARATOR (TRUE),
            QUICK_LABELED_INPUT (N_ ("coole, the query editor (empty: from PATH):"),
                                 input_label_above, cfg->coole_path != NULL ? cfg->coole_path : "",
                                 "postgres-coole", &r_coole, NULL, FALSE, FALSE,
                                 INPUT_COMPLETE_FILENAMES),
            QUICK_LABELED_INPUT (N_ ("psql, without coole (empty: from PATH):"),
                                 input_label_above, cfg->psql_path != NULL ? cfg->psql_path : "",
                                 "postgres-psql", &r_psql, NULL, FALSE, FALSE,
                                 INPUT_COMPLETE_FILENAMES),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END,
            // clang-format on
        };
        WRect r = { -1, -1, 0, 60 };
        quick_dialog_t qdlg = {
            .rect = r,
            .title = N_ ("PostgreSQL settings"),
            .help = POSTGRES_HELP_NODE,
            .help_file = POSTGRES_HELP_FILE,
            .widgets = quick_widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        rc = quick_dialog (&qdlg);
    }

    ok = rc == B_ENTER;
    if (ok
        && !(parse_seconds (r_connect, &connect_timeout)
             && parse_seconds (r_statement, &statement_timeout)))
    {
        message (D_ERROR, _ ("PostgreSQL settings"), "%s",
                 _ ("A timeout is a whole number of seconds, 0 for none."));
        ok = FALSE;
    }

    if (ok)
    {
        char *err = NULL;

        take_field (&cfg->default_connection, r_default);
        take_field (&cfg->coole_path, r_coole);
        take_field (&cfg->psql_path, r_psql);
        r_default = r_coole = r_psql = NULL;
        cfg->connect_timeout = connect_timeout;
        cfg->statement_timeout = statement_timeout;
        cfg->show_system_schemas = show_system;

        ok = postgres_config_save_general (cfg, &err);
        if (!ok)
            message (D_ERROR, _ ("PostgreSQL settings"), "%s", err);
        g_free (err);
    }

    g_free (r_default);
    g_free (r_connect);
    g_free (r_statement);
    g_free (r_coole);
    g_free (r_psql);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */
