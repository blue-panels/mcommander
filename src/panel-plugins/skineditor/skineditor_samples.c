/*
   Skin editor plugin - the mock-ups of the sample pane, one per section.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* The texts are fixed and not translated: they stand for what the screen looks like, and the
   pane must look the same on every run. */

#include <config.h>

#include <string.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/tty/tty.h"
#include "lib/widget.h"

#include "skineditor_sample_priv.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define T(y, x, g, k, text)         sample_text (s, y, x, g, k, text)
#define H(y, x, g, k, hk, text)     sample_hot (s, y, x, g, k, hk, text)
#define F(y, x, l, c, g, k)         sample_fill (s, y, x, l, c, g, k)
#define B(y, x, l, c, g, k, single) sample_box (s, y, x, l, c, g, k, single)
#define L(y, x, c, g, k)            sample_hline (s, y, x, c, g, k)

/*** file scope type declarations ****************************************************************/

typedef struct
{
    const char *group;
    const char *first_key; /* NULL: any section of the group */
    skinsample_draw_fn draw;
} sample_map_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* @text padded with spaces to @width, so a row of a panel or a list is painted to its end */

static void
line (WSkinSample *s, int y, int x, int width, const char *group, const char *key, const char *text)
{
    char *padded;

    padded = g_strdup_printf ("%-*s", width, text);
    sample_text (s, y, x, group, key, padded);
    g_free (padded);
}

/* --------------------------------------------------------------------------------------------- */

/* the frame of a panel with its title and the column header */

static void
panel_frame (WSkinSample *s, const WRect *r, int lines)
{
    int w = r->cols;

    F (0, 0, lines, w, "core", "_default_");
    B (0, 0, lines, w, "core", "frame", TRUE);
    T (0, 2, "core", "reverse", " /home/user ");
    line (s, 1, 1, w - 2, "core", "header", " Name              Size   Modify time");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_panels (WSkinSample *s, const WRect *r)
{
    int w = r->cols - 2;
    int box = MIN (12, r->lines);

    F (0, 0, r->lines, r->cols, "core", "_default_");
    panel_frame (s, r, box);
    line (s, 2, 1, w, "core", "_default_", "/..                UP--DIR Sep  5 10:00");
    line (s, 3, 1, w, "filehighlight", "directory", "/bin                 4096 Sep  5 10:00");
    line (s, 4, 1, w, "core", "_default_", " notes.txt           1234 Sep  5 10:00");
    line (s, 5, 1, w, "core", "marked", "*marked.c            5678 Sep  5 10:00");
    line (s, 6, 1, w, "core", "selected", " cursor.sh            321 Sep  5 10:00");
    line (s, 7, 1, w, "core", "markselect", "*both.tar            9999 Sep  5 10:00");
    L (8, 1, w, "core", "frame");
    line (s, 9, 1, w, "core", "_default_", "notes.txt");
    T (9, 12, "core", "permnone", "-");
    T (9, 13, "core", "permread", "r");
    T (9, 14, "core", "permwrite", "w");
    T (9, 15, "core", "permexec", "x");
    T (9, 16, "core", "permread", "r");
    T (9, 17, "core", "permnone", "-");
    T (9, 18, "core", "permspecial", "s");
    T (9, 19, "core", "permread", "r");
    T (9, 20, "core", "permnone", "-");
    T (9, 21, "core", "permnone", "-");
    T (9, 24, "core", "_default_", "1234  Sep  5 10:00");

    T (12, 1, "core", "_default_", "[");
    F (12, 2, 1, 8, "core", "gauge");
    T (12, 10, "core", "_default_", "        ] 50%");
    T (13, 1, "core", "_default_", "$ ls ");
    T (13, 6, "core", "commandlinemark", "-la");
    T (13, 12, "core", "disabled", "disabled element");

    F (15, 1, 3, 14, "dialog", "_default_");
    B (15, 1, 3, 14, "dialog", "dframe", TRUE);
    T (16, 3, "dialog", "_default_", "shadow ->");
    F (16, 15, 3, 2, "core", "shadow");
    F (18, 3, 1, 14, "core", "shadow");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_filetypes (WSkinSample *s, const WRect *r)
{
    static const struct
    {
        const char *key;
        const char *text;
    } rows[] = {
        { "directory", "/directory          4096" },  { "executable", "*executable         2048" },
        { "symlink", "@symlink               12" },   { "hardlink", " hardlink            1024" },
        { "stalelink", "!stalelink              0" }, { "device", "+device              1, 3" },
        { "special", "=special                0" },   { "core", " core             1048576" },
        { "temp", " temp.tmp              16" },      { "archive", " archive.tar.gz    65536" },
        { "doc", " document.txt         512" },       { "source", " source.c            4096" },
        { "media", " media.mp3         409600" },     { "graph", " graph.png          81920" },
        { "database", " database.db       262144" },  { "_default_", " other.dat           128" },
    };
    int w = r->cols - 2;
    size_t i;

    F (0, 0, r->lines, r->cols, "core", "_default_");
    panel_frame (s, r, r->lines);
    for (i = 0; i < G_N_ELEMENTS (rows) && (int) i + 3 < r->lines; i++)
        line (s, (int) i + 2, 1, w, "filehighlight", rows[i].key, rows[i].text);
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_input (WSkinSample *s, const WRect *r)
{
    int w = MIN (26, r->cols - 14);

    F (0, 0, r->lines, r->cols, "dialog", "_default_");
    T (1, 1, "dialog", "_default_", "Path:");
    F (1, 8, 1, w, "core", "input");
    T (1, 9, "core", "input", "/home/user/file.txt");
    T (1, 9 + w, "core", "inputhistory", "[^]");

    T (3, 1, "dialog", "_default_", "New:");
    F (3, 8, 1, w, "core", "inputunchanged");
    T (3, 9, "core", "inputunchanged", "initial text");

    T (5, 1, "dialog", "_default_", "Mark:");
    F (5, 8, 1, w, "core", "input");
    T (5, 9, "core", "input", "some ");
    T (5, 14, "core", "inputmark", "selected");
    T (5, 22, "core", "input", " text");

    F (7, 0, 1, r->cols, "core", "_default_");
    T (7, 1, "core", "_default_", "$ make install");
    T (7, r->cols - 4, "core", "commandhistory", "[^]");
}

/* --------------------------------------------------------------------------------------------- */

static void
dialog_like (WSkinSample *s, const WRect *r, const char *g, const char *k_frame,
             const char *k_title, const char *k_focus, const char *k_hotfocus,
             const char *k_hotnormal, const char *title, const char *text, const char *ok,
             const char *cancel)
{
    int w = r->cols - 6;
    int h = MIN (10, r->lines - 2);

    F (0, 0, r->lines, r->cols, "core", "_default_");
    F (1, 2, h, w, g, "_default_");
    B (1, 2, h, w, g, k_frame, FALSE);
    F (2, 2 + w, h, 2, "core", "shadow");
    F (1 + h, 4, 1, w, "core", "shadow");
    T (1, 2 + (w - str_term_width1 (title)) / 2, g, k_title, title);
    T (3, 4, g, "_default_", text);
    F (4, 4, 1, w - 4, "core", "input");
    T (4, 5, "core", "input", "/home/user/backup/");
    if (strcmp (g, "dialog") == 0)
    {
        line (s, 6, 4, w - 4, g, "dselfocus", " item one, list focused");
        T (6, 4 + w - 8, g, "dcellfocus", "[x]");
        line (s, 7, 4, w - 4, g, "dselnormal", " item two, list not focused");
    }
    H (h - 1, 6, g, k_focus, k_hotfocus, ok);
    H (h - 1, 6 + str_term_width1 (ok) + 2, g, "_default_", k_hotnormal, cancel);
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_dialog (WSkinSample *s, const WRect *r)
{
    dialog_like (s, r, "dialog", "dframe", "dtitle", "dfocus", "dhotfocus", "dhotnormal", " Copy ",
                 "Copy file \"notes.txt\" to:", "[< &OK >]", "[ &Cancel ]");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_error (WSkinSample *s, const WRect *r)
{
    dialog_like (s, r, "error", "errdframe", "errdtitle", "errdfocus", "errdhotfocus",
                 "errdhotnormal", " Error ", "Cannot copy \"notes.txt\"", "[< &Retry >]",
                 "[ &Skip ]");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_menu (WSkinSample *s, const WRect *r)
{
    F (0, 0, r->lines, r->cols, "core", "_default_");
    F (0, 0, 1, r->cols, "menu", "menuinactive");
    T (0, 1, "menu", "menuinactive", "Left     File     Command     Options     Right");
    T (1, 1, "core", "_default_", "the bar when no menu is open; below, with one open:");

    F (3, 0, 1, r->cols, "menu", "_default_");
    H (3, 1, "menu", "_default_", "menuhot", "&Left");
    H (3, 10, "menu", "menusel", "menuhotsel", "&File");
    H (3, 19, "menu", "_default_", "menuhot", "&Command");
    H (3, 31, "menu", "_default_", "menuhot", "&Options");

    F (4, 8, 8, 26, "menu", "_default_");
    B (4, 8, 8, 26, "menu", "menuframe", TRUE);
    H (5, 10, "menu", "_default_", "menuhot", "&View          F3");
    H (6, 10, "menu", "menusel", "menuhotsel", "&Edit          F4");
    H (7, 10, "menu", "_default_", "menuhot", "&Copy          F5");
    L (8, 9, 24, "menu", "menuframe");
    H (9, 10, "menu", "_default_", "menuhot", "E&xit         F10");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_popupmenu (WSkinSample *s, const WRect *r)
{
    int w = MIN (30, r->cols - 4);

    F (0, 0, r->lines, r->cols, "core", "_default_");
    F (1, 2, 8, w, "popupmenu", "_default_");
    B (1, 2, 8, w, "popupmenu", "menuframe", FALSE);
    T (1, 2 + (w - 11) / 2, "popupmenu", "menutitle", " User menu ");
    line (s, 3, 3, w - 2, "popupmenu", "_default_", " Do something with the file");
    line (s, 4, 3, w - 2, "popupmenu", "menusel", " Selected item");
    line (s, 5, 3, w - 2, "popupmenu", "_default_", " Another item");
    line (s, 6, 3, w - 2, "popupmenu", "_default_", " One more");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_buttonbar (WSkinSample *s, const WRect *r)
{
    static const char *const labels[] = { "Help",   "Menu",  "View",   "Edit",   "Copy",
                                          "RenMov", "Mkdir", "Delete", "PullDn", "Quit" };
    int x = 0, i;

    F (0, 0, r->lines, r->cols, "core", "_default_");
    F (r->lines - 1, 0, 1, r->cols, "buttonbar", "button");
    for (i = 0; i < 10 && x + 8 <= r->cols; i++)
    {
        char num[4];

        g_snprintf (num, sizeof (num), "%2d", i + 1);
        T (r->lines - 1, x, "buttonbar", "hotkey", num);
        line (s, r->lines - 1, x + 2, 6, "buttonbar", "button", labels[i]);
        x += 8;
    }
    T (1, 1, "core", "_default_", "the bottom row of the screen");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_statusbar (WSkinSample *s, const WRect *r)
{
    F (0, 0, r->lines, r->cols, "editor", "_default_");
    F (0, 0, 1, r->cols, "statusbar", "_default_");
    T (0, 1, "statusbar", "_default_", "notes.txt  [----]  0 L:[  1+ 0   1/ 40] *(0 / 0b)");
    T (2, 1, "editor", "_default_", "the top line of the editor, the viewer");
    T (3, 1, "editor", "_default_", "and the diff viewer");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_help (WSkinSample *s, const WRect *r)
{
    F (0, 0, r->lines, r->cols, "help", "_default_");
    B (0, 0, r->lines, r->cols, "help", "helpframe", FALSE);
    T (0, (r->cols - 6) / 2, "help", "helptitle", " Help ");
    T (2, 2, "help", "_default_", "M-Commander is a");
    T (3, 2, "help", "helpbold", "directory browser");
    T (3, 20, "help", "_default_", "with an");
    T (4, 2, "help", "helpitalic", "emphasized");
    T (4, 13, "help", "_default_", "way of doing things.");
    T (6, 2, "help", "_default_", "See also:");
    T (7, 4, "help", "helplink", "Overview");
    T (8, 4, "help", "helpslink", "Keys, the selected link");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_editor (WSkinSample *s, const WRect *r)
{
    int w1 = r->cols - 8, h1 = r->lines - 3;
    const char *state = sample_char (s, "widget-editor", "window-state-char");
    const char *close = sample_char (s, "widget-editor", "window-close-char");
    const char *fold_open = sample_char (s, "widget-editor", "fold-open-char");
    const char *fold_close = sample_char (s, "widget-editor", "fold-close-char");
    char *tmp;

    F (0, 0, r->lines, r->cols, "editor", "editbg");

    // the active window
    F (0, 0, h1, w1, "editor", "_default_");
    B (0, 0, h1, w1, "editor", "editframeactive", FALSE);
    F (0, 1, 1, w1 - 2, "statusbar", "_default_");
    T (0, 2, "statusbar", "_default_", "main.c  [----]  3 L:[ 4+ 0 ]");
    T (0, w1 - 5, "widget-editor", "window-state-char", state);
    T (0, w1 - 3, "widget-editor", "window-close-char", close);

    T (1, 1, "editor", "editlinestate", "   1 ");
    T (1, 6, "editor", "_default_", "int main (void)");
    tmp = g_strdup_printf ("%s  2 ", fold_open);
    T (2, 1, "editor", "editlinestate", tmp);
    g_free (tmp);
    T (2, 6, "editor", "_default_", "{");
    T (3, 1, "editor", "editlinestate", "   3 ");
    T (3, 6, "editor", "editmarked", "    int i = 0;  /* selected */");
    T (4, 1, "editor", "editlinestate", "   4 ");
    T (4, 6, "editor", "_default_", "    puts (\"");
    T (4, 17, "editor", "editbold", "found");
    T (4, 22, "editor", "_default_", "\");");
    T (5, 1, "editor", "editlinestate", "   5 ");
    T (5, 6, "editor", "_default_", "    tab");
    T (5, 13, "editor", "editwhitespace", "-->.....");
    T (5, 21, "editor", "_default_", "end");
    T (6, 1, "editor", "editlinestate", "   6 ");
    T (6, 6, "editor", "editnonprintable", "^M^[");
    T (7, 1, "editor", "editlinestate", "   7 ");
    T (7, 6, "editor", "bookmark", "    /* a bookmarked line */");
    T (8, 1, "editor", "editlinestate", "   8 ");
    T (8, 6, "editor", "bookmarkfound", "    /* found by Find all */");
    tmp = g_strdup_printf ("%s  9 ", fold_close);
    T (9, 1, "editor", "editlinestate", tmp);
    g_free (tmp);
    T (9, 6, "editor", "_default_", "}");
    F (1, w1 - 3, h1 - 2, 1, "editor", "editrightmargin");

    // an inactive window and one being moved
    F (r->lines - 7, r->cols - 16, 5, 15, "editor", "_default_");
    B (r->lines - 7, r->cols - 16, 5, 15, "editor", "editframe", FALSE);
    T (r->lines - 6, r->cols - 14, "editor", "_default_", "inactive");
    F (r->lines - 4, r->cols - 28, 3, 10, "editor", "_default_");
    B (r->lines - 4, r->cols - 28, 3, 10, "editor", "editframedrag", FALSE);
    T (r->lines - 3, r->cols - 26, "editor", "_default_", "moved");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_viewer (WSkinSample *s, const WRect *r)
{
    F (0, 0, r->lines, r->cols, "viewer", "_default_");
    B (0, 0, r->lines, r->cols, "viewer", "viewframe", TRUE);
    T (0, 2, "core", "reverse", " Quick view ");
    T (1, 2, "viewer", "viewheading", "NAME");
    T (2, 6, "viewer", "_default_", "ls - list directory contents");
    T (4, 2, "viewer", "viewbold", "SYNOPSIS");
    T (5, 6, "viewer", "_default_", "ls [");
    T (5, 10, "viewer", "viewunderline", "OPTION");
    T (5, 16, "viewer", "_default_", "]... [");
    T (5, 22, "viewer", "viewunderline", "FILE");
    T (5, 26, "viewer", "_default_", "]...");
    T (7, 6, "viewer", "viewboldunderline", "bold and underlined");
    T (9, 6, "viewer", "_default_", "a search match: ");
    T (9, 22, "viewer", "viewselected", "pattern");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_mcterm (WSkinSample *s, const WRect *r)
{
    F (0, 0, r->lines, r->cols, "mcterm", "_default_");
    T (0, 0, "mcterm", "_default_", "$ ls -la");
    T (1, 0, "mcterm", "_default_", "total 12");
    T (2, 0, "mcterm", "_default_", "drwxr-xr-x  2 user user 4096 Sep  5 .");
    T (3, 0, "mcterm", "mctermselected", "-rw-r--r--  1 user user 1234 Sep  5 notes.txt");
    T (4, 0, "mcterm", "_default_", "$ echo selected line above");
    T (5, 0, "mcterm", "_default_", "$ _");
}

/* --------------------------------------------------------------------------------------------- */

/* like mcdiff: the status bar, two heavy boxes side by side, a line after the numbers */

static void
draw_diffviewer (WSkinSample *s, const WRect *r)
{
    int half = r->cols / 2;
    int h = MIN (8, r->lines - 1);
    int side;

    F (0, 0, r->lines, r->cols, "core", "_default_");
    F (0, 0, 1, r->cols, "statusbar", "_default_");
    T (0, 1, "statusbar", "_default_", "old.c");
    T (0, half + 1, "statusbar", "_default_", "new.c");

    for (side = 0; side < 2; side++)
    {
        int x = side * half;
        int w = side == 0 ? half : r->cols - half;
        int i;

        B (1, x, h, w, "core", "frame", FALSE);
        for (i = 2; i < h; i++)
            sample_piece (s, i, x + 4, "core", "frame", "lines", "vert",
                          mc_tty_frm[MC_TTY_FRM_VERT]);
        line (s, 2, x + 1, 3, "core", "_default_", "  1");
        line (s, 2, x + 5, w - 6, "core", "_default_", "int main ()");
    }

    line (s, 3, 1, 3, "diffviewer", "removed", "  2");
    line (s, 3, 5, half - 6, "diffviewer", "removed", "removed line");
    line (s, 3, half + 1, 3, "diffviewer", "changed", "");
    line (s, 3, half + 5, r->cols - half - 6, "diffviewer", "changed", "");

    line (s, 4, 1, 3, "diffviewer", "changed", "");
    line (s, 4, 5, half - 6, "diffviewer", "changed", "");
    line (s, 4, half + 1, 3, "diffviewer", "added", "  2");
    line (s, 4, half + 5, r->cols - half - 6, "diffviewer", "added", "added line");

    line (s, 5, 1, 3, "diffviewer", "changedline", "  3");
    line (s, 5, 5, half - 6, "diffviewer", "changedline", "return ");
    T (5, 12, "diffviewer", "changednew", "0");
    line (s, 5, half + 1, 3, "diffviewer", "changedline", "  3");
    line (s, 5, half + 5, r->cols - half - 6, "diffviewer", "changedline", "return ");
    T (5, half + 12, "diffviewer", "changednew", "1");

    line (s, 6, 1, 3, "diffviewer", "error", "");
    line (s, 6, 5, half - 6, "diffviewer", "error", "binary files differ");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_mctree (WSkinSample *s, const WRect *r)
{
    F (0, 0, r->lines, r->cols, "mctree", "_default_");
    T (0, 0, "mctree", "marker", "- ");
    T (0, 2, "mctree", "_default_", "{");
    T (1, 2, "mctree", "_default_", "  ");
    T (1, 4, "mctree", "key", "name");
    T (1, 8, "mctree", "_default_", ": ");
    T (1, 10, "mctree", "value", "\"mc\"");
    T (2, 4, "mctree", "key", "version");
    T (2, 11, "mctree", "_default_", ": ");
    T (2, 13, "mctree", "value", "6");
    T (3, 2, "mctree", "marker", "+ ");
    T (3, 4, "mctree", "key", "skins");
    T (3, 9, "mctree", "_default_", ": [...]");
    line (s, 4, 0, r->cols, "mctree", "selected", "  - options: {");
    T (5, 4, "mctree", "key", "shadows");
    T (5, 11, "mctree", "_default_", ": ");
    T (5, 13, "mctree", "value", "true");
    T (6, 2, "mctree", "_default_", "  }");
    T (7, 0, "mctree", "_default_", "}");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_mcstruct_tree (WSkinSample *s, const WRect *r)
{
    int w = r->cols - 2;

    F (0, 0, r->lines, r->cols, "mcstruct-tree", "_default_");
    B (0, 0, r->lines, r->cols, "mcstruct-tree", "frame-active", TRUE);
    line (s, 1, 1, w, "mcstruct-tree", "head", " offset  name          type     value");
    T (2, 1, "mcstruct-tree", "offset", " 000000");
    T (2, 9, "mcstruct-tree", "struct", "PNG");
    T (3, 1, "mcstruct-tree", "offset", " 000000");
    T (3, 9, "mcstruct-tree", "name", "signature");
    T (3, 23, "mcstruct-tree", "type", "bytes");
    T (3, 32, "mcstruct-tree", "value", "89 50 4E 47");
    line (s, 4, 1, w, "mcstruct-tree", "selected", " 000008  chunk         struct   IHDR");
    T (5, 1, "mcstruct-tree", "offset", " 000010");
    T (5, 9, "mcstruct-tree", "name", "width");
    T (5, 23, "mcstruct-tree", "type", "uint32");
    T (5, 32, "mcstruct-tree", "value", "640");
    T (6, 9, "mcstruct-tree", "jump", "-> data at 000021");
    T (7, 9, "mcstruct-tree", "remark", "; image header");
    T (8, 9, "mcstruct-tree", "error", "unknown chunk type");
    F (r->lines - 3, r->cols - 12, 3, 11, "mcstruct-tree", "_default_");
    B (r->lines - 3, r->cols - 12, 3, 11, "mcstruct-tree", "frame", TRUE);
    T (r->lines - 2, r->cols - 10, "mcstruct-tree", "_default_", "inactive");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_mcstruct_hex (WSkinSample *s, const WRect *r)
{
    F (0, 0, r->lines, r->cols, "mcstruct-hex", "_default_");
    line (s, 0, 0, r->cols, "mcstruct-hex", "head", " offset   00 01 02 03  04 05 06 07");
    T (1, 0, "mcstruct-hex", "offset", " 00000000");
    T (1, 10, "mcstruct-hex", "_default_", "89 50 4E 47");
    T (1, 21, "mcstruct-hex", "frame", "|");
    T (1, 23, "mcstruct-hex", "_default_", "0D 0A 1A 0A");
    T (2, 0, "mcstruct-hex", "offset", " 00000008");
    T (2, 10, "mcstruct-hex", "mark", "00 00 00 0D");
    T (2, 21, "mcstruct-hex", "frame", "|");
    T (2, 23, "mcstruct-hex", "mark", "49 48 44 52");
    T (3, 0, "mcstruct-hex", "offset", " 00000010");
    T (3, 10, "mcstruct-hex", "_default_", "00 00 ");
    T (3, 16, "mcstruct-hex", "changed", "02");
    T (3, 19, "mcstruct-hex", "cursor", "80");
    T (3, 21, "mcstruct-hex", "frame", "|");
    T (3, 23, "mcstruct-hex", "block", "00 00 01 E0");
    T (4, 0, "mcstruct-hex", "offset", " 00000018");
    T (4, 10, "mcstruct-hex", "block", "08 02 00 00");
    T (4, 21, "mcstruct-hex", "frame", "|");
    T (4, 23, "mcstruct-hex", "_default_", "00 FC 18 ED");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_mcstruct_def (WSkinSample *s, const WRect *r)
{
    int w = r->cols - 2;

    F (0, 0, r->lines, r->cols, "mcstruct-def", "_default_");
    B (0, 0, r->lines, r->cols, "mcstruct-def", "frame-active", TRUE);
    line (s, 1, 1, w, "mcstruct-def", "head", " png.stl");
    T (2, 1, "mcstruct-def", "lineno", "  1 ");
    T (2, 5, "mcstruct-def", "comment", "; PNG image");
    T (3, 1, "mcstruct-def", "lineno", "  2 ");
    T (3, 5, "mcstruct-def", "directive", "#struct");
    T (3, 13, "mcstruct-def", "_default_", "PNG");
    line (s, 4, 1, w, "mcstruct-def", "selected", "  3   signature  bytes 8");
    T (5, 1, "mcstruct-def", "lineno", "  4 ");
    T (5, 5, "mcstruct-def", "label", "chunk:");
    T (6, 1, "mcstruct-def", "lineno", "  5 ");
    T (6, 5, "mcstruct-def", "_default_", "  length  uint32");
    F (r->lines - 3, r->cols - 12, 3, 11, "mcstruct-def", "_default_");
    B (r->lines - 3, r->cols - 12, 3, 11, "mcstruct-def", "frame", TRUE);
    T (r->lines - 2, r->cols - 10, "mcstruct-def", "_default_", "inactive");
}

/* --------------------------------------------------------------------------------------------- */

/* a box piece by piece with the light or heavy set, a tee on every side, a cross in the middle */

static void
frame_set (WSkinSample *s, int y, int x, int lines, int cols, gboolean heavy)
{
    const int base = heavy ? MC_TTY_FRM_DHORIZ : MC_TTY_FRM_HORIZ;
    const char *k = heavy ? "d" : "";
    int i, j, my = y + lines / 2, mx = x + cols / 2;
    char key[32];

#define P(yy, xx, name, frm)                                                                       \
    do                                                                                             \
    {                                                                                              \
        g_snprintf (key, sizeof (key), "%s%s", k, name);                                           \
        sample_piece (s, yy, xx, "core", "frame", "lines", key, mc_tty_frm[base + (frm)]);         \
    }                                                                                              \
    while (0)

    for (j = x + 1; j < x + cols - 1; j++)
    {
        P (y, j, "horiz", MC_TTY_FRM_HORIZ - MC_TTY_FRM_HORIZ);
        P (y + lines - 1, j, "horiz", 0);
        if (j != mx)
            sample_piece (s, my, j, "core", "frame", "lines", "horiz",
                          mc_tty_frm[MC_TTY_FRM_HORIZ]);
    }
    for (i = y + 1; i < y + lines - 1; i++)
    {
        P (i, x, "vert", MC_TTY_FRM_VERT - MC_TTY_FRM_HORIZ);
        P (i, x + cols - 1, "vert", MC_TTY_FRM_VERT - MC_TTY_FRM_HORIZ);
        if (i != my)
            sample_piece (s, i, mx, "core", "frame", "lines", "vert", mc_tty_frm[MC_TTY_FRM_VERT]);
    }
    P (y, x, "lefttop", MC_TTY_FRM_LEFTTOP - MC_TTY_FRM_HORIZ);
    P (y, x + cols - 1, "righttop", MC_TTY_FRM_RIGHTTOP - MC_TTY_FRM_HORIZ);
    P (y + lines - 1, x, "leftbottom", MC_TTY_FRM_LEFTBOTTOM - MC_TTY_FRM_HORIZ);
    P (y + lines - 1, x + cols - 1, "rightbottom", MC_TTY_FRM_RIGHTBOTTOM - MC_TTY_FRM_HORIZ);
    P (y, mx, "topmiddle", MC_TTY_FRM_TOPMIDDLE - MC_TTY_FRM_HORIZ);
    P (y + lines - 1, mx, "bottommiddle", MC_TTY_FRM_BOTTOMMIDDLE - MC_TTY_FRM_HORIZ);
    P (my, x, "leftmiddle", MC_TTY_FRM_LEFTMIDDLE - MC_TTY_FRM_HORIZ);
    P (my, x + cols - 1, "rightmiddle", MC_TTY_FRM_RIGHTMIDDLE - MC_TTY_FRM_HORIZ);
    sample_piece (s, my, mx, "core", "frame", "lines", "cross", mc_tty_frm[MC_TTY_FRM_CROSS]);
#undef P
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_lines (WSkinSample *s, const WRect *r)
{
    int w = MIN (18, (r->cols - 3) / 2);
    int h = MIN (7, r->lines - 3);

    F (0, 0, r->lines, r->cols, "core", "_default_");
    T (0, 1, "core", "_default_", "light");
    T (0, 2 + w, "core", "_default_", "heavy");
    frame_set (s, 1, 1, h, w, FALSE);
    frame_set (s, 1, 2 + w, h, w, TRUE);
    T (h + 2, 1, "core", "_default_", "the panel frame uses the light set, the");
    T (h + 3, 1, "core", "_default_", "dialogs the heavy one; tees join them");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_panel_marks (WSkinSample *s, const WRect *r)
{
    int w = r->cols;

    F (0, 0, r->lines, r->cols, "core", "_default_");
    B (0, 0, MIN (6, r->lines), w, "core", "frame", TRUE);
    T (0, 1, "widget-panel", "hiddenfiles-show-char",
       sample_char (s, "widget-panel", "hiddenfiles-show-char"));
    T (0, 2, "widget-panel", "history-prev-item-char",
       sample_char (s, "widget-panel", "history-prev-item-char"));
    T (0, 4, "core", "reverse", " /home/user ");
    T (0, w - 7, "widget-panel", "hiddenfiles-hide-char",
       sample_char (s, "widget-panel", "hiddenfiles-hide-char"));
    T (0, w - 5, "core", "frame", "[");
    T (0, w - 4, "widget-panel", "history-show-list-char",
       sample_char (s, "widget-panel", "history-show-list-char"));
    T (0, w - 3, "core", "frame", "]");
    T (0, w - 2, "widget-panel", "history-next-item-char",
       sample_char (s, "widget-panel", "history-next-item-char"));

    line (s, 1, 1, w - 2, "core", "header", "");
    T (1, 1, "widget-panel", "sort-up-char", sample_char (s, "widget-panel", "sort-up-char"));
    T (1, 2, "core", "header", "Name");
    T (1, 12, "widget-panel", "sort-down-char", sample_char (s, "widget-panel", "sort-down-char"));
    T (1, 13, "core", "header", "Size");

    line (s, 2, 1, w - 2, "core", "_default_", " notes.txt");
    line (s, 3, 1, w - 2, "core", "selected", "");
    T (3, 1, "widget-panel", "filename-scroll-left-char",
       sample_char (s, "widget-panel", "filename-scroll-left-char"));
    T (3, 2, "core", "selected", "ery_long_file_name_that_goes_o");
    T (3, 2 + MIN (30, w - 5), "widget-panel", "filename-scroll-right-char",
       sample_char (s, "widget-panel", "filename-scroll-right-char"));
    line (s, 4, 1, w - 2, "core", "_default_", " other.txt");
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_other_marks (WSkinSample *s, const WRect *r)
{
    F (0, 0, r->lines, r->cols, "dialog", "_default_");
    T (1, 1, "dialog", "_default_", "Find file:");
    T (2, 1, "dialog", "_default_", "[");
    T (2, 2, "widget-find", "show-matches-char",
       sample_char (s, "widget-find", "show-matches-char"));
    T (2, 3, "dialog", "_default_", "] /home/user/notes.txt  3 matches");

    F (4, 0, 6, r->cols, "editor", "_default_");
    B (4, 0, 6, r->cols, "editor", "editframeactive", FALSE);
    F (4, 1, 1, r->cols - 2, "statusbar", "_default_");
    T (4, 2, "statusbar", "_default_", "main.c");
    T (4, r->cols - 5, "widget-editor", "window-state-char",
       sample_char (s, "widget-editor", "window-state-char"));
    T (4, r->cols - 3, "widget-editor", "window-close-char",
       sample_char (s, "widget-editor", "window-close-char"));
    T (5, 1, "editor", "editlinestate", "   1 ");
    T (5, 6, "editor", "_default_", "int main (void)");
    T (6, 1, "widget-editor", "fold-open-char", sample_char (s, "widget-editor", "fold-open-char"));
    T (6, 2, "editor", "editlinestate", "  2 ");
    T (6, 6, "editor", "_default_", "{  an open fold");
    T (7, 1, "widget-editor", "fold-close-char",
       sample_char (s, "widget-editor", "fold-close-char"));
    T (7, 2, "editor", "editlinestate", "  9 ");
    T (7, 6, "editor", "_default_", "}  a closed one");
}

/* --------------------------------------------------------------------------------------------- */

/* The cells of the thumb from the skin, the default when it names none */
static int
scrollbar_thumb_len (WSkinSample *s, const char *key, int def, int room)
{
    int len = atoi (sample_char (s, "scrollbar", key));

    return CLAMP (len > 0 ? len : def, 1, MAX (1, room));
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_scrollbar (WSkinSample *s, const WRect *r)
{
    const int h = MIN (10, r->lines - 4);
    const int w = 14;
    const int vthumb = scrollbar_thumb_len (s, "vthumblen", 6, h - 3);
    const int hthumb = scrollbar_thumb_len (s, "hthumblen", 4, w - 3);
    int i;

    F (0, 0, r->lines, r->cols, "core", "_default_");

    T (0, 2, "scrollbar", "vstart", sample_char (s, "scrollbar", "vstart"));
    for (i = 1; i < h - 1; i++)
    {
        const char *key = i >= 2 && i < 2 + vthumb ? "vthumb" : "vtrack";

        T (i, 2, "scrollbar", key, sample_char (s, "scrollbar", key));
    }
    T (h - 1, 2, "scrollbar", "vend", sample_char (s, "scrollbar", "vend"));

    T (h + 1, 2, "scrollbar", "hstart", sample_char (s, "scrollbar", "hstart"));
    for (i = 1; i < w - 1; i++)
    {
        const char *key = i >= 3 && i < 3 + hthumb ? "hthumb" : "htrack";

        T (h + 1, 2 + i, "scrollbar", key, sample_char (s, "scrollbar", key));
    }
    T (h + 1, 2 + w - 1, "scrollbar", "hend", sample_char (s, "scrollbar", "hend"));
}

/* --------------------------------------------------------------------------------------------- */

/* the frames are in the display charset: walked with strutil, not as UTF-8 */

static void
draw_spinner (WSkinSample *s, const WRect *r)
{
    const char *seq = sample_char (s, "core", "spinner_sequence");
    const char *p, *next;
    int n = str_length (seq), x = 2, i;
    char frame[16];

    F (0, 0, r->lines, r->cols, "core", "_default_");
    T (0, 2, "core", "_default_", "frames:");
    for (p = seq; *p != '\0'; p = next)
    {
        next = p;
        str_cnext_char (&next);
        g_strlcpy (frame, p, MIN ((size_t) (next - p) + 1, sizeof (frame)));
        T (1, x, "core", "spinner_sequence", frame);
        x += 2;
    }
    if (n == 0)
        return;

    for (p = seq, i = 0; i < (int) (s->tick % (unsigned int) n); i++)
        str_cnext_char (&p);
    next = p;
    str_cnext_char (&next);
    g_strlcpy (frame, p, MIN ((size_t) (next - p) + 1, sizeof (frame)));
    T (3, 2, "core", "_default_", "Working... ");
    T (3, 13, "core", "spinner_sequence", frame);
}

/* --------------------------------------------------------------------------------------------- */

static const sample_map_t sample_map[] = {
    { "core", "_default_", draw_panels },
    { "core", "input", draw_input },
    { "core", "spinner_sequence", draw_spinner },
    { "filehighlight", NULL, draw_filetypes },
    { "dialog", NULL, draw_dialog },
    { "error", NULL, draw_error },
    { "menu", NULL, draw_menu },
    { "popupmenu", NULL, draw_popupmenu },
    { "buttonbar", NULL, draw_buttonbar },
    { "statusbar", NULL, draw_statusbar },
    { "help", NULL, draw_help },
    { "editor", NULL, draw_editor },
    { "viewer", NULL, draw_viewer },
    { "mcterm", NULL, draw_mcterm },
    { "diffviewer", NULL, draw_diffviewer },
    { "mctree", NULL, draw_mctree },
    { "mcstruct-tree", NULL, draw_mcstruct_tree },
    { "mcstruct-hex", NULL, draw_mcstruct_hex },
    { "mcstruct-def", NULL, draw_mcstruct_def },
    { "lines", NULL, draw_lines },
    { "widget-panel", NULL, draw_panel_marks },
    { "widget-find", NULL, draw_other_marks },
    { "scrollbar", NULL, draw_scrollbar },
};

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

skinsample_draw_fn
skinsample_lookup (const char *group, const char *first_key)
{
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (sample_map); i++)
        if (strcmp (sample_map[i].group, group) == 0
            && (sample_map[i].first_key == NULL
                || strcmp (sample_map[i].first_key, first_key) == 0))
            return sample_map[i].draw;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
