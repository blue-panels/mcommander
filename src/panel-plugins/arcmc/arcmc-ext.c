/*
   Archive browser panel plugin -external archiver registry.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

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

#include <string.h>

#include "lib/global.h"
#include "lib/mcconfig.h"

#include "arcmc-types.h"
#include "arcmc-config.h"
#include "arcmc-ext.h"

/*** file scope macro definitions ****************************************************************/

#define ARCMC_SECTION_EXT       "arcmc-ext"
#define ARCMC_SECTION_EXT_PARAM "arcmc-ext-params-"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    const char *name;
    const char *ext;
    const char *pack_bin;
    const char *pack_args;
    const char *unpack_bin;
    const char *unpack_args;
    const char *test_bin;
    const char *test_args;
    const char *extfs_helper;
    const char *list_file_arg;
} arcmc_ext_archiver_default_t;

/*** file scope variables ************************************************************************/

static const arcmc_ext_archiver_default_t ext_archiver_defaults[] = {
    { "RAR", ".rar", "rar", "a -r", "unrar", "x -o+", "unrar", "t", "urar", "@%s" },
    { "ARJ", ".arj", "arj", "a -r", "arj", "x -y", "arj", "t", "uarj", "!%s" },
    { "ACE", ".ace", NULL, NULL, "unace", "x -o", "unace", "t", "uace", NULL },
    { "ARC", ".arc", "arc", "a", "arc", "x", NULL, NULL, "uarc", NULL },
    { "ALZ", ".alz", NULL, NULL, "unalz", "", NULL, NULL, "ualz", NULL },
    { "ZOO", ".zoo", "zoo", "a", "zoo", "x", NULL, NULL, "uzoo", NULL },
    { "HA", ".ha", "ha", "a", "ha", "x", "ha", "t", "uha", NULL },
    { "WIM", ".wim", NULL, NULL, "wimlib-imagex", "extract", NULL, NULL, "uwim", NULL },
    { "LHA", ".lha", "lha", "a", "lha", "x", "lha", "t", "ulha", "@%s" },
    { "LZH", ".lzh", "lha", "a", "lha", "x", "lha", "t", "ulha", "@%s" },
    { "DEB", ".deb", NULL, NULL, "dpkg-deb", "-x", NULL, NULL, "deb", NULL },
    { "INO", ".exe", NULL, NULL, NULL, NULL, "innoextract", "--test --silent", "uinno", NULL },
};

/*** global variables ****************************************************************************/

arcmc_ext_archiver_t *ext_archivers = NULL;
size_t ext_archivers_count = 0;

/*** file scope functions ************************************************************************/

static void
arcmc_ext_archiver_clear (arcmc_ext_archiver_t *a)
{
    g_free (a->name);
    g_free (a->ext);
    g_free (a->pack_bin);
    g_free (a->pack_args);
    g_free (a->unpack_bin);
    g_free (a->unpack_args);
    g_free (a->test_bin);
    g_free (a->test_args);
    g_free (a->extfs_helper);
    g_free (a->list_file_arg);
}

/* --------------------------------------------------------------------------------------------- */

/* The name is written back as a key of [arcmc-ext], so it has to be one GKeyFile
   accepts: no '=', '[' or ']', no control characters, no blank at either end. */
static gboolean
arcmc_ext_name_is_valid (const char *name)
{
    const char *p;

    if (name == NULL || name[0] == '\0' || g_ascii_isspace (name[0])
        || g_ascii_isspace (name[strlen (name) - 1]))
        return FALSE;

    for (p = name; *p != '\0'; p++)
        if (*p == '=' || *p == '[' || *p == ']' || g_ascii_iscntrl (*p))
            return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static arcmc_ext_archiver_t *
arcmc_ext_archiver_append (const char *name, const char *extension)
{
    arcmc_ext_archiver_t *a;

    ext_archivers = g_renew (arcmc_ext_archiver_t, ext_archivers, ext_archivers_count + 1);
    a = &ext_archivers[ext_archivers_count++];
    memset (a, 0, sizeof (*a));
    a->name = g_ascii_strup (name, -1);
    a->ext = g_strdup (extension);
    a->enabled = TRUE;

    return a;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
arcmc_ext_preferred_name (gchar **names, const char *canonical)
{
    gchar **name;
    const char *fallback = NULL;

    if (names == NULL)
        return NULL;

    for (name = names; *name != NULL; name++)
    {
        if (strcmp (*name, canonical) == 0)
            return *name;
        if (fallback == NULL && g_ascii_strcasecmp (*name, canonical) == 0)
            fallback = *name;
    }

    return fallback;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
arcmc_ext_group_is_preferred (gchar **groups, const char *section)
{
    const char *name = section + strlen (ARCMC_SECTION_EXT_PARAM);
    char *canonical_name = g_ascii_strup (name, -1);
    char *canonical_section = g_strconcat (ARCMC_SECTION_EXT_PARAM, canonical_name, (char *) NULL);
    const char *preferred = arcmc_ext_preferred_name (groups, canonical_section);
    gboolean result = preferred != NULL && strcmp (preferred, section) == 0;

    g_free (canonical_section);
    g_free (canonical_name);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
arcmc_ext_load_enabled (mc_config_t *cfg, gchar **keys, const char *name)
{
    const char *key = arcmc_ext_preferred_name (keys, name);

    return key == NULL || mc_config_get_bool (cfg, ARCMC_SECTION_EXT, key, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static arcmc_ext_archiver_t *
arcmc_ext_archiver_by_name_mutable (const char *name)
{
    size_t i;

    for (i = 0; i < ext_archivers_count; i++)
        if (g_ascii_strcasecmp (ext_archivers[i].name, name) == 0)
            return &ext_archivers[i];

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static char *
arcmc_ext_normalize_extension (char *extension)
{
    char *normalized;

    if (extension == NULL)
        return NULL;

    g_strstrip (extension);
    if (extension[0] == '\0' || strcmp (extension, ".") == 0 || strchr (extension, '/') != NULL)
    {
        g_free (extension);
        return NULL;
    }

    if (extension[0] == '.')
        return extension;

    normalized = g_strconcat (".", extension, (char *) NULL);
    g_free (extension);
    return normalized;
}

/* --------------------------------------------------------------------------------------------- */

static char *
arcmc_ext_load_extension (mc_config_t *cfg, const char *section)
{
    char *extension;

    extension = mc_config_get_string (cfg, section, "extension", NULL);
    if (extension == NULL)
        extension = mc_config_get_string (cfg, section, "ext", NULL);

    return arcmc_ext_normalize_extension (extension);
}

/* --------------------------------------------------------------------------------------------- */

static void
arcmc_ext_load_string (mc_config_t *cfg, const char *section, const char *key, char **field)
{
    char *value;

    value = mc_config_get_string (cfg, section, key, NULL);
    if (value == NULL)
        return;

    g_free (*field);
    if (value[0] != '\0')
        *field = value;
    else
    {
        g_free (value);
        *field = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
arcmc_ext_load_group (mc_config_t *cfg, const char *section)
{
    const char *name = section + strlen (ARCMC_SECTION_EXT_PARAM);
    arcmc_ext_archiver_t *a;
    char *extension;

    if (!arcmc_ext_name_is_valid (name))
        return;

    a = arcmc_ext_archiver_by_name_mutable (name);
    extension = arcmc_ext_load_extension (cfg, section);

    /* A previously unknown name becomes a registry row when its suffix is supplied. */
    if (a == NULL)
    {
        if (extension == NULL)
            return;
        a = arcmc_ext_archiver_append (name, extension);
    }
    else if (extension != NULL)
    {
        g_free (a->ext);
        a->ext = g_strdup (extension);
    }

    g_free (extension);
    arcmc_ext_load_string (cfg, section, "pack_bin", &a->pack_bin);
    arcmc_ext_load_string (cfg, section, "pack_args", &a->pack_args);
    arcmc_ext_load_string (cfg, section, "unpack_bin", &a->unpack_bin);
    arcmc_ext_load_string (cfg, section, "unpack_args", &a->unpack_args);
    arcmc_ext_load_string (cfg, section, "test_bin", &a->test_bin);
    arcmc_ext_load_string (cfg, section, "test_args", &a->test_args);
    arcmc_ext_load_string (cfg, section, "extfs_helper", &a->extfs_helper);
    arcmc_ext_load_string (cfg, section, "list_file_arg", &a->list_file_arg);
}

/* --------------------------------------------------------------------------------------------- */

static void
arcmc_ext_save_string (mc_config_t *cfg, const char *section, const char *key, const char *value)
{
    mc_config_set_string (cfg, section, key, value != NULL ? value : "");
}

/*** public functions ****************************************************************************/

void
arcmc_ext_archivers_load (mc_config_t *cfg)
{
    gchar **groups = NULL;
    size_t i;

    arcmc_ext_archivers_free ();

    for (i = 0; i < G_N_ELEMENTS (ext_archiver_defaults); i++)
    {
        const arcmc_ext_archiver_default_t *d = &ext_archiver_defaults[i];
        arcmc_ext_archiver_t *a = arcmc_ext_archiver_append (d->name, d->ext);

        a->pack_bin = g_strdup (d->pack_bin);
        a->pack_args = g_strdup (d->pack_args);
        a->unpack_bin = g_strdup (d->unpack_bin);
        a->unpack_args = g_strdup (d->unpack_args);
        a->test_bin = g_strdup (d->test_bin);
        a->test_args = g_strdup (d->test_args);
        a->extfs_helper = g_strdup (d->extfs_helper);
        a->list_file_arg = g_strdup (d->list_file_arg);
    }

    if (cfg != NULL)
    {
        gchar **group;

        groups = mc_config_get_groups (cfg, NULL);
        for (group = groups; *group != NULL; group++)
            if (g_str_has_prefix (*group, ARCMC_SECTION_EXT_PARAM)
                && arcmc_ext_group_is_preferred (groups, *group))
                arcmc_ext_load_group (cfg, *group);
    }

    {
        gchar **keys = cfg != NULL ? mc_config_get_keys (cfg, ARCMC_SECTION_EXT, NULL) : NULL;

        for (i = 0; i < ext_archivers_count; i++)
            ext_archivers[i].enabled =
                cfg == NULL || arcmc_ext_load_enabled (cfg, keys, ext_archivers[i].name);
        g_strfreev (keys);
    }

    g_strfreev (groups);
}

/* --------------------------------------------------------------------------------------------- */

void
arcmc_ext_archivers_save (mc_config_t *cfg)
{
    gchar **groups;
    gchar **keys;
    size_t i;

    if (cfg == NULL)
        return;

    /* Collapse case aliases before writing canonical uppercase names. */
    groups = mc_config_get_groups (cfg, NULL);
    if (groups != NULL)
    {
        gchar **group;

        for (group = groups; *group != NULL; group++)
        {
            const char *name;
            const arcmc_ext_archiver_t *a;
            char *canonical_section;

            if (!g_str_has_prefix (*group, ARCMC_SECTION_EXT_PARAM))
                continue;
            name = *group + strlen (ARCMC_SECTION_EXT_PARAM);
            a = arcmc_ext_archiver_by_name (name);
            if (a == NULL)
                continue;
            canonical_section = g_strconcat (ARCMC_SECTION_EXT_PARAM, a->name, (char *) NULL);
            if (strcmp (*group, canonical_section) != 0)
                mc_config_del_group (cfg, *group);
            g_free (canonical_section);
        }
        g_strfreev (groups);
    }

    keys = mc_config_get_keys (cfg, ARCMC_SECTION_EXT, NULL);
    if (keys != NULL)
    {
        gchar **key;

        for (key = keys; *key != NULL; key++)
        {
            const arcmc_ext_archiver_t *a = arcmc_ext_archiver_by_name (*key);

            if (a != NULL && strcmp (*key, a->name) != 0)
                mc_config_del_key (cfg, ARCMC_SECTION_EXT, *key);
        }
        g_strfreev (keys);
    }

    for (i = 0; i < ext_archivers_count; i++)
    {
        const arcmc_ext_archiver_t *a = &ext_archivers[i];
        char *section = g_strconcat (ARCMC_SECTION_EXT_PARAM, a->name, (char *) NULL);

        mc_config_set_bool (cfg, ARCMC_SECTION_EXT, a->name, a->enabled);
        arcmc_ext_save_string (cfg, section, "extension", a->ext);
        arcmc_ext_save_string (cfg, section, "pack_bin", a->pack_bin);
        arcmc_ext_save_string (cfg, section, "pack_args", a->pack_args);
        arcmc_ext_save_string (cfg, section, "unpack_bin", a->unpack_bin);
        arcmc_ext_save_string (cfg, section, "unpack_args", a->unpack_args);
        arcmc_ext_save_string (cfg, section, "test_bin", a->test_bin);
        arcmc_ext_save_string (cfg, section, "test_args", a->test_args);
        arcmc_ext_save_string (cfg, section, "extfs_helper", a->extfs_helper);
        arcmc_ext_save_string (cfg, section, "list_file_arg", a->list_file_arg);
        g_free (section);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* A format is usable when the program that reads it is installed; a row that
   names no program (a helper script alone) is taken at its word. */
gboolean
arcmc_ext_archiver_available (const arcmc_ext_archiver_t *a)
{
    const char *const bins[] = { a->unpack_bin, a->test_bin, a->pack_bin };
    gboolean named = FALSE;
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (bins); i++)
    {
        char *path;

        if (bins[i] == NULL || bins[i][0] == '\0')
            continue;
        named = TRUE;
        path = g_find_program_in_path (bins[i]);
        if (path != NULL)
        {
            g_free (path);
            return TRUE;
        }
    }

    return !named;
}

/* --------------------------------------------------------------------------------------------- */

void
arcmc_ext_archivers_free (void)
{
    size_t i;

    for (i = 0; i < ext_archivers_count; i++)
        arcmc_ext_archiver_clear (&ext_archivers[i]);

    g_free (ext_archivers);
    ext_archivers = NULL;
    ext_archivers_count = 0;
}

/* --------------------------------------------------------------------------------------------- */

const arcmc_ext_archiver_t *
arcmc_ext_archiver_by_name (const char *name)
{
    if (name == NULL)
        return NULL;

    return arcmc_ext_archiver_by_name_mutable (name);
}

/* --------------------------------------------------------------------------------------------- */

const arcmc_ext_archiver_t *
arcmc_find_ext_archiver (const char *archive_path)
{
    const arcmc_ext_archiver_t *result = NULL;
    const char *basename_ptr;
    size_t basename_len, result_len = 0;
    size_t i;

    if (archive_path == NULL)
        return NULL;

    basename_ptr = strrchr (archive_path, '/');
    basename_ptr = basename_ptr != NULL ? basename_ptr + 1 : archive_path;
    basename_len = strlen (basename_ptr);

    /* Prefer the longest suffix when configured formats overlap. */
    for (i = 0; i < ext_archivers_count; i++)
    {
        size_t extension_len = strlen (ext_archivers[i].ext);

        if (extension_len > result_len && basename_len >= extension_len
            && g_ascii_strcasecmp (basename_ptr + basename_len - extension_len,
                                   ext_archivers[i].ext)
                == 0)
        {
            result = &ext_archivers[i];
            result_len = extension_len;
        }
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */
