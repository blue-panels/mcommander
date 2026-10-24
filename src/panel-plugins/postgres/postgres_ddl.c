/*
   Panel plugin postgres for the M-Commander
   the SQL of a relation, rebuilt from the catalog

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

/** \file postgres_ddl.c
 *  \brief Source: the SQL of a relation, rebuilt from the catalog
 *
 *  The server has no function for the CREATE TABLE of a table: it is put together from the
 *  columns, the constraints, the indexes, the triggers, the partitions and the comments, each
 *  part written by the server itself (format_type, pg_get_expr, pg_get_constraintdef,
 *  pg_get_indexdef...). A view is pg_get_viewdef. It is what the table is, not a dump: the
 *  owner, the grants, the policies and the statistics are left out.
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"

#include "postgres_internal.h"

/*** file scope macro definitions ****************************************************************/

#define SQL_RELATION                                                                               \
    "SELECT c.oid, c.relkind, pg_catalog.quote_ident (n.nspname) || '.' || "                       \
    "pg_catalog.quote_ident (c.relname), pg_catalog.obj_description (c.oid, 'pg_class') "          \
    "FROM pg_catalog.pg_class c JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "         \
    "WHERE n.nspname = $1 AND c.relname = $2"

/* %s: attgenerated, or '' before 12 */
#define SQL_COLUMNS                                                                                \
    "SELECT pg_catalog.quote_ident (a.attname), pg_catalog.format_type (a.atttypid, "              \
    "a.atttypmod), "                                                                               \
    "a.attnotnull, pg_catalog.pg_get_expr (d.adbin, d.adrelid), a.attidentity, %s, "               \
    "CASE WHEN a.attcollation <> t.typcollation THEN (SELECT pg_catalog.quote_ident (l.collname) " \
    "FROM pg_catalog.pg_collation l WHERE l.oid = a.attcollation) END, "                           \
    "pg_catalog.col_description (a.attrelid, a.attnum) "                                           \
    "FROM pg_catalog.pg_attribute a JOIN pg_catalog.pg_type t ON t.oid = a.atttypid "              \
    "LEFT JOIN pg_catalog.pg_attrdef d ON d.adrelid = a.attrelid AND d.adnum = a.attnum "          \
    "WHERE a.attrelid = $1 AND a.attnum > 0 AND NOT a.attisdropped ORDER BY a.attnum"

/* Not the NOT NULL of 18: it is written with its column */
#define SQL_CONSTRAINTS                                                                            \
    "SELECT pg_catalog.quote_ident (conname), pg_catalog.pg_get_constraintdef (oid, true) "        \
    "FROM pg_catalog.pg_constraint WHERE conrelid = $1 AND contype <> 'n' "                        \
    "ORDER BY contype <> 'p', contype <> 'u', conname"

/* Not the indexes of the constraints: they are made by them */
#define SQL_INDEXES                                                                                \
    "SELECT pg_catalog.pg_get_indexdef (i.indexrelid) FROM pg_catalog.pg_index i "                 \
    "WHERE i.indrelid = $1 AND NOT EXISTS (SELECT 1 FROM pg_catalog.pg_constraint c "              \
    "WHERE c.conindid = i.indexrelid AND c.contype IN ('p', 'u', 'x')) ORDER BY 1"

#define SQL_TRIGGERS                                                                               \
    "SELECT pg_catalog.pg_get_triggerdef (oid) FROM pg_catalog.pg_trigger "                        \
    "WHERE tgrelid = $1 AND NOT tgisinternal ORDER BY tgname"

#define SQL_PARTITIONS                                                                             \
    "SELECT c.oid::pg_catalog.regclass::text, pg_catalog.pg_get_expr (c.relpartbound, c.oid) "     \
    "FROM pg_catalog.pg_inherits i JOIN pg_catalog.pg_class c ON c.oid = i.inhrelid "              \
    "WHERE i.inhparent = $1 ORDER BY 1"

#define SQL_FOREIGN                                                                                \
    "SELECT pg_catalog.quote_ident (s.srvname), pg_catalog.array_to_string (ARRAY ("               \
    "SELECT pg_catalog.quote_ident (option_name) || ' ' || pg_catalog.quote_literal "              \
    "(option_value) "                                                                              \
    "FROM pg_catalog.pg_options_to_table (ft.ftoptions)), ', ') "                                  \
    "FROM pg_catalog.pg_foreign_table ft JOIN pg_catalog.pg_foreign_server s "                     \
    "ON s.oid = ft.ftserver WHERE ft.ftrelid = $1"

#define SQL_SEQUENCE                                                                               \
    "SELECT pg_catalog.format_type (seqtypid, NULL), seqincrement, seqmin, seqmax, seqstart, "     \
    "seqcache, seqcycle FROM pg_catalog.pg_sequence WHERE seqrelid = $1"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    postgres_data_t *data;
    const char *oid;
    const char *qname;  // schema.name, quoted where it has to be
    GString *out;
} ddl_t;

/*** file scope functions ************************************************************************/

static const char *
field (const PGresult *res, int row, int col)
{
    return PQgetisnull (res, row, col) ? NULL : PQgetvalue (res, row, col);
}

/* --------------------------------------------------------------------------------------------- */

static PGresult *
ddl_query (ddl_t *d, const char *sql)
{
    const char *params[1] = { d->oid };

    return postgres_query (d->data, sql, 1, params);
}

/* --------------------------------------------------------------------------------------------- */

/* A literal of SQL for COMMENT ON */
static void
append_literal (GString *out, const char *text)
{
    g_string_append_c (out, '\'');
    for (const char *p = text; *p != '\0'; p++)
    {
        if (*p == '\'')
            g_string_append_c (out, '\'');
        g_string_append_c (out, *p);
    }
    g_string_append_c (out, '\'');
}

/* --------------------------------------------------------------------------------------------- */

/* The columns and the constraints, in the parentheses of CREATE TABLE; the comments of the
   columns into @comments */
static gboolean
ddl_columns (ddl_t *d, GString *comments)
{
    PGresult *res;
    char *sql;
    int n;

    sql = g_strdup_printf (SQL_COLUMNS,
                           PQserverVersion (d->data->conn) >= 120000 ? "a.attgenerated" : "''");
    res = ddl_query (d, sql);
    g_free (sql);
    if (res == NULL)
        return FALSE;

    g_string_append (d->out, " (");
    n = PQntuples (res);
    for (int i = 0; i < n; i++)
    {
        const char *def = field (res, i, 3);
        const char *identity = PQgetvalue (res, i, 4);
        const char *generated = PQgetvalue (res, i, 5);
        const char *collation = field (res, i, 6);
        const char *comment = field (res, i, 7);

        g_string_append_printf (d->out, "%s\n    %s %s", i == 0 ? "" : ",", PQgetvalue (res, i, 0),
                                PQgetvalue (res, i, 1));
        if (collation != NULL)
            g_string_append_printf (d->out, " COLLATE %s", collation);
        if (*identity == 'a')
            g_string_append (d->out, " GENERATED ALWAYS AS IDENTITY");
        else if (*identity == 'd')
            g_string_append (d->out, " GENERATED BY DEFAULT AS IDENTITY");
        else if (*generated == 's' && def != NULL)
            g_string_append_printf (d->out, " GENERATED ALWAYS AS (%s) STORED", def);
        else if (*generated == 'v' && def != NULL)
            g_string_append_printf (d->out, " GENERATED ALWAYS AS (%s) VIRTUAL", def);
        else if (def != NULL)
            g_string_append_printf (d->out, " DEFAULT %s", def);
        if (*PQgetvalue (res, i, 2) == 't')
            g_string_append (d->out, " NOT NULL");

        if (comment != NULL)
        {
            g_string_append_printf (comments, "COMMENT ON COLUMN %s.%s IS ", d->qname,
                                    PQgetvalue (res, i, 0));
            append_literal (comments, comment);
            g_string_append (comments, ";\n");
        }
    }
    PQclear (res);

    res = ddl_query (d, SQL_CONSTRAINTS);
    if (res == NULL)
        return FALSE;
    for (int i = 0; i < PQntuples (res); i++)
        g_string_append_printf (d->out, "%s\n    CONSTRAINT %s %s", n + i == 0 ? "" : ",",
                                PQgetvalue (res, i, 0), PQgetvalue (res, i, 1));
    PQclear (res);

    g_string_append (d->out, "\n)");
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* A statement for each row of the query: its first column and ";" */
static gboolean
ddl_statements (ddl_t *d, const char *sql)
{
    PGresult *res;

    res = ddl_query (d, sql);
    if (res == NULL)
        return FALSE;
    if (PQntuples (res) > 0)
        g_string_append_c (d->out, '\n');
    for (int i = 0; i < PQntuples (res); i++)
        g_string_append_printf (d->out, "%s;\n", PQgetvalue (res, i, 0));
    PQclear (res);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ddl_table (ddl_t *d, char kind)
{
    GString *comments;
    PGresult *res;
    gboolean ok;

    comments = g_string_new (NULL);
    g_string_append_printf (d->out, "CREATE %sTABLE %s", kind == 'f' ? "FOREIGN " : "", d->qname);
    ok = ddl_columns (d, comments);

    if (ok && kind == 'p')
    {
        res = ddl_query (d, "SELECT pg_catalog.pg_get_partkeydef ($1)");
        ok = res != NULL;
        if (ok)
            g_string_append_printf (d->out, " PARTITION BY %s", PQgetvalue (res, 0, 0));
        PQclear (res);
    }

    if (ok && kind == 'f')
    {
        res = ddl_query (d, SQL_FOREIGN);
        ok = res != NULL;
        if (ok && PQntuples (res) > 0)
        {
            const char *options = PQgetvalue (res, 0, 1);

            g_string_append_printf (d->out, "\nSERVER %s", PQgetvalue (res, 0, 0));
            if (*options != '\0')
                g_string_append_printf (d->out, "\nOPTIONS (%s)", options);
        }
        PQclear (res);
    }
    g_string_append (d->out, ";\n");

    ok = ok && ddl_statements (d, SQL_INDEXES) && ddl_statements (d, SQL_TRIGGERS);

    if (ok && kind == 'p')
    {
        res = ddl_query (d, SQL_PARTITIONS);
        ok = res != NULL;
        if (ok && PQntuples (res) > 0)
            g_string_append (d->out, "\n-- partitions\n");
        for (int i = 0; ok && i < PQntuples (res); i++)
            g_string_append_printf (d->out, "CREATE TABLE %s PARTITION OF %s %s;\n",
                                    PQgetvalue (res, i, 0), d->qname, PQgetvalue (res, i, 1));
        PQclear (res);
    }

    if (ok && comments->len != 0)
        g_string_append_printf (d->out, "\n%s", comments->str);
    g_string_free (comments, TRUE);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ddl_view (ddl_t *d, char kind)
{
    PGresult *res;

    res = ddl_query (d, "SELECT pg_catalog.pg_get_viewdef ($1::pg_catalog.oid, true)");
    if (res == NULL)
        return FALSE;
    g_string_append_printf (d->out, "CREATE %sVIEW %s AS\n%s", kind == 'm' ? "MATERIALIZED " : "",
                            d->qname, g_strchomp (PQgetvalue (res, 0, 0)));
    PQclear (res);

    // pg_get_viewdef ends with the ";" of the query
    if (kind == 'm')
    {
        if (d->out->len != 0 && d->out->str[d->out->len - 1] == ';')
            g_string_truncate (d->out, d->out->len - 1);
        g_string_append (d->out, "\nWITH DATA;\n");
        return ddl_statements (d, SQL_INDEXES);
    }
    g_string_append_c (d->out, '\n');
    return ddl_statements (d, SQL_TRIGGERS);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ddl_sequence (ddl_t *d)
{
    PGresult *res;

    res = ddl_query (d, SQL_SEQUENCE);
    if (res == NULL)
        return FALSE;
    if (PQntuples (res) > 0)
        g_string_append_printf (d->out,
                                "CREATE SEQUENCE %s AS %s\n    INCREMENT BY %s\n    MINVALUE %s\n"
                                "    MAXVALUE %s\n    START WITH %s\n    CACHE %s%s;\n",
                                d->qname, PQgetvalue (res, 0, 0), PQgetvalue (res, 0, 1),
                                PQgetvalue (res, 0, 2), PQgetvalue (res, 0, 3),
                                PQgetvalue (res, 0, 4), PQgetvalue (res, 0, 5),
                                *PQgetvalue (res, 0, 6) == 't' ? "\n    CYCLE" : "");
    PQclear (res);
    return TRUE;
}

/*** public functions ****************************************************************************/

char *
postgres_ddl (postgres_data_t *data, const char *name)
{
    const char *params[2] = { data->schema, name };
    PGresult *rel;
    ddl_t d;
    const char *comment;
    char kind;
    gboolean ok;

    rel = postgres_query (data, SQL_RELATION, 2, params);
    if (rel == NULL)
        return NULL;
    if (PQntuples (rel) == 0)
    {
        PQclear (rel);
        return NULL;
    }

    d.data = data;
    d.oid = PQgetvalue (rel, 0, 0);
    d.qname = PQgetvalue (rel, 0, 2);
    d.out = g_string_new ("-- reconstructed from the catalog, not pg_dump\n\n");
    kind = PQgetvalue (rel, 0, 1)[0];

    switch (kind)
    {
    case 'v':
    case 'm':
        ok = ddl_view (&d, kind);
        break;
    case 'S':
        ok = ddl_sequence (&d);
        break;
    default:
        ok = ddl_table (&d, kind);
        break;
    }

    comment = field (rel, 0, 3);
    if (ok && comment != NULL)
    {
        g_string_append_printf (d.out, "\nCOMMENT ON %s %s IS ",
                                kind == 'v'       ? "VIEW"
                                    : kind == 'm' ? "MATERIALIZED VIEW"
                                    : kind == 'S' ? "SEQUENCE"
                                    : kind == 'f' ? "FOREIGN TABLE"
                                                  : "TABLE",
                                d.qname);
        append_literal (d.out, comment);
        g_string_append (d.out, ";\n");
    }
    PQclear (rel);

    if (!ok)
    {
        g_string_free (d.out, TRUE);
        return NULL;
    }
    return g_string_free (d.out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
