/*
   Hypertext file browser for the M-Commander
   Markdown help pages

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

/** \file help_md.c
 *  \brief Source: markdown help pages
 *
 *  The help window reads the same markdown the man pages are made from.
 *  This turns it into the text the window paints: one node per heading,
 *  named by the anchor in front of it, links between the nodes, and a
 *  contents page built from the headings.
 *
 *  The markdown it understands is what maint/check-man.pl allows.
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"

#include "help.h"
#include "help_md.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define BODY_INDENT "        "

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *id;     // the name the links use
    char *title;  // what the contents page shows
    int level;    // how deep the heading was
} md_node_t;

typedef struct
{
    GString *body;     // the nodes, in the order of the page
    GString *para;     // the paragraph being collected
    GPtrArray *nodes;  // md_node_t, and NULL for a break in the contents
    char *topics;      // the heading of the contents page
    char *anchor;      // the id the next heading is to get
    gboolean skip;     // the next section stays out of the help
    gboolean skipping;
    size_t code;       // the length of the fence of the block we are in; 0 outside
    gboolean front;    // inside the front matter of a page
    gboolean term;     // the paragraph is the term of a definition
    gboolean notitle;  // the next heading is not to be printed
    gboolean open;     // a node has been started
} md_ctx_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** The name a heading is known by when it carries no anchor of its own. */

char *
help_md_node_id (const char *name)
{
    GString *id;
    char *folded;
    const char *p;
    gboolean dash = FALSE;

    folded = g_utf8_strdown (name, -1);
    id = g_string_sized_new (strlen (folded) + 1);

    for (p = folded; *p != '\0'; p = g_utf8_next_char (p))
    {
        gunichar c = g_utf8_get_char (p);

        if (g_unichar_isspace (c) || c == '-')
            dash = id->len != 0;
        else if (g_unichar_isalnum (c) || c == '_')
        {
            if (dash)
            {
                g_string_append_c (id, '-');
                dash = FALSE;
            }
            g_string_append_unichar (id, c);
        }
    }

    g_free (folded);

    return g_string_free (id, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
md_node_free (gpointer data)
{
    md_node_t *node = (md_node_t *) data;

    if (node != NULL)
    {
        g_free (node->id);
        g_free (node->title);
        g_free (node);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Text of one line, without the markdown that marks it up. */

static void
md_inline (GString *out, const char *line)
{
    const char *p = line;

    while (*p != '\0')
    {
        if (p[0] == '\\' && p[1] != '\0')
        {
            // an escaped character stands for itself
            const char *n = g_utf8_next_char (p + 1);

            g_string_append_len (out, p + 1, n - (p + 1));
            p = n;
        }
        else if (p[0] == '<' && p[1] == '!' && strncmp (p, "<!--", 4) == 0)
        {
            const char *end = strstr (p, "-->");

            p = end == NULL ? p + strlen (p) : end + 3;
        }
        else if (p[0] == '<' && strchr (p, '>') != NULL
                 && (strncmp (p, "<http", 5) == 0 || strncmp (p, "<ftp", 4) == 0))
        {
            // an autolink prints the address it holds
            const char *end = strchr (p, '>');

            g_string_append_len (out, p + 1, end - (p + 1));
            p = end + 1;
        }
        else if (p[0] == '`')
        {
            const char *fence = p;
            size_t len;
            const char *end;

            while (*p == '`')
                p++;
            len = p - fence;
            end = strstr (p, fence);
            while (end != NULL && end[len] == '`')
                end = strstr (end + 1, fence);

            if (end == NULL)
                g_string_append_len (out, fence, len);
            else
            {
                const char *text = p;
                const char *stop = end;

                // a fence wider than the text it holds keeps a space on each side
                if (text < stop && *text == ' ' && stop[-1] == ' ')
                {
                    text++;
                    stop--;
                }

                g_string_append_c (out, CHAR_FONT_BOLD);
                g_string_append_len (out, text, stop - text);
                g_string_append_c (out, CHAR_FONT_NORMAL);
                p = end + len;
            }
        }
        else if (p[0] == '[')
        {
            const char *label = p + 1;
            const char *end = strchr (label, ']');

            if (end == NULL || end[1] != '(')
            {
                g_string_append_c (out, *p);
                p++;
            }
            else
            {
                const char *target = end + 2;
                const char *close = strchr (target, ')');

                if (close == NULL)
                {
                    g_string_append_c (out, *p);
                    p++;
                }
                else
                {
                    char *text;

                    text = g_strndup (label, end - label);

                    if (*target == '#' || g_str_has_prefix (close - 3, ".md")
                        || strstr (target, ".md#") != NULL)
                    {
                        // a node of this page, or of another help file
                        g_string_append_c (out, CHAR_LINK_START);
                        md_inline (out, text);
                        g_string_append_c (out, CHAR_LINK_POINTER);
                        g_string_append_len (out, target + (*target == '#' ? 1 : 0),
                                             close - target - (*target == '#' ? 1 : 0));
                        g_string_append_c (out, CHAR_LINK_END);
                    }
                    else
                    {
                        // nothing outside the help to jump to: name it instead
                        md_inline (out, text);
                        g_string_append_c (out, ' ');
                        g_string_append_len (out, target, close - target);
                    }

                    g_free (text);
                    p = close + 1;
                }
            }
        }
        else if (p[0] == '*')
        {
            gboolean strong = p[1] == '*';
            const char *mark = strong ? "**" : "*";
            const char *text = p + (strong ? 2 : 1);
            const char *end = strstr (text, mark);

            while (end != NULL && end > text && end[-1] == '\\')
                end = strstr (end + 1, mark);

            if (end == NULL || end == text)
            {
                g_string_append_c (out, *p);
                p++;
            }
            else
            {
                char *text_dup;

                text_dup = g_strndup (text, end - text);
                g_string_append_c (out, strong ? CHAR_FONT_BOLD : CHAR_FONT_ITALIC);
                md_inline (out, text_dup);
                g_string_append_c (out, CHAR_FONT_NORMAL);
                g_free (text_dup);
                p = end + (strong ? 2 : 1);
            }
        }
        else
        {
            const char *n = g_utf8_next_char (p);

            g_string_append_len (out, p, n - p);
            p = n;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Write out the paragraph held so far.  Markdown fills it, so it goes out as one line and
 * the help window breaks it where the window ends.
 */

static void
md_flush (md_ctx_t *ctx)
{
    if (ctx->para->len == 0)
        return;

    // a break at the end of the last line of a paragraph: the flush ends it anyway
    while (ctx->para->len != 0 && ctx->para->str[ctx->para->len - 1] == '\n')
        g_string_set_size (ctx->para, ctx->para->len - 1);
    if (ctx->para->len == 0)
        return;

    if (ctx->term)
    {
        // the markdown of a term carries its own emphasis
        size_t len = ctx->body->len;

        if (len != 0
            && !(ctx->body->str[len - 1] == '\n' && len > 1 && ctx->body->str[len - 2] == '\n'))
            g_string_append_c (ctx->body, '\n');
        md_inline (ctx->body, ctx->para->str);
        g_string_append_c (ctx->body, '\n');
    }
    else
    {
        md_inline (ctx->body, ctx->para->str);
        g_string_append (ctx->body, "\n\n");
    }

    g_string_set_size (ctx->para, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
md_collect (md_ctx_t *ctx, const char *text, const char *indent)
{
    size_t len = strlen (text);
    gboolean hard;

    /* two spaces at the end of a line are a line break markdown keeps, and the help window
       breaks a line at a newline of its own, so the break is carried over */
    hard = len >= 2 && text[len - 1] == ' ' && text[len - 2] == ' ';
    while (len > 0 && text[len - 1] == ' ')
        len--;

    // after a break the next line starts where the broken one started
    if (ctx->para->len == 0 || ctx->para->str[ctx->para->len - 1] == '\n')
    {
        if (indent != NULL)
            g_string_append (ctx->para, indent);
    }
    else
        g_string_append_c (ctx->para, ' ');

    g_string_append_len (ctx->para, text, len);

    if (hard)
        g_string_append_c (ctx->para, '\n');
}

/* --------------------------------------------------------------------------------------------- */

/** Take the anchor and the markers off the end of a heading, leaving its title. */

static char *
md_heading_marks (md_ctx_t *ctx, const char *line)
{
    GString *title;
    char *p;

    title = g_string_new (line);

    while ((p = strstr (title->str, "<a id=\"")) != NULL)
    {
        char *end = strstr (p + 7, "\"></a>");

        if (end == NULL)
            break;

        g_free (ctx->anchor);
        ctx->anchor = g_strndup (p + 7, end - (p + 7));
        g_string_erase (title, p - title->str, end + 6 - p);
    }

    while ((p = strstr (title->str, "<!-- help:")) != NULL)
    {
        char *end = strstr (p, "-->");

        if (end == NULL)
            break;

        if (strncmp (p + 10, "notitle", 7) == 0)
            ctx->notitle = TRUE;
        else if (strncmp (p + 10, "skip", 4) == 0)
            ctx->skip = TRUE;

        g_string_erase (title, p - title->str, end + 3 - p);
    }

    g_strchomp (title->str);
    title->len = strlen (title->str);

    return g_string_free (title, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
md_heading (md_ctx_t *ctx, const char *line, int level)
{
    md_node_t *node;
    GString *title;
    char *text;

    md_flush (ctx);

    text = md_heading_marks (ctx, line);

    if (ctx->skip)
    {
        ctx->skip = FALSE;
        ctx->skipping = TRUE;
        g_free (ctx->anchor);
        ctx->anchor = NULL;
        g_free (text);
        return;
    }

    ctx->skipping = FALSE;

    title = g_string_sized_new (64);
    md_inline (title, text);
    g_free (text);

    node = g_new0 (md_node_t, 1);
    node->id = ctx->anchor != NULL ? ctx->anchor : help_md_node_id (title->str);
    node->title = g_string_free (title, FALSE);
    node->level = level;
    ctx->anchor = NULL;
    g_ptr_array_add (ctx->nodes, node);

    if (ctx->notitle)
    {
        g_string_append_printf (ctx->body, "%c[%s]\n", CHAR_NODE_END, node->id);
        ctx->notitle = FALSE;
    }
    else
        g_string_append_printf (ctx->body, "%c[%s]\n%s\n\n", CHAR_NODE_END, node->id, node->title);

    ctx->open = TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/** Whether the line holds an anchor and nothing else but blanks. */

static gboolean
md_anchor_alone (const char *line)
{
    const char *p;

    p = strstr (line, "></a>");
    if (p == NULL)
        return FALSE;

    for (p += 5; *p != '\0'; p++)
        if (*p != ' ' && *p != '\t')
            return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/** The value of a marker comment, as in <!-- help:topics "Topics:" --> */

static char *
md_marker_value (const char *line)
{
    const char *start;
    const char *end;

    start = strchr (line, '"');
    if (start == NULL)
        return NULL;
    start++;
    end = strchr (start, '"');

    return end == NULL ? NULL : g_strndup (start, end - start);
}

/* --------------------------------------------------------------------------------------------- */

static void
md_parse (md_ctx_t *ctx, const char *text)
{
    char **lines;
    int i;

    lines = g_strsplit (text, "\n", -1);

    for (i = 0; lines[i] != NULL; i++)
    {
        char *line = lines[i];
        int level;

        if (ctx->code != 0)
        {
            // a fence no shorter than the one that opened the block closes it
            const size_t n = strspn (line, "`");

            if (n >= ctx->code && line[n + strspn (line + n, " \t")] == '\0')
            {
                g_string_append_c (ctx->body, '\n');
                ctx->code = 0;
            }
            else if (!ctx->skipping)
            {
                g_string_append (ctx->body, line);
                g_string_append_c (ctx->body, '\n');
            }
            continue;
        }

        // the front matter of a man page carries its date, nothing the help
        // window shows
        if (i == 0 && strcmp (line, "---") == 0)
        {
            ctx->front = TRUE;
            continue;
        }
        if (ctx->front)
        {
            if (strcmp (line, "---") == 0)
                ctx->front = FALSE;
            continue;
        }

        if (strncmp (line, "<!--", 4) == 0)
        {
            char *value;

            if (strstr (line, "help:break") != NULL)
            {
                md_flush (ctx);
                g_ptr_array_add (ctx->nodes, NULL);
            }
            else if (strstr (line, "help:topics") != NULL)
            {
                value = md_marker_value (line);
                if (value != NULL)
                {
                    g_free (ctx->topics);
                    ctx->topics = value;
                }
            }
            continue;
        }

        // an anchor of its own marks a place inside the node; one with text
        // after it belongs to a heading, and is taken there
        if (strncmp (line, "<a id=\"", 7) == 0 && md_anchor_alone (line))
        {
            const char *end = strchr (line + 7, '"');

            if (end != NULL && !ctx->skipping)
            {
                md_flush (ctx);
                g_string_append_c (ctx->body, CHAR_ANCHOR);
                g_string_append_len (ctx->body, line + 7, end - (line + 7));
                g_string_append_c (ctx->body, CHAR_ANCHOR);
                g_string_append_c (ctx->body, '\n');
            }
            continue;
        }

        for (level = 0; line[level] == '#'; level++)
            ;
        if (level > 0 && line[level] == ' ')
        {
            md_heading (ctx, line + level + 1, level);
            continue;
        }

        if (ctx->skipping)
            continue;

        if (strncmp (line, "```", 3) == 0)
        {
            md_flush (ctx);
            ctx->code = strspn (line, "`");
            continue;
        }

        if (line[0] == '\0')
        {
            md_flush (ctx);
            ctx->term = FALSE;
            continue;
        }

        if (line[0] == ':' && line[1] == ' ')
        {
            // the body of the definition the line before opened
            md_flush (ctx);
            ctx->term = FALSE;
            md_collect (ctx, line + 2, BODY_INDENT);
            continue;
        }

        if (line[0] == '>')
        {
            const char *text_start = line[1] == ' ' ? line + 2 : line + 1;

            md_collect (ctx, text_start, BODY_INDENT);
            continue;
        }

        // a term is a line of its own that the next line describes
        if (ctx->para->len == 0 && lines[i + 1] != NULL && lines[i + 1][0] == ':'
            && lines[i + 1][1] == ' ')
            ctx->term = TRUE;

        md_collect (ctx, line, NULL);
    }

    md_flush (ctx);
    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */
/** A picture is drawn with the alternate character set of the terminal, which has one whatever
 * it can show, so it stands on a terminal that knows no UTF-8.  A line holding a line drawing
 * character goes into the set whole: the window prints such a character where it stands, while
 * it holds an ordinary word back until the word ends, and a picture would fall apart.
 */

static void
md_alternate (GString *text)
{
    static const struct
    {
        gunichar c;
        char acs;
    } line_char[] = {
        { 0x2500, 'q' }, { 0x2502, 'x' }, { 0x250C, 'l' }, { 0x2510, 'k' },
        { 0x2514, 'm' }, { 0x2518, 'j' }, { 0x251C, 't' }, { 0x2524, 'u' },
        { 0x252C, 'w' }, { 0x2534, 'v' }, { 0x253C, 'n' },
    };

    GString *out;
    char **lines;
    int i;

    if (g_utf8_strchr (text->str, text->len, 0x2500) == NULL
        && g_utf8_strchr (text->str, text->len, 0x2502) == NULL)
        return;

    out = g_string_sized_new (text->len + 256);
    lines = g_strsplit (text->str, "\n", -1);

    for (i = 0; lines[i] != NULL; i++)
    {
        const char *p;
        gboolean picture = FALSE;

        for (p = lines[i]; *p != '\0' && !picture; p = g_utf8_next_char (p))
        {
            gunichar c = g_utf8_get_char (p);
            size_t j;

            for (j = 0; j < G_N_ELEMENTS (line_char) && !picture; j++)
                picture = line_char[j].c == c;
        }

        if (i != 0)
            g_string_append_c (out, '\n');

        if (!picture)
        {
            g_string_append (out, lines[i]);
            continue;
        }

        g_string_append_c (out, CHAR_ALTERNATE);

        for (p = lines[i]; *p != '\0'; p = g_utf8_next_char (p))
        {
            gunichar c = g_utf8_get_char (p);
            char acs = '\0';
            size_t j;

            for (j = 0; j < G_N_ELEMENTS (line_char) && acs == '\0'; j++)
                if (line_char[j].c == c)
                    acs = line_char[j].acs;

            if (acs != '\0')
                g_string_append_c (out, acs);
            else
                g_string_append_len (out, p, g_utf8_next_char (p) - p);
        }

        g_string_append_c (out, CHAR_NORMAL);
    }

    g_strfreev (lines);
    g_string_assign (text, out->str);
    g_string_free (out, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/** What the build put in the paths the help text names, and the version of the program. */

static void
md_substitute (GString *text)
{
    static const struct
    {
        const char *name;
        const char *value;
    } paths[] = {
        { "{{pkgdatadir}}", MC_PKGDATADIR },
        { "{{sysconfdir}}", MC_SYSCONFDIR },
        { "{{pkglibexecdir}}", MC_PKGLIBEXECDIR },
        { "{{panel_plugins_dir}}", MC_PANEL_PLUGINS_DIR },
        { "{{bindir}}", MC_BINDIR },
    };

    size_t i;

    for (i = 0; i < G_N_ELEMENTS (paths); i++)
        g_string_replace (text, paths[i].name, paths[i].value, 0);

    // The version stands in a field of its own, and the field says how wide
    // it is: %MC_VERSION:32% takes 32 columns of the markdown, the same 32 the
    // window gives it, so the picture around it stands where it is written.
    // The width goes into the text between two markers for the window to read.
    {
        const char *token = "{{MC_VERSION";
        const size_t len = strlen (token);
        char *p;

        while ((p = strstr (text->str, token)) != NULL)
        {
            size_t start = p - text->str;
            size_t end = len;
            int width = 0;
            char *marker;

            if (p[end] == ':')
                for (end++; g_ascii_isdigit (p[end]); end++)
                    width = width * 10 + (p[end] - '0');

            if (p[end] != '}' || p[end + 1] != '}')
                break;  // not a field of ours, and the next pass would loop
            end += 2;

            // the spaces that make the field as wide as it says it is
            while ((int) end < width && p[end] == ' ')
                end++;

            marker = g_strdup_printf ("%c%d%c", CHAR_VERSION, width, CHAR_VERSION);
            g_string_erase (text, start, end);
            g_string_insert (text, start, marker);
            g_free (marker);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Turn the markdown of a help page, and of the template that goes with it, into the text the
 * help window paints.  The contents page comes first, as the window looks for [Contents] there.
 */

char *
help_md_convert (const char *page, const char *tmpl)
{
    md_ctx_t ctx;
    GString *out;
    guint i;

    memset (&ctx, 0, sizeof (ctx));
    ctx.body = g_string_sized_new (128 * 1024);
    ctx.para = g_string_sized_new (1024);
    ctx.nodes = g_ptr_array_new_with_free_func (md_node_free);

    if (page != NULL)
        md_parse (&ctx, page);
    if (tmpl != NULL)
        md_parse (&ctx, tmpl);

    out = g_string_sized_new (ctx.body->len + 4096);
    g_string_append_printf (out, "%c[Contents]\n", CHAR_NODE_END);
    if (ctx.topics != NULL)
        g_string_append_printf (out, "%s\n\n", ctx.topics);

    for (i = 0; i < ctx.nodes->len; i++)
    {
        const md_node_t *node = (const md_node_t *) g_ptr_array_index (ctx.nodes, i);

        if (node == NULL)
            g_string_append_c (out, '\n');
        else if (strcmp (node->id, "main") == 0)
            ;  // the screen the help opens on is not a topic of its own
        else
            g_string_append_printf (out, "  %*s%c%s%c%s%c\n", 2 * (node->level - 1), "",
                                    CHAR_LINK_START, node->title, CHAR_LINK_POINTER, node->id,
                                    CHAR_LINK_END);
    }

    g_string_append (out, ctx.body->str);
    g_string_append_c (out, CHAR_NODE_END);

    md_substitute (out);
    md_alternate (out);

    g_string_free (ctx.body, TRUE);
    g_string_free (ctx.para, TRUE);
    g_ptr_array_free (ctx.nodes, TRUE);
    g_free (ctx.topics);
    g_free (ctx.anchor);

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
