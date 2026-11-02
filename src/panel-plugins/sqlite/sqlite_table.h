/** \file sqlite_table.h
 *  \brief Header: rows of the SQLite panel plugin into a table of text
 */

#ifndef MC__PANEL_PLUGIN_SQLITE_TABLE_H
#define MC__PANEL_PLUGIN_SQLITE_TABLE_H

#include <glib.h>
#include <sqlite3.h>

#include "lib/text_table.h"

/* Step @stmt for at most @max_rows rows into a table: a column for a column of the result, a
   number to the right, a NULL as an empty cell, a BLOB as its length. @more is set when there is
   a row past @max_rows. NULL when a step fails. Caller mc_text_table_free. */
mc_text_table_t *sqlite_table_read (sqlite3_stmt *stmt, int max_rows, gboolean *more);

#endif /* MC__PANEL_PLUGIN_SQLITE_TABLE_H */
