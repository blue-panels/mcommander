#ifndef MC__DIFFVIEW_INTERNAL_H
#define MC__DIFFVIEW_INTERNAL_H

#include "lib/global.h"

#include "src/syntax/syntax.h"
#include "lib/mcconfig.h"
#include "lib/search.h"
#include "lib/tty/color.h"
#include "lib/widget.h"

/*** typedefs(not structures) and defined constants **********************************************/

typedef int (*DFUNC) (void *ctx, int ch, int line, off_t off, size_t sz, const char *str);
typedef int PAIR[2];

#define error_dialog(h, s) query_dialog (h, s, D_ERROR, 1, _ ("&Dismiss"))

/*** enums ***************************************************************************************/

typedef enum
{
    DATA_SRC_MEM = 0,
    DATA_SRC_TMP = 1,
    DATA_SRC_ORG = 2
} DSRC;

typedef enum
{
    DIFF_LEFT = 0,
    DIFF_RIGHT = 1,
    DIFF_COUNT = 2
} diff_place_t;

typedef enum
{
    DIFF_NONE = 0,
    DIFF_ADD = 1,
    DIFF_DEL = 2,
    DIFF_CHG = 3
} DiffState;

/*** structures declarations (and typedefs of structures)*****************************************/

/* How the files are compared; kept in the [DiffView] section. */
typedef struct
{
    int quality;
    gboolean strip_trailing_cr;
    gboolean ignore_tab_expansion;
    gboolean ignore_space_change;
    gboolean ignore_all_space;
    gboolean ignore_case;
} diff_options_t;

typedef struct
{
    int fd;
    int pos;
    int len;
    char *buf;
    int flags;
    void *data;
} FBUF;

typedef struct
{
    int a[2][2];
    int cmd;
} DIFFCMD;

typedef struct
{
    int off;
    int len;
} BRACKET[DIFF_COUNT];

typedef struct
{
    int ch;
    int line;
    union
    {
        off_t off;
        size_t len;
    } u;
    void *p;
    /* slice of WDiff::syntax_runs[ord] covering this line; count 0 means no colors */
    guint32 run_first;
    guint32 run_count;
} DIFFLN;

typedef struct
{
    FBUF *f;
    GArray *a;
    DSRC dsrc;
    /* byte offset in the original file of each line appended to `a`, in step with
       it; the union in DIFFLN cannot keep it, and it is only needed while the
       syntax runs are being built */
    GArray *line_off;
} PRINTER_CTX;

/* A run of bytes sharing one color of the rule set; pass the color to
   dview_syntax_color() to get something to draw with. */
typedef struct dview_syntax_t dview_syntax_t;

typedef struct WDiff
{
    Widget widget;

    const char *file[DIFF_COUNT];  // filenames
    char *label[DIFF_COUNT];
    FBUF *f[DIFF_COUNT];
    const char *backup_sufix;
    gboolean merged[DIFF_COUNT];
    GArray *a[DIFF_COUNT];
    GPtrArray *hdiff;
    gboolean syntax;                         // color the text by syntax instead of by diff state
    GArray *syntax_runs[DIFF_COUNT];         // syntax_run_t, sliced per line by DIFFLN
    dview_syntax_t *syntax_src[DIFF_COUNT];  // kept for its palette while drawing
    int ndiff;                               // number of hunks
    DSRC dsrc;                               // data source: memory or temporary file

    gboolean view_quit;  // Quit flag

    int height;
    int half1;
    int half2;
    int width1;
    int width2;
    int bias;
    gboolean new_frame;
    int skip_rows;
    int skip_cols;
    gboolean display_symbols;
    int display_numbers;
    gboolean show_cr;
    int tab_size;
    diff_place_t ord;
    gboolean full;

    gboolean utf8;
    // converter for translation of text
    GIConv converter;

    diff_options_t opt;
    gboolean opt_changed;  // the user changed the options in this view

    // Search variables
    struct
    {
        mc_search_t *handle;
        gchar *last_string;

        ssize_t last_found_line;
        ssize_t last_accessed_num_line;
    } search;
} WDiff;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* compare.c */
int dview_compare_files (const diff_options_t *opt, const char *file1, const char *file2,
                         GArray *ops);

/* search.c */
void dview_search_cmd (WDiff *dview);
void dview_continue_search_cmd (WDiff *dview);

/* syntax.c */
dview_syntax_t *dview_syntax_open (const char *filename);
void dview_syntax_release_source (dview_syntax_t *ds);
void dview_syntax_close (dview_syntax_t *ds);
off_t dview_syntax_size (const dview_syntax_t *ds);
void dview_syntax_runs (dview_syntax_t *ds, off_t from, off_t to, GArray *runs);
int dview_syntax_color (const dview_syntax_t *ds, guint32 run_color);

#endif
