/*
   Skin editor plugin - the keys a skin may set, with labels.

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

/* Every key the code reads from a skin. The order is the order in the editor. The texts
   follow misc/skins/README.txt. */

#include <config.h>

#include "lib/global.h"

#include "skinedit_table.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define C(group, key, label, desc)       { group, key, SKINEDIT_ENTRY_COLOR, label, desc, NULL }
#define CH(group, key, label, desc, def) { group, key, SKINEDIT_ENTRY_CHAR, label, desc, def }
#define ST(group, key, label, desc, def) { group, key, SKINEDIT_ENTRY_STRING, label, desc, def }

#define SECTION(label, rows)             { label, rows, G_N_ELEMENTS (rows) }

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* clang-format off */

static const skinedit_table_row_t rows_panels[] = {
    C ("core", "_default_", N_ ("Normal text"),
       N_ ("Panel text and everything that has no color of its own")),
    C ("core", "selected", N_ ("File under cursor"), N_ ("The file under the cursor")),
    C ("core", "marked", N_ ("Marked file"), N_ ("Files marked with Insert or Ctrl-T")),
    C ("core", "markselect", N_ ("Marked file under cursor"),
       N_ ("A file that is both marked and under the cursor")),
    C ("core", "reverse", N_ ("Panel title, active"),
       N_ ("Text on top of the frame of the active panel: the directory name, Quick view")),
    C ("core", "header", N_ ("Column header"), N_ ("Header entries of panels: Name, Size, Modify time")),
    C ("core", "frame", N_ ("Frame"), N_ ("Panel frame")),
    C ("core", "commandlinemark", N_ ("Command line, selected text"),
       N_ ("Selected text in the command line")),
    C ("core", "disabled", N_ ("Disabled element"),
       N_ ("Disabled UI elements, e.g. in Find File and some Options dialogs")),
    C ("core", "gauge", N_ ("Progress bar"), N_ ("Filled part of the progress bar")),
    C ("core", "shadow", N_ ("Dialog shadow"), N_ ("Shadow cast by dialogs")),
    C ("core", "permread", N_ ("Permission: read"),
       N_ ("The r of the permission field when Permission colors is on")),
    C ("core", "permwrite", N_ ("Permission: write"),
       N_ ("The w of the permission field when Permission colors is on")),
    C ("core", "permexec", N_ ("Permission: execute"),
       N_ ("The x of the permission field when Permission colors is on")),
    C ("core", "permspecial", N_ ("Permission: special"),
       N_ ("The s and t of the permission field when Permission colors is on")),
    C ("core", "permnone", N_ ("Permission: none"),
       N_ ("The - of the permission field when Permission colors is on")),
};

static const skinedit_table_row_t rows_filetypes[] = {
    C ("filehighlight", "_default_", N_ ("Other files"), N_ ("Files no filehighlight.ini group matches")),
    C ("filehighlight", "directory", N_ ("Directory"), NULL),
    C ("filehighlight", "executable", N_ ("Executable"), NULL),
    C ("filehighlight", "symlink", N_ ("Symbolic link"), NULL),
    C ("filehighlight", "hardlink", N_ ("Hard link"), NULL),
    C ("filehighlight", "stalelink", N_ ("Stale link"), N_ ("A symbolic link whose target is gone")),
    C ("filehighlight", "device", N_ ("Device"), NULL),
    C ("filehighlight", "special", N_ ("Special file"), N_ ("Socket, FIFO, door")),
    C ("filehighlight", "core", N_ ("Core dump"), NULL),
    C ("filehighlight", "temp", N_ ("Temporary file"), NULL),
    C ("filehighlight", "archive", N_ ("Archive"), NULL),
    C ("filehighlight", "doc", N_ ("Document"), NULL),
    C ("filehighlight", "source", N_ ("Source code"), NULL),
    C ("filehighlight", "media", N_ ("Audio and video"), NULL),
    C ("filehighlight", "graph", N_ ("Image"), NULL),
    C ("filehighlight", "database", N_ ("Database"), NULL),
};

static const skinedit_table_row_t rows_input[] = {
    C ("core", "input", N_ ("Input line"), N_ ("Input lines in dialogs and the command line")),
    C ("core", "inputunchanged", N_ ("Input line, unchanged text"),
       N_ ("Input text before the first modification or cursor movement")),
    C ("core", "inputmark", N_ ("Input line, selected text"),
       N_ ("Text selected in an input line, e.g. with Shift-arrows")),
    C ("core", "inputhistory", N_ ("Input history button"),
       N_ ("The button that opens the history of an input line")),
    C ("core", "commandhistory", N_ ("Command history button"),
       N_ ("The button that opens the command history")),
};

static const skinedit_table_row_t rows_dialog[] = {
    C ("dialog", "_default_", N_ ("Normal text"), N_ ("Dialog background and text")),
    C ("dialog", "dfocus", N_ ("Focused element"), N_ ("The element that has the focus")),
    C ("dialog", "dhotnormal", N_ ("Hotkey"), N_ ("Hotkey letters of buttons and checkboxes")),
    C ("dialog", "dhotfocus", N_ ("Hotkey of the focused element"), NULL),
    C ("dialog", "dselnormal", N_ ("Selected item, list not focused"),
       N_ ("Selected item of a list that does not have the focus, e.g. Find File results after Tab")),
    C ("dialog", "dselfocus", N_ ("Selected item, list focused"),
       N_ ("Selected item of the focused list, e.g. Find File results or the skin list")),
    C ("dialog", "dcellfocus", N_ ("Check box under the cursor"),
       N_ ("Check box under the cursor in the selected row of a table")),
    C ("dialog", "dtitle", N_ ("Title"), NULL),
    C ("dialog", "dframe", N_ ("Frame"), NULL),
};

static const skinedit_table_row_t rows_error[] = {
    C ("error", "_default_", N_ ("Normal text"), N_ ("Error dialog background and text")),
    C ("error", "errdfocus", N_ ("Focused element"), NULL),
    C ("error", "errdhotnormal", N_ ("Hotkey"), NULL),
    C ("error", "errdhotfocus", N_ ("Hotkey of the focused element"), NULL),
    C ("error", "errdtitle", N_ ("Title"), NULL),
    C ("error", "errdframe", N_ ("Frame"), NULL),
};

static const skinedit_table_row_t rows_menu[] = {
    C ("menu", "_default_", N_ ("Item"), N_ ("Items of the dropdown menu")),
    C ("menu", "menusel", N_ ("Selected item"), NULL),
    C ("menu", "menuhot", N_ ("Hotkey"), NULL),
    C ("menu", "menuhotsel", N_ ("Hotkey of the selected item"), NULL),
    C ("menu", "menuinactive", N_ ("Menu bar, inactive"), N_ ("The menu bar when no menu is open")),
    C ("menu", "menuframe", N_ ("Frame"), NULL),
};

static const skinedit_table_row_t rows_popupmenu[] = {
    C ("popupmenu", "_default_", N_ ("Item"),
       N_ ("User menu (F2), editor menu (F11), codepage list (Alt-E)")),
    C ("popupmenu", "menusel", N_ ("Selected item"), NULL),
    C ("popupmenu", "menutitle", N_ ("Title"), NULL),
    C ("popupmenu", "menuframe", N_ ("Frame"), NULL),
};

static const skinedit_table_row_t rows_buttonbar[] = {
    C ("buttonbar", "hotkey", N_ ("Function key number"), N_ ("The numbers in the bottom row")),
    C ("buttonbar", "button", N_ ("Key label"), N_ ("The labels in the bottom row")),
};

static const skinedit_table_row_t rows_statusbar[] = {
    C ("statusbar", "_default_", N_ ("Status bar"),
       N_ ("The top line of the editor, the viewer and the diff viewer")),
};

static const skinedit_table_row_t rows_help[] = {
    C ("help", "_default_", N_ ("Normal text"), NULL),
    C ("help", "helpbold", N_ ("Bold text"), NULL),
    C ("help", "helpitalic", N_ ("Italic text"), NULL),
    C ("help", "helplink", N_ ("Link"), NULL),
    C ("help", "helpslink", N_ ("Selected link"), N_ ("The link under the cursor")),
    C ("help", "helptitle", N_ ("Title"), NULL),
    C ("help", "helpframe", N_ ("Frame"), NULL),
};

static const skinedit_table_row_t rows_editor[] = {
    C ("editor", "_default_", N_ ("Normal text"), NULL),
    C ("editor", "editbold", N_ ("Found text"), N_ ("Text with the bold attribute, e.g. a search match")),
    C ("editor", "editmarked", N_ ("Selected text"), NULL),
    C ("editor", "editwhitespace", N_ ("Tabs and trailing spaces"), NULL),
    C ("editor", "editnonprintable", N_ ("Non-printable characters"), NULL),
    C ("editor", "editrightmargin", N_ ("Right margin"), NULL),
    C ("editor", "editlinestate", N_ ("Line numbers"), N_ ("The line state column at the left")),
    C ("editor", "bookmark", N_ ("Bookmarked line"), N_ ("Lines bookmarked by hand")),
    C ("editor", "bookmarkfound", N_ ("Line found by Find all"), NULL),
    C ("editor", "editbg", N_ ("Outside the windows"),
       N_ ("Area not covered by any editor window in multi-window mode")),
    C ("editor", "editframe", N_ ("Frame, inactive window"), NULL),
    C ("editor", "editframeactive", N_ ("Frame, active window"), NULL),
    C ("editor", "editframedrag", N_ ("Frame, window being moved"),
       N_ ("Frame of a window being moved or resized")),
};

static const skinedit_table_row_t rows_viewer[] = {
    C ("viewer", "_default_", N_ ("Normal text"), NULL),
    C ("viewer", "viewbold", N_ ("Bold text"), N_ ("Bold text of manual pages")),
    C ("viewer", "viewunderline", N_ ("Underlined text"), N_ ("Underlined text of manual pages")),
    C ("viewer", "viewboldunderline", N_ ("Bold underlined text"), NULL),
    C ("viewer", "viewheading", N_ ("Heading"), N_ ("Headings of manual pages")),
    C ("viewer", "viewselected", N_ ("Selected text"), N_ ("A search match")),
    C ("viewer", "viewframe", N_ ("Frame"), N_ ("The frame in Quick view")),
};

static const skinedit_table_row_t rows_mcterm[] = {
    C ("mcterm", "_default_", N_ ("Normal text"),
       N_ ("Default color and the background the terminal is painted on")),
    C ("mcterm", "mctermselected", N_ ("Selected text"), NULL),
};

static const skinedit_table_row_t rows_diffviewer[] = {
    C ("diffviewer", "added", N_ ("Added line"), NULL),
    C ("diffviewer", "removed", N_ ("Removed line"), NULL),
    C ("diffviewer", "changedline", N_ ("Changed line"), NULL),
    C ("diffviewer", "changednew", N_ ("Changed text"), N_ ("The changed part of a changed line")),
    C ("diffviewer", "changed", N_ ("Changed line, filler"),
       N_ ("The rest of a changed line and its counterpart")),
    C ("diffviewer", "error", N_ ("Error"), NULL),
    C ("diffviewer", "conflict", N_ ("Conflict"),
       N_ ("Lines both sides of a merge changed; the removed color when absent")),
    C ("diffviewer", "linemark", N_ ("Line mark"),
       N_ ("The mark on the frame at the line the keys act on")),
    C ("diffviewer", "inactiveframe", N_ ("Inactive frame"),
       N_ ("The frame of the file that is not active")),
};

static const skinedit_table_row_t rows_mctree[] = {
    C ("mctree", "_default_", N_ ("Normal text"), N_ ("Structured view of JSON, YAML and the like")),
    C ("mctree", "key", N_ ("Key"), NULL),
    C ("mctree", "value", N_ ("Value"), NULL),
    C ("mctree", "marker", N_ ("Collapse marker"),
       N_ ("The + and - before a node with children; the viewer's normal color when absent")),
    C ("mctree", "selected", N_ ("Node under cursor"),
       N_ ("The viewer's selected color when absent")),
};

static const skinedit_table_row_t rows_mcstruct_tree[] = {
    C ("mcstruct-tree", "_default_", N_ ("Normal text"), NULL),
    C ("mcstruct-tree", "frame", N_ ("Frame"), NULL),
    C ("mcstruct-tree", "frame-active", N_ ("Frame, active"), NULL),
    C ("mcstruct-tree", "head", N_ ("Header"), NULL),
    C ("mcstruct-tree", "selected", N_ ("Selected row"), NULL),
    C ("mcstruct-tree", "offset", N_ ("Offset"), NULL),
    C ("mcstruct-tree", "name", N_ ("Field name"), NULL),
    C ("mcstruct-tree", "type", N_ ("Field type"), NULL),
    C ("mcstruct-tree", "value", N_ ("Value"), NULL),
    C ("mcstruct-tree", "struct", N_ ("Structure name"), NULL),
    C ("mcstruct-tree", "jump", N_ ("Jump"), NULL),
    C ("mcstruct-tree", "remark", N_ ("Remark"), NULL),
    C ("mcstruct-tree", "error", N_ ("Error"), NULL),
};

static const skinedit_table_row_t rows_mcstruct_hex[] = {
    C ("mcstruct-hex", "_default_", N_ ("Normal text"), NULL),
    C ("mcstruct-hex", "head", N_ ("Header"), NULL),
    C ("mcstruct-hex", "offset", N_ ("Offset column"), NULL),
    C ("mcstruct-hex", "mark", N_ ("Marked range"), N_ ("The bytes of the field under the cursor")),
    C ("mcstruct-hex", "changed", N_ ("Changed byte"), NULL),
    C ("mcstruct-hex", "cursor", N_ ("Cursor"), NULL),
    C ("mcstruct-hex", "frame", N_ ("Group separator"), NULL),
    C ("mcstruct-hex", "block", N_ ("Selected block"), NULL),
};

static const skinedit_table_row_t rows_mcstruct_def[] = {
    C ("mcstruct-def", "_default_", N_ ("Normal text"), NULL),
    C ("mcstruct-def", "frame", N_ ("Frame"), NULL),
    C ("mcstruct-def", "frame-active", N_ ("Frame, active"), NULL),
    C ("mcstruct-def", "head", N_ ("Header"), NULL),
    C ("mcstruct-def", "selected", N_ ("Selected row"), NULL),
    C ("mcstruct-def", "lineno", N_ ("Line number"), NULL),
    C ("mcstruct-def", "directive", N_ ("Directive"), NULL),
    C ("mcstruct-def", "comment", N_ ("Comment"), NULL),
    C ("mcstruct-def", "label", N_ ("Label"), NULL),
};

static const skinedit_table_row_t rows_lines[] = {
    CH ("lines", "horiz", N_ ("Light: horizontal"), NULL, "\xe2\x94\x80"),
    CH ("lines", "vert", N_ ("Light: vertical"), NULL, "\xe2\x94\x82"),
    CH ("lines", "lefttop", N_ ("Light: top left corner"), NULL, "\xe2\x94\x8c"),
    CH ("lines", "righttop", N_ ("Light: top right corner"), NULL, "\xe2\x94\x90"),
    CH ("lines", "leftbottom", N_ ("Light: bottom left corner"), NULL, "\xe2\x94\x94"),
    CH ("lines", "rightbottom", N_ ("Light: bottom right corner"), NULL, "\xe2\x94\x98"),
    CH ("lines", "topmiddle", N_ ("Light: top tee"), NULL, "\xe2\x94\xac"),
    CH ("lines", "bottommiddle", N_ ("Light: bottom tee"), NULL, "\xe2\x94\xb4"),
    CH ("lines", "leftmiddle", N_ ("Light: left tee"), NULL, "\xe2\x94\x9c"),
    CH ("lines", "rightmiddle", N_ ("Light: right tee"), NULL, "\xe2\x94\xa4"),
    CH ("lines", "cross", N_ ("Light: cross"), NULL, "\xe2\x94\xbc"),
    CH ("lines", "dhoriz", N_ ("Heavy: horizontal"), N_ ("Heavy frames are used for major boxes"),
        "\xe2\x95\x90"),
    CH ("lines", "dvert", N_ ("Heavy: vertical"), NULL, "\xe2\x95\x91"),
    CH ("lines", "dlefttop", N_ ("Heavy: top left corner"), NULL, "\xe2\x95\x94"),
    CH ("lines", "drighttop", N_ ("Heavy: top right corner"), NULL, "\xe2\x95\x97"),
    CH ("lines", "dleftbottom", N_ ("Heavy: bottom left corner"), NULL, "\xe2\x95\x9a"),
    CH ("lines", "drightbottom", N_ ("Heavy: bottom right corner"), NULL, "\xe2\x95\x9d"),
    CH ("lines", "dtopmiddle", N_ ("Heavy: top tee"), N_ ("The short stem is light"), "\xe2\x95\xa4"),
    CH ("lines", "dbottommiddle", N_ ("Heavy: bottom tee"), NULL, "\xe2\x95\xa7"),
    CH ("lines", "dleftmiddle", N_ ("Heavy: left tee"), NULL, "\xe2\x95\x9f"),
    CH ("lines", "drightmiddle", N_ ("Heavy: right tee"), NULL, "\xe2\x95\xa2"),
};

static const skinedit_table_row_t rows_panel_marks[] = {
    CH ("widget-panel", "sort-up-char", N_ ("Sort ascending"), N_ ("In the header row"), "'"),
    CH ("widget-panel", "sort-down-char", N_ ("Sort descending"), N_ ("In the header row"), "."),
    CH ("widget-panel", "hiddenfiles-show-char", N_ ("Hidden files shown"), N_ ("In the top border"), "."),
    CH ("widget-panel", "hiddenfiles-hide-char", N_ ("Hidden files hidden"), N_ ("In the top border"), "."),
    CH ("widget-panel", "history-prev-item-char", N_ ("History: previous"), N_ ("In the top border"), "<"),
    CH ("widget-panel", "history-next-item-char", N_ ("History: next"), N_ ("In the top border"), ">"),
    CH ("widget-panel", "history-show-list-char", N_ ("History: list"), N_ ("In the top border"), "^"),
    CH ("widget-panel", "filename-scroll-left-char", N_ ("File name scrolled, left"),
        N_ ("A long file name that continues to the left"), "{"),
    CH ("widget-panel", "filename-scroll-right-char", N_ ("File name scrolled, right"),
        N_ ("A long file name that continues to the right"), "}"),
};

static const skinedit_table_row_t rows_other_marks[] = {
    CH ("widget-find", "show-matches-char", N_ ("Find: show matches"), NULL, "+"),
    CH ("widget-editor", "window-state-char", N_ ("Editor: window state"),
        N_ ("In the title of an editor window"), "*"),
    CH ("widget-editor", "window-close-char", N_ ("Editor: window close"),
        N_ ("In the title of an editor window"), "X"),
    CH ("widget-editor", "fold-open-char", N_ ("Editor: fold open"), NULL, "v"),
    CH ("widget-editor", "fold-close-char", N_ ("Editor: fold closed"), NULL, ">"),
};

static const skinedit_table_row_t rows_scrollbar[] = {
    CH ("scrollbar", "vstart", N_ ("Vertical: top arrow"), NULL, "^"),
    CH ("scrollbar", "vend", N_ ("Vertical: bottom arrow"), NULL, "v"),
    CH ("scrollbar", "vtrack", N_ ("Vertical: track"), N_ ("Default: vert of the lines"), "\xe2\x94\x82"),
    CH ("scrollbar", "vthumb", N_ ("Vertical: thumb"), NULL, "\xe2\x96\x88"),
    ST ("scrollbar", "vthumblen", N_ ("Vertical: thumb length"),
        N_ ("Cells of the thumb when it fits, else half the track"), "6"),
    CH ("scrollbar", "hstart", N_ ("Horizontal: left arrow"), NULL, "<"),
    CH ("scrollbar", "hend", N_ ("Horizontal: right arrow"), NULL, ">"),
    CH ("scrollbar", "htrack", N_ ("Horizontal: track"), N_ ("Default: horiz of the lines"), "\xe2\x94\x80"),
    CH ("scrollbar", "hthumb", N_ ("Horizontal: thumb"), NULL, "\xe2\x96\xa0"),
    ST ("scrollbar", "hthumblen", N_ ("Horizontal: thumb length"),
        N_ ("Cells of the thumb when it fits, else half the track"), "4"),
};

static const skinedit_table_row_t rows_spinner[] = {
    ST ("core", "spinner_sequence", N_ ("Spinner frames"),
        N_ ("One frame per character, played while mc is busy"), "|/-\\"),
};

/* clang-format on */

/*** public variables ****************************************************************************/

const skinedit_table_section_t skinedit_table[] = {
    SECTION (N_ ("Panels"), rows_panels),
    SECTION (N_ ("File types"), rows_filetypes),
    SECTION (N_ ("Input lines"), rows_input),
    SECTION (N_ ("Dialogs"), rows_dialog),
    SECTION (N_ ("Error dialogs"), rows_error),
    SECTION (N_ ("Menu bar"), rows_menu),
    SECTION (N_ ("Popup menu"), rows_popupmenu),
    SECTION (N_ ("Button bar"), rows_buttonbar),
    SECTION (N_ ("Status bar"), rows_statusbar),
    SECTION (N_ ("Help"), rows_help),
    SECTION (N_ ("Editor"), rows_editor),
    SECTION (N_ ("Viewer"), rows_viewer),
    SECTION (N_ ("Terminal"), rows_mcterm),
    SECTION (N_ ("Diff viewer"), rows_diffviewer),
    SECTION (N_ ("Structured view"), rows_mctree),
    SECTION (N_ ("Struct look: tree"), rows_mcstruct_tree),
    SECTION (N_ ("Struct look: hex"), rows_mcstruct_hex),
    SECTION (N_ ("Struct look: def file"), rows_mcstruct_def),
    SECTION (N_ ("Frames"), rows_lines),
    SECTION (N_ ("Panel marks"), rows_panel_marks),
    SECTION (N_ ("Find and editor marks"), rows_other_marks),
    SECTION (N_ ("Scrollbar"), rows_scrollbar),
    SECTION (N_ ("Spinner"), rows_spinner),
};

const size_t skinedit_table_count = G_N_ELEMENTS (skinedit_table);

/* --------------------------------------------------------------------------------------------- */
