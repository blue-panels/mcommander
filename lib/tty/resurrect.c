/*
   lib/tty - an mc that lost its terminal, and a new mc that takes it over

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

/** \file resurrect.c
 *  \brief Source: an mc that lost its terminal, and a new mc that takes it over
 *
 *  The protocol is described in resurrect.h.
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#include "lib/global.h"

#include "lib/tty/resurrect.h"

#if defined(HAVE_POLL_H) && defined(HAVE_SYS_SOCKET_H) && defined(HAVE_SYS_UN_H)                   \
    && defined(HAVE_STRUCT_MSGHDR_MSG_CONTROL) && defined(SCM_RIGHTS)
#define MC_RESURRECT_ENABLED 1
#endif

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define RESURRECT_DIR_NAME  "mc-resurrect"
#define RESURRECT_HELLO     "MCRV1"
#define RESURRECT_TERM_MAX  256
#define RESURRECT_FDS       3
#define RESURRECT_REPLY_MAX (1 + sizeof (pid_t))
/* how long a server waits for the message of a client that has connected (milliseconds) */
#define RESURRECT_MESSAGE_WAIT 2000
/* how long a client waits for the answer (milliseconds) */
#define RESURRECT_ANSWER_WAIT 5000

/*** file scope type declarations ****************************************************************/

struct resurrect_server_t
{
    int fd;
    char *sock_path;
    char *info_path;
    char *term;
};

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static volatile pid_t shim_target = 0;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
shim_signal (int sig)
{
    if (shim_target > 0)
        (void) kill (shim_target, sig);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef MC_RESURRECT_ENABLED

/* --------------------------------------------------------------------------------------------- */

static gboolean
fill_address (struct sockaddr_un *addr, const char *path)
{
    if (strlen (path) >= sizeof (addr->sun_path))
        return FALSE;

    memset (addr, 0, sizeof (*addr));
    addr->sun_family = AF_UNIX;
    g_strlcpy (addr->sun_path, path, sizeof (addr->sun_path));
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static int
new_socket (void)
{
    int fd;

    fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (fd >= 0)
        (void) fcntl (fd, F_SETFD, FD_CLOEXEC);
    return fd;
}

/* --------------------------------------------------------------------------------------------- */
/* A socket connected to the server, or -1 (errno tells why). */

static int
connect_to (const char *sock_path)
{
    struct sockaddr_un addr;
    int fd, saved_errno;

    if (!fill_address (&addr, sock_path))
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    fd = new_socket ();
    if (fd < 0)
        return -1;

    if (connect (fd, (struct sockaddr *) &addr, sizeof (addr)) != 0)
    {
        saved_errno = errno;
        close (fd);
        errno = saved_errno;
        return -1;
    }

    return fd;
}

/* --------------------------------------------------------------------------------------------- */

static void
close_fds (const int *fds, int count)
{
    int i;

    for (i = 0; i < count; i++)
        if (fds[i] >= 0)
            close (fds[i]);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Read the request of a connected client: its TERM and the descriptors.
 *
 * Takes care of @cfd (closes it) and of the descriptors of a request that is refused.
 */

static gboolean
serve_client (const resurrect_server_t *srv, int cfd, int *in_fd, int *out_fd, int *notify_fd)
{
    struct pollfd pfd;
    union
    {
        struct cmsghdr align;
        char buf[CMSG_SPACE (sizeof (int) * RESURRECT_FDS)];
    } ctl;
    char text[RESURRECT_TERM_MAX + 16];
    int fds[RESURRECT_FDS] = { -1, -1, -1 };
    int count = 0;
    struct iovec iov;
    struct msghdr msg;
    struct cmsghdr *cm;
    ssize_t n;
    gboolean ok = FALSE;
    char reply[RESURRECT_REPLY_MAX];
    size_t reply_len = 1;
    const size_t hello_len = strlen (RESURRECT_HELLO) + 1;

    pfd.fd = cfd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll (&pfd, 1, RESURRECT_MESSAGE_WAIT) <= 0)
    {
        close (cfd);
        return FALSE;
    }

    memset (&msg, 0, sizeof (msg));
    memset (&ctl, 0, sizeof (ctl));
    iov.iov_base = text;
    iov.iov_len = sizeof (text) - 1;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctl.buf;
    msg.msg_controllen = sizeof (ctl.buf);

    n = recvmsg (cfd, &msg, 0);
    if (n <= 0)
    {
        // a probe: someone checked that we are alive
        close (cfd);
        return FALSE;
    }

    for (cm = CMSG_FIRSTHDR (&msg); cm != NULL; cm = CMSG_NXTHDR (&msg, cm))
        if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS)
        {
            size_t i, got = (cm->cmsg_len - CMSG_LEN (0)) / sizeof (int);

            for (i = 0; i < got; i++)
            {
                int fd;

                memcpy (&fd, (char *) CMSG_DATA (cm) + i * sizeof (int), sizeof (fd));
                if (count < RESURRECT_FDS)
                    fds[count++] = fd;
                else
                    close (fd);
            }
        }

    text[n] = '\0';
    ok = count == RESURRECT_FDS && (msg.msg_flags & MSG_CTRUNC) == 0 && (size_t) n > hello_len
        && strncmp (text, RESURRECT_HELLO " ", hello_len) == 0
        && strcmp (text + hello_len, srv->term) == 0;

    reply[0] = ok ? 'Y' : 'N';
    if (ok)
    {
        const pid_t pid = getpid ();

        memcpy (reply + 1, &pid, sizeof (pid));
        reply_len += sizeof (pid);
    }
    // a client that has gone is noticed by the terminal, not here
    MC_UNUSED const ssize_t written = write (cfd, reply, reply_len);
    close (cfd);

    if (!ok)
    {
        close_fds (fds, count);
        return FALSE;
    }

    (void) fcntl (fds[2], F_SETFD, FD_CLOEXEC);
    *in_fd = fds[0];
    *out_fd = fds[1];
    *notify_fd = fds[2];
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Read the info file of a waiting mc; the file name is srv-<pid>.info.
 *
 * @return a new entry, or NULL if the file is not one of ours.
 */

static resurrect_entry_t *
read_entry (const char *dir, const char *name)
{
    char *info_path, *contents = NULL, *stem;
    char **lines;
    resurrect_entry_t *entry = NULL;

    info_path = g_build_filename (dir, name, (char *) NULL);
    if (!g_file_get_contents (info_path, &contents, NULL, NULL))
    {
        g_free (info_path);
        return NULL;
    }

    lines = g_strsplit (contents, "\n", 5);
    g_free (contents);

    if (g_strv_length (lines) >= 4 && strcmp (lines[0], RESURRECT_HELLO) == 0)
    {
        const gint64 pid = g_ascii_strtoll (lines[1], NULL, 10);

        if (pid > 0)
        {
            stem = g_strndup (name, strlen (name) - strlen (".info"));
            entry = g_new0 (resurrect_entry_t, 1);
            entry->info_path = info_path;
            info_path = NULL;
            entry->sock_path = g_strdup_printf ("%s/%s.sock", dir, stem);
            entry->pid = (pid_t) pid;
            entry->term = g_strdup (lines[2]);
            entry->title = g_strdup (lines[3]);
            g_free (stem);
        }
    }

    g_strfreev (lines);
    g_free (info_path);
    return entry;
}

/* --------------------------------------------------------------------------------------------- */

static void
entry_free (gpointer data)
{
    resurrect_entry_t *entry = (resurrect_entry_t *) data;

    g_free (entry->sock_path);
    g_free (entry->info_path);
    g_free (entry->term);
    g_free (entry->title);
    g_free (entry);
}

/* --------------------------------------------------------------------------------------------- */

static gint
entry_compare (gconstpointer a, gconstpointer b)
{
    const resurrect_entry_t *ea = *(resurrect_entry_t *const *) a;
    const resurrect_entry_t *eb = *(resurrect_entry_t *const *) b;

    return (ea->pid > eb->pid) - (ea->pid < eb->pid);
}

/* --------------------------------------------------------------------------------------------- */

#endif  // MC_RESURRECT_ENABLED

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
resurrect_available (void)
{
#ifdef MC_RESURRECT_ENABLED
    return TRUE;
#else
    return FALSE;
#endif
}

/* --------------------------------------------------------------------------------------------- */

char *
resurrect_dir (void)
{
#ifdef MC_RESURRECT_ENABLED
    const char *runtime = getenv ("XDG_RUNTIME_DIR");
    char *dir;
    struct stat st;

    if (runtime != NULL && g_path_is_absolute (runtime))
        dir = g_build_filename (runtime, RESURRECT_DIR_NAME, (char *) NULL);
    else
        dir = g_strdup_printf ("%s/%s-%lu", g_get_tmp_dir (), RESURRECT_DIR_NAME,
                               (unsigned long) getuid ());

    // room for the longest file name in it: srv-<pid>.sock
    if (strlen (dir) + 32 >= sizeof (((struct sockaddr_un *) NULL)->sun_path))
    {
        g_free (dir);
        return NULL;
    }

    if (mkdir (dir, 0700) != 0 && errno != EEXIST)
    {
        g_free (dir);
        return NULL;
    }

    // what the sockets of a user are in must belong to the user and be closed to others
    if (lstat (dir, &st) != 0 || !S_ISDIR (st.st_mode) || st.st_uid != getuid ()
        || (st.st_mode & (S_IRWXG | S_IRWXO)) != 0)
    {
        g_free (dir);
        return NULL;
    }

    return dir;
#else
    return NULL;
#endif
}

/* --------------------------------------------------------------------------------------------- */

resurrect_server_t *
resurrect_server_new (const char *dir, const char *term, const char *title)
{
#ifdef MC_RESURRECT_ENABLED
    resurrect_server_t *srv;
    struct sockaddr_un addr;
    char *text, *clean_title, *p;

    if (dir == NULL || term == NULL || strlen (term) > RESURRECT_TERM_MAX)
        return NULL;

    srv = g_new0 (resurrect_server_t, 1);
    srv->fd = -1;
    srv->sock_path = g_strdup_printf ("%s/srv-%ld.sock", dir, (long) getpid ());
    srv->info_path = g_strdup_printf ("%s/srv-%ld.info", dir, (long) getpid ());
    srv->term = g_strdup (term);

    if (!fill_address (&addr, srv->sock_path))
        goto fail;

    srv->fd = new_socket ();
    if (srv->fd < 0)
        goto fail;

    (void) unlink (srv->sock_path);
    if (bind (srv->fd, (struct sockaddr *) &addr, sizeof (addr)) != 0)
        goto fail;
    (void) chmod (srv->sock_path, S_IRUSR | S_IWUSR);
    if (listen (srv->fd, 8) != 0)
        goto fail_unlink;

    // the title is one line
    clean_title = g_strdup (title != NULL ? title : "");
    for (p = clean_title; *p != '\0'; p++)
        if (*p == '\n' || *p == '\r')
            *p = ' ';
    text =
        g_strdup_printf ("%s\n%ld\n%s\n%s\n", RESURRECT_HELLO, (long) getpid (), term, clean_title);
    g_free (clean_title);
    if (!g_file_set_contents (srv->info_path, text, -1, NULL))
    {
        g_free (text);
        goto fail_unlink;
    }
    g_free (text);

    return srv;

fail_unlink:
    (void) unlink (srv->sock_path);
fail:
    if (srv->fd >= 0)
        close (srv->fd);
    g_free (srv->sock_path);
    g_free (srv->info_path);
    g_free (srv->term);
    g_free (srv);
    return NULL;
#else
    (void) dir;
    (void) term;
    (void) title;
    return NULL;
#endif
}

/* --------------------------------------------------------------------------------------------- */

gboolean
resurrect_server_wait (resurrect_server_t *srv, int *in_fd, int *out_fd, int *notify_fd)
{
#ifdef MC_RESURRECT_ENABLED
    while (TRUE)
    {
        struct pollfd pfd;
        int r, cfd;

        pfd.fd = srv->fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        r = poll (&pfd, 1, RESURRECT_KEEPALIVE_SEC * 1000);
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            return FALSE;
        }

        if (r == 0)
        {
            // a tmp cleaner takes what has not been touched for long
            (void) utimes (srv->sock_path, NULL);
            (void) utimes (srv->info_path, NULL);
            continue;
        }

        if ((pfd.revents & (POLLERR | POLLNVAL)) != 0)
            return FALSE;

        cfd = accept (srv->fd, NULL, NULL);
        if (cfd < 0)
        {
            if (errno == EINTR || errno == EAGAIN || errno == ECONNABORTED)
                continue;
            return FALSE;
        }
        (void) fcntl (cfd, F_SETFD, FD_CLOEXEC);

        if (serve_client (srv, cfd, in_fd, out_fd, notify_fd))
            return TRUE;
    }
#else
    (void) srv;
    (void) in_fd;
    (void) out_fd;
    (void) notify_fd;
    return FALSE;
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
resurrect_server_free (resurrect_server_t *srv)
{
    if (srv == NULL)
        return;

    if (srv->fd >= 0)
        close (srv->fd);
    (void) unlink (srv->sock_path);
    (void) unlink (srv->info_path);
    g_free (srv->sock_path);
    g_free (srv->info_path);
    g_free (srv->term);
    g_free (srv);
}

/* --------------------------------------------------------------------------------------------- */

GPtrArray *
resurrect_list (const char *dir, const char *term)
{
    GPtrArray *entries = g_ptr_array_new_with_free_func (NULL);

#ifdef MC_RESURRECT_ENABLED
    GDir *d;
    const char *name;

    d = g_dir_open (dir, 0, NULL);
    if (d == NULL)
        return entries;

    while ((name = g_dir_read_name (d)) != NULL)
    {
        resurrect_entry_t *entry;
        int fd;

        if (!g_str_has_prefix (name, "srv-") || !g_str_has_suffix (name, ".info"))
            continue;

        entry = read_entry (dir, name);
        if (entry == NULL)
            continue;

        fd = connect_to (entry->sock_path);
        if (fd < 0)
        {
            // nobody listens there: the mc is gone and left its files behind
            if (errno == ECONNREFUSED || errno == ENOENT)
            {
                (void) unlink (entry->sock_path);
                (void) unlink (entry->info_path);
            }
            entry_free (entry);
            continue;
        }
        close (fd);

        if (term == NULL || strcmp (entry->term, term) != 0)
        {
            entry_free (entry);
            continue;
        }

        g_ptr_array_add (entries, entry);
    }

    g_dir_close (d);
    g_ptr_array_sort (entries, entry_compare);
#else
    (void) dir;
    (void) term;
#endif

    return entries;
}

/* --------------------------------------------------------------------------------------------- */

void
resurrect_entries_free (GPtrArray *entries)
{
#ifdef MC_RESURRECT_ENABLED
    guint i;

    if (entries == NULL)
        return;

    for (i = 0; i < entries->len; i++)
        entry_free (g_ptr_array_index (entries, i));
#endif
    if (entries != NULL)
        g_ptr_array_free (entries, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
resurrect_visit (const resurrect_entry_t *entry, const char *term, int in_fd, int out_fd,
                 int notify_wr_fd, pid_t *server_pid)
{
#ifdef MC_RESURRECT_ENABLED
    union
    {
        struct cmsghdr align;
        char buf[CMSG_SPACE (sizeof (int) * RESURRECT_FDS)];
    } ctl;
    int fds[RESURRECT_FDS];
    char *text;
    struct iovec iov;
    struct msghdr msg;
    struct cmsghdr *cm;
    struct pollfd pfd;
    char reply[RESURRECT_REPLY_MAX];
    size_t got = 0;
    gboolean ok = FALSE;
    int fd;

    if (entry == NULL || term == NULL || strlen (term) > RESURRECT_TERM_MAX)
        return FALSE;

    fd = connect_to (entry->sock_path);
    if (fd < 0)
        return FALSE;

    fds[0] = in_fd;
    fds[1] = out_fd;
    fds[2] = notify_wr_fd;

    text = g_strdup_printf ("%s %s", RESURRECT_HELLO, term);
    memset (&msg, 0, sizeof (msg));
    memset (&ctl, 0, sizeof (ctl));
    iov.iov_base = text;
    iov.iov_len = strlen (text);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctl.buf;
    msg.msg_controllen = CMSG_SPACE (sizeof (fds));
    cm = CMSG_FIRSTHDR (&msg);
    cm->cmsg_level = SOL_SOCKET;
    cm->cmsg_type = SCM_RIGHTS;
    cm->cmsg_len = CMSG_LEN (sizeof (fds));
    memcpy (CMSG_DATA (cm), fds, sizeof (fds));

    if (sendmsg (fd, &msg, 0) != (ssize_t) iov.iov_len)
    {
        g_free (text);
        close (fd);
        return FALSE;
    }
    g_free (text);

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    while (got < sizeof (reply) && poll (&pfd, 1, RESURRECT_ANSWER_WAIT) > 0)
    {
        const ssize_t n = read (fd, reply + got, sizeof (reply) - got);

        if (n <= 0)
            break;
        got += (size_t) n;
        if (reply[0] != 'Y')
            break;
    }
    close (fd);

    if (got == RESURRECT_REPLY_MAX && reply[0] == 'Y')
    {
        memcpy (server_pid, reply + 1, sizeof (*server_pid));
        ok = *server_pid > 0;
    }

    return ok;
#else
    (void) entry;
    (void) term;
    (void) in_fd;
    (void) out_fd;
    (void) notify_wr_fd;
    (void) server_pid;
    return FALSE;
#endif
}

/* --------------------------------------------------------------------------------------------- */

int
resurrect_shim (int notify_rd_fd, pid_t server_pid)
{
    struct sigaction sa;
    int code = 0;
    size_t got = 0;

    shim_target = server_pid;

    memset (&sa, 0, sizeof (sa));
    sigemptyset (&sa.sa_mask);
    sa.sa_handler = shim_signal;
    (void) sigaction (SIGWINCH, &sa, NULL);
    (void) sigaction (SIGINT, &sa, NULL);

    // mc does not stop or quit from the keyboard, and what it does not take is not for the shim
    sa.sa_handler = SIG_IGN;
    (void) sigaction (SIGQUIT, &sa, NULL);
    (void) sigaction (SIGTSTP, &sa, NULL);

    while (got < sizeof (code))
    {
        const ssize_t n = read (notify_rd_fd, (char *) &code + got, sizeof (code) - got);

        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return EXIT_FAILURE;  // the server is gone without a word
        got += (size_t) n;
    }

    return code;
}

/* --------------------------------------------------------------------------------------------- */

void
resurrect_send_exit_code (int notify_fd, int code)
{
    size_t done = 0;

    if (notify_fd < 0)
        return;

    while (done < sizeof (code))
    {
        const ssize_t n = write (notify_fd, (const char *) &code + done, sizeof (code) - done);

        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        done += (size_t) n;
    }

    close (notify_fd);
}

/* --------------------------------------------------------------------------------------------- */
