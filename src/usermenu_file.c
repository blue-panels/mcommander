/*
   User menu for the M-Commander
   The file of the menu: reading and writing it

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

/** \file usermenu_file.c
 *  \brief Source: the file of the user menu
 */

#include <config.h>

#include <errno.h>
#include <stdio.h>  // rename()
#include <string.h>

#include "lib/global.h"
#include "lib/util.h"

#include "usermenu_ini.h"

/*** file scope macro definitions ****************************************************************/

#define UM_KEY_HOTKEY  "hotkey"
#define UM_KEY_COMMAND "command"
#define UM_KEY_VIEW    "view"
#define UM_KEY_SILENT  "silent"
#define UM_KEY_SUBMENU "submenu"
#define UM_KEY_PARENT  "parent"
#define UM_KEY_DEFAULT "default"

/*** global variables ****************************************************************************/

const char *const user_menu_ini_cond_keys[UM_COND_COUNT] = {
    "path",       "on",       "exec",       "marked",       "path~",       "needs",
    "other.path", "other.on", "other.exec", "other.marked", "other.path~",
};

const char *const user_menu_ini_default_keys[UM_COND_COUNT] = {
    "default.path",       "default.on",           "default.exec",        "default.marked",
    "default.path~",      "default.needs",        "default.other.path",  "default.other.on",
    "default.other.exec", "default.other.marked", "default.other.path~",
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
user_menu_entry_free (user_menu_entry_t *entry)
{
    if (entry == NULL)
        return;

    int k;

    g_free (entry->label);
    g_free (entry->command);
    g_free (entry->parent);
    g_free (entry->group);
    g_free (entry->comment);
    for (k = 0; k < UM_COND_COUNT; k++)
    {
        g_free (entry->cond[k]);
        g_free (entry->dcond[k]);
    }
    g_free (entry);
}

/* --------------------------------------------------------------------------------------------- */

#define UM_FORMAT_LINE "# mc menu format 2"

/** One line of a group, or the lines of a block: kept as written, to be written back. */
typedef struct
{
    char *key;    // NULL for a comment or an empty line
    char *value;  // what the key holds; for a block, the lines between its fences
    char *text;   // the line, or the lines of a block, as they are in the file
    int line;
} um_line_t;

typedef struct
{
    char *name;
    int line;
    GString *leading;  // comments right above the group: they go where it goes
    GPtrArray *body;   // um_line_t
    gboolean used;     // written back already
} um_group_t;

typedef struct
{
    GString *head;  // the lines before the first group
    GPtrArray *groups;
    GString *tail;  // comments after the last key of the file
} um_doc_t;

/* --------------------------------------------------------------------------------------------- */

static void
um_line_free (um_line_t *l)
{
    g_free (l->key);
    g_free (l->value);
    g_free (l->text);
    g_free (l);
}

/* --------------------------------------------------------------------------------------------- */

static void
um_group_free (um_group_t *g)
{
    g_free (g->name);
    g_string_free (g->leading, TRUE);
    g_ptr_array_free (g->body, TRUE);
    g_free (g);
}

/* --------------------------------------------------------------------------------------------- */

static void
um_doc_free (um_doc_t *doc)
{
    if (doc == NULL)
        return;

    g_string_free (doc->head, TRUE);
    g_string_free (doc->tail, TRUE);
    g_ptr_array_free (doc->groups, TRUE);
    g_free (doc);
}

/* --------------------------------------------------------------------------------------------- */

/** The length of a fence: three backticks or more and nothing else, or 0. */
static size_t
um_fence_len (const char *text)
{
    char *t;
    size_t n;

    t = g_strstrip (g_strdup (text));
    n = strspn (t, "`");
    if (n < 3 || t[n] != '\0')
        n = 0;
    g_free (t);

    return n;
}

/* --------------------------------------------------------------------------------------------- */

static void
um_parse_error (GError **error, const char *file, int line, const char *what)
{
    g_set_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_PARSE, _ ("%s, line %d: %s"), file, line,
                 what);
}

/* --------------------------------------------------------------------------------------------- */

static um_line_t *
um_group_find (const um_group_t *g, const char *key)
{
    guint i;

    for (i = 0; i < g->body->len; i++)
    {
        um_line_t *l = g_ptr_array_index (g->body, i);

        if (l->key != NULL && strcmp (l->key, key) == 0)
            return l;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
um_body_add_text (um_group_t *g, const char *text)
{
    um_line_t *l;

    l = g_new0 (um_line_t, 1);
    l->text = g_strdup (text);
    g_ptr_array_add (g->body, l);
}

/* --------------------------------------------------------------------------------------------- */

/** Where the comments met before a group split: after the last empty line they are its own. */
static guint
um_pending_cut (GPtrArray *pending)
{
    guint i, cut = 0;

    for (i = 0; i < pending->len; i++)
    {
        const char *p = g_ptr_array_index (pending, i);

        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            cut = i + 1;
    }

    return cut;
}

/* --------------------------------------------------------------------------------------------- */

/** Whether there is anything in a text but empty lines. */
static gboolean
um_text_is_empty (const char *data)
{
    for (; *data != '\0'; data++)
        if (!g_ascii_isspace (*data))
            return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Parse the text of a menu file; file names it in the errors.  The first line
 * is the format line: a text without it is not a menu of this format, and an
 * empty text is an empty menu.
 */
static um_doc_t *
um_doc_parse (const char *data, const char *file, GError **error)
{
    um_doc_t *doc;
    gchar **lines;
    GPtrArray *pending;
    um_group_t *group = NULL;
    int i;

    doc = g_new0 (um_doc_t, 1);
    doc->head = g_string_new ("");
    doc->tail = g_string_new ("");
    doc->groups = g_ptr_array_new_with_free_func ((GDestroyNotify) um_group_free);

    if (um_text_is_empty (data))
        return doc;

    if (!g_str_has_prefix (data, UM_FORMAT_LINE "\n")
        && !g_str_has_prefix (data, UM_FORMAT_LINE "\r") && strcmp (data, UM_FORMAT_LINE) != 0)
    {
        um_parse_error (error, file, 1, _ ("the first line is not \"" UM_FORMAT_LINE "\""));
        um_doc_free (doc);
        return NULL;
    }

    lines = g_strsplit (data, "\n", -1);

    // the empty piece after the last newline is not a line
    i = (int) g_strv_length (lines);
    if (i > 0 && *lines[i - 1] == '\0')
    {
        g_free (lines[i - 1]);
        lines[i - 1] = NULL;
    }

    pending = g_ptr_array_new ();

    for (i = 0; lines[i] != NULL; i++)
    {
        char *line = lines[i];
        const int lineno = i + 1;
        char *t;
        size_t len;

        len = strlen (line);
        if (len > 0 && line[len - 1] == '\r')
            line[len - 1] = '\0';

        // the format line is the writer's own: it puts it back at the top
        if (lineno == 1)
            continue;

        t = g_strstrip (g_strdup (line));

        if (*t == '\0' || *t == '#')
        {
            g_ptr_array_add (pending, line);
            g_free (t);
            continue;
        }

        if (*t == '[' && t[strlen (t) - 1] == ']')
        {
            guint k;

            t[strlen (t) - 1] = '\0';

            for (k = 0; k < doc->groups->len; k++)
            {
                um_group_t *other = g_ptr_array_index (doc->groups, k);

                if (strcmp (other->name, t + 1) == 0)
                {
                    char *what;

                    what = g_strdup_printf (_ ("[%s] is already at line %d"), t + 1, other->line);
                    um_parse_error (error, file, lineno, what);
                    g_free (what);
                    g_free (t);
                    goto fail;
                }
            }

            {
                const guint cut = um_pending_cut (pending);
                um_group_t *g;

                for (k = 0; k < cut; k++)
                {
                    const char *text = g_ptr_array_index (pending, k);

                    if (group != NULL)
                        um_body_add_text (group, text);
                    else
                    {
                        g_string_append (doc->head, text);
                        g_string_append_c (doc->head, '\n');
                    }
                }

                g = g_new0 (um_group_t, 1);
                g->leading = g_string_new ("");
                for (k = cut; k < pending->len; k++)
                {
                    g_string_append (g->leading, (const char *) g_ptr_array_index (pending, k));
                    g_string_append_c (g->leading, '\n');
                }
                g_ptr_array_set_size (pending, 0);
                group = g;
            }

            group->name = g_strdup (t + 1);
            group->line = lineno;
            group->body = g_ptr_array_new_with_free_func ((GDestroyNotify) um_line_free);
            g_ptr_array_add (doc->groups, group);
            g_free (t);
            continue;
        }

        g_free (t);

        if (strchr (line, '=') == NULL)
        {
            char *what;

            what = g_strdup_printf (_ ("not a key=value, a [group] or a comment:\n%s"), line);
            um_parse_error (error, file, lineno, what);
            g_free (what);
            goto fail;
        }

        if (group == NULL)
        {
            um_parse_error (error, file, lineno, _ ("a key before the first [group]"));
            goto fail;
        }

        {
            um_line_t *l;
            const char *eq = strchr (line, '=');
            const char *v = eq + 1;
            size_t fence;
            guint k;

            l = g_new0 (um_line_t, 1);
            l->line = lineno;
            l->key = g_strstrip (g_strndup (line, (gsize) (eq - line)));

            if (*l->key == '\0')
            {
                um_line_free (l);
                um_parse_error (error, file, lineno, _ ("a value without a key"));
                goto fail;
            }

            for (k = 0; k < group->body->len; k++)
            {
                um_line_t *other = g_ptr_array_index (group->body, k);

                if (other->key != NULL && strcmp (other->key, l->key) == 0)
                {
                    char *what;

                    what = g_strdup_printf (_ ("%s= is already at line %d"), l->key, other->line);
                    um_line_free (l);
                    um_parse_error (error, file, lineno, what);
                    g_free (what);
                    goto fail;
                }
            }

            fence = um_fence_len (v);
            if (fence != 0)
            {
                GString *value, *text;
                int j;

                value = g_string_new ("");
                text = g_string_new (line);

                for (j = i + 1; lines[j] != NULL && um_fence_len (lines[j]) != fence; j++)
                {
                    len = strlen (lines[j]);
                    if (len > 0 && lines[j][len - 1] == '\r')
                        lines[j][len - 1] = '\0';

                    if (j > i + 1)
                        g_string_append_c (value, '\n');
                    g_string_append (value, lines[j]);
                    g_string_append_c (text, '\n');
                    g_string_append (text, lines[j]);
                }

                if (lines[j] == NULL)
                {
                    g_string_free (value, TRUE);
                    g_string_free (text, TRUE);
                    um_line_free (l);
                    um_parse_error (error, file, lineno,
                                    _ ("the block opened here has no closing line"));
                    goto fail;
                }

                g_string_append_c (text, '\n');
                g_string_append (text, lines[j]);
                l->value = g_string_free (value, FALSE);
                l->text = g_string_free (text, FALSE);
                i = j;
            }
            else
            {
                l->value = g_strstrip (g_strdup (v));
                l->text = g_strdup (line);
            }

            // comments met inside the group stay where they are
            for (k = 0; k < pending->len; k++)
                um_body_add_text (group, g_ptr_array_index (pending, k));
            g_ptr_array_set_size (pending, 0);

            g_ptr_array_add (group->body, l);
        }
    }

    for (i = 0; i < (int) pending->len; i++)
    {
        GString *to = group != NULL ? doc->tail : doc->head;

        g_string_append (to, (const char *) g_ptr_array_index (pending, i));
        g_string_append_c (to, '\n');
    }

    g_ptr_array_free (pending, TRUE);
    g_strfreev (lines);

    return doc;

fail:
    g_ptr_array_free (pending, TRUE);
    g_strfreev (lines);
    um_doc_free (doc);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/** Read a menu file.  A file that is not there is an empty document. */
static um_doc_t *
um_doc_read (const char *file, GError **error)
{
    um_doc_t *doc;
    char *data = NULL;
    GError *read_error = NULL;

    if (!g_file_get_contents (file, &data, NULL, &read_error))
    {
        if (!g_error_matches (read_error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
            g_propagate_error (error, read_error);
            return NULL;
        }
        g_error_free (read_error);
    }

    doc = um_doc_parse (data != NULL ? data : "", file, error);
    g_free (data);

    return doc;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
um_doc_value (const um_group_t *g, const char *key)
{
    const um_line_t *l;

    l = um_group_find (g, key);
    return l != NULL ? l->value : NULL;
}

/* --------------------------------------------------------------------------------------------- */

/** A key of true or false; one that is neither is an error, not a false. */
static gboolean
um_doc_flag (const um_group_t *g, const char *key, gboolean *value, const char *file,
             GError **error)
{
    const um_line_t *l;
    char *what;

    *value = FALSE;

    l = um_group_find (g, key);
    if (l == NULL)
        return TRUE;

    if (strcmp (l->value, "true") == 0 || strcmp (l->value, "1") == 0)
    {
        *value = TRUE;
        return TRUE;
    }

    if (strcmp (l->value, "false") == 0 || strcmp (l->value, "0") == 0)
        return TRUE;

    what = g_strdup_printf (_ ("[%s]: %s= takes true or false"), g->name, key);
    um_parse_error (error, file, l->line, what);
    g_free (what);

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/** One set of conditions, the keys given by their names; a wrong value is an error. */
static gboolean
um_conds_from_group (const um_group_t *g, const char *const *keys, char **cond, const char *file,
                     GError **error)
{
    int k;

    for (k = 0; k < UM_COND_COUNT; k++)
    {
        const char *key = keys[k];
        const um_line_t *l;

        l = um_group_find (g, key);
        if (l == NULL)
            continue;

        if (k == UM_COND_EXEC || k == UM_COND_MARKED || k == UM_COND_OTHER_EXEC
            || k == UM_COND_OTHER_MARKED)
        {
            gboolean flag;

            if (!um_doc_flag (g, key, &flag, file, error))
                return FALSE;
            cond[k] = g_strdup (flag ? "true" : "false");
            continue;
        }

        cond[k] = g_strdup (l->value);

        if (k % UM_COND_OTHER == UM_COND_PATH_RE)
        {
            GRegex *re;
            GError *re_error = NULL;

            re = g_regex_new (l->value + (*l->value == '!' ? 1 : 0), 0, 0, &re_error);
            if (re == NULL)
            {
                char *what;

                what = g_strdup_printf (_ ("[%s]: %s=%s: not a regular expression:\n%s"), g->name,
                                        key, l->value, re_error->message);
                um_parse_error (error, file, l->line, what);
                g_free (what);
                g_error_free (re_error);
                return FALSE;
            }
            g_regex_unref (re);
        }

        if (k == UM_COND_ON || k == UM_COND_OTHER_ON)
        {
            char *bad = NULL;

            if (!user_menu_ini_on_check (l->value, &bad))
            {
                char *what;

                what = g_strdup_printf (_ ("[%s]: %s=%s: \"%s\" is not a type.\nTypes: file, dir, "
                                           "link, broken, char, block, fifo, socket"),
                                        g->name, key, l->value, bad);
                um_parse_error (error, file, l->line, what);
                g_free (what);
                g_free (bad);
                return FALSE;
            }
        }
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
um_entry_from_group (user_menu_entry_t *entry, const um_group_t *g, const char *file,
                     GError **error)
{
    const char *value;

    entry->label = g_strdup (g->name);
    entry->group = g_strdup (g->name);

    value = um_doc_value (g, UM_KEY_COMMAND);
    entry->command = g_strdup (value != NULL ? value : "");

    value = um_doc_value (g, UM_KEY_PARENT);
    entry->parent = value != NULL && *value != '\0' ? g_strdup (value) : NULL;

    value = um_doc_value (g, UM_KEY_HOTKEY);
    if (value != NULL)
        entry->hotkey = value[0];

    if (!um_doc_flag (g, UM_KEY_VIEW, &entry->view, file, error)
        || !um_doc_flag (g, UM_KEY_SILENT, &entry->silent, file, error)
        || !um_doc_flag (g, UM_KEY_SUBMENU, &entry->is_submenu, file, error)
        || !um_doc_flag (g, UM_KEY_DEFAULT, &entry->is_default, file, error))
        return FALSE;

    return um_conds_from_group (g, user_menu_ini_cond_keys, entry->cond, file, error)
        && um_conds_from_group (g, user_menu_ini_default_keys, entry->dcond, file, error);
}

/* --------------------------------------------------------------------------------------------- */

/** The entries of a document; a wrong value in any of them is an error, and none is taken. */
static gboolean
um_doc_entries (const um_doc_t *doc, GPtrArray *entries, const char *file, int level,
                GError **error)
{
    const guint start = entries->len;
    guint i;

    for (i = 0; i < doc->groups->len; i++)
    {
        user_menu_entry_t *entry;

        entry = g_new0 (user_menu_entry_t, 1);
        entry->level = level;
        g_ptr_array_add (entries, entry);

        if (!um_entry_from_group (entry, g_ptr_array_index (doc->groups, i), file, error))
        {
            g_ptr_array_remove_range (entries, start, entries->len - start);
            return FALSE;
        }
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read one file.  The groups are taken in the order the file has them, which is
 * the order of the list.  A file that is not there is an empty menu; a file
 * that cannot be read gives no entries and FALSE: nothing of it is taken.
 */
gboolean
user_menu_ini_load_file (GPtrArray *entries, const char *file, int level, GError **error)
{
    um_doc_t *doc;
    gboolean ok;

    if (!exist_file (file))
        return TRUE;

    doc = um_doc_read (file, error);
    if (doc == NULL)
        return FALSE;

    ok = um_doc_entries (doc, entries, file, level, error);
    um_doc_free (doc);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * key=value where the value is one line with nothing to lose; a block where it
 * has lines, spaces at an end, or a fence of its own.
 */
static void
um_render (GString *out, const char *key, const char *value)
{
    gchar **lines;
    size_t fence = 3;
    gboolean plain, again;
    guint i;

    plain = strchr (value, '\n') == NULL && strncmp (value, "```", 3) != 0
        && (*value == '\0'
            || (!g_ascii_isspace (value[0]) && !g_ascii_isspace (value[strlen (value) - 1])));

    if (plain)
    {
        g_string_append_printf (out, "%s=%s\n", key, value);
        return;
    }

    lines = g_strsplit (value, "\n", -1);
    do
    {
        again = FALSE;
        for (i = 0; lines[i] != NULL && !again; i++)
            if (um_fence_len (lines[i]) == fence)
            {
                fence++;
                again = TRUE;
            }
    }
    while (again);
    g_strfreev (lines);

    g_string_append_printf (out, "%s=", key);
    for (i = 0; i < fence; i++)
        g_string_append_c (out, '`');
    g_string_append_printf (out, "\n%s\n", value);
    for (i = 0; i < fence; i++)
        g_string_append_c (out, '`');
    g_string_append_c (out, '\n');
}

/* --------------------------------------------------------------------------------------------- */

// the keys an entry writes, in the order a new group gets them
enum
{
    UM_W_HOTKEY = 0,
    UM_W_PARENT,
    UM_W_SUBMENU,
    UM_W_COND,  // UM_COND_COUNT of them
    UM_W_DEFAULT = UM_W_COND + UM_COND_COUNT,
    UM_W_DCOND,  // UM_COND_COUNT of them
    UM_W_COMMAND = UM_W_DCOND + UM_COND_COUNT,
    UM_W_VIEW,
    UM_W_SILENT,
    UM_W_COUNT
};

/** What an entry has to say, key by key; NULL where the key is not written. */
static void
um_entry_wanted (const user_menu_entry_t *entry, const um_group_t *old, const char **keys,
                 char **values)
{
    int k;

    keys[UM_W_HOTKEY] = UM_KEY_HOTKEY;
    values[UM_W_HOTKEY] = entry->hotkey != '\0' ? g_strdup_printf ("%c", entry->hotkey) : NULL;

    keys[UM_W_PARENT] = UM_KEY_PARENT;
    values[UM_W_PARENT] =
        entry->parent != NULL && *entry->parent != '\0' ? g_strdup (entry->parent) : NULL;

    keys[UM_W_SUBMENU] = UM_KEY_SUBMENU;
    values[UM_W_SUBMENU] = entry->is_submenu ? g_strdup ("true") : NULL;

    for (k = 0; k < UM_COND_COUNT; k++)
    {
        keys[UM_W_COND + k] = user_menu_ini_cond_keys[k];
        values[UM_W_COND + k] = g_strdup (entry->cond[k]);
    }

    keys[UM_W_DEFAULT] = UM_KEY_DEFAULT;
    values[UM_W_DEFAULT] = entry->is_default ? g_strdup ("true") : NULL;

    for (k = 0; k < UM_COND_COUNT; k++)
    {
        keys[UM_W_DCOND + k] = user_menu_ini_default_keys[k];
        values[UM_W_DCOND + k] = g_strdup (entry->dcond[k]);
    }

    keys[UM_W_COMMAND] = UM_KEY_COMMAND;
    keys[UM_W_VIEW] = UM_KEY_VIEW;
    keys[UM_W_SILENT] = UM_KEY_SILENT;

    if (entry->is_submenu)
    {
        values[UM_W_COMMAND] = values[UM_W_VIEW] = values[UM_W_SILENT] = NULL;
        return;
    }

    values[UM_W_COMMAND] = g_strdup (entry->command);

    // a false is written only where the file had the key already
    values[UM_W_VIEW] = entry->view                               ? g_strdup ("true")
        : old != NULL && um_group_find (old, UM_KEY_VIEW) != NULL ? g_strdup ("false")
                                                                  : NULL;
    values[UM_W_SILENT] = entry->silent                             ? g_strdup ("true")
        : old != NULL && um_group_find (old, UM_KEY_SILENT) != NULL ? g_strdup ("false")
                                                                    : NULL;
}

/* --------------------------------------------------------------------------------------------- */

static um_group_t *
um_doc_group (um_doc_t *doc, const char *name)
{
    guint i;

    if (name == NULL)
        return NULL;

    for (i = 0; i < doc->groups->len; i++)
    {
        um_group_t *g = g_ptr_array_index (doc->groups, i);

        if (!g->used && strcmp (g->name, name) == 0)
            return g;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
um_write_entry (GString *out, const user_menu_entry_t *entry, um_doc_t *doc)
{
    const char *keys[UM_W_COUNT];
    char *values[UM_W_COUNT];
    gboolean done[UM_W_COUNT] = { FALSE };
    um_group_t *old;
    guint i, tail = 0;
    int w;

    old = um_doc_group (doc, entry->group);
    um_entry_wanted (entry, old, keys, values);

    if (old != NULL)
    {
        old->used = TRUE;
        g_string_append (out, old->leading->str);
        g_string_append_printf (out, "[%s]\n", entry->label);

        // the empty lines and comments after the last key end the group: new keys go before them
        for (i = 0; i < old->body->len; i++)
            if (((const um_line_t *) g_ptr_array_index (old->body, i))->key != NULL)
                tail = i + 1;

        for (i = 0; i < tail; i++)
        {
            const um_line_t *l = g_ptr_array_index (old->body, i);

            for (w = 0; l->key != NULL && w < UM_W_COUNT; w++)
                if (strcmp (l->key, keys[w]) == 0)
                    break;

            if (l->key == NULL || w == UM_W_COUNT)
                g_string_append_printf (out, "%s\n", l->text);  // a comment, or a key not ours
            else if (values[w] != NULL)
            {
                // a value that did not change keeps the way it was written
                if (strcmp (l->value, values[w]) == 0)
                    g_string_append_printf (out, "%s\n", l->text);
                else
                    um_render (out, keys[w], values[w]);
                done[w] = TRUE;
            }
        }
    }
    else
    {
        if (out->len != 0 && !g_str_has_suffix (out->str, "\n\n"))
            g_string_append_c (out, '\n');
        if (entry->comment != NULL)
            g_string_append (out, entry->comment);
        g_string_append_printf (out, "[%s]\n", entry->label);
    }

    for (w = 0; w < UM_W_COUNT; w++)
    {
        if (values[w] != NULL && !done[w])
            um_render (out, keys[w], values[w]);
        g_free (values[w]);
    }

    for (i = tail; old != NULL && i < old->body->len; i++)
        g_string_append_printf (out, "%s\n",
                                ((const um_line_t *) g_ptr_array_index (old->body, i))->text);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Write the entries of one level into their file, in the order of the list.
 * The file is changed, not built anew: comments, keys of a later version and
 * values that did not change stay as they were written.  A file that is there
 * but cannot be read is not written over: the entries in it were never read,
 * and would be lost.
 */
gboolean
user_menu_ini_save_file (const char *file, GPtrArray *entries, int level, GError **error)
{
    um_doc_t *doc;
    GString *out;
    guint i;
    gboolean ok;

    doc = um_doc_read (file, error);
    if (doc == NULL)
        return FALSE;

    out = g_string_new (UM_FORMAT_LINE "\n");

    g_string_append (out, doc->head->str);

    for (i = 0; i < entries->len; i++)
    {
        const user_menu_entry_t *entry = g_ptr_array_index (entries, i);

        if (entry->level == level)
            um_write_entry (out, entry, doc);
    }

    g_string_append (out, doc->tail->str);

    ok = g_file_set_contents (file, out->str, (gssize) out->len, error);

    if (ok)
        for (i = 0; i < entries->len; i++)
        {
            user_menu_entry_t *entry = g_ptr_array_index (entries, i);

            if (entry->level == level)
            {
                g_free (entry->group);
                entry->group = g_strdup (entry->label);
            }
        }

    g_string_free (out, TRUE);
    um_doc_free (doc);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * The label is the name of the group the entry is kept in, and that decides
 * what a label may hold: no brackets, which end a group name, and nothing that
 * breaks a line.  Two entries cannot share a label either, or the second would
 * take the place of the first, so a repeat is numbered.
 */
void
user_menu_ini_label_fix (GPtrArray *entries, user_menu_entry_t *entry)
{
    char *base;
    char *p;
    guint n = 1;

    if (entry->label == NULL)
        entry->label = g_strdup ("");

    for (p = entry->label; *p != '\0'; p++)
        switch (*p)
        {
        case '[':
            *p = '(';
            break;
        case ']':
            *p = ')';
            break;
        case '\n':
        case '\r':
        case '\t':
            *p = ' ';
            break;
        default:
            break;
        }

    base = g_strdup (entry->label);

    while (TRUE)
    {
        guint i;
        gboolean taken = FALSE;

        for (i = 0; i < entries->len && !taken; i++)
        {
            user_menu_entry_t *other = g_ptr_array_index (entries, i);

            taken = other != entry && strcmp (other->label, entry->label) == 0;
        }

        if (!taken)
            break;

        g_free (entry->label);
        entry->label = g_strdup_printf ("%s (%u)", base, ++n);
    }

    g_free (base);
}

/* --------------------------------------------------------------------------------------------- */

/** The comment GKeyFile kept, its '#' put back. */
static void
um_put_comment (GString *out, const char *comment)
{
    gchar **lines;
    size_t len;
    char *text;
    int i;

    if (comment == NULL)
        return;

    // older GLib ends the comment with a newline of its own
    text = g_strdup (comment);
    len = strlen (text);
    while (len > 0 && text[len - 1] == '\n')
        text[--len] = '\0';

    lines = g_strsplit (text, "\n", -1);
    g_free (text);
    for (i = 0; lines[i] != NULL; i++)
        if (*lines[i] == '\0')
            g_string_append_c (out, '\n');
        else
            g_string_append_printf (out, "#%s\n", lines[i]);
    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * A file written by GKeyFile, with its escapes, in the format of the menu:
 * the values as GKeyFile reads them, each written the way the menu writes
 * it, and the comments where they were.
 */
static char *
um_from_key_file (GKeyFile *kf)
{
    GString *out;
    gchar **groups;
    gsize i, count = 0;

    out = g_string_new (UM_FORMAT_LINE "\n");
    groups = g_key_file_get_groups (kf, &count);

    for (i = 0; i < count; i++)
    {
        gchar **keys;
        gsize k, nkeys = 0;
        char *comment;

        if (i != 0)
            g_string_append_c (out, '\n');

        // the comment of the first group holds the one at the top of the file as well
        comment = g_key_file_get_comment (kf, groups[i], NULL, NULL);
        um_put_comment (out, comment);
        g_free (comment);

        g_string_append_printf (out, "[%s]\n", groups[i]);

        keys = g_key_file_get_keys (kf, groups[i], &nkeys, NULL);
        for (k = 0; k < nkeys; k++)
        {
            char *value;

            comment = g_key_file_get_comment (kf, groups[i], keys[k], NULL);
            um_put_comment (out, comment);
            g_free (comment);

            value = g_key_file_get_string (kf, groups[i], keys[k], NULL);
            um_render (out, keys[k], value != NULL ? value : "");
            g_free (value);
        }
        g_strfreev (keys);
    }

    g_strfreev (groups);

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
user_menu_ini_needs_conversion (const char *file)
{
    char *data = NULL;
    gboolean needs;

    if (!g_file_get_contents (file, &data, NULL, NULL))
        return FALSE;

    needs = !um_text_is_empty (data) && !g_str_has_prefix (data, UM_FORMAT_LINE);
    g_free (data);

    return needs;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Bring a menu file without the format line to the format: one GKeyFile reads
 * was written by an older version and is converted; one it does not read was
 * written by hand in this format and gets the line.  The result has to read
 * before the file is touched; the file as it was is kept as file.old.
 */
gboolean
user_menu_ini_convert_file (const char *file, GError **error)
{
    char *data = NULL, *text, *backup, *fresh;
    GKeyFile *kf;
    um_doc_t *doc;
    GPtrArray *check;
    gboolean ok;

    if (!g_file_get_contents (file, &data, NULL, error))
        return FALSE;

    kf = g_key_file_new ();
    if (g_key_file_load_from_data (kf, data, -1, G_KEY_FILE_KEEP_COMMENTS, NULL))
        text = um_from_key_file (kf);
    else
        text = g_strconcat (UM_FORMAT_LINE "\n", data, (char *) NULL);
    g_key_file_free (kf);
    g_free (data);

    doc = um_doc_parse (text, file, error);
    check = g_ptr_array_new_with_free_func ((GDestroyNotify) user_menu_entry_free);
    ok = doc != NULL && um_doc_entries (doc, check, file, MENU_LEVEL_USER, error);
    g_ptr_array_free (check, TRUE);
    um_doc_free (doc);

    if (ok)
    {
        backup = g_strconcat (file, ".old", (char *) NULL);
        fresh = g_strconcat (file, ".new", (char *) NULL);

        ok = g_file_set_contents (fresh, text, -1, error);
        if (ok && (rename (file, backup) != 0 || rename (fresh, file) != 0))
        {
            g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), "%s: %s", file,
                         g_strerror (errno));
            ok = FALSE;
        }

        g_free (backup);
        g_free (fresh);
    }

    g_free (text);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */
