/*
   paths to configuration files

   Copyright (C) 2010-2025
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2010.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/vfs/vfs.h"
#include "lib/util.h"  // unix_error_string()

#include "lib/mcconfig.h"

#include <glib/gstdio.h>  // g_rename()

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static gboolean xdg_vars_initialized = FALSE;
static char *mc_config_str = NULL;
static char *mc_cache_str = NULL;
static char *mc_data_str = NULL;

static gboolean config_dir_present = FALSE;

static const struct
{
    char **basedir;
    const char *filename;
} mc_config_files_reference[] = {
    // config
    { &mc_config_str, MC_CONFIG_FILE },
    { &mc_config_str, MC_FHL_INI_FILE },
    { &mc_config_str, MC_HOTLIST_FILE },
    { &mc_config_str, GLOBAL_KEYMAP_FILE },
    { &mc_config_str, MC_USERMENU_FILE },
    { &mc_config_str, EDIT_HOME_MENU },
    { &mc_config_str, MC_PANELS_FILE },
    { &mc_config_str, MC_MAGIC_FILE },

    // User should move this file with applying some changes in file
    { &mc_config_str, MC_EXT_FILE },
    { &mc_config_str, MC_EXT_OLD_FILE },

    // data
    { &mc_data_str, MC_SKINS_DIR },
    { &mc_data_str, VFS_SHELL_PREFIX },
    { &mc_data_str, MC_ASHRC_CUSTOM_PROFILE_FILE },
    { &mc_data_str, MC_KSHRC_CUSTOM_PROFILE_FILE },
    { &mc_data_str, MC_MKSHRC_CUSTOM_PROFILE_FILE },
    { &mc_data_str, MC_BASHRC_CUSTOM_PROFILE_FILE },
    { &mc_data_str, MC_INPUTRC_FILE },
    { &mc_data_str, MC_ZSHRC_CUSTOM_PROFILE_FILE },
    { &mc_data_str, MC_EXTFS_DIR },
    { &mc_data_str, MC_HISTORY_FILE },
    { &mc_data_str, MC_FILEPOS_FILE },
    { &mc_data_str, EDIT_SYNTAX_FILE },
    { &mc_data_str, EDIT_HOME_CLIP_FILE },
    { &mc_data_str, MC_MACRO_FILE },
    { &mc_data_str, MC_FAR2L_CLIENT_ID_FILE },
    { &mc_data_str, MC_FAR2L_CLIENTS_FILE },

    // cache
    { &mc_cache_str, "mc.log" },
    { &mc_cache_str, MC_TREESTORE_FILE },
    { &mc_cache_str, EDIT_HOME_TEMP_FILE },
    { &mc_cache_str, EDIT_HOME_BLOCK_FILE },

    {
        NULL,
        NULL,
    },
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions *********************************************************************** */
/* --------------------------------------------------------------------------------------------- */

static void
mc_config_mkdir (const char *directory_name, GError **mcerror)
{
    mc_return_if_error (mcerror);

    if (!(g_file_test (directory_name, G_FILE_TEST_EXISTS)
          && g_file_test (directory_name, G_FILE_TEST_IS_DIR))
        && g_mkdir_with_parents (directory_name, 0700) != 0)
        mc_propagate_error (mcerror, 0, _ ("Cannot create %s directory"), directory_name);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_config_copy_file (const char *src, const char *dst, mode_t mode)
{
    int in_fd, out_fd;
    char buf[BUF_8K];
    ssize_t n;
    gboolean ok = TRUE;

    in_fd = open (src, O_RDONLY | O_CLOEXEC);
    if (in_fd < 0)
        return FALSE;

    out_fd = open (dst, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode & 0777);
    if (out_fd < 0)
    {
        close (in_fd);
        return FALSE;
    }

    while ((n = read (in_fd, buf, sizeof (buf))) > 0)
    {
        ssize_t off = 0;

        while (off < n)
        {
            ssize_t w = write (out_fd, buf + off, n - off);

            if (w < 0)
            {
                ok = FALSE;
                break;
            }
            off += w;
        }
        if (!ok)
            break;
    }
    if (n < 0)
        ok = FALSE;

    close (in_fd);
    if (close (out_fd) != 0)
        ok = FALSE;

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_config_copy_tree (const char *src, const char *dst)
{
    DIR *dir;
    struct dirent *de;
    struct stat st;
    gboolean ok = TRUE;

    if (lstat (src, &st) != 0 || !S_ISDIR (st.st_mode))
        return FALSE;

    if (mkdir (dst, st.st_mode & 0777) != 0)
        return FALSE;

    dir = opendir (src);
    if (dir == NULL)
        return FALSE;

    while ((de = readdir (dir)) != NULL)
    {
        char *s, *d;

        if (DIR_IS_DOT (de->d_name) || DIR_IS_DOTDOT (de->d_name))
            continue;

        s = g_build_filename (src, de->d_name, (char *) NULL);
        d = g_build_filename (dst, de->d_name, (char *) NULL);

        if (lstat (s, &st) == 0)
        {
            if (S_ISDIR (st.st_mode))
                ok = mc_config_copy_tree (s, d) && ok;
            else if (S_ISREG (st.st_mode))
                ok = mc_config_copy_file (s, d, st.st_mode) && ok;
            else if (S_ISLNK (st.st_mode))
            {
                char target[PATH_MAX];
                ssize_t len = readlink (s, target, sizeof (target) - 1);

                if (len < 0)
                    ok = FALSE;
                else
                {
                    target[len] = '\0';
                    ok = (symlink (target, d) == 0) && ok;
                }
            }
        }

        g_free (s);
        g_free (d);
    }

    closedir (dir);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* First run of this version: copy <path_base>/mc to <path_base>/mc6 if only the old one exists. */
static void
mc_config_import_legacy (const char *path_base, const char *full_path)
{
    char *legacy;

    if (strcmp (MC_USERCONF_DIR, MC_USERCONF_LEGACY_DIR) == 0)
        return;

    if (g_file_test (full_path, G_FILE_TEST_EXISTS))
        return;

    legacy = g_build_filename (path_base, MC_USERCONF_LEGACY_DIR, (char *) NULL);

    if (g_file_test (legacy, G_FILE_TEST_IS_DIR) && !g_file_test (legacy, G_FILE_TEST_IS_SYMLINK))
    {
        if (mc_config_copy_tree (legacy, full_path))
            fprintf (stderr, _ ("Imported %s to %s\n"), legacy, full_path);
        else
            fprintf (stderr, _ ("Warning: import of %s to %s is incomplete\n"), legacy, full_path);
    }

    g_free (legacy);
}

/* --------------------------------------------------------------------------------------------- */

/* Data files renamed in 6.1: take the old name over when the new one is not there yet. */
static void
mc_config_rename_legacy_files (void)
{
    static const struct
    {
        char **basedir;
        const char *legacy;
        const char *current;
    } renamed[] = {
        { &mc_config_str, "mc.keymap", GLOBAL_KEYMAP_FILE },
        { &mc_config_str, "mc.ext.ini", MC_EXT_FILE },
        { &mc_data_str, "mc.macros", MC_MACRO_FILE },
    };

    for (size_t i = 0; i < G_N_ELEMENTS (renamed); i++)
    {
        char *legacy, *current;

        if (*renamed[i].basedir == NULL)
            continue;

        legacy = g_build_filename (*renamed[i].basedir, renamed[i].legacy, (char *) NULL);
        current = g_build_filename (*renamed[i].basedir, renamed[i].current, (char *) NULL);

        if (g_file_test (legacy, G_FILE_TEST_EXISTS) && !g_file_test (current, G_FILE_TEST_EXISTS)
            && g_rename (legacy, current) == 0)
            fprintf (stderr, _ ("Renamed %s to %s\n"), legacy, current);

        g_free (legacy);
        g_free (current);
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
mc_config_init_one_config_path (const char *path_base, const char *subdir, GError **mcerror)
{
    char *full_path;

    mc_return_val_if_error (mcerror, FALSE);

    full_path = g_build_filename (path_base, subdir, (char *) NULL);

    mc_config_import_legacy (path_base, full_path);

    if (g_file_test (full_path, G_FILE_TEST_EXISTS))
    {
        if (g_file_test (full_path, G_FILE_TEST_IS_DIR))
            config_dir_present = TRUE;
        else
        {
            fprintf (stderr, "%s %s\n", _ ("FATAL: not a directory:"), full_path);
            exit (EXIT_FAILURE);
        }
    }

    mc_config_mkdir (full_path, mcerror);
    if (mcerror != NULL && *mcerror != NULL)
        MC_PTR_FREE (full_path);

    return full_path;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_config_init_config_paths (GError **mcerror)
{
    const char *profile_root;
    char *dir;

    mc_return_if_error (mcerror);

    if (xdg_vars_initialized)
        return;

    profile_root = mc_get_profile_root ();

    if (strcmp (profile_root, mc_config_get_home_dir ()) != 0)
    {
        /*
         * The user overrode the default profile root.
         *
         * In this case we can't use GLib's g_get_user_{config,cache,data}_dir()
         * as these functions use the user's home dir as the root.
         */

        dir = g_build_filename (profile_root, ".config", (char *) NULL);
        mc_config_str = mc_config_init_one_config_path (dir, MC_USERCONF_DIR, mcerror);
        g_free (dir);

        dir = g_build_filename (profile_root, ".cache", (char *) NULL);
        mc_cache_str = mc_config_init_one_config_path (dir, MC_USERCONF_DIR, mcerror);
        g_free (dir);

        dir = g_build_filename (profile_root, ".local", "share", (char *) NULL);
        mc_data_str = mc_config_init_one_config_path (dir, MC_USERCONF_DIR, mcerror);
        g_free (dir);
    }
    else
    {
        mc_config_str =
            mc_config_init_one_config_path (g_get_user_config_dir (), MC_USERCONF_DIR, mcerror);
        mc_cache_str =
            mc_config_init_one_config_path (g_get_user_cache_dir (), MC_USERCONF_DIR, mcerror);
        mc_data_str =
            mc_config_init_one_config_path (g_get_user_data_dir (), MC_USERCONF_DIR, mcerror);
    }

    mc_config_rename_legacy_files ();

    xdg_vars_initialized = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_config_deinit_config_paths (void)
{
    if (!xdg_vars_initialized)
        return;

    g_free (mc_config_str);
    g_free (mc_cache_str);
    g_free (mc_data_str);

    g_free (mc_global.share_data_dir);
    g_free (mc_global.sysconfig_dir);

    xdg_vars_initialized = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mc_config_get_data_path (void)
{
    if (!xdg_vars_initialized)
        mc_config_init_config_paths (NULL);

    return (const char *) mc_data_str;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mc_config_get_cache_path (void)
{
    if (!xdg_vars_initialized)
        mc_config_init_config_paths (NULL);

    return (const char *) mc_cache_str;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mc_config_get_home_dir (void)
{
    static const char *homedir = NULL;

    if (homedir == NULL)
    {
        /* Prior to GLib 2.36, g_get_home_dir() ignores $HOME, which is why
         * we read it ourselves. As that function's documentation explains,
         * using $HOME is good for compatibility with other programs and
         * for running from test frameworks. */
        homedir = g_getenv ("HOME");
        if (homedir == NULL || *homedir == '\0')
            homedir = g_get_home_dir ();
    }

    return homedir;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mc_config_get_path (void)
{
    if (!xdg_vars_initialized)
        mc_config_init_config_paths (NULL);

    return (const char *) mc_config_str;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get full path to config file by short name.
 *
 * @param config_name short name
 * @return full path to config file
 */

char *
mc_config_get_full_path (const char *config_name)
{
    size_t rule_index;

    if (config_name == NULL)
        return NULL;

    if (!xdg_vars_initialized)
        mc_config_init_config_paths (NULL);

    for (rule_index = 0; mc_config_files_reference[rule_index].filename != NULL; rule_index++)
        if (strcmp (config_name, mc_config_files_reference[rule_index].filename) == 0)
            return g_build_filename (*mc_config_files_reference[rule_index].basedir,
                                     mc_config_files_reference[rule_index].filename, (char *) NULL);

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get full path to config file by short name.
 *
 * @param config_name short name
 * @return object with full path to config file
 */

vfs_path_t *
mc_config_get_full_vpath (const char *config_name)
{
    vfs_path_t *ret_vpath;
    char *str_path;

    str_path = mc_config_get_full_path (config_name);

    ret_vpath = vfs_path_from_str (str_path);
    g_free (str_path);

    return ret_vpath;
}

/* --------------------------------------------------------------------------------------------- */
