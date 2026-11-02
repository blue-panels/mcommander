/** \file text_table.h
 *  \brief Header: a table of text as psql prints it
 *
 *  For F3 of the panel plugins that show rows: a column for a column, the numbers to the right,
 *  as wide as the screen. The texts come in UTF-8, the table is written in the codeset of the
 *  terminal, so the viewer shows it as it is.
 */

#ifndef MC__TEXT_TABLE_H
#define MC__TEXT_TABLE_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

typedef struct mc_text_table_t mc_text_table_t;

/*** declarations of public functions ************************************************************/

mc_text_table_t *mc_text_table_new (void);
void mc_text_table_free (mc_text_table_t *table);

/* A column more, at the right; returns its number. @title is UTF-8. */
int mc_text_table_add_column (mc_text_table_t *table, const char *title);

/* A row more, at the bottom, with all its cells empty */
void mc_text_table_add_row (mc_text_table_t *table);
guint mc_text_table_rows (const mc_text_table_t *table);

/* The cell of @column in the last row: @length bytes of UTF-8 (-1: up to the NUL), written on
   one line and cut with "~" at 256 bytes. A column is put to the right when every cell set in it
   is @numeric. */
void mc_text_table_set_cell (mc_text_table_t *table, int column, const char *text, gssize length,
                             gboolean numeric);

/* The table, below @caption ("-- caption" and an empty line; NULL: none), above "(N rows)".
   When a line is wider than @screen_width (0: no limit), the widest columns are made narrower,
   down to 8, and their texts cut at the end with "~". Caller g_free. */
char *mc_text_table_render (const mc_text_table_t *table, const char *caption, int screen_width);

#endif /* MC__TEXT_TABLE_H */
