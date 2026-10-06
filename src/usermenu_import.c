/*
   User menu for the M-Commander
   Import of a menu file written by hand

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

/** \file usermenu_import.c
 *  \brief Source: the import of an old user menu
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"

#include "src/setup.h"

#include "usermenu_ini.h"

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * A regular expression of an old menu as a glob mask, or NULL where no mask
 * matches the same names.  What is not anchored gets a '*' at that end.
 */
char *
user_menu_ini_regex_to_glob (const char *regex)
{
    GString *out;
    const char *p = regex, *end = regex + strlen (regex);
    gboolean anchored_end;
    char *dup;

    out = g_string_new ("");

    if (*p == '^')
        p++;
    else
        g_string_append_c (out, '*');

    anchored_end = end > p && end[-1] == '$' && (end - 1 == p || end[-2] != '\\');
    if (anchored_end)
        end--;

    for (; p < end; p++)
    {
        if (*p == '\\' && p + 1 < end && strchr (".$^()+|/-\\", p[1]) != NULL)
            g_string_append_c (out, *++p);
        else if (*p == '.' && p + 1 < end && p[1] == '*')
        {
            g_string_append_c (out, '*');
            p++;
        }
        else if (*p == '.')
            g_string_append_c (out, '?');
        else if (strchr ("\\*?[]{}+|()$^", *p) != NULL)
        {
            // \d, a set, a group and the like have no mask
            g_string_free (out, TRUE);
            return NULL;
        }
        else
            g_string_append_c (out, *p);
    }

    if (!anchored_end)
        g_string_append_c (out, '*');

    // "**" means no more than "*"
    while ((dup = strstr (out->str, "**")) != NULL)
        g_string_erase (out, dup - out->str, 1);

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/** The letters of "t" as types of on=; x and t are not types and are taken alone. */
static char *
um_import_types (const char *letters)
{
    static const struct
    {
        char letter;
        const char *type;
    } map[] = {
        { 'r', "file" },  { 'd', "dir" },  { 'l', "link" },   { 'c', "char" },
        { 'b', "block" }, { 'f', "fifo" }, { 's', "socket" }, { 'n', "!dir" },
    };
    GString *out;
    const char *p;

    out = g_string_new ("");

    for (p = letters; *p != '\0'; p++)
    {
        size_t m;

        for (m = 0; m < G_N_ELEMENTS (map) && map[m].letter != *p; m++)
            ;
        if (m == G_N_ELEMENTS (map))
        {
            g_string_free (out, TRUE);
            return NULL;
        }

        if (out->len != 0)
            g_string_append_c (out, ';');
        g_string_append (out, map[m].type);
    }

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Masks for path=, turned over where the condition was: the masks are read
 * as .gitignore reads them, so "not these" is everything, then each of them
 * taken away.
 */
static char *
um_import_negate_masks (char *masks, gboolean negate)
{
    GString *out;
    gchar **items;
    int i;

    if (!negate)
        return masks;

    out = g_string_new ("*");
    items = g_strsplit (masks, ";", -1);
    for (i = 0; items[i] != NULL; i++)
        g_string_append_printf (out, ";!%s", items[i]);
    g_strfreev (items);
    g_free (masks);

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/** One condition of an old menu as a key and its value; FALSE where no key says it. */
static gboolean
um_import_term (char letter, gboolean negate, const char *arg, gboolean regex, int *key,
                char **value)
{
    const int panel = g_ascii_isupper (letter) ? UM_COND_OTHER : 0;
    char *v = NULL;

    switch (g_ascii_tolower (letter))
    {
    case 'f':
        *key = panel + UM_COND_PATH;
        v = regex ? user_menu_ini_regex_to_glob (arg) : g_strdup (arg);
        if (v == NULL)
        {
            // no mask says the same: the expression goes to path~= as it is, and
            // with no '/' in it, it looks at the name, as it did
            if (strchr (arg, '/') != NULL)
                return FALSE;
            *key = panel + UM_COND_PATH_RE;
            *value = negate ? g_strconcat ("!", arg, (char *) NULL) : g_strdup (arg);
            return TRUE;
        }
        *value = um_import_negate_masks (v, negate);
        return TRUE;

    case 't':
        if (strcmp (arg, "x") == 0 || strcmp (arg, "t") == 0)
        {
            *key = panel + (arg[0] == 'x' ? UM_COND_EXEC : UM_COND_MARKED);
            *value = g_strdup (negate ? "false" : "true");
            return TRUE;
        }
        *key = panel + UM_COND_ON;
        v = um_import_types (arg);
        break;

    case 'x':
        if (negate)
            return FALSE;
        *key = UM_COND_NEEDS;
        *value = g_strdup (arg);
        return TRUE;

    default:
        return FALSE;  // d: the directory of the panel, y: the editor menu
    }

    if (v == NULL)
        return FALSE;

    if (negate)
    {
        char *n;

        // "not any of these" is not a list the keys know
        if (strchr (v, ';') != NULL)
        {
            g_free (v);
            return FALSE;
        }

        n = *v == '!' ? g_strdup (v + 1) : g_strconcat ("!", v, (char *) NULL);
        g_free (v);
        v = n;
    }

    *value = v;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    char op;  // '=' for the first, then '&' or '|'
    gboolean negate;
    char letter;
    char *arg;
} um_term_t;

/* --------------------------------------------------------------------------------------------- */

static void
um_term_free (um_term_t *t)
{
    g_free (t->arg);
    g_free (t);
}

/* --------------------------------------------------------------------------------------------- */

/** The conditions of a line of an old menu, one after another; NULL where it is not one. */
static GPtrArray *
um_import_terms (const char *line)
{
    GPtrArray *terms;
    const char *p = line + 1;

    if (*p == '+' || *p == '=')
        p++;

    terms = g_ptr_array_new_with_free_func ((GDestroyNotify) um_term_free);

    while (TRUE)
    {
        um_term_t *t;
        GString *arg;

        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;

        t = g_new0 (um_term_t, 1);
        g_ptr_array_add (terms, t);
        t->op = '=';

        if (terms->len > 1)
        {
            t->op = *p++;
            if (t->op != '&' && t->op != '|')
                goto fail;
            while (*p == ' ' || *p == '\t')
                p++;
        }

        while (*p == '!')
        {
            t->negate = !t->negate;
            p++;
            while (*p == ' ' || *p == '\t')
                p++;
        }

        t->letter = *p;
        if (!g_ascii_isalpha (t->letter))
            goto fail;
        p++;
        while (*p == ' ' || *p == '\t')
            p++;

        arg = g_string_new ("");
        while (*p != '\0' && *p != ' ' && *p != '\t')
        {
            if (*p == '\\' && p[1] == ' ')
                p++;
            g_string_append_c (arg, *p++);
        }
        t->arg = g_string_free (arg, FALSE);
        if (*t->arg == '\0')
            goto fail;
    }

    if (terms->len != 0)
        return terms;

fail:
    g_ptr_array_free (terms, TRUE);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Alternatives of one condition as one key: masks of the path or types,
 * joined by ';'.  The turned over masks come first, for the last mask that
 * matches decides: "not a, or b" is "*;!a;b".
 */
static gboolean
um_import_group (GPtrArray *terms, guint count, gboolean regex, int *key, char **value)
{
    const um_term_t *first = g_ptr_array_index (terms, 0);
    GString *negated, *plain;
    guint i;

    negated = g_string_new ("");
    plain = g_string_new ("");

    for (i = 0; i < count; i++)
    {
        const um_term_t *t = g_ptr_array_index (terms, i);
        GString *to;
        int k, kind;
        char *v;

        if (t->letter != first->letter
            || !um_import_term (t->letter, t->negate, t->arg, regex, &k, &v))
            goto fail;

        if (i == 0)
            *key = k;
        kind = k % UM_COND_OTHER;

        // an expression, exec, marked, needs: no alternatives of them
        if (k != *key || (count > 1 && kind != UM_COND_PATH && kind != UM_COND_ON))
        {
            g_free (v);
            goto fail;
        }

        to = t->negate && kind == UM_COND_PATH ? negated : plain;
        if (to->len != 0)
            g_string_append_c (to, ';');
        g_string_append (to, v);
        g_free (v);
    }

    if (negated->len != 0 && plain->len != 0)
        g_string_append_c (negated, ';');
    g_string_append (negated, plain->str);
    g_string_free (plain, TRUE);

    *value = g_string_free (negated, FALSE);
    return TRUE;

fail:
    g_string_free (negated, TRUE);
    g_string_free (plain, TRUE);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * A "+" line of an old menu as keys.  The old menu reads a line from left to
 * right, so "f a | f b & t r" is (a or b) and r: alternatives of one condition
 * first, then conditions of other keys joined by "&".  Anything else is left
 * out.
 */
static gboolean
um_import_condition (const char *line, gboolean regex, char **cond)
{
    char *found[UM_COND_COUNT] = { NULL };
    GPtrArray *terms;
    guint group, i;
    gboolean ok = TRUE;
    int key, k;
    char *value;

    terms = um_import_terms (line);
    if (terms == NULL)
        return FALSE;

    // the alternatives at the start; after the first '&' there may be no '|'
    for (group = 1; group < terms->len; group++)
        if (((um_term_t *) g_ptr_array_index (terms, group))->op != '|')
            break;
    for (i = group; i < terms->len && ok; i++)
        ok = ((um_term_t *) g_ptr_array_index (terms, i))->op == '&';

    if (ok && (ok = um_import_group (terms, group, regex, &key, &value)))
        found[key] = value;

    for (i = group; i < terms->len && ok; i++)
    {
        const um_term_t *t = g_ptr_array_index (terms, i);

        ok = um_import_term (t->letter, t->negate, t->arg, regex, &key, &value);
        if (ok && found[key] != NULL)
        {
            ok = FALSE;
            g_free (value);
        }
        else if (ok)
            found[key] = value;
    }

    g_ptr_array_free (terms, TRUE);

    // a key a line before this one has set already
    for (k = 0; k < UM_COND_COUNT && ok; k++)
        ok = found[k] == NULL || cond[k] == NULL;

    for (k = 0; k < UM_COND_COUNT; k++)
        if (ok && found[k] != NULL)
            cond[k] = found[k];
        else
            g_free (found[k]);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/** The conditions met above an entry, as keys or as comments. */
static void
um_import_conditions (user_menu_entry_t *entry, GPtrArray *conditions, gboolean regex,
                      guint *not_converted)
{
    GString *comment;
    guint i;

    comment = g_string_new ("");

    for (i = 0; i < conditions->len; i++)
    {
        const char *line = g_ptr_array_index (conditions, i);
        const gboolean shown = line[0] == '+' || line[1] == '+';
        const gboolean is_default = line[0] == '=' || line[1] == '=';
        gboolean ok = TRUE;

        // "+" says where the entry is shown, "=" where the menu opens on it, "=+" both
        if (shown)
            ok = um_import_condition (line, regex, entry->cond);
        if (ok && is_default)
            ok = um_import_condition (line, regex, entry->dcond);

        if (!ok)
        {
            g_string_append_printf (comment,
                                    "# imported from mc.menu, condition not converted: %s\n", line);
            if (not_converted != NULL)
                (*not_converted)++;
        }
    }

    entry->comment = comment->len != 0 ? g_string_free (comment, FALSE) : NULL;
    if (entry->comment == NULL)
        g_string_free (comment, TRUE);

    g_ptr_array_set_size (conditions, 0);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Read a menu file written by hand into entries.  A line that does not start
 * with a space is the title of an entry, its first character the hotkey; the
 * lines under it, indented, are the commands.  The conditions above an entry
 * become its keys where the keys can say them, and comments above it where
 * they cannot.  The file they came from stays where it is.
 */
guint
user_menu_ini_import_file (GPtrArray *entries, const char *file, int level, guint *not_converted)
{
    char *data = NULL;
    gchar **lines;
    guint i, added = 0;
    user_menu_entry_t *entry = NULL;
    GString *command = NULL;
    GPtrArray *conditions;
    gboolean regex = !easy_patterns;
    char *indent = NULL;

    if (!g_file_get_contents (file, &data, NULL, NULL))
        return 0;

    lines = g_strsplit (data, "\n", -1);
    conditions = g_ptr_array_new_with_free_func (g_free);

    for (i = 0; lines[i] != NULL; i++)
    {
        const char *line = lines[i];

        if (*line == ' ' || *line == '\t')
        {
            // a command of the entry above: the indentation of its first line
            // is the one of the entry, what is more belongs to the script
            if (entry != NULL)
            {
                const size_t own = strspn (line, " \t");

                if (indent == NULL)
                    indent = g_strndup (line, own);

                if (g_str_has_prefix (line, indent))
                    line += strlen (indent);
                else
                    line += own;

                if (command->len != 0)
                    g_string_append_c (command, '\n');
                g_string_append (command, line);
            }
            continue;
        }

        // whatever was collected belongs to the entry that is ending here
        if (entry != NULL)
        {
            entry->command = g_string_free (command, FALSE);
            command = NULL;

            // An entry with no commands is a line that only looked like one.
            if (*entry->command == '\0')
                user_menu_entry_free (entry);
            else
            {
                user_menu_ini_label_fix (entries, entry);
                g_ptr_array_add (entries, entry);
                added++;
            }

            entry = NULL;
        }

        if (strncmp (line, "shell_patterns=", 15) == 0)
        {
            regex = line[15] == '0';
            continue;
        }

        if (*line == '+' || *line == '=')
        {
            g_ptr_array_add (conditions, g_strchomp (g_strdup (line)));
            continue;
        }

        if (*line == '\0' || *line == '#')
            continue;

        g_clear_pointer (&indent, g_free);
        entry = g_new0 (user_menu_entry_t, 1);
        entry->hotkey = *line;
        entry->level = level;
        um_import_conditions (entry, conditions, regex, not_converted);

        line++;
        while (*line == ' ' || *line == '\t')
            line++;
        entry->label = g_strchomp (g_strdup (*line != '\0' ? line : lines[i]));
        command = g_string_new ("");
    }

    if (entry != NULL)
    {
        entry->command = g_string_free (command, FALSE);

        if (*entry->command == '\0')
            user_menu_entry_free (entry);
        else
        {
            user_menu_ini_label_fix (entries, entry);
            g_ptr_array_add (entries, entry);
            added++;
        }
    }
    else if (command != NULL)
        g_string_free (command, TRUE);

    g_free (indent);
    g_ptr_array_free (conditions, TRUE);
    g_strfreev (lines);
    g_free (data);

    return added;
}

/* --------------------------------------------------------------------------------------------- */
