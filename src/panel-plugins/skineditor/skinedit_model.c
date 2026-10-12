/*
   Skin editor plugin - a skin opened for editing.

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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/fileloc.h"  // MC_SKINS_DIR
#include "lib/mcconfig.h"
#include "lib/skin.h"  // mc_skin_fallbacks
#include "lib/strutil.h"
#include "lib/util.h"  // exist_file

#include "skinedit_model.h"
#include "skinedit_table.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define ALIAS_MAX_HOPS 32

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

static void entry_update_shown (skinedit_entry_t *e);

/*** file scope variables ************************************************************************/

static const char *const basic_colors[] = {
    "black",       "gray",          "red",    "brightred",  "green",
    "brightgreen", "brown",         "yellow", "blue",       "brightblue",
    "magenta",     "brightmagenta", "cyan",   "brightcyan", "lightgray",
    "white",       "default",       "base",   NULL,
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
is_char_group (const char *group)
{
    return strcmp (group, "lines") == 0 || strncmp (group, "widget-", 7) == 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
is_core_default (const skinedit_entry_t *e)
{
    return strcmp (e->group, "core") == 0 && strcmp (e->key, "_default_") == 0;
}

/* --------------------------------------------------------------------------------------------- */

static skinedit_entry_t *
entry_new (skinedit_kind_t kind, const char *group, const char *key, const char *label,
           const char *description, const char *builtin, gboolean known)
{
    skinedit_entry_t *e;

    e = g_new0 (skinedit_entry_t, 1);
    e->kind = kind;
    e->group = g_strdup (group);
    e->key = g_strdup (key);
    e->label = label != NULL ? label : e->key;
    e->description = description;
    e->builtin = builtin;
    e->known = known;
    return e;
}

/* --------------------------------------------------------------------------------------------- */

static void
entry_free (gpointer data)
{
    skinedit_entry_t *e = (skinedit_entry_t *) data;
    int i;

    for (i = 0; i < SKINEDIT_PARTS; i++)
    {
        g_free (e->raw[i]);
        g_free (e->file_raw[i]);
        g_free (e->effective[i]);
        g_free (e->alias[i]);
    }
    g_free (e->shown);
    g_free (e->group);
    g_free (e->key);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

static skinedit_section_t *
section_new (const char *group, const char *label)
{
    skinedit_section_t *s;

    s = g_new0 (skinedit_section_t, 1);
    s->group = g_strdup (group);
    s->label = label != NULL ? label : s->group;
    s->entries = g_ptr_array_new_with_free_func (entry_free);
    return s;
}

/* --------------------------------------------------------------------------------------------- */

static void
section_free (gpointer data)
{
    skinedit_section_t *s = (skinedit_section_t *) data;

    g_ptr_array_free (s->entries, TRUE);
    g_free (s->group);
    g_free (s);
}

/* --------------------------------------------------------------------------------------------- */

/* the value of a key, NULL when absent or empty; mc_config_get_string_raw would write a default */

static char *
config_value (const skinedit_model_t *m, const char *group, const char *key)
{
    char *s;

    if (!g_key_file_has_key (m->config->handle, group, key, NULL))
        return NULL;

    s = g_key_file_get_string (m->config->handle, group, key, NULL);
    if (s != NULL && *s == '\0')
        MC_PTR_FREE (s);
    return s;
}

/* --------------------------------------------------------------------------------------------- */

/* follow [aliases] from @value; *alias_name is the first alias or NULL; a loop gives @value */

static char *
alias_resolve (const skinedit_model_t *m, const char *value, char **alias_name)
{
    char *cur;
    int hop;

    *alias_name = NULL;
    cur = g_strdup (value);

    for (hop = 0; hop < ALIAS_MAX_HOPS; hop++)
    {
        char *next;

        if (!g_key_file_has_key (m->config->handle, "aliases", cur, NULL))
            return cur;

        // the target is taken as written, blanks included: so does the engine
        next = g_key_file_get_string (m->config->handle, "aliases", cur, NULL);
        if (next == NULL)
            return cur;
        if (*next == '\0')
        {
            g_free (next);
            return cur;
        }
        if (hop == 0)
            *alias_name = g_strdup (cur);
        g_free (cur);
        cur = next;
    }

    // a loop: the engine keeps the name as written
    g_free (cur);
    MC_PTR_FREE (*alias_name);
    return g_strdup (value);
}

/* --------------------------------------------------------------------------------------------- */

/* [Lines] of mc <= 4.8.33 when [lines] has no such key, as the engine reads it */

static const char *
read_group (const skinedit_model_t *m, const skinedit_entry_t *e)
{
    if (strcmp (e->group, "lines") == 0
        && !g_key_file_has_key (m->config->handle, "lines", e->key, NULL)
        && g_key_file_has_key (m->config->handle, "Lines", e->key, NULL))
        return "Lines";
    return e->group;
}

/* --------------------------------------------------------------------------------------------- */

static void
entry_read (const skinedit_model_t *m, skinedit_entry_t *e)
{
    int i;

    for (i = 0; i < SKINEDIT_PARTS; i++)
        MC_PTR_FREE (e->raw[i]);

    if (e->kind == SKINEDIT_ENTRY_COLOR)
    {
        // the same list reader as the engine: ';' separated, escapes honoured, extra items dropped
        gchar **items;
        gsize n = 0;

        items = g_key_file_get_string_list (m->config->handle, e->group, e->key, &n, NULL);
        for (i = 0; i < SKINEDIT_PARTS && i < (int) n; i++)
        {
            g_strstrip (items[i]);
            if (items[i][0] != '\0')
                e->raw[i] = g_strdup (items[i]);
        }
        g_strfreev (items);
    }
    else
    {
        char *s;

        s = config_value (m, read_group (m, e), e->key);
        if (s != NULL)
            e->raw[0] = s;
    }
    entry_update_shown (e);
}

/* --------------------------------------------------------------------------------------------- */

static void
entry_write (const skinedit_model_t *m, const skinedit_entry_t *e)
{
    char *s;

    if (e->kind == SKINEDIT_ENTRY_COLOR)
        s = skinedit_color_join (e->raw);
    else
        s = g_strdup (e->raw[0]);
    if (s == NULL && is_core_default (e))
        s = g_strdup ("default;default");

    if (s == NULL)
        g_key_file_remove_key (m->config->handle, e->group, e->key, NULL);
    else
        g_key_file_set_string (m->config->handle, e->group, e->key, s);
    // the value now lives in [lines]; a legacy [Lines] copy would win in the engine
    if (strcmp (e->group, "lines") == 0)
        g_key_file_remove_key (m->config->handle, "Lines", e->key, NULL);
    g_free (s);
}

/* --------------------------------------------------------------------------------------------- */

static void
entry_update_shown (skinedit_entry_t *e)
{
    const char *v;

    if (e->kind == SKINEDIT_ENTRY_COLOR)
        return;
    v = e->raw[0] != NULL ? e->raw[0] : e->builtin;
    g_free (e->shown);
    e->shown = v != NULL ? skinedit_text_from_skin (v) : g_strdup ("");
}

/* --------------------------------------------------------------------------------------------- */

static void
entry_set_baseline (skinedit_entry_t *e)
{
    int i;

    for (i = 0; i < SKINEDIT_PARTS; i++)
    {
        g_free (e->file_raw[i]);
        e->file_raw[i] = g_strdup (e->raw[i]);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
entry_resolve_part (const skinedit_model_t *m, skinedit_entry_t *e, int i)
{
    const skinedit_entry_t *d;
    const char *v = e->raw[i];
    gboolean is_default = strcmp (e->key, "_default_") == 0;

    MC_PTR_FREE (e->effective[i]);
    MC_PTR_FREE (e->alias[i]);

    if (v != NULL && strcmp (v, "base") != 0)
    {
        e->effective[i] = alias_resolve (m, v, &e->alias[i]);
        e->src[i] = SKINEDIT_SRC_SET;
        return;
    }

    if (v == NULL)
    {
        const mc_skin_fallback_t *fb;
        size_t n, k;

        fb = mc_skin_fallbacks (&n);
        for (k = 0; k < n; k++)
            if (strcmp (fb[k].group, e->group) == 0 && strcmp (fb[k].key, e->key) == 0)
                break;
        if (k < n)
        {
            // the engine acts only when the key is absent as a whole
            gboolean absent = TRUE;
            int j;

            for (j = 0; j < SKINEDIT_PARTS; j++)
                if (e->raw[j] != NULL)
                    absent = FALSE;
            if (absent)
            {
                if (fb[k].fg != NULL)
                {
                    // the foreground given, the group's background, no attributes
                    if (i == SKINEDIT_PART_FG)
                        e->effective[i] = g_strdup (fb[k].fg);
                    else if (i == SKINEDIT_PART_BG)
                    {
                        d = skinedit_model_find (m, e->group, "_default_");
                        if (d != NULL && d->effective[i] != NULL)
                            e->effective[i] = g_strdup (d->effective[i]);
                    }
                }
                else
                {
                    d = skinedit_model_find (m, fb[k].src_group, fb[k].src_key);
                    if (i == SKINEDIT_PART_ATTRS && fb[k].attrs != NULL)
                        e->effective[i] = g_strdup (fb[k].attrs);
                    else if (d != NULL && d->effective[i] != NULL)
                        e->effective[i] = g_strdup (d->effective[i]);
                }
                e->src[i] = SKINEDIT_SRC_FALLBACK;
                return;
            }
        }
    }

    if (!is_default && strcmp (e->group, "core") != 0 && v == NULL)
    {
        d = skinedit_model_find (m, e->group, "_default_");
        if (d != NULL && d->raw[i] != NULL && strcmp (d->raw[i], "base") != 0)
        {
            e->effective[i] = alias_resolve (m, d->raw[i], &e->alias[i]);
            e->src[i] = SKINEDIT_SRC_GROUP_DEFAULT;
            return;
        }
    }

    d = skinedit_model_find (m, "core", "_default_");
    if (d != NULL && d != e && d->raw[i] != NULL && strcmp (d->raw[i], "base") != 0)
    {
        e->effective[i] = alias_resolve (m, d->raw[i], &e->alias[i]);
        e->src[i] = SKINEDIT_SRC_CORE_DEFAULT;
        return;
    }

    e->src[i] = SKINEDIT_SRC_TERMINAL;
}

/* --------------------------------------------------------------------------------------------- */

static void
model_resolve (skinedit_model_t *m)
{
    guint si, ei;
    int pass;

    // a fallback copies another key's effective value: a second pass sees them all resolved
    for (pass = 0; pass < 2; pass++)
        for (si = 0; si < m->sections->len; si++)
        {
            skinedit_section_t *s = g_ptr_array_index (m->sections, si);

            for (ei = 0; ei < s->entries->len; ei++)
            {
                skinedit_entry_t *e = g_ptr_array_index (s->entries, ei);
                int i;

                if (e->kind != SKINEDIT_ENTRY_COLOR)
                    continue;
                for (i = 0; i < SKINEDIT_PARTS; i++)
                    entry_resolve_part (m, e, i);
            }
        }
}

/* --------------------------------------------------------------------------------------------- */

/* the section of @group: the one it is primary for, else the first with an entry of it */

static skinedit_section_t *
section_for_group (const skinedit_model_t *m, const char *group)
{
    guint si, ei;

    for (si = 0; si < m->sections->len; si++)
    {
        skinedit_section_t *s = g_ptr_array_index (m->sections, si);

        if (strcmp (s->group, group) == 0)
            return s;
    }
    for (si = 0; si < m->sections->len; si++)
    {
        skinedit_section_t *s = g_ptr_array_index (m->sections, si);

        for (ei = 0; ei < s->entries->len; ei++)
        {
            const skinedit_entry_t *e = g_ptr_array_index (s->entries, ei);

            if (strcmp (e->group, group) == 0)
                return s;
        }
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
model_build (skinedit_model_t *m)
{
    size_t ti;
    gchar **groups, **g;

    m->sections = g_ptr_array_new_with_free_func (section_free);

    for (ti = 0; ti < skinedit_table_count; ti++)
    {
        const skinedit_table_section_t *ts = &skinedit_table[ti];
        skinedit_section_t *s;
        size_t ri;

        s = section_new (ts->rows[0].group, _ (ts->label));
        for (ri = 0; ri < ts->nrows; ri++)
        {
            const skinedit_table_row_t *r = &ts->rows[ri];
            skinedit_entry_t *e;

            e = entry_new (r->kind, r->group, r->key, _ (r->label),
                           r->description != NULL ? _ (r->description) : NULL, r->builtin, TRUE);
            g_ptr_array_add (s->entries, e);
        }
        g_ptr_array_add (m->sections, s);
    }

    // keys of the file the table does not know
    groups = g_key_file_get_groups (m->config->handle, NULL);
    for (g = groups; g != NULL && *g != NULL; g++)
    {
        gchar **keys, **k;
        skinedit_section_t *s;

        if (strcmp (*g, "skin") == 0 || strcmp (*g, "aliases") == 0 || strcmp (*g, "Lines") == 0)
            continue;

        keys = g_key_file_get_keys (m->config->handle, *g, NULL, NULL);
        s = NULL;
        for (k = keys; k != NULL && *k != NULL; k++)
        {
            skinedit_entry_t *e;

            if (skinedit_model_find (m, *g, *k) != NULL)
                continue;
            if (s == NULL)
            {
                s = section_for_group (m, *g);
                if (s == NULL)
                {
                    s = section_new (*g, NULL);
                    g_ptr_array_add (m->sections, s);
                }
            }
            e = entry_new (is_char_group (*g) ? SKINEDIT_ENTRY_CHAR : SKINEDIT_ENTRY_COLOR, *g, *k,
                           NULL, NULL, NULL, FALSE);
            g_ptr_array_add (s->entries, e);
        }
        g_strfreev (keys);
    }
    g_strfreev (groups);

    {
        guint si, ei;

        for (si = 0; si < m->sections->len; si++)
        {
            skinedit_section_t *s = g_ptr_array_index (m->sections, si);

            for (ei = 0; ei < s->entries->len; ei++)
            {
                skinedit_entry_t *e = g_ptr_array_index (s->entries, ei);

                entry_read (m, e);
                entry_set_baseline (e);
            }
        }
    }

    model_resolve (m);
}

/* --------------------------------------------------------------------------------------------- */

static char *
user_skins_dir (void)
{
    const char *data;

    data = mc_config_get_data_path ();
    if (data == NULL)
        return NULL;
    return g_build_filename (data, MC_SKINS_DIR, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
path_is_user (const char *path)
{
    char *dir;
    gboolean ret;

    dir = user_skins_dir ();
    if (dir == NULL)
        return FALSE;
    ret = g_str_has_prefix (path, dir) && path[strlen (dir)] == PATH_SEP;
    g_free (dir);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static char *
name_from_path (const char *path)
{
    char *base;
    size_t len;

    base = g_path_get_basename (path);
    len = strlen (base);
    if (len > 4 && strcmp (base + len - 4, ".ini") == 0)
        base[len - 4] = '\0';
    return base;
}

/* --------------------------------------------------------------------------------------------- */

static char *
find_in_dir (const char *base_dir, const char *name)
{
    char *path, *ini;

    if (base_dir == NULL)
        return NULL;

    path = g_build_filename (base_dir, MC_SKINS_DIR, name, (char *) NULL);
    if (exist_file (path))
        return path;
    g_free (path);

    ini = g_strdup_printf ("%s.ini", name);
    path = g_build_filename (base_dir, MC_SKINS_DIR, ini, (char *) NULL);
    g_free (ini);
    if (exist_file (path))
        return path;
    g_free (path);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* [skin] 256colors / truecolors from the skin's class */

static void
model_update_flags (const skinedit_model_t *m, GKeyFile *kf)
{
    skinedit_color_class_t cls = skinedit_model_class (m);

    g_key_file_remove_key (kf, "skin", "256colors", NULL);
    g_key_file_remove_key (kf, "skin", "truecolors", NULL);
    if (cls == SKINEDIT_COLOR_TRUECOLOR)
        g_key_file_set_boolean (kf, "skin", "truecolors", TRUE);
    else if (cls == SKINEDIT_COLOR_256)
        g_key_file_set_boolean (kf, "skin", "256colors", TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
skinedit_color_join (char *const parts[SKINEDIT_PARTS])
{
    int last, i;
    GString *s;

    for (last = SKINEDIT_PARTS - 1; last >= 0 && parts[last] == NULL; last--)
        ;
    if (last < 0)
        return NULL;

    s = g_string_new (NULL);
    for (i = 0; i <= last; i++)
    {
        if (i > 0)
            g_string_append_c (s, ';');
        if (parts[i] != NULL)
            g_string_append (s, parts[i]);
    }
    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static char *
convert_text (GIConv conv, const char *text)
{
    GString *buffer;

    if (conv == INVALID_CONV)
        return g_strdup (text);
    buffer = g_string_new ("");
    if (str_convert (conv, text, buffer) == ESTR_FAILURE)
    {
        g_string_free (buffer, TRUE);
        str_close_conv (conv);
        return g_strdup (text);
    }
    str_close_conv (conv);
    return g_string_free (buffer, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

char *
skinedit_text_from_skin (const char *utf8)
{
    if (mc_global.utf8_display)
        return g_strdup (utf8);
    return convert_text (str_crt_conv_from ("UTF-8"), utf8);
}

/* --------------------------------------------------------------------------------------------- */

char *
skinedit_text_to_skin (const char *text)
{
    if (mc_global.utf8_display)
        return g_strdup (text);
    return convert_text (str_crt_conv_to ("UTF-8"), text);
}

/* --------------------------------------------------------------------------------------------- */

skinedit_color_class_t
skinedit_color_classify (const char *value)
{
    int i, n;
    char dummy;

    if (value == NULL)
        return SKINEDIT_COLOR_BASIC;

    for (i = 0; basic_colors[i] != NULL; i++)
        if (strcmp (value, basic_colors[i]) == 0)
            return SKINEDIT_COLOR_BASIC;

    if (sscanf (value, "color%d%c", &n, &dummy) == 1 && n >= 0 && n < 256)
        return n < 16 ? SKINEDIT_COLOR_BASIC : SKINEDIT_COLOR_256;
    if (sscanf (value, "gray%d%c", &n, &dummy) == 1 && n >= 0 && n < 24)
        return SKINEDIT_COLOR_256;
    if (strncmp (value, "rgb", 3) == 0 && strlen (value) == 6 && value[3] >= '0' && value[3] < '6'
        && value[4] >= '0' && value[4] < '6' && value[5] >= '0' && value[5] < '6')
        return SKINEDIT_COLOR_256;

    if (value[0] == '#')
    {
        size_t len = strlen (value + 1);

        if (len == 3 || len == 6)
        {
            size_t k;

            for (k = 1; k <= len; k++)
                if (!g_ascii_isxdigit (value[k]))
                    return SKINEDIT_COLOR_UNKNOWN;
            return SKINEDIT_COLOR_TRUECOLOR;
        }
    }

    return SKINEDIT_COLOR_UNKNOWN;
}

/* --------------------------------------------------------------------------------------------- */

skinedit_model_t *
skinedit_model_open_file (const char *path, const char *name, GError **error)
{
    skinedit_model_t *m;

    if (!exist_file (path))
    {
        g_set_error (error, MC_ERROR, -1, _ ("Skin file %s does not exist"), path);
        return NULL;
    }

    {
        // mc_config_init hides a parse error and keeps what came before it: check first
        GKeyFile *probe = g_key_file_new ();
        GError *perror = NULL;

        if (!g_key_file_load_from_file (probe, path, G_KEY_FILE_NONE, &perror))
        {
            g_set_error (error, MC_ERROR, -1, _ ("Cannot read skin file %s:\n%s"), path,
                         perror->message);
            g_error_free (perror);
            g_key_file_free (probe);
            return NULL;
        }
        g_key_file_free (probe);
    }

    m = g_new0 (skinedit_model_t, 1);
    m->config = mc_config_init (path, FALSE);
    if (m->config == NULL)
    {
        g_set_error (error, MC_ERROR, -1, _ ("Cannot read skin file %s"), path);
        g_free (m);
        return NULL;
    }

    m->path = g_strdup (path);
    m->name = name != NULL ? g_strdup (name) : name_from_path (path);
    m->system = !path_is_user (path);
    m->description = config_value (m, "skin", "description");
    if (m->description == NULL)
        m->description = g_strdup ("");
    if (mc_config_get_bool (m->config, "skin", "truecolors", FALSE))
        m->colors = SKINEDIT_COLOR_TRUECOLOR;
    else if (mc_config_get_bool (m->config, "skin", "256colors", FALSE))
        m->colors = SKINEDIT_COLOR_256;
    else
        m->colors = SKINEDIT_COLOR_BASIC;
    m->file_colors = m->colors;

    model_build (m);
    return m;
}

/* --------------------------------------------------------------------------------------------- */

skinedit_model_t *
skinedit_model_open (const char *name, GError **error)
{
    char *path = NULL;
    skinedit_model_t *m;

    if (g_path_is_absolute (name))
        return skinedit_model_open_file (name, NULL, error);

    {
        char *user_dir;

        user_dir =
            mc_config_get_data_path () != NULL ? g_strdup (mc_config_get_data_path ()) : NULL;
        path = find_in_dir (user_dir, name);
        g_free (user_dir);
    }
    if (path == NULL)
        path = find_in_dir (mc_global.sysconfig_dir, name);
    if (path == NULL)
        path = find_in_dir (mc_global.share_data_dir, name);
    if (path == NULL)
    {
        g_set_error (error, MC_ERROR, -1, _ ("Skin %s not found"), name);
        return NULL;
    }

    m = skinedit_model_open_file (path, name, error);
    g_free (path);
    return m;
}

/* --------------------------------------------------------------------------------------------- */

void
skinedit_model_free (skinedit_model_t *m)
{
    if (m == NULL)
        return;
    if (m->sections != NULL)
        g_ptr_array_free (m->sections, TRUE);
    mc_config_deinit (m->config);
    g_free (m->name);
    g_free (m->path);
    g_free (m->description);
    g_free (m);
}

/* --------------------------------------------------------------------------------------------- */

skinedit_entry_t *
skinedit_model_find (const skinedit_model_t *m, const char *group, const char *key)
{
    guint si, ei;

    for (si = 0; si < m->sections->len; si++)
    {
        const skinedit_section_t *s = g_ptr_array_index (m->sections, si);

        for (ei = 0; ei < s->entries->len; ei++)
        {
            skinedit_entry_t *e = g_ptr_array_index (s->entries, ei);

            if (strcmp (e->group, group) == 0 && strcmp (e->key, key) == 0)
                return e;
        }
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

skinedit_section_t *
skinedit_model_find_section (const skinedit_model_t *m, const char *group)
{
    return section_for_group (m, group);
}

/* --------------------------------------------------------------------------------------------- */

void
skinedit_model_set (skinedit_model_t *m, skinedit_entry_t *e, skinedit_part_t part,
                    const char *value)
{
    char *v = NULL;

    if (value != NULL)
    {
        v = g_strstrip (g_strdup (value));
        if (*v == '\0')
            MC_PTR_FREE (v);
    }
    if (v == NULL && part != SKINEDIT_PART_ATTRS && is_core_default (e))
        v = g_strdup ("default");
    g_free (e->raw[part]);
    e->raw[part] = v;
    entry_write (m, e);
    model_resolve (m);
}

/* --------------------------------------------------------------------------------------------- */

void
skinedit_model_set_text_raw (skinedit_model_t *m, skinedit_entry_t *e, const char *utf8)
{
    char *v = NULL;

    if (utf8 != NULL && *utf8 != '\0' && g_strcmp0 (utf8, e->builtin) != 0)
        v = g_strdup (utf8);
    g_free (e->raw[0]);
    e->raw[0] = v;
    entry_update_shown (e);
    entry_write (m, e);
}

/* --------------------------------------------------------------------------------------------- */

void
skinedit_model_set_text (skinedit_model_t *m, skinedit_entry_t *e, const char *value)
{
    char *utf8;

    utf8 = value != NULL ? skinedit_text_to_skin (value) : NULL;
    skinedit_model_set_text_raw (m, e, utf8);
    g_free (utf8);
}

/* --------------------------------------------------------------------------------------------- */

void
skinedit_model_reset (skinedit_model_t *m, skinedit_entry_t *e)
{
    int i;

    for (i = 0; i < SKINEDIT_PARTS; i++)
    {
        g_free (e->raw[i]);
        e->raw[i] = g_strdup (e->file_raw[i]);
    }
    entry_update_shown (e);
    entry_write (m, e);
    model_resolve (m);
}

/* --------------------------------------------------------------------------------------------- */

void
skinedit_model_reset_all (skinedit_model_t *m)
{
    guint si, ei;

    m->colors = m->file_colors;
    for (si = 0; si < m->sections->len; si++)
    {
        skinedit_section_t *s = g_ptr_array_index (m->sections, si);

        for (ei = 0; ei < s->entries->len; ei++)
        {
            skinedit_entry_t *e = g_ptr_array_index (s->entries, ei);
            int i;

            for (i = 0; i < SKINEDIT_PARTS; i++)
            {
                g_free (e->raw[i]);
                e->raw[i] = g_strdup (e->file_raw[i]);
            }
            entry_update_shown (e);
            entry_write (m, e);
        }
    }
    model_resolve (m);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
skinedit_entry_changed (const skinedit_entry_t *e)
{
    int i;

    for (i = 0; i < SKINEDIT_PARTS; i++)
        if (g_strcmp0 (e->raw[i], e->file_raw[i]) != 0)
            return TRUE;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
skinedit_section_changed (const skinedit_section_t *s)
{
    guint ei;

    for (ei = 0; ei < s->entries->len; ei++)
        if (skinedit_entry_changed (g_ptr_array_index (s->entries, ei)))
            return TRUE;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
skinedit_model_dirty (const skinedit_model_t *m)
{
    guint si;

    if (m->colors != m->file_colors)
        return TRUE;
    for (si = 0; si < m->sections->len; si++)
        if (skinedit_section_changed (g_ptr_array_index (m->sections, si)))
            return TRUE;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

mc_config_t *
skinedit_model_config_copy (const skinedit_model_t *m)
{
    mc_config_t *copy;
    gchar *data;
    gsize len = 0;

    copy = mc_config_init (NULL, TRUE);
    if (copy == NULL)
        return NULL;
    data = g_key_file_to_data (m->config->handle, &len, NULL);
    if (data != NULL)
        g_key_file_load_from_data (copy->handle, data, len, G_KEY_FILE_KEEP_COMMENTS, NULL);
    g_free (data);
    // the 256colors / truecolors flags from the skin's class, raised to what its colors need
    model_update_flags (m, copy->handle);
    return copy;
}

/* --------------------------------------------------------------------------------------------- */

void
skinedit_model_needs (const skinedit_model_t *m, gboolean *needs_256, gboolean *needs_true)
{
    guint si, ei;

    *needs_256 = FALSE;
    *needs_true = FALSE;
    for (si = 0; si < m->sections->len; si++)
    {
        const skinedit_section_t *s = g_ptr_array_index (m->sections, si);

        for (ei = 0; ei < s->entries->len; ei++)
        {
            const skinedit_entry_t *e = g_ptr_array_index (s->entries, ei);
            int i;

            if (e->kind != SKINEDIT_ENTRY_COLOR)
                continue;
            for (i = SKINEDIT_PART_FG; i <= SKINEDIT_PART_BG; i++)
            {
                char *v, *alias;

                if (e->raw[i] == NULL)
                    continue;
                v = alias_resolve (m, e->raw[i], &alias);
                switch (skinedit_color_classify (v))
                {
                case SKINEDIT_COLOR_256:
                    *needs_256 = TRUE;
                    break;
                case SKINEDIT_COLOR_TRUECOLOR:
                    *needs_true = TRUE;
                    break;
                default:
                    break;
                }
                g_free (v);
                g_free (alias);
            }
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

skinedit_color_class_t
skinedit_model_class (const skinedit_model_t *m)
{
    gboolean has_256, has_true;

    skinedit_model_needs (m, &has_256, &has_true);
    if (has_true)
        return SKINEDIT_COLOR_TRUECOLOR;
    if (has_256 && m->colors < SKINEDIT_COLOR_256)
        return SKINEDIT_COLOR_256;
    return m->colors;
}

/* --------------------------------------------------------------------------------------------- */

skinedit_entry_t *
skinedit_model_over_class (const skinedit_model_t *m, skinedit_color_class_t cls,
                           skinedit_part_t *part)
{
    skinedit_entry_t *best = NULL;
    skinedit_color_class_t best_cls = cls;
    guint si, ei;

    for (si = 0; si < m->sections->len; si++)
    {
        const skinedit_section_t *s = g_ptr_array_index (m->sections, si);

        for (ei = 0; ei < s->entries->len; ei++)
        {
            skinedit_entry_t *e = g_ptr_array_index (s->entries, ei);
            int i;

            if (e->kind != SKINEDIT_ENTRY_COLOR)
                continue;
            for (i = SKINEDIT_PART_FG; i <= SKINEDIT_PART_BG; i++)
            {
                char *v, *alias;
                skinedit_color_class_t c;

                if (e->raw[i] == NULL)
                    continue;
                v = alias_resolve (m, e->raw[i], &alias);
                c = skinedit_color_classify (v);
                g_free (v);
                g_free (alias);
                if ((c == SKINEDIT_COLOR_256 || c == SKINEDIT_COLOR_TRUECOLOR) && c > best_cls)
                {
                    best = e;
                    best_cls = c;
                    *part = (skinedit_part_t) i;
                    if (c == SKINEDIT_COLOR_TRUECOLOR)
                        return best;
                }
            }
        }
    }
    return best;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
skinedit_model_name_ok (const char *name)
{
    size_t len;

    if (name == NULL || *name == '\0' || strchr (name, PATH_SEP) != NULL || strcmp (name, ".") == 0
        || strcmp (name, "..") == 0)
        return FALSE;
    len = strlen (name);
    return !(len >= 4 && strcmp (name + len - 4, ".ini") == 0);
}

/* --------------------------------------------------------------------------------------------- */

char *
skinedit_model_user_path (const char *name)
{
    char *dir, *ini, *path;

    if (!skinedit_model_name_ok (name))
        return NULL;
    dir = user_skins_dir ();
    if (dir == NULL)
        return NULL;
    ini = g_strdup_printf ("%s.ini", name);
    path = g_build_filename (dir, ini, (char *) NULL);
    g_free (ini);
    g_free (dir);
    return path;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
skinedit_model_save_to (skinedit_model_t *m, const char *path, const char *name,
                        const char *description, GError **error)
{
    mc_config_t *out;
    char *dir;
    guint si, ei;

    // written from a copy: a failure leaves the model as it was
    out = skinedit_model_config_copy (m);
    if (out == NULL)
        return FALSE;
    if (description != NULL)
        g_key_file_set_string (out->handle, "skin", "description", description);

    dir = g_path_get_dirname (path);
    if (g_mkdir_with_parents (dir, 0755) != 0)
    {
        g_set_error (error, MC_ERROR, -1, _ ("Cannot create directory %s"), dir);
        g_free (dir);
        mc_config_deinit (out);
        return FALSE;
    }
    g_free (dir);

    if (!mc_config_save_to_file (out, path, error))
    {
        mc_config_deinit (out);
        return FALSE;
    }

    // what the file has now, the model has too
    if (description != NULL)
    {
        g_key_file_set_string (m->config->handle, "skin", "description", description);
        g_free (m->description);
        m->description = g_strdup (description);
    }
    model_update_flags (m, m->config->handle);
    mc_config_deinit (out);
    m->colors = skinedit_model_class (m);
    m->file_colors = m->colors;

    g_free (m->path);
    m->path = g_strdup (path);
    if (name != NULL)
    {
        g_free (m->name);
        m->name = g_strdup (name);
    }
    m->system = !path_is_user (path);

    for (si = 0; si < m->sections->len; si++)
    {
        skinedit_section_t *s = g_ptr_array_index (m->sections, si);

        for (ei = 0; ei < s->entries->len; ei++)
            entry_set_baseline (g_ptr_array_index (s->entries, ei));
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
skinedit_model_save (skinedit_model_t *m, const char *name, const char *description, GError **error)
{
    char *path;
    gboolean ret;

    if (!skinedit_model_name_ok (name))
    {
        g_set_error (error, MC_ERROR, -1,
                     _ ("\"%s\" is not a skin name: no directory part, not .ini"), name);
        return FALSE;
    }
    path = skinedit_model_user_path (name);
    if (path == NULL)
    {
        g_set_error (error, MC_ERROR, -1, "%s", _ ("Cannot find the user's skins directory"));
        return FALSE;
    }
    ret = skinedit_model_save_to (m, path, name, description, error);
    g_free (path);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
