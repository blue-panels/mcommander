/*
   Lexical scanner for the M-Commander
   Header: rule sets, scanners and palettes

   Copyright (C) 1996-2025
   Free Software Foundation, Inc.
   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

   Written by:
   Paul Sheer, 1998
   Leonard den Ottolander <leonard den ottolander nl>, 2005, 2006
   Egmont Koblinger <egmont@gmail.com>, 2010
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2013, 2014, 2021
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the M-Commander
   a fork of GNU Midnight Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file syntax.h
 *  \brief Header: lexical scanner driven by mc's .syntax files
 *
 *  Three objects with different lifetimes:
 *
 *  - a rule set is the parsed .syntax file.  Immutable, shared, reference
 *    counted.  Its colors are kept symbolically, as names rather than allocated pairs,
 *    so one rule set can serve several consumers and survive a skin change.
 *  - a scanner is the state of one walk over one source of bytes.  The diff
 *    viewer keeps two, one per side, over a single rule set.
 *  - a palette projects a rule set's colors onto a skin and a backend.
 *
 *  The invariant everything rests on: the lexical state at X is a function of
 *  the bytes [0..X).  Hence the checkpoints, hence the sharing, hence the fact
 *  that the scanner knows nothing about screens, widgets or who is asking.
 *
 *  Color is one projection of the state, not the state itself: syntax_state_at()
 *  is the primitive, and a consumer that wants to know "am I inside a string"
 *  rather than "what color is this" asks the same function.
 */

#ifndef MC__SYNTAX_H
#define MC__SYNTAX_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/**
 * Byte source.  Must return '\n' for byte_index < 0 and for byte_index >= size:
 * the automaton reads one byte back and up to the end of the line forward, and
 * treats '\n' as the edge of the world.
 */
typedef int (*syntax_get_byte_fn) (void *data, off_t byte_index);

/** Turns one symbolic color of a rule set into whatever the consumer draws with. */
typedef int (*syntax_color_alloc_fn) (void *backend_data, const char *fg, const char *bg,
                                      const char *attrs);

/** Gives back what syntax_color_alloc_fn handed out. */
typedef void (*syntax_color_release_fn) (int color);

/*** structures declarations (and typedefs of structures)*****************************************/

/** Which rule set to load, most specific first. */
typedef struct
{
    const char *type;        // explicit type; wins when given
    const char *filename;    // else matched by name
    const char *first_line;  // and by the first line of the text
} syntax_select_t;

/**
 * Lexical state at a byte: where in the rule set we are.
 *
 * @layer is 0 for the rule set asked for.  A rule set that is a layer over
 * another leaves the bytes outside its contexts to that one, its host: there
 * the state is the host's, with @layer 1, and so on down the hosts.
 */
typedef struct
{
    unsigned short context;
    unsigned short keyword;
    unsigned short layer;
} syntax_state_t;

/** A stretch of bytes sharing one color. */
typedef struct
{
    guint32 len;
    guint32 color;  // index into the rule set's color table
} syntax_run_t;

/** Line-local rules keep no state between lines; this is all of it. */
typedef struct
{
    off_t number_end;
    unsigned char quote;
    unsigned char quote_escaped;
    unsigned char number_overflow;
} syntax_line_local_state_t;

typedef struct syntax_rules_t syntax_rules_t;
typedef struct syntax_scanner_t syntax_scanner_t;
typedef struct syntax_palette_t syntax_palette_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* ---- rule set ---- */

/**
 * Parse a Syntax file and keep the rule set it selects.
 *
 * The Syntax file installed with the program is read after @syntax_file, for
 * the file types @syntax_file does not name: the file of the user holds what
 * the user added or wants otherwise, not a copy of everything.
 *
 * Errors are returned, never shown: this layer opens no dialogs.
 *
 * @param syntax_file the Syntax file listing the rule sets
 * @param sel which rule set is wanted
 * @param rules where the rule set is stored
 * @param error_file name of the included file at fault, if any; freed by the caller
 * @return 0 on success, -1 if the file could not be opened, otherwise the line
 *         the parser choked on
 */
int syntax_rules_load (const char *syntax_file, const syntax_select_t *sel, syntax_rules_t **rules,
                       char **error_file);

syntax_rules_t *syntax_rules_ref (syntax_rules_t *r);
void syntax_rules_unref (syntax_rules_t *r);

const char *syntax_rules_type (const syntax_rules_t *r);
gboolean syntax_rules_is_line_local (const syntax_rules_t *r);
guint syntax_rules_color_count (const syntax_rules_t *r);
void syntax_rules_color_spec (const syntax_rules_t *r, guint color, const char **fg,
                              const char **bg, const char **attrs);
/** Color a state maps to; the projection that makes the scanner a highlighter. */
guint syntax_rules_color_of (const syntax_rules_t *r, syntax_state_t st);

/** Names of every type @syntax_file and the installed Syntax file describe, for a chooser. */
int syntax_rules_list_types (const char *syntax_file, GPtrArray *names);

/* ---- scanner ---- */

syntax_scanner_t *syntax_scanner_new (syntax_rules_t *rules, syntax_get_byte_fn get_byte,
                                      void *data, off_t size);
void syntax_scanner_free (syntax_scanner_t *sc);
void syntax_scanner_set_size (syntax_scanner_t *sc, off_t size);
const syntax_rules_t *syntax_scanner_rules (const syntax_scanner_t *sc);

/** Lexical state at @byte_index.  Cheap going forward, cheap enough going back. */
syntax_state_t syntax_state_at (syntax_scanner_t *sc, off_t byte_index);

/** Runs covering [@from, @to), coalesced, appended to @runs. */
void syntax_runs_for_range (syntax_scanner_t *sc, off_t from, off_t to, GArray *runs);

/** The source gained or lost a byte at @pos; @inclusive when @pos itself moved. */
void syntax_notify_insert (syntax_scanner_t *sc, off_t pos, gboolean inclusive);
void syntax_notify_delete (syntax_scanner_t *sc, off_t pos, gboolean inclusive);
/** Forget everything learned about the source. */
void syntax_scanner_reset (syntax_scanner_t *sc);

/* ---- line-local rules: the degenerate case of the invariant ---- */

void syntax_line_local_reset (syntax_line_local_state_t *st, off_t line_start);
guint syntax_line_local_color (const syntax_rules_t *r, syntax_line_local_state_t *st,
                               syntax_get_byte_fn get_byte, void *data, off_t byte_index);

/* ---- palette ---- */

/**
 * Project a rule set's colors through a backend.
 *
 * @param rules rule set whose colors are projected
 * @param alloc turns one symbolic color into something to draw with
 * @param release takes a color back as the palette goes, NULL to keep them
 * @param backend_data passed to alloc unchanged
 * @param normal what an uncolored byte gets
 * @return the palette, or NULL if there is nothing to project
 */
syntax_palette_t *syntax_palette_new (const syntax_rules_t *rules, syntax_color_alloc_fn alloc,
                                      syntax_color_release_fn release, void *backend_data,
                                      int normal);
void syntax_palette_free (syntax_palette_t *p);
int syntax_palette_get (const syntax_palette_t *p, guint color);

#endif
