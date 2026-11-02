/*
   Panel plugin sqlite for the M-Commander
   Rows of a table into a table of text, for F3

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

#include <sqlite3.h>

#include "lib/global.h"
#include "lib/text_table.h"

#include "sqlite_table.h"

/*** file scope functions ************************************************************************/

/* The value of @column in the row @stmt stands on into the last row of @table; NULL stays an
   empty cell */
static void
set_cell (mc_text_table_t *table, sqlite3_stmt *stmt, int column)
{
    switch (sqlite3_column_type (stmt, column))
    {
    case SQLITE_INTEGER:
    {
        char *number =
            g_strdup_printf ("%" G_GINT64_FORMAT, (gint64) sqlite3_column_int64 (stmt, column));

        mc_text_table_set_cell (table, column, number, -1, TRUE);
        g_free (number);
        break;
    }
    case SQLITE_FLOAT:
    {
        char number[G_ASCII_DTOSTR_BUF_SIZE];

        g_ascii_dtostr (number, sizeof (number), sqlite3_column_double (stmt, column));
        mc_text_table_set_cell (table, column, number, -1, TRUE);
        break;
    }
    case SQLITE_TEXT:
    {
        const char *text = (const char *) sqlite3_column_text (stmt, column);

        mc_text_table_set_cell (table, column, text != NULL ? text : "",
                                sqlite3_column_bytes (stmt, column), FALSE);
        break;
    }
    case SQLITE_BLOB:
    {
        char *size = g_strdup_printf ("<blob %d B>", sqlite3_column_bytes (stmt, column));

        mc_text_table_set_cell (table, column, size, -1, FALSE);
        g_free (size);
        break;
    }
    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mc_text_table_t *
sqlite_table_read (sqlite3_stmt *stmt, int max_rows, gboolean *more)
{
    int ncols = sqlite3_column_count (stmt);
    mc_text_table_t *table;
    int rc;

    *more = FALSE;
    table = mc_text_table_new ();
    for (int c = 0; c < ncols; c++)
    {
        const char *name = sqlite3_column_name (stmt, c);

        (void) mc_text_table_add_column (table, name != NULL ? name : "");
    }

    while ((rc = sqlite3_step (stmt)) == SQLITE_ROW)
    {
        if ((int) mc_text_table_rows (table) == max_rows)
        {
            *more = TRUE;
            break;
        }
        mc_text_table_add_row (table);
        for (int c = 0; c < ncols; c++)
            set_cell (table, stmt, c);
    }

    if (rc != SQLITE_ROW && rc != SQLITE_DONE)
    {
        mc_text_table_free (table);
        return NULL;
    }
    return table;
}

/* --------------------------------------------------------------------------------------------- */
