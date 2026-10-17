/*
   Session plugin for the M-Commander
   Frames and the directory of the sockets

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

/** \file session_proto.c
 *  \brief Source: frames and the directory of the sockets of the session plugin
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "lib/fileloc.h"  // MC_USERCONF_DIR

#include "session_proto.h"

/*** global variables ****************************************************************************/

const char *const session_env_names[] = {
    "SSH_AUTH_SOCK",
    "SSH_AGENT_PID",
    "DISPLAY",
    "WAYLAND_DISPLAY",
    "DBUS_SESSION_BUS_ADDRESS",
    "XAUTHORITY",
    "SSH_CONNECTION",
    "SSH_CLIENT",
    NULL,
};

/*** file scope macro definitions ****************************************************************/

/* How long a connection that is in progress may take */
#define CONNECT_WAIT_MS 1000

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gint
session_info_compare (gconstpointer a, gconstpointer b)
{
    const session_info_t *x = *(const session_info_t *const *) a;
    const session_info_t *y = *(const session_info_t *const *) b;

    return x->started < y->started ? -1 : x->started > y->started ? 1 : 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
write_all (int fd, const char *data, gsize len)
{
    while (len > 0)
    {
        ssize_t n;

        n = write (fd, data, len);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return FALSE;
        }
        data += n;
        len -= (gsize) n;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* A non-blocking connect() that is in progress: whether it gets through within timeout_ms */

static gboolean
connect_completes (int fd, int timeout_ms)
{
    struct pollfd pfd = { fd, POLLOUT, 0 };
    int error = 0;
    socklen_t len = sizeof (error);

    if (errno != EINPROGRESS)
        return FALSE;
    if (poll (&pfd, 1, timeout_ms) <= 0)
    {
        errno = EAGAIN;
        return FALSE;
    }
    if (getsockopt (fd, SOL_SOCKET, SO_ERROR, &error, &len) != 0)
        return FALSE;
    errno = error;
    return error == 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
ms_left (gint64 deadline)
{
    const gint64 left = (deadline - g_get_monotonic_time ()) / 1000;

    return left > 0 ? (int) left : 0;
}

/* --------------------------------------------------------------------------------------------- */

/* A directory of ours only: made with 0700 if it is not there, refused if it is anyone else's or
   others can get in */

static gboolean
private_dir (const char *path)
{
    struct stat st;

    if (mkdir (path, 0700) != 0 && errno != EEXIST)
        return FALSE;

    return lstat (path, &st) == 0 && S_ISDIR (st.st_mode) && st.st_uid == getuid ()
        && (st.st_mode & 077) == 0;
}

/* --------------------------------------------------------------------------------------------- */

/* A socket that does not answer: busy, or its holder is dead. The lock of the holder tells which,
   whatever the system says about a full backlog (BSD refuses then, as for a dead socket). */

static void
session_remove_if_dead (const char *path, int connect_errno)
{
    const int base_len = (int) (strlen (path) - strlen (".sock"));
    char *lock_path;
    int fd;
    gboolean dead;

    lock_path = g_strdup_printf ("%.*s.lock", base_len, path);
    fd = open (lock_path, O_RDWR | O_CLOEXEC);
    if (fd >= 0)
        dead = session_lock_hold (fd);
    else
        // a holder of a version without the lock
        dead = errno == ENOENT && (connect_errno == ECONNREFUSED || connect_errno == ENOENT);

    if (dead)
    {
        char *agent;

        agent = g_strdup_printf ("%.*s.agent", base_len, path);
        (void) unlink (path);
        (void) unlink (agent);
        (void) unlink (lock_path);
        g_free (agent);
    }
    if (fd >= 0)
        close (fd);
    g_free (lock_path);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

GString *
session_frame_new (char type, const void *data, gsize len)
{
    GString *frame;
    unsigned char header[SESSION_FRAME_HEADER];

    header[0] = (unsigned char) type;
    header[1] = (unsigned char) ((len >> 24) & 0xff);
    header[2] = (unsigned char) ((len >> 16) & 0xff);
    header[3] = (unsigned char) ((len >> 8) & 0xff);
    header[4] = (unsigned char) (len & 0xff);

    frame = g_string_sized_new (SESSION_FRAME_HEADER + len);
    g_string_append_len (frame, (const char *) header, SESSION_FRAME_HEADER);
    if (len > 0)
        g_string_append_len (frame, (const char *) data, (gssize) len);

    return frame;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
session_frame_write (int fd, char type, const void *data, gsize len)
{
    GString *frame;
    gboolean ok;

    frame = session_frame_new (type, data, len);
    ok = write_all (fd, frame->str, frame->len);
    g_string_free (frame, TRUE);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
session_frame_write_timed (int fd, char type, const void *data, gsize len, int timeout_ms)
{
    const gint64 deadline = g_get_monotonic_time () + (gint64) timeout_ms * 1000;
    GString *frame;
    gsize done = 0;

    frame = session_frame_new (type, data, len);
    while (done < frame->len)
    {
        ssize_t n;

        n = write (fd, frame->str + done, frame->len - done);
        if (n > 0)
            done += (gsize) n;
        else if (n < 0 && errno == EINTR)
            continue;
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            struct pollfd pfd = { fd, POLLOUT, 0 };

            if (poll (&pfd, 1, ms_left (deadline)) <= 0)
                break;
        }
        else
            break;
    }
    g_string_free (frame, TRUE);

    return done == SESSION_FRAME_HEADER + len;
}

/* --------------------------------------------------------------------------------------------- */

char *
session_random_hex (int digits)
{
    GString *hex;

    hex = g_string_sized_new ((gsize) digits);
    while ((int) hex->len < digits)
        g_string_append_printf (hex, "%08x", g_random_int ());
    g_string_truncate (hex, (gsize) digits);

    return g_string_free (hex, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
session_reader_init (session_reader_t *reader)
{
    reader->buf = g_string_new (NULL);
    reader->bad = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
session_reader_free (session_reader_t *reader)
{
    if (reader->buf != NULL)
        g_string_free (reader->buf, TRUE);
    reader->buf = NULL;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
session_reader_fill (session_reader_t *reader, int fd)
{
    char chunk[65536];
    ssize_t n;

    do
        n = read (fd, chunk, sizeof (chunk));
    while (n < 0 && errno == EINTR);

    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return TRUE;
    if (n <= 0)
        return FALSE;

    g_string_append_len (reader->buf, chunk, n);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
session_reader_next (session_reader_t *reader, char *type, GString *data)
{
    const unsigned char *p = (const unsigned char *) reader->buf->str;
    gsize len;

    if (reader->bad || reader->buf->len < SESSION_FRAME_HEADER)
        return FALSE;

    len = ((gsize) p[1] << 24) | ((gsize) p[2] << 16) | ((gsize) p[3] << 8) | (gsize) p[4];
    if (len > SESSION_FRAME_MAX)
    {
        // nobody of ours sends that
        g_string_truncate (reader->buf, 0);
        reader->bad = TRUE;
        return FALSE;
    }
    if (reader->buf->len < SESSION_FRAME_HEADER + len)
        return FALSE;

    *type = (char) p[0];
    g_string_assign (data, "");
    g_string_append_len (data, reader->buf->str + SESSION_FRAME_HEADER, (gssize) len);
    g_string_erase (reader->buf, 0, (gssize) (SESSION_FRAME_HEADER + len));

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

char *
session_dir (void)
{
    const char *runtime_dir = g_getenv ("XDG_RUNTIME_DIR");
    struct sockaddr_un addr;
    char *base, *dir;

    // "/<id>.agent.new" has to fit after the directory
    if (runtime_dir != NULL && strlen (runtime_dir) + 32 >= sizeof (addr.sun_path))
        runtime_dir = NULL;

    if (runtime_dir != NULL && runtime_dir[0] == '/')
    {
        base = g_build_filename (runtime_dir, MC_USERCONF_DIR, (char *) NULL);
        dir = g_build_filename (base, "sessions", (char *) NULL);
    }
    else
    {
        char *name;

        name = g_strdup_printf ("%s-sessions-%lu", MC_USERCONF_DIR, (unsigned long) getuid ());
        base = g_strdup (g_get_tmp_dir ());
        dir = g_build_filename (base, name, (char *) NULL);
        g_free (name);
    }

    // the base is only made here, it may be shared: only the last one must be ours alone
    if (runtime_dir != NULL && runtime_dir[0] == '/' && mkdir (base, 0700) != 0 && errno != EEXIST)
        g_clear_pointer (&dir, g_free);
    g_free (base);

    if (dir != NULL && !private_dir (dir))
        g_clear_pointer (&dir, g_free);

    return dir;
}

/* --------------------------------------------------------------------------------------------- */

int
session_connect (const char *path)
{
    struct sockaddr_un addr;
    int fd, flags;

    if (strlen (path) >= sizeof (addr.sun_path))
        return -1;

    fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    (void) fcntl (fd, F_SETFD, FD_CLOEXEC);

    memset (&addr, 0, sizeof (addr));
    addr.sun_family = AF_UNIX;
    strcpy (addr.sun_path, path);
    // a holder that does not accept fills its backlog: then connect() would wait for ever. A
    // non-blocking one says EAGAIN on Linux: the session is left out of this list, and its lock
    // tells whether it is dead. Elsewhere it may say EINPROGRESS and go on.
    flags = fcntl (fd, F_GETFL);
    if (flags < 0 || fcntl (fd, F_SETFL, flags | O_NONBLOCK) != 0
        || (connect (fd, (struct sockaddr *) &addr, sizeof (addr)) != 0
            && !connect_completes (fd, CONNECT_WAIT_MS))
        || fcntl (fd, F_SETFL, flags) != 0)
    {
        const int saved = errno;

        close (fd);
        errno = saved;
        return -1;
    }

    return fd;
}

/* --------------------------------------------------------------------------------------------- */

int
session_lock_create (const char *lock_path)
{
    // O_EXCL: a lock file that is there belongs to another holder, alive or not
    return open (lock_path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
session_lock_hold (int fd)
{
    struct flock l;  // not flock(): Solaris has none

    memset (&l, 0, sizeof (l));
    l.l_type = F_WRLCK;
    l.l_whence = SEEK_SET;

    return fcntl (fd, F_SETLK, &l) == 0;
}

/* --------------------------------------------------------------------------------------------- */

/* One frame, waiting at most timeout_ms in all for it */

gboolean
session_read_frame (int fd, session_reader_t *reader, char *type, GString *data, int timeout_ms)
{
    const gint64 deadline = g_get_monotonic_time () + (gint64) timeout_ms * 1000;

    while (!session_reader_next (reader, type, data))
    {
        struct pollfd pfd = { fd, POLLIN, 0 };

        // a sender that gives a byte at a time does not get a new wait for each
        if (reader->bad || poll (&pfd, 1, ms_left (deadline)) <= 0
            || !session_reader_fill (reader, fd))
            return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

const char *
session_info_value (char **lines, const char *key)
{
    const size_t key_len = strlen (key);
    char **l;

    for (l = lines; l != NULL && *l != NULL; l++)
        if (strncmp (*l, key, key_len) == 0 && (*l)[key_len] == '=')
            return *l + key_len + 1;

    return "";
}

/* --------------------------------------------------------------------------------------------- */

void
session_info_free (gpointer data)
{
    session_info_t *info = (session_info_t *) data;

    g_free (info->path);
    g_free (info->id);
    g_free (info->term);
    g_free (info->cwd);
    g_free (info);
}

/* --------------------------------------------------------------------------------------------- */

/* Ask every socket of the directory who it is; the ones nobody listens on are removed */

GPtrArray *
session_list (const char *dir)
{
    GPtrArray *list;
    GDir *d;
    const char *name;

    list = g_ptr_array_new_with_free_func (session_info_free);

    d = g_dir_open (dir, 0, NULL);
    if (d == NULL)
        return list;

    while ((name = g_dir_read_name (d)) != NULL)
    {
        char *path;
        int fd;
        session_reader_t reader;
        GString *data;
        char type;

        if (!g_str_has_suffix (name, ".sock"))
            continue;

        path = g_build_filename (dir, name, (char *) NULL);
        fd = session_connect (path);
        if (fd < 0)
        {
            session_remove_if_dead (path, errno);
            g_free (path);
            continue;
        }

        session_reader_init (&reader);
        data = g_string_new (NULL);
        if (session_frame_write (fd, SF_INFO, NULL, 0)
            && session_read_frame (fd, &reader, &type, data, 1000) && type == SF_INFO)
        {
            char **lines = g_strsplit (data->str, "\n", -1);
            session_info_t *info = g_new0 (session_info_t, 1);

            info->path = path;
            path = NULL;
            info->id = g_strdup (session_info_value (lines, "id"));
            info->term = g_strdup (session_info_value (lines, "term"));
            info->cwd = g_strdup (session_info_value (lines, "cwd"));
            info->pid = atol (session_info_value (lines, "pid"));
            info->started = atol (session_info_value (lines, "started"));
            info->attached = atoi (session_info_value (lines, "attached")) != 0;
            info->lost = atoi (session_info_value (lines, "lost")) != 0;
            g_ptr_array_add (list, info);
            g_strfreev (lines);
        }
        g_string_free (data, TRUE);
        session_reader_free (&reader);
        close (fd);
        g_free (path);
    }
    g_dir_close (d);

    g_ptr_array_sort (list, session_info_compare);
    return list;
}

/* --------------------------------------------------------------------------------------------- */

/* Ask the mc of a session to end; what it says, or why there is no answer */

char *
session_end (const char *path)
{
    session_reader_t reader;
    GString *data;
    char *answer = NULL;
    char type;
    int fd;

    fd = session_connect (path);
    if (fd < 0)
        return g_strdup ("gone");

    session_reader_init (&reader);
    data = g_string_new (NULL);
    if (session_frame_write (fd, SF_END, NULL, 0)
        && session_read_frame (fd, &reader, &type, data, SESSION_END_WAIT_MS) && type == SF_END)
        answer = g_strdup (data->str);
    g_string_free (data, TRUE);
    session_reader_free (&reader);
    close (fd);

    return answer != NULL ? answer : g_strdup ("no answer");
}

/* --------------------------------------------------------------------------------------------- */
