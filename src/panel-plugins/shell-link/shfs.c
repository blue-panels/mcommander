/*
   libshfs - file access over a shell connection.

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

/**
 * \file
 * \brief Source: file access over a shell connection
 */

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

#include "lib/global.h"

#include "lib/strutil.h"  // str_shell_escape()
#include "lib/util.h"
#include "lib/vfs/utilvfs.h"  // vfs_parse_filemode() and friends
#include "lib/fileloc.h"      // VFS_SHELL_PREFIX
#include "lib/mcconfig.h"     // mc_config_get_data_path()

#include <shelldef.h>
#include "shfs.h"
#include "shfs-priv.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Read one byte from the channel. The protocol switches between line replies
 * and raw file data, so reading ahead would swallow payload.
 */
static gboolean
shfs_read_byte (shfs_conn_t *conn, char *c)
{
    ssize_t n;

    n = shfs_read (conn, c, 1);

    return (n == 1);
}

// how far a chain of symbolic links is followed before it is given up
#define SHFS_LINK_HOPS 32

/** Every helper, with the text compiled into the binary. */
static const struct
{
    const char *name;
    const char *def_content;
} shfs_helper_table[] = {
    { VFS_SHELL_LS_FILE, VFS_SHELL_LS_DEF_CONTENT },
    { VFS_SHELL_EXISTS_FILE, VFS_SHELL_EXISTS_DEF_CONTENT },
    { VFS_SHELL_MKDIR_FILE, VFS_SHELL_MKDIR_DEF_CONTENT },
    { VFS_SHELL_UNLINK_FILE, VFS_SHELL_UNLINK_DEF_CONTENT },
    { VFS_SHELL_CHOWN_FILE, VFS_SHELL_CHOWN_DEF_CONTENT },
    { VFS_SHELL_CHMOD_FILE, VFS_SHELL_CHMOD_DEF_CONTENT },
    { VFS_SHELL_UTIME_FILE, VFS_SHELL_UTIME_DEF_CONTENT },
    { VFS_SHELL_RMDIR_FILE, VFS_SHELL_RMDIR_DEF_CONTENT },
    { VFS_SHELL_LN_FILE, VFS_SHELL_LN_DEF_CONTENT },
    { VFS_SHELL_MV_FILE, VFS_SHELL_MV_DEF_CONTENT },
    { VFS_SHELL_HARDLINK_FILE, VFS_SHELL_HARDLINK_DEF_CONTENT },
    { VFS_SHELL_GET_FILE, VFS_SHELL_GET_DEF_CONTENT },
    { VFS_SHELL_SEND_FILE, VFS_SHELL_SEND_DEF_CONTENT },
    { VFS_SHELL_APPEND_FILE, VFS_SHELL_APPEND_DEF_CONTENT },
    { VFS_SHELL_INFO_FILE, VFS_SHELL_INFO_DEF_CONTENT },
    { VFS_SHELL_PUTAT_FILE, VFS_SHELL_PUTAT_DEF_CONTENT },
    { VFS_SHELL_CKSUMRANGE_FILE, VFS_SHELL_CKSUMRANGE_DEF_CONTENT },
    { VFS_SHELL_BLOCKDIGESTS_FILE, VFS_SHELL_BLOCKDIGESTS_DEF_CONTENT },
};

/* --------------------------------------------------------------------------------------------- */

/**
 * Read the version a script declares. The first line of every helper says which
 * protocol revision it was written against; a file without that line is
 * reported as version 0.
 */
static int
shfs_helper_parse_version (const char *text)
{
    const char *tag = "# shfs-helper:";
    const char *p;
    const char *nl;
    char *line;
    int version = 0;

    if (text == NULL || strncmp (text, tag, strlen (tag)) != 0)
        return 0;

    nl = strchr (text, '\n');
    line = nl != NULL ? g_strndup (text, (gsize) (nl - text)) : g_strdup (text);

    // "# shfs-helper: <name> <version>"; the name is skipped
    p = strrchr (line, ' ');
    if (p != NULL)
        version = (int) g_ascii_strtoll (p + 1, NULL, 10);

    g_free (line);

    return version;
}

/* --------------------------------------------------------------------------------------------- */

static void
shfs_helper_free (gpointer p)
{
    shfs_helper_t *h = (shfs_helper_t *) p;

    if (h == NULL)
        return;

    g_free (h->name);
    g_free (h->path);
    g_free (h);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Find a helper script: the user's override for this host first, then the
 * system set, then the copy compiled into the binary.
 */
static char *
shfs_load_script_from_file (const char *hostname, const char *script_name, const char *def_content)
{
    char *scr_filename = NULL;
    char *scr_content = NULL;
    gsize scr_len = 0;

    // 1st: scan user directory
    scr_filename = shfs_helper_user_path (hostname, script_name);
    // silent about user dir
    g_file_get_contents (scr_filename, &scr_content, &scr_len, NULL);
    g_free (scr_filename);
    // 2nd: scan system dir
    if (scr_content == NULL)
    {
        scr_filename = shfs_helper_system_path (script_name);
        g_file_get_contents (scr_filename, &scr_content, &scr_len, NULL);
        g_free (scr_filename);
    }

    if (scr_content != NULL)
        return scr_content;

    return g_strdup (def_content);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions: helpers *******************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
shfs_helper_user_path (const char *hostname, const char *name)
{
    return g_build_path (PATH_SEP_STR, mc_config_get_data_path (), VFS_SHELL_PREFIX,
                         hostname != NULL ? hostname : "", name, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

char *
shfs_helper_system_path (const char *name)
{
    return g_build_path (PATH_SEP_STR, SHELL_LINK_HELPERS_DIR, name, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

const char *
shfs_helper_builtin (const char *name)
{
    gsize i;

    for (i = 0; i < G_N_ELEMENTS (shfs_helper_table); i++)
        if (strcmp (shfs_helper_table[i].name, name) == 0)
            return shfs_helper_table[i].def_content;

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

GPtrArray *
shfs_helpers_list (const char *hostname)
{
    GPtrArray *list;
    gsize i;

    list = g_ptr_array_new_with_free_func (shfs_helper_free);

    for (i = 0; i < G_N_ELEMENTS (shfs_helper_table); i++)
    {
        shfs_helper_t *h;
        char *path;

        h = g_new0 (shfs_helper_t, 1);
        h->name = g_strdup (shfs_helper_table[i].name);
        h->source = SHFS_HELPER_BUILTIN;
        h->size = strlen (shfs_helper_table[i].def_content);
        h->expected_version = shfs_helper_parse_version (shfs_helper_table[i].def_content);
        h->version = h->expected_version;

        path = shfs_helper_user_path (hostname, h->name);
        if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
        {
            h->source = SHFS_HELPER_USER;
            h->path = path;
        }
        else
        {
            g_free (path);
            path = shfs_helper_system_path (h->name);
            if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
            {
                h->source = SHFS_HELPER_SYSTEM;
                h->path = path;
            }
            else
                g_free (path);
        }

        if (h->path != NULL)
        {
            struct stat st;
            char *content = NULL;

            if (stat (h->path, &st) == 0)
                h->size = (gsize) st.st_size;

            if (g_file_get_contents (h->path, &content, NULL, NULL))
            {
                h->version = shfs_helper_parse_version (content);
                g_free (content);
            }
        }

        g_ptr_array_add (list, h);
    }

    return list;
}

/* --------------------------------------------------------------------------------------------- */

void
shfs_helpers_free (GPtrArray *list)
{
    if (list != NULL)
        g_ptr_array_free (list, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

char *
shfs_helper_content (const char *hostname, const char *name, shfs_helper_source_t *source)
{
    char *path;
    char *content = NULL;

    path = shfs_helper_user_path (hostname, name);
    if (g_file_get_contents (path, &content, NULL, NULL))
    {
        if (source != NULL)
            *source = SHFS_HELPER_USER;
        g_free (path);
        return content;
    }
    g_free (path);

    path = shfs_helper_system_path (name);
    if (g_file_get_contents (path, &content, NULL, NULL))
    {
        if (source != NULL)
            *source = SHFS_HELPER_SYSTEM;
        g_free (path);
        return content;
    }
    g_free (path);

    if (source != NULL)
        *source = SHFS_HELPER_BUILTIN;

    {
        const char *def = shfs_helper_builtin (name);

        return def != NULL ? g_strdup (def) : NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/** Run one helper script and turn anything but a clean completion into a GError. */
static gboolean shfs_op (shfs_conn_t *conn, const char *script, GError **error, const char *fmt,
                         ...) G_GNUC_PRINTF (4, 5);

static gboolean
shfs_op (shfs_conn_t *conn, const char *script, GError **error, const char *fmt, ...)
{
    va_list ap;
    int r;

    if (conn == NULL || !conn->alive)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_CLOSED, "%s", _ ("shell: connection is closed"));
        return FALSE;
    }

    va_start (ap, fmt);
    r = shfs_command_va (conn, TRUE, FALSE, script, fmt, ap);
    va_end (ap);

    if (r == SHFS_REPLY_COMPLETE)
        return TRUE;

    shfs_log_printf (SHFS_LOG_ERRORS, "operation failed, reply class %d", r);

    g_set_error (error, SHFS_ERROR,
                 r == SHFS_REPLY_ERROR ? SHFS_ERR_PERMISSION_DENIED : SHFS_ERR_FAILED, "%s",
                 _ ("shell: the remote command failed"));

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_exists_path (shfs_conn_t *conn, const char *path, GError **error)
{
    char *rpath;
    gboolean ret;

    rpath = str_shell_escape (path);
    ret = shfs_op (conn, conn->scr_exists, error, "SHELL_FILENAME=%s;\n", rpath);
    g_free (rpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

/* The stat of what @path leads to. The protocol has no stat, so it comes out of a
   listing of the parent directory; a symbolic link is followed to its target, as
   the get helper follows it, so the size is that of the bytes a fetch brings. */
gboolean
shfs_file_stat (shfs_conn_t *conn, const char *path, struct stat *st, GError **error)
{
    char *cur;
    gboolean found = FALSE;
    int hops;

    mc_return_val_if_error (error, FALSE);

    cur = g_strdup (path);

    for (hops = 0; hops < SHFS_LINK_HOPS && !found; hops++)
    {
        GPtrArray *entries;
        char *dir;
        char *base;
        char *next = NULL;
        guint i;

        dir = g_path_get_dirname (cur);
        base = g_path_get_basename (cur);

        entries = shfs_list_dir (conn, dir, error);
        if (entries != NULL)
        {
            for (i = 0; i < entries->len; i++)
            {
                const shfs_entry_t *e = (const shfs_entry_t *) g_ptr_array_index (entries, i);

                if (strcmp (e->name, base) != 0)
                    continue;

                if (S_ISLNK (e->st.st_mode) && e->linkname != NULL && e->linkname[0] != '\0')
                    next = g_path_is_absolute (e->linkname)
                        ? g_strdup (e->linkname)
                        : mc_build_filename (dir, e->linkname, (char *) NULL);
                else
                {
                    *st = e->st;
                    found = TRUE;
                }
                break;
            }

            shfs_entries_free (entries);
        }

        g_free (dir);
        g_free (base);
        g_free (cur);
        cur = next;

        if (cur == NULL)
            break;
    }

    g_free (cur);

    return found;
}

/* --------------------------------------------------------------------------------------------- */

gint64
shfs_file_size (shfs_conn_t *conn, const char *path, GError **error)
{
    struct stat st;

    return shfs_file_stat (conn, path, &st, error) ? (gint64) st.st_size : -1;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_unlink_path (shfs_conn_t *conn, const char *path, GError **error)
{
    char *rpath;
    gboolean ret;

    rpath = str_shell_escape (path);
    ret = shfs_op (conn, conn->scr_unlink, error, "SHELL_FILENAME=%s;\n", rpath);
    g_free (rpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_rmdir_path (shfs_conn_t *conn, const char *path, GError **error)
{
    char *rpath;
    gboolean ret;

    rpath = str_shell_escape (path);
    ret = shfs_op (conn, conn->scr_rmdir, error, "SHELL_FILENAME=%s;\n", rpath);
    g_free (rpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_mkdir_path (shfs_conn_t *conn, const char *path, mode_t mode, GError **error)
{
    char *rpath;
    gboolean ret;

    (void) mode;  // the helper creates with the remote umask

    rpath = str_shell_escape (path);
    ret = shfs_op (conn, conn->scr_mkdir, error, "SHELL_FILENAME=%s;\n", rpath);
    g_free (rpath);

    if (!ret)
    {
        /* The helper answers every mkdir failure with one code, so ask whether the
           name is simply taken. */
        if (shfs_exists_path (conn, path, NULL))
        {
            g_clear_error (error);
            g_set_error (error, SHFS_ERROR, SHFS_ERR_EXISTS, _ ("shell: %s already exists"), path);
        }

        return FALSE;
    }

    // Some hosts report success while creating nothing, so confirm.
    if (!shfs_exists_path (conn, path, NULL))
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_PERMISSION_DENIED, "%s",
                     _ ("shell: the directory was not created"));
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_chmod_path (shfs_conn_t *conn, const char *path, mode_t mode, GError **error)
{
    char *rpath;
    gboolean ret;

    rpath = str_shell_escape (path);
    ret = shfs_op (conn, conn->scr_chmod, error, "SHELL_FILENAME=%s SHELL_FILEMODE=%4.4o;\n", rpath,
                   (unsigned int) (mode & 07777));
    g_free (rpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_chown_path (shfs_conn_t *conn, const char *path, uid_t owner, gid_t group, GError **error)
{
    struct passwd *pw;
    struct group *gr;
    char *rpath;
    gboolean ret;

    /* The protocol speaks names, not numbers: a local uid/gid means nothing on
       the remote host. Without a local name there is nothing to send. */
    pw = getpwuid (owner);
    if (pw == NULL)
        return TRUE;

    gr = getgrgid (group);
    if (gr == NULL)
        return TRUE;

    rpath = str_shell_escape (path);
    ret = shfs_op (conn, conn->scr_chown, error,
                   "SHELL_FILENAME=%s SHELL_FILEOWNER=%s SHELL_FILEGROUP=%s;\n", rpath, pw->pw_name,
                   gr->gr_name);
    g_free (rpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_rename_path (shfs_conn_t *conn, const char *from, const char *to, GError **error)
{
    char *rfrom, *rto;
    gboolean ret;

    rfrom = str_shell_escape (from);
    rto = str_shell_escape (to);
    ret = shfs_op (conn, conn->scr_mv, error, "SHELL_FILEFROM=%s SHELL_FILETO=%s;\n", rfrom, rto);
    g_free (rfrom);
    g_free (rto);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_hardlink_path (shfs_conn_t *conn, const char *target, const char *link, GError **error)
{
    char *rtarget, *rlink;
    gboolean ret;

    rtarget = str_shell_escape (target);
    rlink = str_shell_escape (link);
    ret = shfs_op (conn, conn->scr_hardlink, error, "SHELL_FILEFROM=%s SHELL_FILETO=%s;\n", rtarget,
                   rlink);
    g_free (rtarget);
    g_free (rlink);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_symlink_path (shfs_conn_t *conn, const char *target, const char *link, GError **error)
{
    char *rtarget, *rlink;
    gboolean ret;

    rtarget = str_shell_escape (target);
    rlink = str_shell_escape (link);
    ret =
        shfs_op (conn, conn->scr_ln, error, "SHELL_FILEFROM=%s SHELL_FILETO=%s;\n", rtarget, rlink);
    g_free (rtarget);
    g_free (rlink);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_utime_path (shfs_conn_t *conn, const char *path, time_t atime, time_t mtime, GError **error)
{
    char utcatime[16], utcmtime[16];
    char utcatime_w_nsec[30], utcmtime_w_nsec[30];
    struct tm *gmt;
    char *rpath;
    gboolean ret;

    gmt = gmtime (&atime);
    g_snprintf (utcatime, sizeof (utcatime), "%04d%02d%02d%02d%02d.%02d", gmt->tm_year + 1900,
                gmt->tm_mon + 1, gmt->tm_mday, gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
    g_snprintf (utcatime_w_nsec, sizeof (utcatime_w_nsec), "%04d-%02d-%02d %02d:%02d:%02d.%09ld",
                gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday, gmt->tm_hour, gmt->tm_min,
                gmt->tm_sec, 0L);

    gmt = gmtime (&mtime);
    g_snprintf (utcmtime, sizeof (utcmtime), "%04d%02d%02d%02d%02d.%02d", gmt->tm_year + 1900,
                gmt->tm_mon + 1, gmt->tm_mday, gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
    g_snprintf (utcmtime_w_nsec, sizeof (utcmtime_w_nsec), "%04d-%02d-%02d %02d:%02d:%02d.%09ld",
                gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday, gmt->tm_hour, gmt->tm_min,
                gmt->tm_sec, 0L);

    rpath = str_shell_escape (path);
    ret = shfs_op (conn, conn->scr_utime, error,
                   "SHELL_FILENAME=%s SHELL_FILEATIME=%ld SHELL_FILEMTIME=%ld "
                   "SHELL_TOUCHATIME=%s SHELL_TOUCHMTIME=%s SHELL_TOUCHATIME_W_NSEC=\"%s\" "
                   "SHELL_TOUCHMTIME_W_NSEC=\"%s\";\n",
                   rpath, (long) atime, (long) mtime, utcatime, utcmtime, utcatime_w_nsec,
                   utcmtime_w_nsec);
    g_free (rpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Spawn ssh (or rsh) and take over the two pipes. The child talks the shell
 * protocol on stdin/stdout; its stderr goes to /dev/null because the protocol
 * cannot tell diagnostics from payload.
 */
static gboolean
shfs_spawn (shfs_conn_t *conn, const shfs_conn_params_t *params, GError **error)
{
    int fileset1[2], fileset2[2];
    char gbuf[10];
    const char *argv[10];
    const char *xsh;
    const char *user = params->user;
    int i = 0;
    int res;

    xsh = (params->use_rsh || params->port == SHELL_FLAG_RSH) ? "rsh" : "ssh";

    argv[i++] = xsh;
    if (params->compressed || params->port == SHELL_FLAG_COMPRESSED)
        argv[i++] = "-C";

    if (params->port > SHELL_FLAG_RSH)
    {
        argv[i++] = "-p";
        g_snprintf (gbuf, sizeof (gbuf), "%d", params->port);
        argv[i++] = gbuf;
    }

    /* Name the user only when the URL did: ssh picks the current one anyway,
       and an explicit -l defeats the overrides people keep in ~/.ssh/config. */
    if (user != NULL)
    {
        argv[i++] = "-l";
        argv[i++] = user;
    }

    argv[i++] = params->host;
    argv[i++] = "echo SHELL:; /bin/sh";
    argv[i++] = NULL;

    if (pipe (fileset1) < 0 || pipe (fileset2) < 0)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_FAILED, "%s", _ ("shell: cannot create a pipe"));
        return FALSE;
    }

    res = my_fork ();
    if (res < 0)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_FAILED, "%s", _ ("shell: cannot fork"));
        return FALSE;
    }

    if (res != 0)
    {
        // parent
        close (fileset1[0]);
        close (fileset2[1]);
        conn->sockw = fileset1[1];
        conn->sockr = fileset2[0];
        return TRUE;
    }

    // child
    res = dup2 (fileset1[0], STDIN_FILENO);
    close (fileset1[0]);
    close (fileset1[1]);
    res = dup2 (fileset2[1], STDOUT_FILENO);
    close (STDERR_FILENO);
    res = open ("/dev/null", O_WRONLY);
    (void) res;
    close (fileset2[0]);
    close (fileset2[1]);
    my_execvp (xsh, (char **) argv);
    my_exit (3);

    return FALSE;  // not reached
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Wait for the "SHELL:" line the remote side echoes before handing over to
 * /bin/sh. An external ssh asking for a password shows up here, because it
 * reads from /dev/tty and we only see the prompt go by.
 */
static gboolean
shfs_await_greeting (shfs_conn_t *conn, GError **error)
{
    char answer[BUF_2K];

    if (!shfs_get_line (conn, answer, sizeof (answer), ':'))
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_PROTO, "%s",
                     _ ("shell: no greeting from the remote shell"));
        return FALSE;
    }

    if (strstr (answer, "assword") != NULL)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_FAILED, "%s",
                     _ ("shell: the host wants a password; build with libssh2 to supply one"));
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/** Ask the remote host which utilities it has. */
static gboolean
shfs_probe_host (shfs_conn_t *conn, GError **error)
{
    if (shfs_command (conn, FALSE, FALSE, conn->scr_info, -1) != SHFS_REPLY_COMPLETE)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_PROTO, "%s", _ ("shell: cannot probe the host"));
        return FALSE;
    }

    while (TRUE)
    {
        char buffer[BUF_8K] = "";
        int res;

        res = shfs_get_line_cancellable (conn, buffer, sizeof (buffer));
        if (res == 0 || res == EINTR)
        {
            g_set_error (error, SHFS_ERROR, res == EINTR ? SHFS_ERR_CANCELLED : SHFS_ERR_CLOSED,
                         "%s", _ ("shell: cannot probe the host"));
            return FALSE;
        }

        if (strncmp (buffer, "### ", 4) == 0)
            break;

        conn->host_flags = atol (buffer);
    }

    shfs_conn_set_env (conn, conn->host_flags);

    /* cksum is POSIX and assumed present; the other two are what the probe
       actually found. */
    conn->digest_algos = SHFS_DIGEST_CKSUM;
    if ((conn->host_flags & SHELL_HAVE_MD5) != 0)
        conn->digest_algos |= SHFS_DIGEST_MD5;
    if ((conn->host_flags & SHELL_HAVE_SHA256) != 0)
        conn->digest_algos |= SHFS_DIGEST_SHA256;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

shfs_conn_t *
shfs_conn_open (const shfs_conn_params_t *params, const shfs_connect_cb_t *cb, GError **error)
{
    shfs_conn_t *conn;
    shfs_transport_t transport;
    gboolean terminal_taken = FALSE;

    mc_return_val_if_error (error, NULL);

    conn = g_new0 (shfs_conn_t, 1);
    conn->sockr = -1;
    conn->sockw = -1;
    conn->alive = TRUE;
    conn->env = g_string_sized_new (256);

    if (cb != NULL)
        shfs_conn_set_cancel_cb (conn, cb->cancelled, cb->user_data);

    transport = shfs_transport_resolve (params);

#ifdef ENABLE_SHELL_SSH2
    if (transport == SHFS_TRANSPORT_LIBSSH2)
    {
        GError *ssh2_error = NULL;

        conn->ssh2 = shell_ssh2_open (params, cb, &ssh2_error);

        /* rsh mode and hosts libssh2 cannot reach still work through the
           external program. */
        if (conn->ssh2 == NULL)
        {
            shfs_log_printf (SHFS_LOG_ERRORS, "libssh2 failed (%s), falling back to external ssh",
                             ssh2_error != NULL ? ssh2_error->message : "no reason given");
            transport = SHFS_TRANSPORT_EXTERNAL_SSH;
        }

        g_clear_error (&ssh2_error);
    }
#endif

    if (transport == SHFS_TRANSPORT_EXTERNAL_SSH)
    {
        if (cb != NULL && cb->terminal_acquire != NULL)
        {
            cb->terminal_acquire (cb->user_data);
            terminal_taken = TRUE;
        }

        if (!shfs_spawn (conn, params, error))
            goto err;
    }

    /* The screen stays surrendered until the greeting is in: an external ssh
       asks for the password and the host key while we are waiting for it. */
    if (!shfs_await_greeting (conn, error))
        goto err;

    if (terminal_taken)
    {
        if (cb->terminal_release != NULL)
            cb->terminal_release (cb->user_data);
        terminal_taken = FALSE;
    }

    /* Pin the remote locale before anything parses its output: dates from a
       localised ls are unrecognisable. */
    if (shfs_command (conn, TRUE, FALSE,
                      "LANG=C LC_ALL=C LC_TIME=C; export LANG LC_ALL LC_TIME;\n"
                      "echo '### 200'\n",
                      -1)
        != SHFS_REPLY_COMPLETE)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_PROTO, "%s",
                     _ ("shell: the remote shell did not answer"));
        goto err;
    }

    shfs_conn_load_scripts (conn, params->host);

    if (!shfs_probe_host (conn, error))
        goto err;

    shfs_log_printf (SHFS_LOG_ERRORS, "connected to %s as %s over %s; host flags %#x",
                     params->host != NULL ? params->host : "?",
                     params->user != NULL ? params->user : "(local user)",
                     transport == SHFS_TRANSPORT_LIBSSH2 ? "libssh2" : "external ssh",
                     (unsigned int) conn->host_flags);

    return conn;

err:
    if (terminal_taken && cb->terminal_release != NULL)
        cb->terminal_release (cb->user_data);
    shfs_conn_close (conn);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/** Name the helpers use for an algorithm. */
static const char *
shfs_algo_name (shfs_digest_algo_t algo)
{
    switch (algo)
    {
    case SHFS_DIGEST_SHA256:
        return "sha256";
    case SHFS_DIGEST_MD5:
        return "md5";
    default:
        return "cksum";
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Collect the digest a helper printed on the line before its final reply. It is
 * absent when the host has no digest utility or the helper is an older one;
 * neither is an error, so the algo field says whether anything was obtained.
 */
static void
shfs_take_digest (shfs_conn_t *conn, shfs_digest_algo_t algo, shfs_digest_t *digest)
{
    if (digest == NULL)
        return;

    memset (digest, 0, sizeof (*digest));

    if (algo == SHFS_DIGEST_NONE || conn->reply_str[0] == '\0')
        return;

    g_strlcpy (digest->hex, conn->reply_str, sizeof (digest->hex));
    digest->algo = algo;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_get_begin (shfs_conn_t *conn, const char *path, gint64 offset, gint64 max_bytes,
                gint64 *remaining, GError **error)
{
    char *rpath;
    int code;

    mc_return_val_if_error (error, FALSE);

    if (conn == NULL || !conn->alive)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_CLOSED, "%s", _ ("shell: connection is closed"));
        return FALSE;
    }

    rpath = str_shell_escape (path);
    code = shfs_command_v (conn, TRUE, TRUE, conn->scr_get,
                           "SHELL_FILENAME=%s SHELL_START_OFFSET=%" G_GINT64_FORMAT
                           " SHELL_MAX_BYTES=%" G_GINT64_FORMAT ";\n",
                           rpath, offset, max_bytes > 0 ? max_bytes : (gint64) 0);
    g_free (rpath);

    if (code != SHFS_REPLY_PRELIM)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_NOT_FOUND, "%s",
                     _ ("shell: cannot read the remote file"));
        return FALSE;
    }

    conn->xfer_left = g_ascii_strtoll (conn->reply_str, NULL, 10);
    conn->xfer_reading = TRUE;
    conn->xfer_algo = SHFS_DIGEST_NONE;

    shfs_log_printf (SHFS_LOG_COMMANDS,
                     "reading %s from offset %" G_GINT64_FORMAT ", %" G_GINT64_FORMAT
                     " bytes to come",
                     path, offset, conn->xfer_left);

    if (remaining != NULL)
        *remaining = conn->xfer_left;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gssize
shfs_get_read (shfs_conn_t *conn, void *buf, gsize size, GError **error)
{
    ssize_t n;

    mc_return_val_if_error (error, -1);

    if (conn == NULL || !conn->alive || !conn->xfer_reading)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_CLOSED, "%s",
                     _ ("shell: no transfer in progress"));
        return -1;
    }

    if (conn->xfer_left == 0)
        return 0;

    if ((gint64) size > conn->xfer_left)
        size = (gsize) conn->xfer_left;

    // A signal is not a failure: resizing the window must not tear down a transfer.
    while ((n = shfs_read (conn, buf, size)) < 0)
    {
        if (errno == EINTR && !(conn->cancelled != NULL && conn->cancelled (conn->cancel_data)))
            continue;
        break;
    }

    if (n < 0)
    {
        gboolean cancelled;

        cancelled = (errno == EINTR);
        conn->alive = FALSE;
        g_set_error (error, SHFS_ERROR, cancelled ? SHFS_ERR_CANCELLED : SHFS_ERR_CLOSED, "%s",
                     cancelled ? _ ("shell: the transfer was cancelled")
                               : _ ("shell: the connection broke"));
        return -1;
    }

    conn->xfer_left -= n;

    return n;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_get_finish (shfs_conn_t *conn, shfs_digest_t *digest, GError **error)
{
    int code;

    mc_return_val_if_error (error, FALSE);

    if (conn == NULL || !conn->xfer_reading)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_CLOSED, "%s",
                     _ ("shell: no transfer in progress"));
        return FALSE;
    }

    conn->xfer_reading = FALSE;

    /* Finishing before the stream has run out would leave the channel in the
       middle of the payload, with nothing to tell data from replies. */
    if (conn->xfer_left != 0)
    {
        conn->alive = FALSE;
        g_set_error (error, SHFS_ERROR, SHFS_ERR_PROTO, "%s",
                     _ ("shell: the transfer was left unfinished"));
        return FALSE;
    }

    code = shfs_get_reply (conn, conn->reply_str, sizeof (conn->reply_str) - 1);
    if (code != SHFS_REPLY_COMPLETE)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_PROTO, "%s", _ ("shell: the transfer failed"));
        return FALSE;
    }

    shfs_take_digest (conn, conn->xfer_algo, digest);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_put_begin (shfs_conn_t *conn, const char *path, shfs_write_mode_t mode, gint64 offset,
                gint64 segment_size, gint64 final_size, shfs_digest_algo_t algo, GError **error)
{
    char *rpath;
    int code;

    mc_return_val_if_error (error, FALSE);

    (void) final_size;

    if (conn == NULL || !conn->alive)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_CLOSED, "%s", _ ("shell: connection is closed"));
        return FALSE;
    }

    rpath = str_shell_escape (path);

    if (mode == SHFS_WRITE_TRUNCATE)
        code = shfs_command_v (conn, TRUE, FALSE, conn->scr_send,
                               "SHELL_FILENAME=%s SHELL_FILESIZE=%" G_GINT64_FORMAT
                               " SHELL_DIGEST=%s;\n",
                               rpath, segment_size, shfs_algo_name (algo));
    else if (mode == SHFS_WRITE_APPEND)
        code = shfs_command_v (conn, TRUE, FALSE, conn->scr_append,
                               "SHELL_FILENAME=%s SHELL_FILESIZE=%" G_GINT64_FORMAT
                               " SHELL_DIGEST=%s;\n",
                               rpath, segment_size, shfs_algo_name (algo));
    else
        code = shfs_command_v (conn, TRUE, FALSE, conn->scr_putat,
                               "SHELL_FILENAME=%s SHELL_START_OFFSET=%" G_GINT64_FORMAT
                               " SHELL_FILESIZE=%" G_GINT64_FORMAT " SHELL_DIGEST=%s;\n",
                               rpath, offset, segment_size, shfs_algo_name (algo));

    g_free (rpath);

    if (code != SHFS_REPLY_PRELIM)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_PERMISSION_DENIED, "%s",
                     _ ("shell: cannot write the remote file"));
        return FALSE;
    }

    conn->xfer_left = segment_size;
    conn->xfer_writing = TRUE;
    conn->xfer_algo = algo;

    shfs_log_printf (SHFS_LOG_COMMANDS,
                     "writing %s at offset %" G_GINT64_FORMAT ", %" G_GINT64_FORMAT
                     " bytes, digest %s",
                     path, offset, segment_size, shfs_algo_name (algo));

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_put_write (shfs_conn_t *conn, const void *buf, gsize size, GError **error)
{
    ssize_t n;

    mc_return_val_if_error (error, FALSE);

    if (conn == NULL || !conn->alive || !conn->xfer_writing)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_CLOSED, "%s",
                     _ ("shell: no transfer in progress"));
        return FALSE;
    }

    n = shfs_write (conn, buf, size);
    if (n != (ssize_t) size)
    {
        conn->alive = FALSE;
        g_set_error (error, SHFS_ERROR, SHFS_ERR_CLOSED, "%s", _ ("shell: the connection broke"));
        return FALSE;
    }

    conn->xfer_left -= n;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_put_finish (shfs_conn_t *conn, shfs_digest_t *digest, GError **error)
{
    int code;

    mc_return_val_if_error (error, FALSE);

    if (conn == NULL || !conn->xfer_writing)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_CLOSED, "%s",
                     _ ("shell: no transfer in progress"));
        return FALSE;
    }

    conn->xfer_writing = FALSE;

    if (conn->xfer_left != 0)
    {
        conn->alive = FALSE;
        g_set_error (error, SHFS_ERROR, SHFS_ERR_PROTO, "%s",
                     _ ("shell: fewer bytes were sent than announced"));
        return FALSE;
    }

    code = shfs_get_reply (conn, conn->reply_str, sizeof (conn->reply_str) - 1);
    if (code != SHFS_REPLY_COMPLETE)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_PROTO, "%s", _ ("shell: the transfer failed"));
        return FALSE;
    }

    shfs_take_digest (conn, conn->xfer_algo, digest);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_checksum_range (shfs_conn_t *conn, const char *path, gint64 offset, gint64 length,
                     shfs_digest_algo_t algo, shfs_digest_t *digest, GError **error)
{
    char *rpath;
    char line[BUF_1K];
    int code;

    mc_return_val_if_error (error, FALSE);

    if (conn == NULL || !conn->alive)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_CLOSED, "%s", _ ("shell: connection is closed"));
        return FALSE;
    }

    rpath = str_shell_escape (path);
    code = shfs_command_v (conn, TRUE, FALSE, conn->scr_cksumrange,
                           "SHELL_FILENAME=%s SHELL_START_OFFSET=%" G_GINT64_FORMAT
                           " SHELL_LENGTH=%" G_GINT64_FORMAT " SHELL_DIGEST=%s;\n",
                           rpath, offset, length, shfs_algo_name (algo));
    g_free (rpath);

    /* The helper says "### 001" before anything else, the way get and send do.
       Taking it for the final reply leaves the digest and the closing line
       unread, and every later command on this connection is then out of step. */
    if (code != SHFS_REPLY_PRELIM)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_FAILED, "%s",
                     _ ("shell: cannot checksum the remote file"));
        return FALSE;
    }

    if (!shfs_get_line (conn, line, sizeof (line), '\n'))
    {
        conn->alive = FALSE;
        g_set_error (error, SHFS_ERROR, SHFS_ERR_PROTO, "%s",
                     _ ("shell: cannot checksum the remote file"));
        return FALSE;
    }

    // A host that could not read the file reports it here instead of a digest.
    if (strncmp (line, "### ", 4) == 0)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_FAILED, "%s",
                     _ ("shell: cannot checksum the remote file"));
        return FALSE;
    }

    if (shfs_get_reply (conn, NULL, 0) != SHFS_REPLY_COMPLETE)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_FAILED, "%s",
                     _ ("shell: cannot checksum the remote file"));
        return FALSE;
    }

    memset (digest, 0, sizeof (*digest));
    g_strlcpy (digest->hex, line, sizeof (digest->hex));
    digest->algo = algo;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_block_digests (shfs_conn_t *conn, const char *path, gint64 offset, gint64 length, gint64 block,
                    shfs_digest_algo_t algo, GPtrArray **digests, GError **error)
{
    char *rpath;
    GPtrArray *out;
    char buffer[BUF_1K];
    gboolean started = FALSE;

    mc_return_val_if_error (error, FALSE);

    if (conn == NULL || !conn->alive)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_CLOSED, "%s", _ ("shell: connection is closed"));
        return FALSE;
    }

    rpath = str_shell_escape (path);
    (void) shfs_command_v (conn, FALSE, FALSE, conn->scr_blockdigests,
                           "SHELL_FILENAME=%s SHELL_START_OFFSET=%" G_GINT64_FORMAT
                           " SHELL_LENGTH=%" G_GINT64_FORMAT " SHELL_BLOCKSIZE=%" G_GINT64_FORMAT
                           " SHELL_DIGEST=%s;\n",
                           rpath, offset, length, block, shfs_algo_name (algo));
    g_free (rpath);

    out = g_ptr_array_new_with_free_func (g_free);

    while (TRUE)
    {
        int res;

        res = shfs_get_line_cancellable (conn, buffer, sizeof (buffer));
        if (res == 0 || res == EINTR)
        {
            g_ptr_array_unref (out);
            g_set_error (error, SHFS_ERROR, res == EINTR ? SHFS_ERR_CANCELLED : SHFS_ERR_CLOSED,
                         "%s", _ ("shell: cannot checksum the remote file"));
            return FALSE;
        }

        if (strncmp (buffer, "### ", 4) == 0)
        {
            int cls;

            cls = shfs_decode_reply (buffer + 4, FALSE);

            /* "### 001" opens the answer; it is not the end of it. */
            if (cls == SHFS_REPLY_PRELIM && !started)
            {
                started = TRUE;
                continue;
            }

            if (cls != SHFS_REPLY_COMPLETE)
            {
                g_ptr_array_unref (out);
                g_set_error (error, SHFS_ERROR, SHFS_ERR_FAILED, "%s",
                             _ ("shell: cannot checksum the remote file"));
                return FALSE;
            }
            break;
        }

        if (buffer[0] != '\0')
        {
            shfs_digest_t *d;

            d = g_new0 (shfs_digest_t, 1);
            d->algo = algo;
            g_strlcpy (d->hex, buffer, sizeof (d->hex));
            g_ptr_array_add (out, d);
        }
    }

    *digests = out;

    return TRUE;
}
/* --------------------------------------------------------------------------------------------- */

void
shfs_conn_set_cancel_cb (shfs_conn_t *conn, gboolean (*cancelled) (void *), void *user_data)
{
    conn->cancelled = cancelled;
    conn->cancel_data = user_data;
}

/* --------------------------------------------------------------------------------------------- */

int
shfs_get_line_cancellable (shfs_conn_t *conn, char *buffer, int size)
{
    int i;

    if (conn == NULL)
        return 0;

    for (i = 0; i < size - 1; i++)
    {
        char c;

        if (conn->cancelled != NULL && conn->cancelled (conn->cancel_data))
        {
            buffer[i] = '\0';
            // The channel is left mid-answer, so it cannot be reused.
            conn->alive = FALSE;
            return EINTR;
        }

        if (shfs_read (conn, &c, 1) <= 0)
        {
            buffer[i] = '\0';
            conn->alive = FALSE;
            return 0;
        }

        if (c == '\n')
        {
            buffer[i] = '\0';
            return 1;
        }

        if (c == '\0')
        {
            buffer[i] = '\0';
            return 0;
        }

        buffer[i] = c;
    }

    buffer[i] = '\0';

    return 1;
}

/* --------------------------------------------------------------------------------------------- */

/* The owner the helper sends after the mode, as "uid.gid" in numbers; the panel names them. */
static void
shfs_parse_owner (const char *s, struct stat *st)
{
    unsigned int uid, gid;

    if (sscanf (s, " %u.%u", &uid, &gid) == 2)
    {
        st->st_uid = (uid_t) uid;
        st->st_gid = (gid_t) gid;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
shfs_parse_ls (char *buffer, shfs_entry_t *ent)
{
#define ST ent->st

    buffer++;

    switch (buffer[-1])
    {
    case ':':
    {
        char *filename;
        char *filename_bound;
        char *temp;

        filename = buffer;

        if (strcmp (filename, "\".\"") == 0 || strcmp (filename, "\"..\"") == 0
            || DIR_IS_DOT (filename) || DIR_IS_DOTDOT (filename))
            break;  // We'll do "." and ".." ourselves

        filename_bound = filename + strlen (filename);

        if (S_ISLNK (ST.st_mode))
        {
            char *linkname;
            char *linkname_bound;
            /* we expect: "escaped-name" -> "escaped-name"
               //     -> cannot occur in filenames,
               //     because it will be escaped to -\> */

            linkname_bound = filename_bound;

            if (*filename == '"')
                ++filename;

            linkname = strstr (filename, "\" -> \"");
            if (linkname == NULL)
            {
                // broken client, or smth goes wrong
                linkname = filename_bound;
                if (filename_bound > filename && *(filename_bound - 1) == '"')
                    --filename_bound;  // skip trailing "
            }
            else
            {
                filename_bound = linkname;
                linkname += 6;  // strlen ("\" -> \"")
                if (*(linkname_bound - 1) == '"')
                    --linkname_bound;  // skip trailing "
            }

            ent->name = g_strndup (filename, filename_bound - filename);
            temp = ent->name;
            ent->name = str_shell_unescape (ent->name);
            g_free (temp);

            ent->linkname = g_strndup (linkname, linkname_bound - linkname);
            temp = ent->linkname;
            ent->linkname = str_shell_unescape (ent->linkname);
            g_free (temp);
        }
        else
        {
            // we expect: "escaped-name"
            if (filename_bound - filename > 2)
            {
                /*
                   there is at least 2 "
                   and we skip them
                 */
                if (*filename == '"')
                    ++filename;
                if (*(filename_bound - 1) == '"')
                    --filename_bound;
            }

            ent->name = g_strndup (filename, filename_bound - filename);
            temp = ent->name;
            ent->name = str_shell_unescape (ent->name);
            g_free (temp);
        }
        break;
    }

    case 'S':
        ST.st_size = (off_t) g_ascii_strtoll (buffer, NULL, 10);
        break;

    case 'P':
    {
        // we expect: Pfilemode uid.gid
        size_t skipped;

        if (vfs_parse_filemode (buffer, &skipped, &ST.st_mode))
            shfs_parse_owner (buffer + skipped, &ST);
        break;
    }

    case 'R':
    {
        /*
           raw filemode:
           we expect: Roctal-filemode octal-filetype uid.gid
         */
        size_t skipped;

        if (vfs_parse_raw_filemode (buffer, &skipped, &ST.st_mode))
            shfs_parse_owner (buffer + skipped, &ST);
        break;
    }

    case 'd':
        vfs_split_text (buffer);
        vfs_zero_stat_times (&ST);
        if (vfs_parse_filedate (0, &ST.st_ctime) == 0)
            break;
        ST.st_atime = ST.st_mtime = ST.st_ctime;
        break;

    case 'D':
    {
        struct tm tim;

        memset (&tim, 0, sizeof (tim));
        if (sscanf (buffer, "%d %d %d %d %d %d", &tim.tm_year, &tim.tm_mon, &tim.tm_mday,
                    &tim.tm_hour, &tim.tm_min, &tim.tm_sec)
            != 6)
            break;
        vfs_zero_stat_times (&ST);
        ST.st_atime = ST.st_mtime = ST.st_ctime = mktime (&tim);
    }
    break;

    case 'T':
        // what the link points to: "Td" is a directory, "T!" is nothing
        ent->link_to_dir = buffer[0] == 'd';
        ent->stale_link = buffer[0] == '!';
        break;

    case 'E':
    {
        int maj, min;

        if (sscanf (buffer, "%d,%d", &maj, &min) != 2)
            break;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
        ST.st_rdev = makedev (maj, min);
#endif
    }
    break;

    default:
        break;
    }

#undef ST
}

/* --------------------------------------------------------------------------------------------- */

static void
shfs_entry_free (gpointer data)
{
    shfs_entry_t *ent = (shfs_entry_t *) data;

    g_free (ent->name);
    g_free (ent->linkname);
    g_free (ent);
}

/* --------------------------------------------------------------------------------------------- */

void
shfs_entries_free (GPtrArray *entries)
{
    if (entries != NULL)
        g_ptr_array_unref (entries);
}

/* --------------------------------------------------------------------------------------------- */

GQuark
shfs_error_quark (void)
{
    return g_quark_from_static_string ("shfs-error");
}

/* --------------------------------------------------------------------------------------------- */

GPtrArray *
shfs_list_dir (shfs_conn_t *conn, const char *path, GError **error)
{
    char buffer[BUF_8K] = "\0";
    GPtrArray *entries;
    shfs_entry_t *ent;
    char *quoted_path;
    int reply_code;

    mc_return_val_if_error (error, NULL);

    if (conn == NULL || !conn->alive)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_CLOSED, "%s", _ ("shell: connection is closed"));
        return NULL;
    }

    quoted_path = str_shell_escape (path);
    (void) shfs_command_v (conn, FALSE, FALSE, conn->scr_ls, "SHELL_FILENAME=%s;\n", quoted_path);
    g_free (quoted_path);

    entries = g_ptr_array_new_with_free_func (shfs_entry_free);
    ent = g_new0 (shfs_entry_t, 1);
    ent->st.st_nlink = 1;

    while (TRUE)
    {
        int res;

        res = shfs_get_line_cancellable (conn, buffer, sizeof (buffer));

        if (res == 0 || res == EINTR)
        {
            shfs_entry_free (ent);
            g_ptr_array_unref (entries);
            mc_propagate_error (error, res == EINTR ? SHFS_ERR_CANCELLED : SHFS_ERR_CLOSED, "%s",
                                _ ("shell: reading the directory failed"));
            return NULL;
        }

        if (strncmp (buffer, "### ", 4) == 0)
            break;

        if (buffer[0] != '\0')
            shfs_parse_ls (buffer, ent);
        else if (ent->name != NULL)
        {
            // An empty line ends one entry and starts the next.
            g_ptr_array_add (entries, ent);
            ent = g_new0 (shfs_entry_t, 1);
            ent->st.st_nlink = 1;
        }
    }

    shfs_entry_free (ent);

    reply_code = shfs_decode_reply (buffer + 4, FALSE);
    if (reply_code == SHFS_REPLY_COMPLETE)
        return entries;

    g_ptr_array_unref (entries);
    mc_propagate_error (
        error, reply_code == SHFS_REPLY_ERROR ? SHFS_ERR_PERMISSION_DENIED : SHFS_ERR_FAILED, "%s",
        _ ("shell: reading the directory failed"));

    return NULL;
}
/* --------------------------------------------------------------------------------------------- */

shfs_transport_t
shfs_transport_resolve (const shfs_conn_params_t *params)
{
    if (params->transport != SHFS_TRANSPORT_AUTO)
        return params->transport;

#ifdef ENABLE_SHELL_SSH2
    // rsh asks for a different program entirely, libssh2 cannot serve it.
    if (!params->use_rsh && params->port != SHELL_FLAG_RSH)
        return SHFS_TRANSPORT_LIBSSH2;
#endif

    return SHFS_TRANSPORT_EXTERNAL_SSH;
}

/* --------------------------------------------------------------------------------------------- */

shfs_digest_algo_t
shfs_digest_common (shfs_digest_algo_t a, shfs_digest_algo_t b)
{
    shfs_digest_algo_t both = a & b;

    if ((both & SHFS_DIGEST_SHA256) != 0)
        return SHFS_DIGEST_SHA256;
    if ((both & SHFS_DIGEST_MD5) != 0)
        return SHFS_DIGEST_MD5;
    if ((both & SHFS_DIGEST_CKSUM) != 0)
        return SHFS_DIGEST_CKSUM;

    return SHFS_DIGEST_NONE;
}

/* --------------------------------------------------------------------------------------------- */

void
shfs_conn_load_scripts (shfs_conn_t *conn, const char *hostname)
{
    GPtrArray *found;
    guint k;

    if (conn == NULL)
        return;

    conn->scr_ls =
        shfs_load_script_from_file (hostname, VFS_SHELL_LS_FILE, VFS_SHELL_LS_DEF_CONTENT);
    conn->scr_exists =
        shfs_load_script_from_file (hostname, VFS_SHELL_EXISTS_FILE, VFS_SHELL_EXISTS_DEF_CONTENT);
    conn->scr_mkdir =
        shfs_load_script_from_file (hostname, VFS_SHELL_MKDIR_FILE, VFS_SHELL_MKDIR_DEF_CONTENT);
    conn->scr_unlink =
        shfs_load_script_from_file (hostname, VFS_SHELL_UNLINK_FILE, VFS_SHELL_UNLINK_DEF_CONTENT);
    conn->scr_chown =
        shfs_load_script_from_file (hostname, VFS_SHELL_CHOWN_FILE, VFS_SHELL_CHOWN_DEF_CONTENT);
    conn->scr_chmod =
        shfs_load_script_from_file (hostname, VFS_SHELL_CHMOD_FILE, VFS_SHELL_CHMOD_DEF_CONTENT);
    conn->scr_utime =
        shfs_load_script_from_file (hostname, VFS_SHELL_UTIME_FILE, VFS_SHELL_UTIME_DEF_CONTENT);
    conn->scr_rmdir =
        shfs_load_script_from_file (hostname, VFS_SHELL_RMDIR_FILE, VFS_SHELL_RMDIR_DEF_CONTENT);
    conn->scr_ln =
        shfs_load_script_from_file (hostname, VFS_SHELL_LN_FILE, VFS_SHELL_LN_DEF_CONTENT);
    conn->scr_mv =
        shfs_load_script_from_file (hostname, VFS_SHELL_MV_FILE, VFS_SHELL_MV_DEF_CONTENT);
    conn->scr_hardlink = shfs_load_script_from_file (hostname, VFS_SHELL_HARDLINK_FILE,
                                                     VFS_SHELL_HARDLINK_DEF_CONTENT);
    conn->scr_get =
        shfs_load_script_from_file (hostname, VFS_SHELL_GET_FILE, VFS_SHELL_GET_DEF_CONTENT);
    conn->scr_send =
        shfs_load_script_from_file (hostname, VFS_SHELL_SEND_FILE, VFS_SHELL_SEND_DEF_CONTENT);
    conn->scr_append =
        shfs_load_script_from_file (hostname, VFS_SHELL_APPEND_FILE, VFS_SHELL_APPEND_DEF_CONTENT);
    conn->scr_info =
        shfs_load_script_from_file (hostname, VFS_SHELL_INFO_FILE, VFS_SHELL_INFO_DEF_CONTENT);
    conn->scr_putat =
        shfs_load_script_from_file (hostname, VFS_SHELL_PUTAT_FILE, VFS_SHELL_PUTAT_DEF_CONTENT);
    conn->scr_cksumrange = shfs_load_script_from_file (hostname, VFS_SHELL_CKSUMRANGE_FILE,
                                                       VFS_SHELL_CKSUMRANGE_DEF_CONTENT);
    conn->scr_blockdigests = shfs_load_script_from_file (hostname, VFS_SHELL_BLOCKDIGESTS_FILE,
                                                         VFS_SHELL_BLOCKDIGESTS_DEF_CONTENT);

    if (conn->stale_helpers != NULL)
        g_ptr_array_free (conn->stale_helpers, TRUE);
    conn->stale_helpers = g_ptr_array_new_with_free_func (shfs_helper_free);

    found = shfs_helpers_list (hostname);
    for (k = 0; k < found->len; k++)
    {
        shfs_helper_t *h = (shfs_helper_t *) g_ptr_array_index (found, k);

        if (h->source != SHFS_HELPER_BUILTIN && h->version < h->expected_version)
        {
            shfs_log_printf (SHFS_LOG_ERRORS, "helper %s is revision %d, this build expects %d: %s",
                             h->name, h->version, h->expected_version, h->path);
            g_ptr_array_add (conn->stale_helpers, h);
            g_ptr_array_index (found, k) = NULL;
        }
    }
    shfs_helpers_free (found);
}

/* --------------------------------------------------------------------------------------------- */

void
shfs_conn_set_env (shfs_conn_t *conn, int host_flags)
{
    GString *env = conn->env;

    g_string_truncate (env, 0);

    if ((host_flags & SHELL_HAVE_HEAD) != 0)
        g_string_append (env, "SHELL_HAVE_HEAD=1 export SHELL_HAVE_HEAD; ");
    if ((host_flags & SHELL_HAVE_SED) != 0)
        g_string_append (env, "SHELL_HAVE_SED=1 export SHELL_HAVE_SED; ");
    if ((host_flags & SHELL_HAVE_AWK) != 0)
        g_string_append (env, "SHELL_HAVE_AWK=1 export SHELL_HAVE_AWK; ");
    if ((host_flags & SHELL_HAVE_PERL) != 0)
        g_string_append (env, "SHELL_HAVE_PERL=1 export SHELL_HAVE_PERL; ");
    if ((host_flags & SHELL_HAVE_LSQ) != 0)
        g_string_append (env, "SHELL_HAVE_LSQ=1 export SHELL_HAVE_LSQ; ");
    if ((host_flags & SHELL_HAVE_DATE_MDYT) != 0)
        g_string_append (env, "SHELL_HAVE_DATE_MDYT=1 export SHELL_HAVE_DATE_MDYT; ");
    if ((host_flags & SHELL_HAVE_TAIL) != 0)
        g_string_append (env, "SHELL_HAVE_TAIL=1 export SHELL_HAVE_TAIL; ");
    if ((host_flags & SHELL_HAVE_DD) != 0)
        g_string_append (env, "SHELL_HAVE_DD=1 export SHELL_HAVE_DD; ");
}

/* --------------------------------------------------------------------------------------------- */

void
shfs_conn_close (shfs_conn_t *conn)
{
    if (conn == NULL)
        return;

    shfs_log_printf (SHFS_LOG_ERRORS, "connection closed");

    if (conn->stale_helpers != NULL)
    {
        g_ptr_array_free (conn->stale_helpers, TRUE);
        conn->stale_helpers = NULL;
    }

    conn->alive = FALSE;

#ifdef ENABLE_SHELL_SSH2
    if (conn->ssh2 != NULL)
    {
        shell_ssh2_close (conn->ssh2);
        conn->ssh2 = NULL;
    }
    else
#endif
    {
        if (conn->sockw != -1)
            close (conn->sockw);
        if (conn->sockr != -1 && conn->sockr != conn->sockw)
            close (conn->sockr);
    }

    conn->sockr = -1;
    conn->sockw = -1;

    g_free (conn->scr_ls);
    g_free (conn->scr_exists);
    g_free (conn->scr_mkdir);
    g_free (conn->scr_unlink);
    g_free (conn->scr_chown);
    g_free (conn->scr_chmod);
    g_free (conn->scr_utime);
    g_free (conn->scr_rmdir);
    g_free (conn->scr_ln);
    g_free (conn->scr_mv);
    g_free (conn->scr_hardlink);
    g_free (conn->scr_get);
    g_free (conn->scr_send);
    g_free (conn->scr_append);
    g_free (conn->scr_info);
    g_free (conn->scr_putat);
    g_free (conn->scr_cksumrange);
    g_free (conn->scr_blockdigests);

    if (conn->env != NULL)
        g_string_free (conn->env, TRUE);

    g_free (conn);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_conn_is_alive (const shfs_conn_t *conn)
{
    return (conn != NULL && conn->alive);
}

/* --------------------------------------------------------------------------------------------- */

const GPtrArray *
shfs_conn_stale_helpers (const shfs_conn_t *conn)
{
    return conn != NULL ? conn->stale_helpers : NULL;
}

/* --------------------------------------------------------------------------------------------- */

shfs_digest_algo_t
shfs_conn_digest_algos (const shfs_conn_t *conn)
{
    return (conn != NULL) ? conn->digest_algos : SHFS_DIGEST_NONE;
}

/* --------------------------------------------------------------------------------------------- */

const char *
shfs_last_reply_str (const shfs_conn_t *conn)
{
    if (conn == NULL)
        return "";

    return conn->reply_str;
}

/* --------------------------------------------------------------------------------------------- */

ssize_t
shfs_write (shfs_conn_t *conn, const void *buf, size_t len)
{
    if (conn == NULL)
        return -1;

#ifdef ENABLE_SHELL_SSH2
    if (conn->ssh2 != NULL)
        return shell_ssh2_write (conn->ssh2, buf, len);
#endif
    return write (conn->sockw, buf, len);
}

/* --------------------------------------------------------------------------------------------- */

ssize_t
shfs_read (shfs_conn_t *conn, void *buf, size_t len)
{
    if (conn == NULL)
        return -1;

#ifdef ENABLE_SHELL_SSH2
    if (conn->ssh2 != NULL)
        return shell_ssh2_read (conn->ssh2, buf, len);
#endif
    return read (conn->sockr, buf, len);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_get_line (shfs_conn_t *conn, char *buffer, int size, char term)
{
    int i;

    if (conn == NULL)
        return FALSE;

    for (i = 0; i < size - 1; i++)
    {
        char c;

        if (!shfs_read_byte (conn, &c))
        {
            buffer[i] = '\0';
            return (i > 0);
        }

        if (c == term)
        {
            buffer[i] = '\0';
            return TRUE;
        }

        if (c == '\0')
        {
            buffer[i] = '\0';
            return (i > 0);
        }

        buffer[i] = c;
    }

    buffer[i] = '\0';

    return (i > 0);
}

/* --------------------------------------------------------------------------------------------- */

int
shfs_decode_reply (char *s, gboolean was_garbage)
{
    int code;

    if (sscanf (s, "%d", &code) == 0)
        return SHFS_REPLY_ERROR;

    if (code < 100)
        return was_garbage ? SHFS_REPLY_ERROR
                           : (code == 0 ? SHFS_REPLY_COMPLETE : SHFS_REPLY_PRELIM);

    return code / 100;
}

/* --------------------------------------------------------------------------------------------- */

int
shfs_get_reply (shfs_conn_t *conn, char *string_buf, int string_len)
{
    char answer[BUF_1K];
    gboolean was_garbage = FALSE;

    while (TRUE)
    {
        if (!shfs_get_line (conn, answer, sizeof (answer), '\n'))
        {
            if (string_buf != NULL)
                *string_buf = '\0';

            shfs_log_printf (SHFS_LOG_ERRORS, "the peer stopped talking mid-conversation");

            // The channel is out of step with the protocol and cannot be trusted again.
            conn->alive = FALSE;
            return SHFS_REPLY_TRANSIENT;
        }

        if (strncmp (answer, "### ", 4) == 0)
        {
            shfs_log_printf (SHFS_LOG_COMMANDS, "<- %s%s", answer,
                             was_garbage ? "  (after unexpected output)" : "");
            return shfs_decode_reply (answer + 4, was_garbage);
        }

        shfs_log_printf (SHFS_LOG_TRAFFIC, "<- %s", answer);
        was_garbage = TRUE;
        if (string_buf != NULL)
            g_strlcpy (string_buf, answer, string_len);
    }
}

/* --------------------------------------------------------------------------------------------- */

int
shfs_command (shfs_conn_t *conn, gboolean wait_reply, gboolean want_string, const char *cmd,
              size_t cmd_len)
{
    ssize_t status;

    if (conn == NULL || !conn->alive)
        return SHFS_REPLY_TRANSIENT;

    if (cmd_len == (size_t) (-1))
        cmd_len = strlen (cmd);

    shfs_log_blob (SHFS_LOG_TRAFFIC, "-> sent", cmd, cmd_len);

    status = shfs_write (conn, cmd, cmd_len);
    if (status < 0)
    {
        shfs_log_printf (SHFS_LOG_ERRORS, "write failed: %s", unix_error_string (errno));
        conn->alive = FALSE;
        return SHFS_REPLY_TRANSIENT;
    }

    if (!wait_reply)
    {
        g_assert (!want_string);
        return SHFS_REPLY_COMPLETE;
    }

    return shfs_get_reply (conn, want_string ? conn->reply_str : NULL,
                           sizeof (conn->reply_str) - 1);
}

/* --------------------------------------------------------------------------------------------- */

int
shfs_command_va (shfs_conn_t *conn, gboolean wait_reply, gboolean want_string, const char *scr,
                 const char *vars, va_list ap)
{
    GString *command;
    int r;

    if (conn == NULL)
        return SHFS_REPLY_TRANSIENT;

    if (scr == NULL)
    {
        /* A NULL script trips a GLib assertion inside g_string_append that does
           not say which helper is missing. */
        shfs_log_printf (SHFS_LOG_ERRORS, "a helper script was not loaded; command not sent");
        return SHFS_REPLY_ERROR;
    }

    command = g_string_new (conn->env->str);
    g_string_append_vprintf (command, vars, ap);

    if (shfs_log_get_level () >= SHFS_LOG_COMMANDS)
    {
        /* The variables carry the file names; the script body is the same every
           time and only matters at the traffic level. */
        shfs_log_printf (SHFS_LOG_COMMANDS, "-> %s", command->str + conn->env->len);
    }

    g_string_append (command, scr);

    r = shfs_command (conn, wait_reply, want_string, command->str, command->len);

    g_string_free (command, TRUE);

    return r;
}

/* --------------------------------------------------------------------------------------------- */

int
shfs_command_v (shfs_conn_t *conn, gboolean wait_reply, gboolean want_string, const char *scr,
                const char *vars, ...)
{
    va_list ap;
    int r;

    va_start (ap, vars);
    r = shfs_command_va (conn, wait_reply, want_string, scr, vars, ap);
    va_end (ap);

    return r;
}

/* --------------------------------------------------------------------------------------------- */
