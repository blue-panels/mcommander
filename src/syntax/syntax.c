/*
   Lexical scanner for the M-Commander
   Reads mc's .syntax files and tells what every byte of a text is

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

/** \file syntax.c
 *  \brief Source: lexical scanner driven by mc's .syntax files
 *
 *  Grown out of the editor's highlighting, which was its only consumer for
 *  thirty years.  See syntax.h for what the three objects are and why.
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/search.h"
#include "lib/strutil.h"
#include "lib/util.h"

#include "syntax.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* bytes between checkpoints */
#define SYNTAX_MARKER_DENSITY 512

#define RULE_ON_LEFT_BORDER   1
#define RULE_ON_RIGHT_BORDER  2

#define SYNTAX_TOKEN_STAR     '\001'
#define SYNTAX_TOKEN_PLUS     '\002'
#define SYNTAX_TOKEN_BRACKET  '\003'
#define SYNTAX_TOKEN_BRACE    '\004'

#define SYNTAX_KEYWORD(x)     ((syntax_keyword_t *) (x))
#define CONTEXT_RULE(x)       ((context_rule_t *) (x))

#define ARGS_LEN              1024

/* how deep one rule set can be embedded in another; deeper, a body is left plain */
#define SYNTAX_EMBED_DEPTH_MAX 4

/* color 0 is "whatever an uncolored byte gets"; the table starts at 1 */
#define SYNTAX_COLOR_NONE 0

/*** file scope type declarations ****************************************************************/

/**
 * A set of bytes a rule tests one byte against: what counts as part of a word.
 *
 * The .syntax file spells it as a string, and the automaton used to walk that
 * string with strchr() for every byte of the text.  It is a table now, built
 * once when the rules are read.  A set is shared by every rule that asks for
 * the same bytes, so a rule set holds one or two of them.
 */
typedef struct
{
    char *chars;  // the set as the file spelled it, for interning
    gboolean in_set[UCHAR_MAX + 1];
} syntax_charset_t;

typedef struct
{
    GString *keyword;
    const syntax_charset_t *whole_word_chars_left;
    const syntax_charset_t *whole_word_chars_right;
    gboolean line_start;
    guint color;
} syntax_keyword_t;

typedef struct
{
    GString *left;
    unsigned char first_left;
    GString *right;
    unsigned char first_right;
    gboolean line_start_left;
    gboolean line_start_right;
    gboolean between_delimiters;
    const syntax_charset_t *whole_word_chars_left;
    const syntax_charset_t *whole_word_chars_right;
    char *keyword_first_chars;
    /* which keywords to try for a byte: keyword_candidates[start[b] .. start[b + 1])
       are the ones that can begin with byte b, in the order the rules name them */
    guint32 *keyword_candidates;
    guint32 *keyword_candidate_start;  // UCHAR_MAX + 2 entries
    gboolean spelling;
    gboolean case_insensitive;  // of the rule set the context comes from
    // first word is word[1]
    GPtrArray *keyword;

    /* Nesting.  Every context belongs to a scope, named by the default context
       the text falls back to when the context is over: 0 at the top.  A context
       that embeds a rule set opens a scope of its own for its body, and the
       contexts of that rule set are copied in behind it. */
    guint base;   // default context of the scope this one belongs to
    guint host;   // the context whose body this scope is, 0 at the top
    guint embed;  // default context of the scope the body opens, 0 if it opens none
    /* the right delimiter ends the body only where the text is in the default
       context of the embedded rules, not in a string or a comment of theirs */
    gboolean soft_escape;
    /* on the default context of a scope: which contexts of it can start on a
       byte, context_candidates[start[b] .. start[b + 1]), in the order named */
    guint32 *context_candidates;
    guint32 *context_candidate_start;  // UCHAR_MAX + 2 entries
    // what 'embed' said, until the rule set named there is read
    char *embed_type;
    char *embed_file;
    int embed_line;
} context_rule_t;

/** A color as the .syntax file spells it, before it is interned. */
typedef struct
{
    const char *fg;
    const char *bg;
    const char *attrs;
} syntax_color_spec_t;

/** One symbolic color of a rule set: names, not an allocated pair. */
typedef struct
{
    char *fg;
    char *bg;
    char *attrs;
} syntax_color_t;

/** A resumable point in the walk.  State only, never bytes. */
struct syntax_rules_t
{
    int refs;
    char *type;
    GPtrArray *contexts;  // context_rule_t
    GTree *defines;
    GPtrArray *charsets;  // syntax_charset_t, shared by the rules that ask alike
    GArray *colors;       // syntax_color_t, index 0 unused
    gboolean case_insensitive;

    gboolean line_local;
    guint ll_number_max;
    guint ll_number_color;
    guint ll_single_quote_color;
    guint ll_double_quote_color;
    char *ll_symbols;
    guint ll_symbols_color;
};

/** The running rule, wider than what callers see: borders are ours. */
typedef struct
{
    unsigned short keyword;
    off_t end;
    unsigned short context;
    unsigned short _context;
    unsigned char border;
} syntax_rule_t;

/**
 * What one pass over one byte has found so far.
 *
 * The pass answers five questions in a fixed order - is the keyword over, is
 * the context over, does a keyword start here, does a context start here, and
 * does a keyword of the new context start here - and every answer is read by
 * the questions below it.
 */
typedef struct
{
    gboolean left;             // a border was closed here
    gboolean right;            // a right delimiter matched here
    gboolean keyword_left;     // the keyword ended here
    gboolean keyword_right;    // a keyword started here
    gboolean context_changed;  // the context is not the one the byte was entered with
    off_t end;                 // how far the longest match found here reaches
} syntax_found_t;

/** What a directive of a .syntax file says about the line it was read from. */
typedef enum
{
    SYNTAX_DIRECTIVE_OK,
    SYNTAX_DIRECTIVE_ERROR,
    SYNTAX_DIRECTIVE_END  // 'file': the rule set is over
} syntax_directive_result_t;

/** What the directives of one rule set read and write while it is parsed. */
typedef struct
{
    syntax_rules_t *rules;
    char **args;  // the line, split into words
    int argc;
    context_rule_t *context;  // the context opened last
    gboolean no_words;        // no context has been opened yet

    // what counts as a word, as the last 'wholechars' left it
    char whole_left[BUF_MEDIUM];
    char whole_right[BUF_MEDIUM];

    // the color a keyword inherits from the context it is in
    char last_fg[BUF_TINY / 2];
    char last_bg[BUF_TINY / 2];
    char last_attrs[BUF_TINY];

    // the file being read, and the one 'include' interrupted
    FILE *f;
    FILE *g;
    int line;
    int save_line;
    char **error_file;
} syntax_parser_t;

/** A resumable point in the walk.  State only, never bytes. */
typedef struct
{
    off_t offset;
    syntax_rule_t rule;
} syntax_checkpoint_t;

struct syntax_scanner_t
{
    syntax_rules_t *rules;
    syntax_get_byte_fn get_byte;
    void *data;
    off_t size;

    syntax_rule_t rule;
    gboolean fold;  // whether the rules at hand ignore case
    off_t last;     // byte the rule above describes
    GArray *index;  // syntax_checkpoint_t, ascending by offset
};

struct syntax_palette_t
{
    GArray *pairs;  // int
    syntax_color_release_fn release;
    int normal;
};

/*** forward declarations (file scope functions) *************************************************/

static void syntax_rules_free (syntax_rules_t *r);
static void destroy_defines (GTree **defines);
static int syntax_rules_load_embedded (const char *syntax_file, const char *type, int depth,
                                       syntax_rules_t **rules);

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
syntax_keyword_free (gpointer keyword)
{
    syntax_keyword_t *k = SYNTAX_KEYWORD (keyword);

    g_string_free (k->keyword, TRUE);
    g_free (k);
}

/* --------------------------------------------------------------------------------------------- */

static void
context_rule_free (gpointer rule)
{
    context_rule_t *r = CONTEXT_RULE (rule);

    g_string_free (r->left, TRUE);
    g_string_free (r->right, TRUE);
    g_free (r->keyword_first_chars);
    g_free (r->keyword_candidates);
    g_free (r->keyword_candidate_start);
    g_free (r->context_candidates);
    g_free (r->context_candidate_start);
    g_free (r->embed_type);
    g_free (r->embed_file);

    if (r->keyword != NULL)
        g_ptr_array_free (r->keyword, TRUE);

    g_free (r);
}

/* --------------------------------------------------------------------------------------------- */

static gint
mc_defines_destroy (gpointer key, gpointer value, gpointer data)
{
    (void) data;

    g_free (key);
    g_strfreev ((char **) value);

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Back to the state a fresh rule set is in.
 *
 * Used where the parser decides the rules it just read are not worth keeping:
 * a lone context with a lone keyword means the file describes nothing.
 */

static void
syntax_rules_clear (syntax_rules_t *r)
{
    guint i;

    if (r->defines != NULL)
        destroy_defines (&r->defines);
    if (r->contexts != NULL)
    {
        g_ptr_array_free (r->contexts, TRUE);
        r->contexts = NULL;
    }
    if (r->charsets != NULL)
    {
        g_ptr_array_free (r->charsets, TRUE);
        r->charsets = NULL;
    }

    for (i = 1; i < r->colors->len; i++)
    {
        syntax_color_t *c = &g_array_index (r->colors, syntax_color_t, i);

        g_free (c->fg);
        g_free (c->bg);
        g_free (c->attrs);
    }
    g_array_set_size (r->colors, 1);

    MC_PTR_FREE (r->ll_symbols);
    r->line_local = FALSE;
    r->ll_number_max = 0;
    r->ll_number_color = SYNTAX_COLOR_NONE;
    r->ll_single_quote_color = SYNTAX_COLOR_NONE;
    r->ll_double_quote_color = SYNTAX_COLOR_NONE;
    r->ll_symbols_color = SYNTAX_COLOR_NONE;
}

/* --------------------------------------------------------------------------------------------- */
/** Completely destroys the defines tree */

static void
destroy_defines (GTree **defines)
{
    g_tree_foreach (*defines, mc_defines_destroy, NULL);
    g_tree_destroy (*defines);
    *defines = NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Index of a color in the rule set's table, adding it if new.
 *
 * Kept symbolically on purpose: a rule set outlives the skin it is looked at
 * through, and is shared by consumers that draw in different ways.
 */

static void
syntax_charset_free (gpointer charset)
{
    syntax_charset_t *cs = (syntax_charset_t *) charset;

    g_free (cs->chars);
    g_free (cs);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The set of bytes @chars names, as a table to look one byte up in.
 *
 * Rules that name the same bytes get the same set back: a .syntax file says
 * 'wholechars' once and every rule below it asks for that same set.
 *
 * The byte 0 is in every set, because strchr(), which this replaces, finds it
 * at the end of any string.
 */
static const syntax_charset_t *
syntax_intern_charset (syntax_rules_t *r, const char *chars)
{
    syntax_charset_t *cs;
    const char *p;
    guint i;

    if (r->charsets == NULL)
        r->charsets = g_ptr_array_new_with_free_func (syntax_charset_free);

    for (i = 0; i < r->charsets->len; i++)
    {
        cs = (syntax_charset_t *) g_ptr_array_index (r->charsets, i);

        if (strcmp (cs->chars, chars) == 0)
            return cs;
    }

    cs = g_new0 (syntax_charset_t, 1);
    cs->chars = g_strdup (chars);
    cs->in_set[0] = TRUE;
    for (p = chars; *p != '\0'; p++)
        cs->in_set[(unsigned char) *p] = TRUE;
    g_ptr_array_add (r->charsets, cs);

    return cs;
}

/* --------------------------------------------------------------------------------------------- */

static guint
syntax_intern_color (syntax_rules_t *r, const syntax_color_spec_t *color)
{
    guint i;
    syntax_color_t c;
    char *attrs;

    if ((color->fg == NULL || *color->fg == '\0') && (color->bg == NULL || *color->bg == '\0')
        && (color->attrs == NULL || *color->attrs == '\0'))
        return SYNTAX_COLOR_NONE;

    /* get_args() mangles the + signs of an attribute list; unmangle them before
       anything else, so that what a rule set hands out is the name as the file
       spelled it and two rules that ask for the same attributes meet here */
    attrs = g_strdup (color->attrs);
    if (attrs != NULL)
    {
        char *p;

        for (p = attrs; (p = strchr (p, SYNTAX_TOKEN_PLUS)) != NULL;)
            *p++ = '+';
    }

    for (i = 1; i < r->colors->len; i++)
    {
        const syntax_color_t *e = &g_array_index (r->colors, syntax_color_t, i);

        if (g_strcmp0 (e->fg, color->fg) == 0 && g_strcmp0 (e->bg, color->bg) == 0
            && g_strcmp0 (e->attrs, attrs) == 0)
        {
            g_free (attrs);
            return i;
        }
    }

    c.fg = g_strdup (color->fg);
    c.bg = g_strdup (color->bg);
    c.attrs = attrs;
    g_array_append_val (r->colors, c);

    return r->colors->len - 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
syntax_rules_free (syntax_rules_t *r)
{
    guint i;

    if (r->defines != NULL)
        destroy_defines (&r->defines);
    if (r->contexts != NULL)
        g_ptr_array_free (r->contexts, TRUE);
    if (r->charsets != NULL)
        g_ptr_array_free (r->charsets, TRUE);

    for (i = 0; i < r->colors->len; i++)
    {
        syntax_color_t *c = &g_array_index (r->colors, syntax_color_t, i);

        g_free (c->fg);
        g_free (c->bg);
        g_free (c->attrs);
    }
    g_array_free (r->colors, TRUE);

    g_free (r->ll_symbols);
    g_free (r->type);
    g_free (r);
}

/* --------------------------------------------------------------------------------------------- */

static syntax_rules_t *
syntax_rules_new (void)
{
    syntax_rules_t *r;
    syntax_color_t none = { NULL, NULL, NULL };

    r = g_new0 (syntax_rules_t, 1);
    r->refs = 1;
    r->colors = g_array_new (FALSE, FALSE, sizeof (syntax_color_t));
    g_array_append_val (r->colors, none);  // index 0 is "no color"
    r->ll_number_color = SYNTAX_COLOR_NONE;
    r->ll_single_quote_color = SYNTAX_COLOR_NONE;
    r->ll_double_quote_color = SYNTAX_COLOR_NONE;
    r->ll_symbols_color = SYNTAX_COLOR_NONE;

    return r;
}

/* --------------------------------------------------------------------------------------------- */

/** Wrapper for case insensitive mode */
inline static int
xx_tolower (gboolean case_insensitive, int c)
{
    return case_insensitive ? tolower (c) : c;
}

/* --------------------------------------------------------------------------------------------- */

/** The byte at @i as the automaton reads it: folded when the rules at hand ignore case. */
inline static int
get_byte_folded (const syntax_scanner_t *sc, off_t i)
{
    return xx_tolower (sc->fold, sc->get_byte (sc->data, i));
}

/* --------------------------------------------------------------------------------------------- */

static void
subst_defines (GTree *defines, char **argv, char **argv_end)
{
    for (; *argv != NULL && argv < argv_end; argv++)
    {
        char **t;

        t = g_tree_lookup (defines, *argv);
        if (t != NULL)
        {
            int argc, count;
            char **p;

            // Count argv array members
            argc = g_strv_length (argv + 1);

            // Count members of definition array
            count = g_strv_length (t);

            p = argv + count + argc;
            // Buffer overflow or infinitive loop in define
            if (p >= argv_end)
                break;

            // Move rest of argv after definition members
            while (argc >= 0)
                *p-- = argv[argc-- + 1];

            // Copy definition members to argv
            for (p = argv; *t != NULL; *p++ = *t++)
                ;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Can a rule that wants @whole_left on its left, or the start of a line, begin
 * on the byte that follows @prev?
 *
 * This is the first thing compare_word_to_right() asks, and the answer is the
 * same for every rule that wants the same border, so the loop that tries the
 * keywords of a context asks it before it calls: more than half of the calls
 * used to end right here.
 */
inline static gboolean
border_allows_start (const syntax_charset_t *whole_left, gboolean line_start, int prev)
{
    if (line_start && prev != '\n')
        return FALSE;

    return whole_left == NULL || !whole_left->in_set[(unsigned char) prev];
}

/* --------------------------------------------------------------------------------------------- */

/** Is @c one of the bytes listed at @p, up to the closing @token? */
static gboolean
in_char_set (const unsigned char *p, int c, unsigned char token)
{
    for (; *p != token && *p != '\0'; p++)
        if (c == (int) *p)
            return TRUE;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * '*' in a pattern: any run of bytes up to the one the pattern asks for next,
 * never across a line break.
 *
 * Every match_* below takes the token at @pp and the byte at @ii and leaves
 * both on the last byte the token ate, for the loop of compare_word_to_right()
 * to step over.
 */
static gboolean
match_star (const syntax_scanner_t *sc, const syntax_charset_t *whole_right,
            const unsigned char **pp, off_t *ii)
{
    const unsigned char *p = *pp + 1;
    off_t i = *ii;

    while (TRUE)
    {
        int c;

        c = get_byte_folded (sc, i);
        if (*p == '\0' && whole_right != NULL && !whole_right->in_set[(unsigned char) c])
            break;
        if (c == *p)
            break;
        if (c == '\n')
            return FALSE;
        i++;
    }

    *pp = p;
    *ii = i;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/** '+' in a pattern: a run of bytes that are part of a word.  Empty will do. */
static gboolean
match_plus (const syntax_scanner_t *sc, const GString *text, const syntax_charset_t *whole_right,
            const unsigned char **pp, off_t *ii)
{
    const unsigned char *p = *pp + 1;
    off_t i = *ii;
    off_t j = 0;

    while (TRUE)
    {
        int c;

        c = get_byte_folded (sc, i);
        if (c == *p)
        {
            j = i;
            if (p[0] == text->str[0] && p[1] == '\0')  // handle eg '+' and @+@ keywords properly
                break;
        }
        if (j != 0
            && strchr ((const char *) p + 1, c) != NULL)  // c exists further down, matched later
            break;
        if (whiteness (c) || (whole_right != NULL && !whole_right->in_set[(unsigned char) c]))
        {
            if (*p == '\0')
            {
                i--;
                break;
            }
            if (j == 0)
                return FALSE;
            i = j;
            break;
        }
        i++;
    }

    *pp = p;
    *ii = i;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/** '[abc]' in a pattern: a run of bytes out of the set. */
static gboolean
match_bracket (const syntax_scanner_t *sc, const unsigned char **pp, const unsigned char *q,
               off_t *ii)
{
    const unsigned char *p = *pp + 1;
    off_t i = *ii;
    int c = -1, d;

    while (TRUE)
    {
        d = c;
        c = get_byte_folded (sc, i);
        if (!in_char_set (p, c, SYNTAX_TOKEN_BRACKET))
            break;
        i++;
    }
    i--;

    while (*p != SYNTAX_TOKEN_BRACKET && p <= q)
        p++;
    if (p > q)
        return FALSE;
    // the last byte of the set is what the pattern asks for next: give it back
    if (p[1] == d)
        i--;

    *pp = p;
    *ii = i;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/** '{abc}' in a pattern: one byte out of the set. */
static gboolean
match_brace (const syntax_scanner_t *sc, const unsigned char **pp, const unsigned char *q, off_t i)
{
    const unsigned char *p = *pp + 1;

    if (!in_char_set (p, get_byte_folded (sc, i), SYNTAX_TOKEN_BRACE))
        return FALSE;

    while (*p != SYNTAX_TOKEN_BRACE && p < q)
        p++;

    *pp = p;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * How far the pattern @text matches the bytes from @i to the right.
 *
 * @return the byte after the match, or -1 if the pattern does not match
 */
static off_t
compare_word_to_right (const syntax_scanner_t *sc, off_t i, int prev, const GString *text,
                       const syntax_charset_t *whole_left, const syntax_charset_t *whole_right,
                       gboolean line_start)
{
    const unsigned char *p, *q;

    if (!border_allows_start (whole_left, line_start, prev))
        return -1;

    for (p = (const unsigned char *) text->str, q = p + text->len; p < q; p++, i++)
    {
        gboolean ok;

        switch (*p)
        {
        case SYNTAX_TOKEN_STAR:
            ok = match_star (sc, whole_right, &p, &i);
            break;
        case SYNTAX_TOKEN_PLUS:
            ok = match_plus (sc, text, whole_right, &p, &i);
            break;
        case SYNTAX_TOKEN_BRACKET:
            ok = match_bracket (sc, &p, q, &i);
            break;
        case SYNTAX_TOKEN_BRACE:
            ok = match_brace (sc, &p, q, i);
            break;
        default:
            ok = (*p == get_byte_folded (sc, i));
            break;
        }

        if (!ok)
            return -1;
    }

    if (whole_right == NULL)
        return i;

    return whole_right->in_set[(unsigned char) get_byte_folded (sc, i)] ? -1 : i;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Turn on the keyword of the current context that starts at byte @i, if there
 * is one.
 *
 * @param c the byte at @i, folded
 * @param end how far the longest match found at @i reaches
 * @param stop_newline_overflow hand the line break back to the context when the
 *        keyword and the context both end with one
 * @return TRUE when a keyword was turned on
 */
static gboolean
try_keyword (const syntax_scanner_t *sc, off_t i, int c, int prev, syntax_rule_t *rule, off_t *end,
             gboolean stop_newline_overflow)
{
    const context_rule_t *r;
    guint n, last;

    r = CONTEXT_RULE (g_ptr_array_index (sc->rules->contexts, rule->context));
    if (r->keyword_candidates == NULL)
        return FALSE;

    last = r->keyword_candidate_start[(unsigned char) c + 1];
    for (n = r->keyword_candidate_start[(unsigned char) c]; n < last; n++)
    {
        const syntax_keyword_t *k;
        guint count;
        off_t e = -1;

        count = r->keyword_candidates[n];
        k = SYNTAX_KEYWORD (g_ptr_array_index (r->keyword, count));
        if (k->keyword != NULL
            && border_allows_start (k->whole_word_chars_left, k->line_start, prev))
            e = compare_word_to_right (sc, i, prev, k->keyword, k->whole_word_chars_left,
                                       k->whole_word_chars_right, k->line_start);
        if (e > 0)
        {
            /* when both context and keyword terminate with a newline,
               the context overflows to the next line and colorizes it incorrectly */
            if (stop_newline_overflow && e > i + 1 && rule->_context != 0
                && k->keyword->str[k->keyword->len - 1] == '\n')
            {
                const context_rule_t *rc;

                rc = CONTEXT_RULE (g_ptr_array_index (sc->rules->contexts, rule->_context));
                if (rc->right != NULL && rc->right->len != 0
                    && rc->right->str[rc->right->len - 1] == '\n')
                    e--;
            }

            *end = e;
            rule->end = e;
            rule->keyword = count;
            return TRUE;
        }
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/** The context numbered @n of the rule set the scanner walks over. */
inline static context_rule_t *
context_of (const syntax_scanner_t *sc, guint n)
{
    return CONTEXT_RULE (g_ptr_array_index (sc->rules->contexts, n));
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The byte at @i and the one before it, read the way the rules of context @n
 * want them.  The rule sets embedded in one another need not agree on case.
 */
static int
read_bytes_for (syntax_scanner_t *sc, guint n, off_t i, int *prev)
{
    sc->fold = context_of (sc, n)->case_insensitive;
    *prev = get_byte_folded (sc, i - 1);

    return get_byte_folded (sc, i);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Does the body of an embedded rule set end at @i?
 *
 * The right delimiter of the context that embeds the rules ends the body
 * wherever it turns up, in a string or a comment of those rules as well: a
 * "</script>" inside a string of a script ends the script all the same.  When
 * the rules embed some more of their own, every body around is asked, the
 * innermost first: a rule set embedded in itself closes its own parenthesis
 * before the one around it.
 *
 * A soft delimiter ends the body only from the default context of the rules
 * it embeds: the "}}" in {{ "}}" }} is part of a string.
 *
 * @param end how far the delimiter found reaches
 * @return the context whose body ends at @i, 0 if none does
 */
static guint
find_escape (syntax_scanner_t *sc, guint context, off_t i, off_t *end)
{
    guint h;
    gboolean inside = FALSE;  // in a context of the rules that h embeds

    for (h = context_of (sc, context)->host; h != 0; h = context_of (sc, h)->host)
    {
        const context_rule_t *r = context_of (sc, h);
        int c, prev;
        off_t e;

        inside = inside || context != r->embed;
        if (r->soft_escape && inside)
            continue;

        c = read_bytes_for (sc, h, i, &prev);
        if (r->first_right == c && r->right->len != 0
            && (e = compare_word_to_right (sc, i, prev, r->right, r->whole_word_chars_left,
                                           r->whole_word_chars_right, r->line_start_right))
                > 0)
        {
            *end = e;
            return h;
        }
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Can the right delimiter of @r end it on the byte at hand?
 *
 * Not while it is being ended already.  Nor, for a context that embeds a rule
 * set, while its left delimiter lasts: the right one ends the body, and there is
 * none yet.  "python -c '" ends with the quote that ends the code as well.
 */
inline static gboolean
right_can_match (const syntax_scanner_t *sc, const context_rule_t *r)
{
    if ((sc->rule.border & RULE_ON_RIGHT_BORDER) != 0)
        return FALSE;

    return r->embed == 0 || (sc->rule.border & RULE_ON_LEFT_BORDER) == 0;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Does the right delimiter of the context @rule is in start at @i?  Asked the
 * way the part of apply_rules_going_right() that turns a context off asks it.
 */
static gboolean
context_ends_here (const syntax_scanner_t *sc, const syntax_rule_t *rule, off_t i, int c, int prev)
{
    const context_rule_t *r = context_of (sc, rule->context);

    return rule->context != r->base && rule->keyword == 0 && r->first_right == c
        && right_can_match (sc, r) && r->right->len != 0
        && compare_word_to_right (sc, i, prev, r->right, r->whole_word_chars_left,
                                  r->whole_word_chars_right, r->line_start_right)
        > 0;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Leave the body of an embedded rule set if it ends at @i.
 *
 * @return the byte at @i as the rules the text is in now read it, @prev the
 *         one before; @c and @prev as they were when the body goes on
 */
static int
leave_body (syntax_scanner_t *sc, syntax_rule_t *rule, syntax_found_t *found, off_t i, int c,
            int *prev)
{
    const context_rule_t *r;
    guint escape;
    off_t e;

    if (context_of (sc, rule->context)->host == 0)
        return c;

    escape = find_escape (sc, rule->context, i, &e);
    if (escape != 0)
    {
        r = context_of (sc, escape);
        rule->context = r->between_delimiters ? r->base : escape;
        rule->keyword = 0;
        rule->end = e;
        rule->border = RULE_ON_RIGHT_BORDER;
        found->left = TRUE;
        found->right = TRUE;
    }

    return read_bytes_for (sc, rule->context, i, prev);
}

/* --------------------------------------------------------------------------------------------- */

static void
apply_rules_going_right (syntax_scanner_t *sc, off_t i)
{
    context_rule_t *r;
    int c, prev;
    syntax_found_t found = { FALSE, FALSE, FALSE, FALSE, FALSE, 0 };
    gboolean is_end;
    syntax_rule_t _rule = sc->rule;
    guint base;

    if (sc->get_byte (sc->data, i) == 0)
        return;

    /* the byte before is what every rule that starts here is tested against:
       read it once, not once per rule */
    c = read_bytes_for (sc, _rule.context, i, &prev);
    is_end = (sc->rule.end == i);

    // check to turn off a keyword
    if (_rule.keyword != 0)
    {
        if (prev == '\n')
            _rule.keyword = 0;
        if (is_end)
        {
            _rule.keyword = 0;
            found.keyword_left = TRUE;
        }
    }

    /* the left delimiter of a context that embeds a rule set is over: from here
       on the body is read by the rules it embeds */
    if (is_end && (sc->rule.border & RULE_ON_LEFT_BORDER) != 0
        && context_of (sc, _rule._context)->embed != 0)
    {
        _rule.context = context_of (sc, _rule._context)->embed;
        _rule.keyword = 0;
        _rule.border = 0;
        found.left = TRUE;
        c = read_bytes_for (sc, _rule.context, i, &prev);
    }

    /* check to leave the body of an embedded rule set, unless the context the
       text is in ends here by itself: that one is inner still */
    if (!context_ends_here (sc, &_rule, i, c, prev))
        c = leave_body (sc, &_rule, &found, i, c, &prev);

    // check to turn off a context
    base = context_of (sc, _rule.context)->base;
    if (!found.right && _rule.context != base && _rule.keyword == 0)
    {
        off_t e;

        r = context_of (sc, _rule.context);
        if (r->first_right == c && right_can_match (sc, r) && r->right->len != 0
            && (e = compare_word_to_right (sc, i, prev, r->right, r->whole_word_chars_left,
                                           r->whole_word_chars_right, r->line_start_right))
                > 0)
        {
            _rule.end = e;
            found.right = TRUE;
            _rule.border = RULE_ON_RIGHT_BORDER;
            if (r->between_delimiters)
                _rule.context = base;
        }
        else if (is_end && (sc->rule.border & RULE_ON_RIGHT_BORDER) != 0)
        {
            // always turn off a context at 4
            found.left = TRUE;
            _rule.border = 0;
            if (!found.keyword_left)
                _rule.context = base;
        }
        else if (is_end && (sc->rule.border & RULE_ON_LEFT_BORDER) != 0)
        {
            // never turn off a context at 2
            found.left = TRUE;
            _rule.border = 0;
        }
    }

    /* a context of the embedded rules that ended here leaves the text in their
       default context, where a soft delimiter counts */
    if (!found.right && _rule.context == base)
        c = leave_body (sc, &_rule, &found, i, c, &prev);
    base = context_of (sc, _rule.context)->base;

    // check to turn on a keyword
    if (_rule.keyword == 0)
        found.keyword_right = try_keyword (sc, i, c, prev, &_rule, &found.end, TRUE);

    // check to turn on a context
    if (_rule.context == base)
    {
        if (!found.left && is_end)
        {
            if ((sc->rule.border & RULE_ON_RIGHT_BORDER) != 0)
            {
                _rule.border = 0;
                _rule.context = base;
                found.context_changed = TRUE;
                _rule.keyword = 0;
            }
            else if ((sc->rule.border & RULE_ON_LEFT_BORDER) != 0)
            {
                r = context_of (sc, _rule._context);
                _rule.border = 0;
                if (r->between_delimiters)
                {
                    _rule.context = _rule._context;
                    found.context_changed = TRUE;
                    _rule.keyword = 0;

                    if (r->first_right == c)
                    {
                        off_t e = -1;

                        if (r->right->len != 0)
                            e = compare_word_to_right (
                                sc, i, prev, r->right, r->whole_word_chars_left,
                                r->whole_word_chars_right, r->line_start_right);
                        if (e >= found.end)
                        {
                            _rule.end = e;
                            found.right = TRUE;
                            _rule.border = RULE_ON_RIGHT_BORDER;
                            _rule.context = base;
                        }
                    }
                }
            }
        }

        /* an exclusive context that has just been entered keeps its first
           byte: no other context starts on it */
        if (!found.right && _rule.context == base)
        {
            const context_rule_t *scope = context_of (sc, base);
            guint n, last;

            last = scope->context_candidate_start[(unsigned char) c + 1];
            for (n = scope->context_candidate_start[(unsigned char) c]; n < last; n++)
            {
                const guint count = scope->context_candidates[n];
                off_t e = -1;

                r = context_of (sc, count);
                if (r->left->len != 0)
                    e = compare_word_to_right (sc, i, prev, r->left, r->whole_word_chars_left,
                                               r->whole_word_chars_right, r->line_start_left);
                if (e >= found.end && (_rule.keyword == 0 || found.keyword_right))
                {
                    _rule.end = e;
                    _rule.border = RULE_ON_LEFT_BORDER;
                    _rule._context = count;
                    if (!r->between_delimiters && _rule.keyword == 0)
                    {
                        _rule.context = count;
                        found.context_changed = TRUE;
                    }
                    break;
                }
            }
        }
    }

    /* check again to turn on a keyword if the context switched.  The guard
       against a keyword and a context that both end with a line break is not
       applied here; a keyword that starts on the byte the context starts on
       keeps the break.  Pinned by test_newline_keyword_at_context_start. */
    if (found.context_changed && _rule.keyword == 0)
        (void) try_keyword (sc, i, c, prev, &_rule, &found.end, FALSE);

    sc->rule = _rule;
}

/* --------------------------------------------------------------------------------------------- */
/**
   Returns 0 on error/eof or a count of the number of bytes read
   including the newline. Result must be free'd.
   In case of an error, *line will not be modified.
 */

static size_t
read_one_line (char **line, FILE *f)
{
    GString *p;
    size_t r = 0;

    // not reallocate string too often
    p = g_string_sized_new (64);

    while (TRUE)
    {
        int c;

        c = fgetc (f);
        if (c == EOF)
        {
            if (ferror (f))
            {
                if (errno == EINTR)
                    continue;
                r = 0;
            }
            break;
        }
        r++;

        // handle all of \r\n, \r, \n correctly.
        if (c == '\n')
            break;
        if (c == '\r')
        {
            c = fgetc (f);
            if (c == '\n')
                r++;
            else
                ungetc (c, f);
            break;
        }

        g_string_append_c (p, c);
    }
    if (r != 0)
        *line = g_string_free (p, FALSE);
    else
        g_string_free (p, TRUE);

    return r;
}

/* --------------------------------------------------------------------------------------------- */

static char *
convert (char *s)
{
    char *r, *p;

    p = r = s;
    while (*s)
    {
        switch (*s)
        {
        case '\\':
            s++;
            switch (*s)
            {
            case ' ':
                *p = ' ';
                s--;
                break;
            case 'n':
                *p = '\n';
                break;
            case 'r':
                *p = '\r';
                break;
            case 't':
                *p = '\t';
                break;
            case 's':
                *p = ' ';
                break;
            case '*':
                *p = '*';
                break;
            case '\\':
                *p = '\\';
                break;
            case '[':
            case ']':
                *p = SYNTAX_TOKEN_BRACKET;
                break;
            case '{':
            case '}':
                *p = SYNTAX_TOKEN_BRACE;
                break;
            case 0:
                *p = *s;
                return r;
            default:
                *p = *s;
                break;
            }
            break;
        case '*':
            *p = SYNTAX_TOKEN_STAR;
            break;
        case '+':
            *p = SYNTAX_TOKEN_PLUS;
            break;
        default:
            *p = *s;
            break;
        }
        s++;
        p++;
    }
    *p = '\0';
    return r;
}

/* --------------------------------------------------------------------------------------------- */

static int
get_args (char *l, char **args, int args_size)
{
    int argc = 0;

    while (argc < args_size)
    {
        char *p = l;

        while (*p != '\0' && whiteness (*p))
            p++;
        if (*p == '\0')
            break;
        for (l = p + 1; *l != '\0' && !whiteness (*l); l++)
            ;
        if (*l != '\0')
            *l++ = '\0';
        args[argc++] = convert (p);
    }
    args[argc] = (char *) NULL;
    return argc;
}

/* --------------------------------------------------------------------------------------------- */

/** The up to three color words that end a line: foreground, background, attributes. */
static void
read_color_spec (syntax_color_spec_t *color, char ***args)
{
    char **a = *args;

    color->fg = *a;
    if (*a != NULL)
        a++;
    color->bg = *a;
    if (*a != NULL)
        a++;
    color->attrs = *a;
    if (*a != NULL)
        a++;
    *args = a;
}

/* --------------------------------------------------------------------------------------------- */

static int
read_line_local_color (syntax_rules_t *r, char ***args)
{
    syntax_color_spec_t color;

    if (**args == NULL)
        return -1;

    read_color_spec (&color, args);

    return (int) syntax_intern_color (r, &color);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Open a file named by an 'include' line.  @error_file is left holding the path
 * tried last: while an included file is open it names the file being read, and
 * that is the name handed out when the parser chokes on it.
 */
static FILE *
open_include_file (const char *filename, char **error_file)
{
    FILE *f;

    g_free (*error_file);
    *error_file = g_strdup (filename);
    if (g_path_is_absolute (filename))
        return fopen (filename, "r");

    g_free (*error_file);
    *error_file =
        g_build_filename (mc_config_get_data_path (), EDIT_SYNTAX_DIR, filename, (char *) NULL);
    f = fopen (*error_file, "r");
    if (f != NULL)
        return f;

    g_free (*error_file);
    *error_file =
        g_build_filename (mc_global.share_data_dir, EDIT_SYNTAX_DIR, filename, (char *) NULL);

    return fopen (*error_file, "r");
}

/* --------------------------------------------------------------------------------------------- */

inline static void
xx_lowerize_line (gboolean case_insensitive, char *line, size_t len)
{
    if (case_insensitive)
    {
        size_t i;

        for (i = 0; i < len; ++i)
            line[i] = tolower (line[i]);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** the directives of a .syntax file ************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * The optional 'whole', 'wholeleft' or 'wholeright' in front of a pattern: which
 * side of it has to fall on a word border.
 */
static void
read_whole_word_chars (syntax_parser_t *p, char ***args, const syntax_charset_t **left,
                       const syntax_charset_t **right)
{
    char **a = *args;

    if (strcmp (*a, "whole") == 0)
    {
        a++;
        *left = syntax_intern_charset (p->rules, p->whole_left);
        *right = syntax_intern_charset (p->rules, p->whole_right);
    }
    else if (strcmp (*a, "wholeleft") == 0)
    {
        a++;
        *left = syntax_intern_charset (p->rules, p->whole_left);
    }
    else if (strcmp (*a, "wholeright") == 0)
    {
        a++;
        *right = syntax_intern_charset (p->rules, p->whole_right);
    }

    *args = a;
}

/* --------------------------------------------------------------------------------------------- */

/** line-local: the rules of this set keep no state between lines. */
static syntax_directive_result_t
directive_line_local (syntax_parser_t *p)
{
    if (p->argc != 1 || p->rules->contexts->len != 0)
        return SYNTAX_DIRECTIVE_ERROR;

    p->rules->line_local = TRUE;

    return SYNTAX_DIRECTIVE_OK;
}

/* --------------------------------------------------------------------------------------------- */

/** number <max> <color>: line-local rules color numbers up to <max>. */
static syntax_directive_result_t
directive_number (syntax_parser_t *p)
{
    syntax_rules_t *r = p->rules;
    char **a = p->args + 1;
    char *end;
    guint64 max;
    int syntax_color;

    if (!r->line_local || *a == NULL)
        return SYNTAX_DIRECTIVE_ERROR;

    errno = 0;
    max = g_ascii_strtoull (*a++, &end, 10);
    if (errno != 0 || *end != '\0' || max == 0 || max > G_MAXUINT)
        return SYNTAX_DIRECTIVE_ERROR;

    syntax_color = read_line_local_color (r, &a);
    if (syntax_color < 0)
        return SYNTAX_DIRECTIVE_ERROR;

    r->ll_number_max = (guint) max;
    r->ll_number_color = syntax_color;

    return *a == NULL ? SYNTAX_DIRECTIVE_OK : SYNTAX_DIRECTIVE_ERROR;
}

/* --------------------------------------------------------------------------------------------- */

/** string <quote> <color>: line-local rules color what stands between quotes. */
static syntax_directive_result_t
directive_string (syntax_parser_t *p)
{
    syntax_rules_t *r = p->rules;
    char **a = p->args + 1;
    char quote_char;
    int syntax_color;

    if (!r->line_local || *a == NULL || (*a)[1] != '\0')
        return SYNTAX_DIRECTIVE_ERROR;
    quote_char = **a;
    if (quote_char != '\'' && quote_char != '"')
        return SYNTAX_DIRECTIVE_ERROR;
    a++;

    syntax_color = read_line_local_color (r, &a);
    if (syntax_color < 0)
        return SYNTAX_DIRECTIVE_ERROR;

    if (quote_char == '\'')
        r->ll_single_quote_color = syntax_color;
    else
        r->ll_double_quote_color = syntax_color;

    return *a == NULL ? SYNTAX_DIRECTIVE_OK : SYNTAX_DIRECTIVE_ERROR;
}

/* --------------------------------------------------------------------------------------------- */

/** symbols <chars> <color>: line-local rules color these bytes wherever they stand. */
static syntax_directive_result_t
directive_symbols (syntax_parser_t *p)
{
    syntax_rules_t *r = p->rules;
    char **a = p->args + 1;
    int syntax_color;

    if (!r->line_local || *a == NULL || **a == '\0')
        return SYNTAX_DIRECTIVE_ERROR;
    MC_PTR_FREE (r->ll_symbols);
    r->ll_symbols = g_strdup (*a++);

    syntax_color = read_line_local_color (r, &a);
    if (syntax_color < 0)
        return SYNTAX_DIRECTIVE_ERROR;

    r->ll_symbols_color = syntax_color;

    return *a == NULL ? SYNTAX_DIRECTIVE_OK : SYNTAX_DIRECTIVE_ERROR;
}

/* --------------------------------------------------------------------------------------------- */

/** include <file>: go on reading the rules there.  One level deep, no nesting. */
static syntax_directive_result_t
directive_include (syntax_parser_t *p)
{
    if (p->g != NULL || p->argc != 2)
        return SYNTAX_DIRECTIVE_ERROR;

    p->g = p->f;
    p->f = open_include_file (p->args[1], p->error_file);
    if (p->f == NULL)
    {
        MC_PTR_FREE (*p->error_file);
        return SYNTAX_DIRECTIVE_ERROR;
    }
    p->save_line = p->line;
    p->line = 0;

    return SYNTAX_DIRECTIVE_OK;
}

/* --------------------------------------------------------------------------------------------- */

/** caseinsensitive: fold every byte of the text and of the rules. */
static syntax_directive_result_t
directive_caseinsensitive (syntax_parser_t *p)
{
    p->rules->case_insensitive = TRUE;

    return SYNTAX_DIRECTIVE_OK;
}

/* --------------------------------------------------------------------------------------------- */

/** wholechars [left|right] <chars>: what counts as a word from here on. */
static syntax_directive_result_t
directive_wholechars (syntax_parser_t *p)
{
    char **a = p->args + 1;

    if (*a == NULL)
        return SYNTAX_DIRECTIVE_ERROR;

    if (strcmp (*a, "left") == 0)
    {
        a++;
        g_strlcpy (p->whole_left, *a, sizeof (p->whole_left));
    }
    else if (strcmp (*a, "right") == 0)
    {
        a++;
        g_strlcpy (p->whole_right, *a, sizeof (p->whole_right));
    }
    else
    {
        g_strlcpy (p->whole_left, *a, sizeof (p->whole_left));
        g_strlcpy (p->whole_right, *a, sizeof (p->whole_right));
    }
    a++;

    return *a == NULL ? SYNTAX_DIRECTIVE_OK : SYNTAX_DIRECTIVE_ERROR;
}

/* --------------------------------------------------------------------------------------------- */

/** context [exclusive] [whole...] [linestart] <left> [linestart] <right> [colors] */
static syntax_directive_result_t
directive_context (syntax_parser_t *p)
{
    syntax_rules_t *r = p->rules;
    char **a = p->args + 1;
    syntax_color_spec_t color;
    context_rule_t *c;
    syntax_keyword_t *k;

    if (r->line_local)
        return SYNTAX_DIRECTIVE_ERROR;
    if (*a == NULL)
        return SYNTAX_DIRECTIVE_ERROR;

    if (r->contexts->len == 0)
    {
        // first context is the default
        if (strcmp (*a, "default") != 0)
            return SYNTAX_DIRECTIVE_ERROR;

        a++;
        c = g_new0 (context_rule_t, 1);
        g_ptr_array_add (r->contexts, c);
        c->left = g_string_new (" ");
        c->right = g_string_new (" ");
    }
    else
    {
        // Start new context.
        c = g_new0 (context_rule_t, 1);
        g_ptr_array_add (r->contexts, c);
        if (strcmp (*a, "exclusive") == 0)
        {
            a++;
            c->between_delimiters = TRUE;
        }
        if (*a == NULL)
            return SYNTAX_DIRECTIVE_ERROR;
        read_whole_word_chars (p, &a, &c->whole_word_chars_left, &c->whole_word_chars_right);
        if (*a == NULL)
            return SYNTAX_DIRECTIVE_ERROR;
        if (strcmp (*a, "linestart") == 0)
        {
            a++;
            c->line_start_left = TRUE;
        }
        if (*a == NULL)
            return SYNTAX_DIRECTIVE_ERROR;
        c->left = g_string_new (*a++);
        if (*a == NULL)
            return SYNTAX_DIRECTIVE_ERROR;
        if (strcmp (*a, "linestart") == 0)
        {
            a++;
            c->line_start_right = TRUE;
        }
        if (*a == NULL)
            return SYNTAX_DIRECTIVE_ERROR;
        c->right = g_string_new (*a++);
        c->first_left = c->left->str[0];
        c->first_right = c->right->str[0];
    }

    c->keyword = g_ptr_array_new_with_free_func (syntax_keyword_free);
    k = g_new0 (syntax_keyword_t, 1);
    g_ptr_array_add (c->keyword, k);

    p->context = c;
    p->no_words = FALSE;

    subst_defines (r->defines, a, &p->args[ARGS_LEN]);
    read_color_spec (&color, &a);
    // a keyword of this context that names no color of its own takes this one
    g_strlcpy (p->last_fg, color.fg != NULL ? color.fg : "", sizeof (p->last_fg));
    g_strlcpy (p->last_bg, color.bg != NULL ? color.bg : "", sizeof (p->last_bg));
    g_strlcpy (p->last_attrs, color.attrs != NULL ? color.attrs : "", sizeof (p->last_attrs));
    k->color = syntax_intern_color (r, &color);
    k->keyword = g_string_new (" ");

    return *a == NULL ? SYNTAX_DIRECTIVE_OK : SYNTAX_DIRECTIVE_ERROR;
}

/* --------------------------------------------------------------------------------------------- */

/** spellcheck: the words of this context are run past the spell checker. */
static syntax_directive_result_t
directive_spellcheck (syntax_parser_t *p)
{
    if (p->rules->line_local || p->context == NULL)
        return SYNTAX_DIRECTIVE_ERROR;

    p->context->spelling = TRUE;

    return SYNTAX_DIRECTIVE_OK;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * embed [soft] <type>: what stands between the delimiters of the context opened
 * last is read by the rules of <type>, until the right delimiter turns up.
 * Soft, it turns up only outside the strings and comments of those rules.
 */
static syntax_directive_result_t
directive_embed (syntax_parser_t *p)
{
    context_rule_t *c = p->context;
    char **a = p->args + 1;

    // the default context has no delimiters to embed between
    if (p->rules->line_local || c == NULL || p->rules->contexts->len < 2 || c->embed_type != NULL)
        return SYNTAX_DIRECTIVE_ERROR;

    if (*a != NULL && strcmp (*a, "soft") == 0)
    {
        a++;
        c->soft_escape = TRUE;
    }
    if (*a == NULL || a[1] != NULL)
        return SYNTAX_DIRECTIVE_ERROR;

    c->embed_type = g_strdup (*a);
    c->embed_file = g_strdup (*p->error_file);
    c->embed_line = p->line;

    return SYNTAX_DIRECTIVE_OK;
}

/* --------------------------------------------------------------------------------------------- */

/** keyword [whole...] [linestart] <word> [colors] */
static syntax_directive_result_t
directive_keyword (syntax_parser_t *p)
{
    syntax_rules_t *r = p->rules;
    char **a = p->args + 1;
    syntax_color_spec_t color;
    context_rule_t *last_rule;
    syntax_keyword_t *k;

    if (r->line_local || p->no_words)
        return SYNTAX_DIRECTIVE_ERROR;
    if (*a == NULL)
        return SYNTAX_DIRECTIVE_ERROR;

    last_rule = CONTEXT_RULE (g_ptr_array_index (r->contexts, r->contexts->len - 1));
    k = g_new0 (syntax_keyword_t, 1);
    g_ptr_array_add (last_rule->keyword, k);

    read_whole_word_chars (p, &a, &k->whole_word_chars_left, &k->whole_word_chars_right);
    if (*a == NULL)
        return SYNTAX_DIRECTIVE_ERROR;
    if (strcmp (*a, "linestart") == 0)
    {
        a++;
        k->line_start = TRUE;
    }
    if (*a == NULL)
        return SYNTAX_DIRECTIVE_ERROR;
    if (strcmp (*a, "whole") == 0)
        return SYNTAX_DIRECTIVE_ERROR;

    k->keyword = g_string_new (*a++);
    subst_defines (r->defines, a, &p->args[ARGS_LEN]);
    read_color_spec (&color, &a);
    if (color.fg == NULL)
        color.fg = p->last_fg;
    if (color.bg == NULL)
        color.bg = p->last_bg;
    if (color.attrs == NULL)
        color.attrs = p->last_attrs;
    k->color = syntax_intern_color (r, &color);

    return *a == NULL ? SYNTAX_DIRECTIVE_OK : SYNTAX_DIRECTIVE_ERROR;
}

/* --------------------------------------------------------------------------------------------- */

/** file: the next rule set starts here, this one is done. */
static syntax_directive_result_t
directive_file (syntax_parser_t *p)
{
    (void) p;

    return SYNTAX_DIRECTIVE_END;
}

/* --------------------------------------------------------------------------------------------- */

/** define <name> <words...>: one word of a rule stands for several. */
static syntax_directive_result_t
directive_define (syntax_parser_t *p)
{
    syntax_rules_t *r = p->rules;
    char **a = p->args + 1;
    char *key = *a++;
    char **argv;

    if (p->argc < 3)
        return SYNTAX_DIRECTIVE_ERROR;

    argv = g_tree_lookup (r->defines, key);
    if (argv != NULL)
        mc_defines_destroy (NULL, argv, NULL);
    else
        key = g_strdup (key);

    argv = g_new (char *, p->argc - 1);
    g_tree_insert (r->defines, key, argv);
    while (*a != NULL)
        *argv++ = g_strdup (*a++);
    *argv = NULL;

    return SYNTAX_DIRECTIVE_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const struct
{
    const char *name;
    syntax_directive_result_t (*handler) (syntax_parser_t *p);
} syntax_directives[] = {
    { "line-local", directive_line_local }, { "number", directive_number },
    { "string", directive_string },         { "symbols", directive_symbols },
    { "include", directive_include },       { "caseinsensitive", directive_caseinsensitive },
    { "wholechars", directive_wholechars }, { "context", directive_context },
    { "spellcheck", directive_spellcheck }, { "embed", directive_embed },
    { "keyword", directive_keyword },       { "file", directive_file },
    { "define", directive_define },
};

/* --------------------------------------------------------------------------------------------- */

/** Run the directive the line begins with.  An unknown word is an error. */
static syntax_directive_result_t
run_directive (syntax_parser_t *p)
{
    size_t n;

    // an empty line and a comment say nothing
    if (p->args[0] == NULL || p->args[0][0] == '#')
        return SYNTAX_DIRECTIVE_OK;

    for (n = 0; n < G_N_ELEMENTS (syntax_directives); n++)
        if (strcmp (p->args[0], syntax_directives[n].name) == 0)
            return syntax_directives[n].handler (p);

    return SYNTAX_DIRECTIVE_ERROR;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The bytes the pattern @text can begin with, marked in @set.
 *
 * @return FALSE when the answer is any byte at all, and @set says nothing
 */
static gboolean
pattern_first_bytes (const GString *text, gboolean case_insensitive, guchar *set)
{
    const unsigned char *p = (const unsigned char *) text->str;
    const unsigned char *q = p + text->len;

    for (; p < q; p++)
        switch (*p)
        {
        case SYNTAX_TOKEN_STAR:
        case SYNTAX_TOKEN_PLUS:
            // both run over bytes of any kind, and both are happy with none
            return FALSE;

        case SYNTAX_TOKEN_BRACKET:
            /* a run out of the set, and an empty run will do, so whatever
               stands after the set can be the first byte as well */
            for (p++; p < q && *p != SYNTAX_TOKEN_BRACKET; p++)
                set[*p] = TRUE;
            break;

        case SYNTAX_TOKEN_BRACE:
            // exactly one byte out of the set, so the set is the answer
            for (p++; p < q && *p != SYNTAX_TOKEN_BRACE; p++)
                set[*p] = TRUE;
            return TRUE;

        default:
            set[xx_tolower (case_insensitive, *p)] = TRUE;
            return TRUE;
        }

    // nothing of the pattern is left: it is happy anywhere
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Which keywords of @c can begin with which byte.
 *
 * The scanner used to walk the string of first bytes for every byte of the
 * text, about seventy steps a byte. The walk is done once here instead, and
 * what it would have found is kept.
 *
 * Two rules of that walk are kept with it: it starts past the first byte, which
 * is a marker and no keyword, and it ends at the first NUL, so a keyword that is
 * an empty word hides the ones named after it.
 *
 * The third rule, that a keyword beginning with a pattern token is tried
 * whatever the byte is, is asked properly instead: '{abc}' wants one byte out of
 * its set and nothing else will do, '[abc]' wants that set or whatever follows
 * it, and only '*' and '+' are really happy with any byte.
 */
static void
build_keyword_candidates (context_rule_t *c, gboolean case_insensitive)
{
    GArray *cand;
    guchar *sets;
    guint b;
    size_t j, n;

    // an empty word hides every word named after it, as it did for the walk
    for (n = 1; n < c->keyword->len && c->keyword_first_chars[n] != '\0'; n++)
        ;

    sets = g_new0 (guchar, n * (UCHAR_MAX + 2));
    for (j = 1; j < n; j++)
    {
        const syntax_keyword_t *k = SYNTAX_KEYWORD (g_ptr_array_index (c->keyword, j));
        guchar *set = sets + j * (UCHAR_MAX + 2);

        // the last slot of a set says "any byte will do"
        set[UCHAR_MAX + 1] = !pattern_first_bytes (k->keyword, case_insensitive, set);
    }

    cand = g_array_new (FALSE, FALSE, sizeof (guint32));
    c->keyword_candidate_start = g_new (guint32, UCHAR_MAX + 2);

    for (b = 0; b <= UCHAR_MAX; b++)
    {
        c->keyword_candidate_start[b] = cand->len;

        for (j = 1; j < n; j++)
        {
            const guchar *set = sets + j * (UCHAR_MAX + 2);

            if (set[UCHAR_MAX + 1] || set[b])
            {
                guint32 idx = (guint32) j;

                g_array_append_val (cand, idx);
            }
        }
    }
    c->keyword_candidate_start[UCHAR_MAX + 1] = cand->len;

    g_free (sets);
    c->keyword_candidates = (guint32 *) g_array_free (cand, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/** The first byte of every keyword of every context, for the scanner to sieve by. */
static void
collect_keyword_first_chars (syntax_rules_t *r)
{
    GString *first_chars;
    size_t i;

    first_chars = g_string_sized_new (32);

    for (i = 0; i < r->contexts->len; i++)
    {
        context_rule_t *c;
        size_t j;

        g_string_set_size (first_chars, 0);
        c = CONTEXT_RULE (g_ptr_array_index (r->contexts, i));

        g_string_append_c (first_chars, (char) 1);
        for (j = 1; j < c->keyword->len; j++)
        {
            syntax_keyword_t *k;

            k = SYNTAX_KEYWORD (g_ptr_array_index (c->keyword, j));
            g_string_append_c (first_chars, k->keyword->str[0]);
        }

        c->keyword_first_chars = g_strndup (first_chars->str, first_chars->len);
        build_keyword_candidates (c, r->case_insensitive);
    }

    g_string_free (first_chars, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/** @cs of another rule set, as the same bytes in @r. */
static const syntax_charset_t *
syntax_reintern_charset (syntax_rules_t *r, const syntax_charset_t *cs)
{
    return cs == NULL ? NULL : syntax_intern_charset (r, cs->chars);
}

/* --------------------------------------------------------------------------------------------- */

/** @color of @from, as a color of @r. */
static guint
syntax_reintern_color (syntax_rules_t *r, const syntax_rules_t *from, guint color)
{
    syntax_color_spec_t spec;

    syntax_rules_color_spec (from, color, &spec.fg, &spec.bg, &spec.attrs);

    return syntax_intern_color (r, &spec);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * A copy of the context @from of the rule set @from_rules, to be kept in @r.
 *
 * Colors and word borders are the rule set's, so they are asked for again in
 * @r.  Where the context stands among the others is left to the caller.
 */
static context_rule_t *
context_rule_copy (syntax_rules_t *r, const syntax_rules_t *from_rules, const context_rule_t *from)
{
    context_rule_t *c;
    guint j, n;

    c = g_new0 (context_rule_t, 1);
    c->left = g_string_new_len (from->left->str, from->left->len);
    c->first_left = from->first_left;
    c->right = g_string_new_len (from->right->str, from->right->len);
    c->first_right = from->first_right;
    c->line_start_left = from->line_start_left;
    c->line_start_right = from->line_start_right;
    c->between_delimiters = from->between_delimiters;
    c->whole_word_chars_left = syntax_reintern_charset (r, from->whole_word_chars_left);
    c->whole_word_chars_right = syntax_reintern_charset (r, from->whole_word_chars_right);
    c->spelling = from->spelling;
    c->case_insensitive = from->case_insensitive;

    c->keyword = g_ptr_array_new_with_free_func (syntax_keyword_free);
    for (j = 0; j < from->keyword->len; j++)
    {
        const syntax_keyword_t *fk = SYNTAX_KEYWORD (g_ptr_array_index (from->keyword, j));
        syntax_keyword_t *k;

        k = g_new0 (syntax_keyword_t, 1);
        k->keyword = g_string_new_len (fk->keyword->str, fk->keyword->len);
        k->whole_word_chars_left = syntax_reintern_charset (r, fk->whole_word_chars_left);
        k->whole_word_chars_right = syntax_reintern_charset (r, fk->whole_word_chars_right);
        k->line_start = fk->line_start;
        k->color = syntax_reintern_color (r, from_rules, fk->color);
        g_ptr_array_add (c->keyword, k);
    }

    // the keywords keep their order, so which of them to try for a byte holds as it is
    if (from->keyword_candidates != NULL)
    {
        n = from->keyword_candidate_start[UCHAR_MAX + 1];
        c->keyword_candidates = g_new (guint32, MAX (n, 1));
        memcpy (c->keyword_candidates, from->keyword_candidates, n * sizeof (guint32));
        c->keyword_candidate_start = g_new (guint32, UCHAR_MAX + 2);
        memcpy (c->keyword_candidate_start, from->keyword_candidate_start,
                (UCHAR_MAX + 2) * sizeof (guint32));
    }

    return c;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Copy the contexts of @child in behind those of @r, as the body of the
 * context @host.
 *
 * @child has had its own embeds copied in already, so what is copied is a tree
 * of scopes, and only its numbers move: the top scope of @child becomes the one
 * @host opens.
 *
 * @return FALSE when the contexts would no longer fit in the state
 */
static gboolean
syntax_splice_embedded (syntax_rules_t *r, guint host, const syntax_rules_t *child)
{
    const guint offset = r->contexts->len;
    guint j;

    if ((guint64) offset + child->contexts->len > G_MAXUSHORT)
        return FALSE;

    for (j = 0; j < child->contexts->len; j++)
    {
        const context_rule_t *from = CONTEXT_RULE (g_ptr_array_index (child->contexts, j));
        context_rule_t *c;

        c = context_rule_copy (r, child, from);
        c->base = from->base + offset;
        c->host = from->host == 0 ? host : from->host + offset;
        c->embed = from->embed == 0 ? 0 : from->embed + offset;
        g_ptr_array_add (r->contexts, c);
    }

    CONTEXT_RULE (g_ptr_array_index (r->contexts, host))->embed = offset;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Can the body of @c be read in the scope @other opens for its own?
 *
 * The scope knows one context around it, the one whose right delimiter ends it
 * and which the text is in while that delimiter lasts.  Two contexts that end
 * alike and look alike there can share it, and the rules are copied in once:
 * a fence named "sh" and one named "bash" read the same rules to the same end.
 */
static gboolean
syntax_embed_shareable (const context_rule_t *c, const context_rule_t *other)
{
    const syntax_keyword_t *k, *ok;

    if (other->embed == 0 || g_ascii_strcasecmp (c->embed_type, other->embed_type) != 0)
        return FALSE;
    if (c->base != other->base || c->between_delimiters != other->between_delimiters
        || c->soft_escape != other->soft_escape || c->line_start_right != other->line_start_right
        || c->whole_word_chars_left != other->whole_word_chars_left
        || c->whole_word_chars_right != other->whole_word_chars_right
        || !g_string_equal (c->right, other->right))
        return FALSE;

    // no keywords of their own, which could start on the right delimiter
    if (c->keyword->len != 1 || other->keyword->len != 1)
        return FALSE;
    k = SYNTAX_KEYWORD (g_ptr_array_index (c->keyword, 0));
    ok = SYNTAX_KEYWORD (g_ptr_array_index (other->keyword, 0));

    return k->color == ok->color;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read the rule sets the contexts of @r embed, and copy them in.
 *
 * @param error_file set to the file of the 'embed' line at fault
 * @return 0 on success, otherwise the line of the 'embed' that could not be kept
 */
static int
syntax_resolve_embeds (syntax_rules_t *r, const char *syntax_file, int depth, char **error_file)
{
    const guint n = r->contexts->len;  // the ones copied in have theirs done
    guint i, j;

    for (i = 1; i < n; i++)
    {
        context_rule_t *c = CONTEXT_RULE (g_ptr_array_index (r->contexts, i));
        syntax_rules_t *child = NULL;
        gboolean ok;

        if (c->embed_type == NULL)
            continue;

        // a rule set that embeds itself, or one too deep down: the body stays plain
        if (depth >= SYNTAX_EMBED_DEPTH_MAX)
            continue;

        for (j = 1; j < i; j++)
        {
            const context_rule_t *other = CONTEXT_RULE (g_ptr_array_index (r->contexts, j));

            if (syntax_embed_shareable (c, other))
            {
                c->embed = other->embed;
                break;
            }
        }
        if (c->embed != 0)
            continue;

        ok = syntax_rules_load_embedded (syntax_file, c->embed_type, depth + 1, &child) == 0
            && child->contexts != NULL && syntax_splice_embedded (r, i, child);
        syntax_rules_unref (child);

        if (!ok)
        {
            g_free (*error_file);
            *error_file = g_strdup (c->embed_file);
            return c->embed_line;
        }
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Which contexts of every scope can start on which byte.
 *
 * The scanner used to try every context of the rule set on every byte; with
 * rule sets copied into one another there are too many of them for that, and
 * most belong to a scope the text is not in anyway.
 */
static void
build_context_candidates (syntax_rules_t *r)
{
    const guint n = r->contexts->len;
    guint s;

    for (s = 0; s < n; s++)
    {
        context_rule_t *scope = CONTEXT_RULE (g_ptr_array_index (r->contexts, s));
        guint32 *start;
        guint32 fill[UCHAR_MAX + 1];
        guint j, b;

        if (scope->base != s)
            continue;

        // count the contexts of the scope by first byte, then lay them out in order
        start = g_new0 (guint32, UCHAR_MAX + 2);
        for (j = 1; j < n; j++)
        {
            const context_rule_t *c = CONTEXT_RULE (g_ptr_array_index (r->contexts, j));

            if (j != s && c->base == s)
                start[c->first_left + 1]++;
        }
        for (b = 0; b <= UCHAR_MAX; b++)
        {
            start[b + 1] += start[b];
            fill[b] = start[b];
        }

        scope->context_candidates = g_new (guint32, MAX (start[UCHAR_MAX + 1], 1));
        for (j = 1; j < n; j++)
        {
            const context_rule_t *c = CONTEXT_RULE (g_ptr_array_index (r->contexts, j));

            if (j != s && c->base == s)
                scope->context_candidates[fill[c->first_left]++] = j;
        }
        scope->context_candidate_start = start;
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read the rules of one set, from the 'file' line already read up to the next
 * one, or the whole of an included file.
 *
 * @param syntax_file the Syntax file, where the rule sets named by 'embed' are
 * @param depth how deep in other rule sets this one is embedded
 * @param error_file holds the name of the included file being read, NULL while
 *        the parser is in the Syntax file itself; freed by the caller
 * @return 0 on success, otherwise the line the parser choked on
 */

static int
edit_read_syntax_rules (syntax_rules_t *r, FILE *f, char **args, int args_size,
                        const char *syntax_file, int depth, char **error_file)
{
    syntax_parser_t p;
    char *l = NULL;
    int result = 0;

    memset (&p, 0, sizeof (p));
    p.rules = r;
    p.args = args;
    p.no_words = TRUE;
    p.f = f;
    p.error_file = error_file;

    strcpy (p.whole_left, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");
    strcpy (p.whole_right, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");

    args[0] = NULL;
    r->case_insensitive = FALSE;
    r->line_local = FALSE;
    r->ll_number_max = 0;
    r->ll_number_color = SYNTAX_COLOR_NONE;
    r->ll_single_quote_color = SYNTAX_COLOR_NONE;
    r->ll_double_quote_color = SYNTAX_COLOR_NONE;
    MC_PTR_FREE (r->ll_symbols);
    r->ll_symbols_color = SYNTAX_COLOR_NONE;

    /* a set of rules can be read twice into the same object: an 'include' above
       the first 'file' line reads one, the 'file' line that matches reads the
       one that is kept */
    if (r->contexts != NULL)
        g_ptr_array_free (r->contexts, TRUE);
    r->contexts = g_ptr_array_new_with_free_func (context_rule_free);

    if (r->defines == NULL)
        r->defines = g_tree_new ((GCompareFunc) strcmp);

    while (TRUE)
    {
        size_t len;
        syntax_directive_result_t res;

        p.line++;
        l = NULL;

        len = read_one_line (&l, p.f);
        if (len == 0)
        {
            // the included file is over: go on where 'include' left off
            if (p.g == NULL)
                break;

            fclose (p.f);
            p.f = p.g;
            p.g = NULL;
            p.line = p.save_line + 1;
            MC_PTR_FREE (*error_file);
            MC_PTR_FREE (l);
            len = read_one_line (&l, p.f);
            if (len == 0)
                break;
        }

        xx_lowerize_line (r->case_insensitive, l, len);
        p.argc = get_args (l, args, args_size);

        res = run_directive (&p);
        if (res == SYNTAX_DIRECTIVE_ERROR)
        {
            result = p.line;
            break;
        }
        if (res == SYNTAX_DIRECTIVE_END)
            break;

        MC_PTR_FREE (l);
    }
    MC_PTR_FREE (l);

    // the parser stopped inside an included file: that file is ours to close
    if (p.g != NULL && p.f != NULL)
        fclose (p.f);

    if (r->contexts->len == 0)
    {
        g_ptr_array_free (r->contexts, TRUE);
        r->contexts = NULL;
    }

    if (result != 0)
        return result;

    if (r->contexts == NULL)
        return r->line_local ? 0 : p.line;

    collect_keyword_first_chars (r);

    {
        guint i;

        for (i = 0; i < r->contexts->len; i++)
            CONTEXT_RULE (g_ptr_array_index (r->contexts, i))->case_insensitive =
                r->case_insensitive;
    }

    result = syntax_resolve_embeds (r, syntax_file, depth, error_file);
    if (result != 0)
        return result;

    build_context_candidates (r);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/** Open the Syntax file, falling back to the one shipped with the program. */
static FILE *
open_syntax_file (const char *syntax_file)
{
    FILE *f;
    char *global_syntax_file;

    f = fopen (syntax_file, "r");
    if (f != NULL)
        return f;

    global_syntax_file =
        g_build_filename (mc_global.share_data_dir, EDIT_SYNTAX_FILE, (char *) NULL);
    f = fopen (global_syntax_file, "r");
    g_free (global_syntax_file);

    return f;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Does this 'file' line describe the rule set the caller asked for?
 *
 * Two ways to ask: by name, and by regular expressions on the name of the file
 * being edited and on its first line.
 */
static gboolean
syntax_file_line_selected (const syntax_rules_t *r, char **args, const char *editor_file,
                           const char *first_line, const char *type, int depth)
{
    /* rule set was explicitly specified by the caller.  An 'embed' names it
       in any case: the file it stands in may ignore case, and fold its lines */
    if (type != NULL)
        return depth == 0 ? strcmp (type, args[2]) == 0 : g_ascii_strcasecmp (type, args[2]) == 0;

    if (editor_file == NULL || r == NULL)
        return FALSE;

    // does filename match arg 1 ?
    if (mc_search (args[1], NULL, editor_file, MC_SEARCH_T_REGEX))
        return TRUE;

    // does first line match arg 3 ?
    return args[3] != NULL && mc_search (args[3], NULL, first_line, MC_SEARCH_T_REGEX);
}

/* --------------------------------------------------------------------------------------------- */

/** A set of one empty context colors nothing; highlighting is turned off for speed. */
static gboolean
syntax_rules_are_empty (const syntax_rules_t *r)
{
    const context_rule_t *r0;

    if (r->contexts == NULL || r->contexts->len != 1)
        return FALSE;

    r0 = CONTEXT_RULE (g_ptr_array_index (r->contexts, 0));

    return r0->keyword->len == 1 && !r0->spelling;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Walk the Syntax file and read the rules of the set it selects.
 *
 * Three ways to ask, most specific first: @pnames collects the name of every
 * set and reads none, @type names the set wanted, and without either the set is
 * guessed from @editor_file and @first_line.
 *
 * @param depth how deep in other rule sets the one read is embedded
 * @param error_file name of the included file at fault, if any; freed by the caller
 * @return 0 on success, -1 if no Syntax file could be opened, otherwise the
 *         line the parser choked on
 */
static int
edit_read_syntax_file (syntax_rules_t *r, GPtrArray *pnames, const char *syntax_file,
                       const char *editor_file, const char *first_line, const char *type, int depth,
                       char **error_file)
{
    FILE *f;
    char *args[ARGS_LEN], *l = NULL;
    long line = 0;
    int result = 0;
    gboolean found = FALSE;

    f = open_syntax_file (syntax_file);
    if (f == NULL)
        return -1;

    args[0] = NULL;
    while (TRUE)
    {
        FILE *g = NULL;
        const char *syntax_type;
        int line_error;

        line++;
        MC_PTR_FREE (l);
        if (read_one_line (&l, f) == 0)
            break;
        (void) get_args (l, args, ARGS_LEN - 1);  // Final NULL
        if (args[0] == NULL)
            continue;

        // Looking for 'include ...' lines before first 'file ...' ones
        if (!found && strcmp (args[0], "include") == 0)
        {
            if (args[1] == NULL || (g = open_include_file (args[1], error_file)) == NULL)
            {
                result = line;
                break;
            }
        }
        else
        {
            // looking for 'file ...' lines only
            if (strcmp (args[0], "file") != 0)
                continue;

            found = TRUE;

            // must have two args or report error
            if (args[1] == NULL || args[2] == NULL)
            {
                result = line;
                break;
            }

            if (pnames != NULL)
            {
                // just collecting a list of names of rule sets
                g_ptr_array_add (pnames, g_strdup (args[2]));
                continue;
            }

            if (!syntax_file_line_selected (r, args, editor_file, first_line, type, depth))
                continue;
        }

        /* The rules of the set are read from here on.  args[] is refilled by the
           parser, so the name of the set has to be kept before the call; it
           points into l, which the parser does not touch. */
        syntax_type = args[2];
        line_error = edit_read_syntax_rules (r, g != NULL ? g : f, args, ARGS_LEN - 1, syntax_file,
                                             depth, error_file);
        if (line_error != 0)
        {
            // an included file counts its own lines, the Syntax file continues ours
            result = *error_file != NULL ? line_error : line + line_error;
        }
        else
        {
            g_free (r->type);
            r->type = g_strdup (syntax_type);

            // if there are no rules then turn off syntax highlighting for speed
            if (g == NULL && syntax_rules_are_empty (r))
            {
                syntax_rules_clear (r);
                break;
            }
        }

        if (g == NULL)
            break;

        fclose (g);
    }

    g_free (l);
    fclose (f);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_line_local_reset (syntax_line_local_state_t *state, off_t line_start)
{
    state->number_end = line_start;
    state->quote = '\0';
    state->quote_escaped = FALSE;
    state->number_overflow = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

guint
syntax_line_local_color (const syntax_rules_t *r, syntax_line_local_state_t *state,
                         syntax_get_byte_fn get_byte, void *data, off_t byte_index)
{
    const int c = get_byte (data, byte_index);

    if (c == '\n')
    {
        state->quote = '\0';
        state->quote_escaped = FALSE;
        state->number_end = byte_index;
        state->number_overflow = FALSE;
        return SYNTAX_COLOR_NONE;
    }

    if (state->number_overflow)
    {
        if (g_ascii_isdigit (c))
            return SYNTAX_COLOR_NONE;
        state->number_overflow = FALSE;
    }

    if (state->quote != '\0')
    {
        const guint color =
            state->quote == '\'' ? r->ll_single_quote_color : r->ll_double_quote_color;

        if (c == '\\' && !state->quote_escaped)
            state->quote_escaped = TRUE;
        else
        {
            if (c == state->quote && !state->quote_escaped)
                state->quote = '\0';
            state->quote_escaped = FALSE;
        }

        return color;
    }

    if (c == '\'' || c == '"')
    {
        state->quote = (unsigned char) c;
        state->quote_escaped = FALSE;
        return c == '\'' ? r->ll_single_quote_color : r->ll_double_quote_color;
    }

    if (byte_index < state->number_end)
        return r->ll_number_color;

    if (g_ascii_isdigit (c))
    {
        off_t end = byte_index;
        guint count = 0;

        do
        {
            end++;
            count++;
        }
        while (g_ascii_isdigit (get_byte (data, end)) && count <= r->ll_number_max);

        if (count <= r->ll_number_max)
        {
            state->number_end = end;
            return r->ll_number_color;
        }

        state->number_overflow = TRUE;
    }

    if (r->ll_symbols != NULL && strchr (r->ll_symbols, c) != NULL)
        return r->ll_symbols_color;

    return SYNTAX_COLOR_NONE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Newest checkpoint at or before the wanted byte, NULL if the walk starts over.
 *
 * The index is append-only and sorted, and moving backwards does not disturb it:
 * the state at an offset is a function of the bytes before it, so a checkpoint
 * stays true until the source itself changes.  (It used to be a stack that any
 * backward step popped, which made a jump back cost a rescan of everything above
 * it.)
 */

static const syntax_checkpoint_t *
syntax_checkpoint_find (const syntax_scanner_t *sc, off_t target)
{
    guint lo = 0, hi = sc->index->len;

    while (lo < hi)
    {
        const guint mid = (lo + hi) / 2;

        if (g_array_index (sc->index, syntax_checkpoint_t, mid).offset <= target)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo == 0 ? NULL : &g_array_index (sc->index, syntax_checkpoint_t, lo - 1);
}

/* --------------------------------------------------------------------------------------------- */

static void
syntax_index_truncate (syntax_scanner_t *sc, off_t from)
{
    while (sc->index->len != 0
           && g_array_index (sc->index, syntax_checkpoint_t, sc->index->len - 1).offset >= from)
        g_array_set_size (sc->index, sc->index->len - 1);
}

/* --------------------------------------------------------------------------------------------- */

static void
syntax_get_rule (syntax_scanner_t *sc, off_t byte_index)
{
    off_t i;

    if (byte_index < sc->last)
    {
        const syntax_checkpoint_t *cp;

        cp = syntax_checkpoint_find (sc, byte_index);
        if (cp != NULL)
        {
            sc->rule = cp->rule;
            sc->last = cp->offset;
        }
        else
        {
            memset (&sc->rule, 0, sizeof (sc->rule));
            sc->last = -2;  // so that the walk below starts at -1, as it does from scratch
        }
    }

    for (i = sc->last + 1; i <= byte_index; i++)
    {
        off_t d = SYNTAX_MARKER_DENSITY;

        apply_rules_going_right (sc, i);

        if (sc->index->len != 0)
            d += g_array_index (sc->index, syntax_checkpoint_t, sc->index->len - 1).offset;

        if (i > d)
        {
            syntax_checkpoint_t cp;

            cp.offset = i;
            cp.rule = sc->rule;
            g_array_append_val (sc->index, cp);
        }
    }

    sc->last = byte_index;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read the rule set an 'embed' names, @depth levels down.
 *
 * @return 0 on success; where the embedded rules are at fault is not told,
 *         the 'embed' line is what gets the blame
 */
static int
syntax_rules_load_embedded (const char *syntax_file, const char *type, int depth,
                            syntax_rules_t **rules)
{
    syntax_rules_t *r;
    char *err_file = NULL;
    int res;

    *rules = NULL;

    r = syntax_rules_new ();
    res = edit_read_syntax_file (r, NULL, syntax_file, NULL, "", type, depth, &err_file);
    g_free (err_file);

    if (res != 0 || (r->contexts == NULL && !r->line_local))
    {
        syntax_rules_free (r);
        return res != 0 ? res : -1;
    }

    *rules = r;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
syntax_rules_load (const char *syntax_file, const syntax_select_t *sel, syntax_rules_t **rules,
                   char **error_file)
{
    syntax_rules_t *r;
    char *err_file = NULL;
    int res;

    if (error_file != NULL)
        *error_file = NULL;
    *rules = NULL;

    r = syntax_rules_new ();
    res = edit_read_syntax_file (r, NULL, syntax_file, sel->filename,
                                 sel->first_line != NULL ? sel->first_line : "", sel->type, 0,
                                 &err_file);

    if (res != 0 || (r->contexts == NULL && !r->line_local))
    {
        if (error_file != NULL)
        {
            *error_file = err_file;
            err_file = NULL;
        }
        g_free (err_file);
        syntax_rules_free (r);
        return res != 0 ? res : -1;
    }

    g_free (err_file);
    *rules = r;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

int
syntax_rules_list_types (const char *syntax_file, GPtrArray *names)
{
    syntax_rules_t *r;
    char *err_file = NULL;
    int res;

    r = syntax_rules_new ();
    res = edit_read_syntax_file (r, names, syntax_file, NULL, "", NULL, 0, &err_file);
    syntax_rules_free (r);
    g_free (err_file);

    return res;
}

/* --------------------------------------------------------------------------------------------- */

syntax_rules_t *
syntax_rules_ref (syntax_rules_t *r)
{
    if (r != NULL)
        r->refs++;

    return r;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_rules_unref (syntax_rules_t *r)
{
    if (r != NULL && --r->refs == 0)
        syntax_rules_free (r);
}

/* --------------------------------------------------------------------------------------------- */

const char *
syntax_rules_type (const syntax_rules_t *r)
{
    return r == NULL ? NULL : r->type;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
syntax_rules_is_line_local (const syntax_rules_t *r)
{
    return r != NULL && r->line_local;
}

/* --------------------------------------------------------------------------------------------- */

guint
syntax_rules_color_count (const syntax_rules_t *r)
{
    return r == NULL ? 0 : r->colors->len;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_rules_color_spec (const syntax_rules_t *r, guint color, const char **fg, const char **bg,
                         const char **attrs)
{
    const syntax_color_t *c;

    *fg = NULL;
    *bg = NULL;
    *attrs = NULL;

    if (r == NULL || color >= r->colors->len)
        return;

    c = &g_array_index (r->colors, syntax_color_t, color);
    *fg = c->fg;
    *bg = c->bg;
    *attrs = c->attrs;
}

/* --------------------------------------------------------------------------------------------- */

guint
syntax_rules_color_of (const syntax_rules_t *r, syntax_state_t st)
{
    const context_rule_t *c;
    const syntax_keyword_t *k;

    if (r == NULL || r->contexts == NULL || st.context >= r->contexts->len)
        return SYNTAX_COLOR_NONE;

    c = CONTEXT_RULE (g_ptr_array_index (r->contexts, st.context));
    if (c->keyword == NULL || st.keyword >= c->keyword->len)
        return SYNTAX_COLOR_NONE;

    k = SYNTAX_KEYWORD (g_ptr_array_index (c->keyword, st.keyword));

    return k->color;
}

/* --------------------------------------------------------------------------------------------- */

syntax_scanner_t *
syntax_scanner_new (syntax_rules_t *rules, syntax_get_byte_fn get_byte, void *data, off_t size)
{
    syntax_scanner_t *sc;

    if (rules == NULL || get_byte == NULL)
        return NULL;

    sc = g_new0 (syntax_scanner_t, 1);
    sc->rules = syntax_rules_ref (rules);
    sc->get_byte = get_byte;
    sc->data = data;
    sc->size = size;
    sc->index = g_array_new (FALSE, FALSE, sizeof (syntax_checkpoint_t));
    sc->last = -2;  // nothing walked yet; the first step is byte -1

    return sc;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_scanner_free (syntax_scanner_t *sc)
{
    if (sc == NULL)
        return;

    syntax_rules_unref (sc->rules);
    g_array_free (sc->index, TRUE);
    g_free (sc);
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_scanner_set_size (syntax_scanner_t *sc, off_t size)
{
    if (sc != NULL)
        sc->size = size;
}

/* --------------------------------------------------------------------------------------------- */

const syntax_rules_t *
syntax_scanner_rules (const syntax_scanner_t *sc)
{
    return sc == NULL ? NULL : sc->rules;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_scanner_reset (syntax_scanner_t *sc)
{
    if (sc == NULL)
        return;

    memset (&sc->rule, 0, sizeof (sc->rule));
    sc->last = -2;
    g_array_set_size (sc->index, 0);
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_notify_insert (syntax_scanner_t *sc, off_t pos, gboolean inclusive)
{
    if (sc == NULL)
        return;

    if (inclusive ? sc->last >= pos : sc->last > pos)
        sc->last++;
    syntax_index_truncate (sc, pos);
    sc->size++;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_notify_delete (syntax_scanner_t *sc, off_t pos, gboolean inclusive)
{
    if (sc == NULL)
        return;

    if (inclusive ? sc->last >= pos : sc->last > pos)
        sc->last--;
    syntax_index_truncate (sc, pos);
    if (sc->size > 0)
        sc->size--;
}

/* --------------------------------------------------------------------------------------------- */

syntax_state_t
syntax_state_at (syntax_scanner_t *sc, off_t byte_index)
{
    syntax_state_t st = { 0, 0 };

    if (sc == NULL || sc->rules->contexts == NULL || byte_index >= sc->size)
        return st;

    syntax_get_rule (sc, byte_index);
    st.context = sc->rule.context;
    st.keyword = sc->rule.keyword;

    return st;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_runs_for_range (syntax_scanner_t *sc, off_t from, off_t to, GArray *runs)
{
    off_t i;
    guint cur = 0;
    guint32 len = 0;
    const GPtrArray *contexts;

    if (sc == NULL || runs == NULL || from < 0)
        return;

    contexts = sc->rules->contexts;
    if (contexts == NULL)
        return;

    if (to > sc->size)
        to = sc->size;

    for (i = from; i < to; i++)
    {
        const context_rule_t *c;
        guint color;

        syntax_get_rule (sc, i);
        c = CONTEXT_RULE (g_ptr_array_index (contexts, sc->rule.context));
        color = SYNTAX_KEYWORD (g_ptr_array_index (c->keyword, sc->rule.keyword))->color;

        if (color != cur && len != 0)
        {
            syntax_run_t r = { len, cur };

            g_array_append_val (runs, r);
            len = 0;
        }

        cur = color;
        len++;
    }

    if (len != 0)
    {
        syntax_run_t r = { len, cur };

        g_array_append_val (runs, r);
    }
}

/* --------------------------------------------------------------------------------------------- */

syntax_palette_t *
syntax_palette_new (const syntax_rules_t *rules, syntax_color_alloc_fn alloc,
                    syntax_color_release_fn release, void *backend_data, int normal)
{
    syntax_palette_t *p;
    guint i;

    if (rules == NULL || alloc == NULL)
        return NULL;

    p = g_new0 (syntax_palette_t, 1);
    p->normal = normal;
    p->release = release;
    p->pairs = g_array_sized_new (FALSE, FALSE, sizeof (int), rules->colors->len);

    for (i = 0; i < rules->colors->len; i++)
    {
        const syntax_color_t *c = &g_array_index (rules->colors, syntax_color_t, i);
        int pair;

        pair = i == SYNTAX_COLOR_NONE ? normal : alloc (backend_data, c->fg, c->bg, c->attrs);
        g_array_append_val (p->pairs, pair);
    }

    return p;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_palette_free (syntax_palette_t *p)
{
    if (p == NULL)
        return;

    if (p->release != NULL)
    {
        guint i;

        for (i = 0; i < p->pairs->len; i++)
            if (i != SYNTAX_COLOR_NONE)
                p->release (g_array_index (p->pairs, int, i));
    }

    g_array_free (p->pairs, TRUE);
    g_free (p);
}

/* --------------------------------------------------------------------------------------------- */

int
syntax_palette_get (const syntax_palette_t *p, guint color)
{
    if (p == NULL)
        return 0;
    if (color >= p->pairs->len)
        return p->normal;

    return g_array_index (p->pairs, int, color);
}

/* --------------------------------------------------------------------------------------------- */
