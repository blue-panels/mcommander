/*
   Util for external clipboard.

   Copyright (C) 2009-2025
   Free Software Foundation, Inc.
   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2010.
   Andrew Borodin <aborodin@vmail.ru>, 2014.
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/mcconfig.h"
#include "lib/util.h"
#include "lib/event.h"
#include "lib/tty/tty.h"  // tty_osc52_write()

#include "lib/vfs/vfs.h"
#include "lib/tty/key.h"  // the far2l clipboard

#include "src/execute.h"

#include "clipboard.h"

/*** global variables ****************************************************************************/

/* path to X clipboard utility */
char *clipboard_store_path = NULL;
char *clipboard_paste_path = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static const int clip_open_flags = O_CREAT | O_WRONLY | O_TRUNC | O_BINARY;
static const mode_t clip_open_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static char *
clip_info_path (const char *clip_path)
{
    return g_strconcat (clip_path, CLIP_INFO_SUFFIX, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* The clipfile was replaced by text from outside the editor: its info no longer applies */
static void
clip_info_drop_home (void)
{
    char *fname;

    fname = mc_config_get_full_path (EDIT_HOME_CLIP_FILE);
    clipboard_info_drop (fname);
    g_free (fname);
}

/* Put the clipfile on the terminal's clipboard with OSC 52; a file that is too long or a terminal
   that does not take it leaves the clipfile as the only copy. */
static void
clipboard_file_to_osc52 (void)
{
    char *tmp, *contents = NULL;
    gsize length = 0;

    tmp = mc_config_get_full_path (EDIT_HOME_CLIP_FILE);
    if (g_file_get_contents (tmp, &contents, &length, NULL))
        (void) tty_osc52_write (contents, length);

    g_free (contents);
    g_free (tmp);
}

/* --------------------------------------------------------------------------------------------- */
/* With no clipboard_store command, a terminal with the far2l extensions gets the clipfile as
   the clipboard: the terminal asks its user once and keeps the answer. FALSE when the terminal
   has no far2l clipboard and nothing was done. */
static gboolean
clip_file_to_far2l (void)
{
    char *fname, *text = NULL;
    gsize len = 0;

    if (!tty_far2l_clipboard_available ())
        return FALSE;

    fname = mc_config_get_full_path (EDIT_HOME_CLIP_FILE);
    if (g_file_get_contents (fname, &text, &len, NULL))
        (void) tty_far2l_clipboard_set (text, len);
    g_free (text);
    g_free (fname);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The other way: with no clipboard_paste command the clipboard of a far2l terminal becomes the
   clipfile. Nothing changes when the terminal has none, refuses, or holds no text. */
static void
clip_file_from_far2l (void)
{
    char *text = NULL;
    size_t len = 0;

    if (!tty_far2l_clipboard_available () || !tty_far2l_clipboard_get (&text, &len))
        return;

    {
        char *fname = mc_config_get_full_path (EDIT_HOME_CLIP_FILE);

        if (g_file_set_contents (fname, text, (gssize) len, NULL))
        {
            (void) chmod (fname, clip_open_mode);
            clip_info_drop_home ();
        }
        g_free (fname);
    }
    g_free (text);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* event callback */
gboolean
clipboard_file_to_ext_clip (const gchar *event_group_name, const gchar *event_name,
                            gpointer init_data, gpointer data)
{
    char *tmp, *cmd;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;
    (void) data;

    // the far2l terminal, which was asked for its clipboard, goes before the OSC 52 of the terminal
    if ((clipboard_store_path == NULL || clipboard_store_path[0] == '\0') && clip_file_to_far2l ())
        return TRUE;

    if (clipboard_store_path == NULL || clipboard_store_path[0] == '\0')
    {
        // no external clipboard command: the terminal's own clipboard, through OSC 52
        clipboard_file_to_osc52 ();
        return TRUE;
    }

    tmp = mc_config_get_full_path (EDIT_HOME_CLIP_FILE);
    cmd = g_strconcat (clipboard_store_path, " ", tmp, " 2>/dev/null", (char *) NULL);

    if (cmd != NULL)
        my_system (EXECUTE_AS_SHELL, mc_global.shell->path, cmd);

    g_free (cmd);
    g_free (tmp);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
gboolean
clipboard_file_from_ext_clip (const gchar *event_group_name, const gchar *event_name,
                              gpointer init_data, gpointer data)
{
    mc_pipe_t *p;
    int file = -1;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;
    (void) data;

    if (clipboard_paste_path == NULL || clipboard_paste_path[0] == '\0')
    {
        clip_file_from_far2l ();
        return TRUE;
    }

    p = mc_popen (clipboard_paste_path, TRUE, TRUE, NULL);
    if (p == NULL)
        return TRUE;  // don't show error message

    p->out.null_term = FALSE;
    p->err.null_term = TRUE;

    while (TRUE)
    {
        GError *error = NULL;

        p->out.len = MC_PIPE_BUFSIZE;
        p->err.len = MC_PIPE_BUFSIZE;

        mc_pread (p, &error);

        if (error != NULL)
        {
            // don't show error message
            g_error_free (error);
            break;
        }

        // ignore stderr and get stdout
        if (p->out.len == MC_PIPE_STREAM_EOF || p->out.len == MC_PIPE_ERROR_READ)
            break;

        if (p->out.len > 0)
        {
            ssize_t nwrite;

            if (file < 0)
            {
                vfs_path_t *fname_vpath;

                fname_vpath = mc_config_get_full_vpath (EDIT_HOME_CLIP_FILE);
                file = mc_open (fname_vpath, clip_open_flags, clip_open_mode);
                vfs_path_free (fname_vpath, TRUE);

                if (file < 0)
                    break;
            }

            nwrite = mc_write (file, p->out.buf, p->out.len);
            (void) nwrite;
        }
    }

    if (file >= 0)
    {
        mc_close (file);
        clip_info_drop_home ();
    }

    mc_pclose (p, NULL);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
gboolean
clipboard_text_to_file (const gchar *event_group_name, const gchar *event_name, gpointer init_data,
                        gpointer data)
{
    int file;
    vfs_path_t *fname_vpath = NULL;
    size_t str_len;
    const char *text = (const char *) data;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    if (text == NULL)
        return FALSE;

    fname_vpath = mc_config_get_full_vpath (EDIT_HOME_CLIP_FILE);
    file = mc_open (fname_vpath, clip_open_flags, clip_open_mode);

    if (file == -1)
    {
        // The editor makes this directory, and it may never have run.
        char *dir;
        vfs_path_t *dir_vpath;

        dir = g_path_get_dirname (vfs_path_as_str (fname_vpath));
        dir_vpath = vfs_path_from_str (dir);
        mc_mkdir (dir_vpath, 0700);
        vfs_path_free (dir_vpath, TRUE);
        g_free (dir);

        file = mc_open (fname_vpath, clip_open_flags, clip_open_mode);
    }

    vfs_path_free (fname_vpath, TRUE);

    if (file == -1)
        return TRUE;

    str_len = strlen (text);
    {
        ssize_t ret;

        ret = mc_write (file, text, str_len);
        (void) ret;
    }
    mc_close (file);
    clip_info_drop_home ();

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
gboolean
clipboard_text_from_file (const gchar *event_group_name, const gchar *event_name,
                          gpointer init_data, gpointer data)
{
    char *fname;
    char *text = NULL;
    gsize len = 0;
    ev_clipboard_text_from_file_t *event_data = (ev_clipboard_text_from_file_t *) data;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    fname = mc_config_get_full_path (EDIT_HOME_CLIP_FILE);
    if (!g_file_get_contents (fname, &text, &len, NULL) || len == 0)
    {
        g_free (text);
        text = NULL;
    }
    g_free (fname);

    *(event_data->text) = text;
    event_data->ret = (text != NULL);
    event_data->len = (text != NULL) ? (size_t) len : 0;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
clipboard_info_write (const char *clip_path, const char *digest, const gboolean vertical,
                      const char *codeset)
{
    char *path, *line;

    path = clip_info_path (clip_path);
    line = g_strdup_printf ("%s %s %s\n", digest, vertical ? "column" : "text",
                            codeset != NULL ? codeset : "");
    g_file_set_contents (path, line, -1, NULL);
    g_free (line);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

/* Read the info of the clipfile; FALSE when there is none. digest gets CLIP_DIGEST_LEN + 1
   bytes, codeset gets CLIP_CODESET_MAX + 1; the caller checks the digest against the content. */
gboolean
clipboard_info_read (const char *clip_path, char *digest, gboolean *vertical, char *codeset)
{
    char *path, *data = NULL;
    char **f;
    gboolean ok;

    digest[0] = '\0';
    *vertical = FALSE;
    codeset[0] = '\0';

    path = clip_info_path (clip_path);
    ok = g_file_get_contents (path, &data, NULL, NULL);
    g_free (path);
    if (!ok)
        return FALSE;

    f = g_strsplit_set (g_strstrip (data), " ", 3);
    ok = f[0] != NULL && strlen (f[0]) == CLIP_DIGEST_LEN && f[1] != NULL;
    if (ok)
    {
        strcpy (digest, f[0]);
        *vertical = strcmp (f[1], "column") == 0;
        if (f[2] != NULL)
            g_strlcpy (codeset, f[2], CLIP_CODESET_MAX + 1);
    }
    g_strfreev (f);
    g_free (data);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

void
clipboard_info_drop (const char *clip_path)
{
    char *path;

    path = clip_info_path (clip_path);
    unlink (path);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */