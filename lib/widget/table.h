/** \file table.h
 *  \brief Header: WTable widget
 */

#ifndef MC__WIDGET_TABLE_H
#define MC__WIDGET_TABLE_H

#include "lib/global.h" /* GLib types */

/* forward declarations needed by widget-common.h */
struct Widget;
typedef struct Widget Widget;
struct WGroup;
typedef struct WGroup WGroup;

#include "lib/widget/rect.h"          /* WRect */
#include "lib/widget/widget-common.h" /* Widget */
#include "lib/widget/hline.h"         /* WHLine */
#include "lib/widget/scrollbar.h"     /* WScrollBar */
#include "lib/strutil.h"              /* align_crt_t */

/*** typedefs(not structures) and defined constants **********************************************/

#define TABLE(x) ((WTable *) (x))

/*** enums ***************************************************************************************/

typedef enum
{
    TABLE_COL_TEXT = 0,
    TABLE_COL_CHECK,
    TABLE_COL_CHOICE /* text cell whose value is cycled in place */
} table_col_type_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    int width;             /* column width in terminal cells */
    align_crt_t align;     /* J_LEFT, J_RIGHT, J_CENTER, J_LEFT_FIT */
    table_col_type_t type; /* TABLE_COL_TEXT, _CHECK or _CHOICE (default 0 = TEXT) */
} table_column_def_t;

typedef struct
{
    int (*get_nrows) (const void *data);
    const char *(*get_text) (const void *data, int row, int col);
    gboolean (*get_checked) (const void *data, int row, int col);
    void (*set_checked) (void *data, int row, int col, gboolean val);
    void *data;
    /* step a TABLE_COL_CHOICE cell, dir +1 or -1; last, so older datasources still fit */
    void (*cycle_choice) (void *data, int row, int col, int dir);
} table_datasource_t;

typedef struct
{
    Widget widget;

    int ncols;                    /* number of columns */
    table_column_def_t *col_defs; /* column definitions array (owned) */

    table_datasource_t datasource; /* external data provider */

    int top;                     /* first visible row index */
    int current;                 /* current (selected) row index */
    int current_col;             /* current column, only meaningful with choice columns */
    int cursor_y;                /* cached cursor row for MSG_CURSOR */
    gboolean scrollbar;          /* draw scrollbar when rows > visible lines */
    gboolean scrollbar_on_frame; /* last column lies on the frame, so always paint it */
    int color_idx;               /* override normal color: DLG_COLOR_* index, or -1 for default */
    int normal_color;            /* absolute skin color for rows, -1 = dialog colors */
    int selected_color;          /* absolute skin color for the current row, -1 = dialog colors */
    int scrollbar_color;         /* absolute skin color for the scrollbar, -1 = dialog frame */
    gboolean has_check_cols;     /* TRUE when at least one col has TABLE_COL_CHECK */
    gboolean has_choice_cols;    /* a choice column adds the column cursor */

    /* Header, column sizing and column scrolling: off until set after table_new(),
       so the callers that fill col_defs on the stack keep their fixed layout. */
    char **titles;           /* header row, one text per column; NULL: no header row */
    int *min_widths;         /* per column: floor of an auto-sized column, 0: fixed width */
    gboolean *expands;       /* per column: takes a share of the spare width */
    int *widths;             /* effective widths from table_layout(); NULL: col_defs widths */
    gboolean scroll_columns; /* Left/Right scroll the columns when they do not fit */
    int first_col;           /* first visible column */
    void (*prefetch) (void *data, int first, int count); /* before the rows are drawn */
    int (*cell_color) (void *data, int row, int col);    /* a color, or -1 for the row's */
    gboolean check_cursor; /* the column cursor walks the check columns as well */

    /* Quick search and quick filter over the text of one column, as in the panels: off until
       table_set_quick_search(). */
    gboolean qs_enabled;
    gboolean qs_letters; /* a letter starts the search too */
    gboolean qs_case_sensitive;
    int qs_col; /* the column searched */
    gboolean qs_active;
    gboolean qs_filtering; /* the rows that do not match are hidden */
    GString *qs_text;
    char qs_ch[MB_LEN_MAX]; /* the bytes of a character not whole yet */
    int qs_chpoint;
    GArray *qs_rows; /* int: the data rows shown while filtering, else NULL */
    /* the text a row is searched by, when it is not the text shown in qs_col */
    const char *(*qs_text_of) (void *data, int row);
    gboolean qs_fuzzy;          /* the filter is fuzzy and ranks the rows by how well they match */
    gboolean qs_letters_filter; /* a letter starts the filter, not the search */
    int (*qs_group_of) (void *data, int row); /* rows rank within groups, lower groups first */
    WHLine *qs_label; /* shows the text of a quick search instead of the last line */
    /* last: the fields above keep their places for the plugins built before it */
    WScrollBar *bar; /* the scrollbar, the table's own */
} WTable;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WTable *table_new (int y, int x, int height, int width, int ncols,
                   const table_column_def_t *col_defs);
void table_set_datasource (WTable *t, table_datasource_t ds);
int table_get_current (const WTable *t);
void table_set_current (WTable *t, int pos);

/* A header row with these titles (copied; NULL clears it). */
void table_set_header (WTable *t, const char *const *titles);
/* Column col is sized from its content: at least min_width, and when expands it
   shares the width left over.  A col_defs width of 0 means "sized here". */
void table_set_column_sizing (WTable *t, int col, int min_width, gboolean expands);
void table_set_scroll_columns (WTable *t, gboolean enable);
void table_set_prefetch (WTable *t, void (*prefetch) (void *data, int first, int count));
void table_set_cell_color (WTable *t, int (*cell_color) (void *data, int row, int col));
void table_set_check_cursor (WTable *t, gboolean on);
/* Ctrl-S searches the text of column col, Alt-Shift-S filters the rows by it; with letters a
   letter starts the search too.  The rows given to and taken from the table stay the rows of
   the data source while a filter hides some of them. */
void table_set_quick_search (WTable *t, int col, gboolean letters, gboolean case_sensitive);
/* End a quick search or filter, as when the rows of the data source change */
void table_quick_search_stop (WTable *t);
/* A fuzzy filter: the letters typed are in the text in their order, anywhere, and the rows
   rank by how well they match, inside the groups group_of gives (NULL: one group); a row of
   a group below 0 stays whatever is typed; with letters_filter a letter starts the filter */
void table_set_quick_filter_fuzzy (WTable *t, gboolean letters_filter,
                                   int (*group_of) (void *data, int row));
/* Show the text of a quick search on @label, a line of the dialog, instead of the last line
   of the table */
void table_set_quick_search_label (WTable *t, WHLine *label);
/* Search the rows by this text instead of the text shown, a name without its decoration */
void table_set_quick_search_text (WTable *t, const char *(*text_of) (void *data, int row));
/* Recompute the effective widths for the current size; done on resize too. */
void table_layout (WTable *t);
int table_get_first_column (const WTable *t);
void table_scroll_columns (WTable *t, int delta);
/* The lines that hold rows: all of them, or all but the header. */
int table_data_lines (const WTable *t);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_TABLE_H */
