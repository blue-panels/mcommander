/*
   Session plugin for the M-Commander
   The client in the terminal and the holder of the pty of mc

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

/** \file session_helper.c
 *  \brief Source: the client and the holder of a session
 *
 *  session_helper client MC ARGV0 [ARGS...]
 *
 *  The client is the process the user started in a terminal. It lists the sessions that wait,
 *  lets the user pick one or start a new one, and then copies bytes between the terminal and the
 *  holder of the session. The holder owns the master side of the pty mc lives on and the socket
 *  the clients come to. It is the parent of mc: it gets its exit code, and it lives as long as mc.
 *  It never waits for a client: what a slow client does not take is dropped, and mc draws the
 *  whole screen again when a terminal is attached.
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#ifdef HAVE_PTY_H
#include <pty.h>
#endif
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif

#include "lib/global.h"

#include "session_proto.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* What the holder keeps for a client that does not read */
#define CLIENT_BUFFER_LIMIT (1024 * 1024)

/* A holder whose first client never came */
#define FIRST_CLIENT_WAIT_MS (30 * 1000)

/* What the holder keeps of the keys mc does not take, before it drops them */
#define MASTER_BUFFER_LIMIT (1024 * 1024)

/* How long the last words of mc wait for a client that does not read them */
#define FINISH_WAIT_MS 2000

/* How long the holder waits for mc to write a control frame to it */
#define CONTROL_WRITE_MS 1000

/* A client that has not taken what it was sent for so long looks lost */
#define CLIENT_LOST_SECONDS 10

/*** file scope type declarations ****************************************************************/

typedef enum
{
    ATTACH_EXIT,      // mc is over: exit with its code
    ATTACH_SWITCH,    // mc let the terminal go to another session or a new one
    ATTACH_DETACHED,  // mc let the terminal go back to the shell
    ATTACH_KICKED,    // another terminal took it
    ATTACH_REFUSED,   // the session cannot be taken
    ATTACH_LOST,      // the holder went away
    ATTACH_HANGUP     // the terminal went away
} attach_result_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static int winch_pipe[2] = { -1, -1 };
static int child_pipe[2] = { -1, -1 };

static struct termios saved_termios;
static volatile sig_atomic_t termios_saved = 0;  // the terminal is raw: a session is in it

/* What mc may have turned on in a terminal and not turned off */
static const char terminal_reset[] = "\033[?2026l"                      // synchronized output
                                     "\033[?1000;1002;1003;1006;1015l"  // mouse
                                     "\033[?2004l"                      // bracketed paste
                                     "\033[?1004l"                      // focus events
                                     "\033[?9001l"                      // win32-input
                                     "\033[<u"                          // kitty keyboard
                                     "\033[?1049l"                      // alternate screen
                                     "\033[23;0;0t"                     // the title mc pushed
                                     "\033[?25h\033[?1l\033>\033[m";

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
set_nonblock (int fd)
{
    int flags;

    flags = fcntl (fd, F_GETFL);
    if (flags >= 0)
        (void) fcntl (fd, F_SETFL, flags | O_NONBLOCK);
}

/* --------------------------------------------------------------------------------------------- */

static void
set_cloexec (int fd)
{
    int flags;

    flags = fcntl (fd, F_GETFD);
    if (flags >= 0)
        (void) fcntl (fd, F_SETFD, flags | FD_CLOEXEC);
}

/* --------------------------------------------------------------------------------------------- */

static void
self_pipe_handler (int sig)
{
    const int saved = errno;
    const char c = (char) sig;
    const int fd = sig == SIGWINCH ? winch_pipe[1] : child_pipe[1];

    // a full pipe already has a wake-up in it
    if (fd >= 0 && write (fd, &c, 1) < 0)
        errno = saved;
    errno = saved;
}

/* --------------------------------------------------------------------------------------------- */

static void
catch_signal (int sig, void (*handler) (int))
{
    struct sigaction sa;

    memset (&sa, 0, sizeof (sa));
    sigemptyset (&sa.sa_mask);
    sa.sa_handler = handler;
    sa.sa_flags = SA_RESTART;
    (void) sigaction (sig, &sa, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
drain_pipe (int fd)
{
    char buf[64];

    while (read (fd, buf, sizeof (buf)) > 0)
        ;
}

/* --------------------------------------------------------------------------------------------- */

static void
get_window_size (struct winsize *ws)
{
    memset (ws, 0, sizeof (*ws));
    if (ioctl (STDOUT_FILENO, TIOCGWINSZ, ws) != 0 || ws->ws_row == 0 || ws->ws_col == 0)
    {
        ws->ws_row = 24;
        ws->ws_col = 80;
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
session_taken_elsewhere (const char *path)
{
    session_reader_t reader;
    GString *data;
    gboolean taken = FALSE;
    char type;
    int fd;

    fd = session_connect (path);
    if (fd < 0)
        return FALSE;

    session_reader_init (&reader);
    data = g_string_new (NULL);
    if (session_frame_write (fd, SF_INFO, NULL, 0)
        && session_read_frame (fd, &reader, &type, data, 1000) && type == SF_INFO)
    {
        char **lines = g_strsplit (data->str, "\n", -1);

        taken = atoi (session_info_value (lines, "attached")) != 0;
        g_strfreev (lines);
    }
    g_string_free (data, TRUE);
    session_reader_free (&reader);
    close (fd);

    return taken;
}

/* --------------------------------------------------------------------------------------------- */

static void
terminal_raw (void)
{
    struct termios raw;

    if (tcgetattr (STDIN_FILENO, &saved_termios) != 0)
        return;
    termios_saved = 1;

    raw = saved_termios;
    // what cfmakeraw() does, which not every system has (Solaris)
    raw.c_iflag &= ~(tcflag_t) (IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    raw.c_oflag &= ~(tcflag_t) OPOST;
    raw.c_lflag &= ~(tcflag_t) (ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    raw.c_cflag &= ~(tcflag_t) (CSIZE | PARENB);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    (void) tcsetattr (STDIN_FILENO, TCSADRAIN, &raw);
}

/* --------------------------------------------------------------------------------------------- */

static void
terminal_restore (void)
{
    if (termios_saved)
        (void) tcsetattr (STDIN_FILENO, TCSADRAIN, &saved_termios);
    termios_saved = 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
write_terminal (const char *data, gsize len)
{
    while (len > 0)
    {
        ssize_t n;

        n = write (STDOUT_FILENO, data, len);
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

/* mc did not let this terminal go, so the modes it turned on are still on: the keyboard sends
   the codes of win32-input or kitty, the mouse reports, the screen is the alternate one */

static void
terminal_reset_modes (void)
{
    (void) write_terminal (terminal_reset, sizeof (terminal_reset) - 1);
}

/* --------------------------------------------------------------------------------------------- */

/* The client is killed while a session is in its terminal: the terminal is left as the shell
   wants it, as far as what is safe in a signal handler goes */

static void
client_killed (int sig)
{
    if (termios_saved)
    {
        (void) write_terminal (terminal_reset, sizeof (terminal_reset) - 1);
        (void) tcsetattr (STDIN_FILENO, TCSANOW, &saved_termios);
    }
    signal (sig, SIG_DFL);
    raise (sig);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
send_hello (int fd, const char *term)
{
    struct winsize ws;
    GString *hello;
    gboolean ok;
    int i;

    get_window_size (&ws);
    hello = g_string_new (NULL);
    g_string_printf (hello, "%d\n%s\n%u\n%u\n", SESSION_PROTO_VERSION, term, ws.ws_row, ws.ws_col);
    for (i = 0; session_env_names[i] != NULL; i++)
    {
        const char *value = g_getenv (session_env_names[i]);

        if (value != NULL && strchr (value, '\n') == NULL)
            g_string_append_printf (hello, "%s=%s\n", session_env_names[i], value);
        else
            g_string_append_printf (hello, "%s\n", session_env_names[i]);
    }
    ok = session_frame_write (fd, SF_HELLO, hello->str, hello->len);
    g_string_free (hello, TRUE);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Be the terminal of the session behind the socket path until it ends one way or another */

static attach_result_t
attach (const char *path, const char *term, int *exit_code, GString *why)
{
    session_reader_t reader;
    GString *data;
    attach_result_t result = ATTACH_LOST;
    int fd;
    gboolean done = FALSE;

    fd = session_connect (path);
    if (fd < 0 || !send_hello (fd, term))
    {
        if (fd >= 0)
            close (fd);
        return ATTACH_LOST;
    }

    session_reader_init (&reader);
    data = g_string_new (NULL);
    terminal_raw ();
    drain_pipe (winch_pipe[0]);

    while (!done)
    {
        struct pollfd pfd[3];
        char type;

        pfd[0].fd = STDIN_FILENO;
        pfd[0].events = POLLIN;
        pfd[1].fd = fd;
        pfd[1].events = POLLIN;
        pfd[2].fd = winch_pipe[0];
        pfd[2].events = POLLIN;
        pfd[0].revents = pfd[1].revents = pfd[2].revents = 0;

        if (poll (pfd, 3, -1) < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        if ((pfd[2].revents & POLLIN) != 0)
        {
            struct winsize ws;
            char size[32];

            drain_pipe (winch_pipe[0]);
            get_window_size (&ws);
            g_snprintf (size, sizeof (size), "%u %u", ws.ws_row, ws.ws_col);
            (void) session_frame_write (fd, SF_WINCH, size, strlen (size));
        }

        if ((pfd[0].revents & (POLLIN | POLLHUP | POLLERR)) != 0)
        {
            char buf[4096];
            ssize_t n;

            n = read (STDIN_FILENO, buf, sizeof (buf));
            if (n <= 0 && !(n < 0 && errno == EINTR))
            {
                result = ATTACH_HANGUP;
                break;
            }
            if (n > 0 && !session_frame_write (fd, SF_DATA, buf, (gsize) n))
                break;
        }

        if ((pfd[1].revents & (POLLIN | POLLHUP | POLLERR)) != 0)
        {
            gboolean alive;

            alive = session_reader_fill (&reader, fd);
            while (!done && session_reader_next (&reader, &type, data))
                switch (type)
                {
                case SF_DATA:
                    if (!write_terminal (data->str, data->len))
                    {
                        result = ATTACH_HANGUP;
                        done = TRUE;
                    }
                    break;
                case SF_EXIT:
                    *exit_code = atoi (data->str);
                    result = ATTACH_EXIT;
                    done = TRUE;
                    break;
                case SF_SWITCH:
                    g_string_assign (why, data->str);
                    result = ATTACH_SWITCH;
                    done = TRUE;
                    break;
                case SF_DETACHED:
                    result = ATTACH_DETACHED;
                    done = TRUE;
                    break;
                case SF_KICKED:
                    result = ATTACH_KICKED;
                    done = TRUE;
                    break;
                case SF_REFUSED:
                    g_string_assign (why, data->str);
                    result = ATTACH_REFUSED;
                    done = TRUE;
                    break;
                default:
                    break;
                }
            if (!alive && !done)
                break;
        }
    }

    terminal_restore ();
    g_string_free (data, TRUE);
    session_reader_free (&reader);
    close (fd);

    return result;
}

/* --------------------------------------------------------------------------------------------- */
/* The holder */
/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    char *path;
    char *agent_link;  // SSH_AUTH_SOCK of mc: points to the agent of the terminal behind it
    char *lock_path;   // locked for as long as the holder lives, see session_lock_hold()
    char *id;
    char *token;  // starts every mark of this mc, see SESSION_MARK_HEAD
    char *term;
    char *cwd;
    char **mc_argv;  // the program first, then its argv[0] and the arguments
    time_t started;

    int listen_fd;
    int master_fd;
    GString *master_out;  // keys of the client that mc has not taken yet
    int control_fd;
    pid_t mc_pid;

    int client_fd;
    session_reader_t client_reader;
    GString *client_out;
    gsize client_out_sent;
    gboolean forwarding;
    gboolean client_closing;  // close it once what it has is written
    time_t client_stalled;    // since when the client takes nothing, or 0

    int end_fd;        // the client of the list that asked mc to end
    gint64 end_since;  // when it asked

    session_reader_t control_reader;
    GString *pending;  // output of mc that may be the start of a mark
} holder_t;

/* --------------------------------------------------------------------------------------------- */

static void
holder_queue (holder_t *h, char type, const char *data, gsize len)
{
    GString *frame;

    if (h->client_fd < 0)
        return;
    if (type == SF_DATA && h->client_out->len - h->client_out_sent > CLIENT_BUFFER_LIMIT)
        return;  // the client does not read: this is dropped, the redraw on attach repairs it

    frame = session_frame_new (type, data, len);
    g_string_append_len (h->client_out, frame->str, (gssize) frame->len);
    g_string_free (frame, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
holder_drop_client (holder_t *h)
{
    if (h->client_fd >= 0)
        close (h->client_fd);
    h->client_fd = -1;
    h->forwarding = FALSE;
    h->client_closing = FALSE;
    h->client_stalled = 0;
    g_string_truncate (h->client_out, 0);
    h->client_out_sent = 0;
    g_string_truncate (h->client_reader.buf, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
holder_flush_client (holder_t *h)
{
    while (h->client_fd >= 0 && h->client_out_sent < h->client_out->len)
    {
        ssize_t n;

        n = write (h->client_fd, h->client_out->str + h->client_out_sent,
                   h->client_out->len - h->client_out_sent);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                if (h->client_stalled == 0)
                    h->client_stalled = time (NULL);
                return;
            }
            holder_drop_client (h);
            return;
        }
        h->client_out_sent += (gsize) n;
        h->client_stalled = 0;
    }

    if (h->client_fd >= 0)
    {
        g_string_truncate (h->client_out, 0);
        h->client_out_sent = 0;
        if (h->client_closing)
            holder_drop_client (h);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
holder_client_lost (const holder_t *h)
{
    return h->client_fd >= 0 && h->forwarding && h->client_stalled != 0
        && time (NULL) - h->client_stalled >= CLIENT_LOST_SECONDS;
}

/* --------------------------------------------------------------------------------------------- */

static void
holder_answer_end (holder_t *h, const char *answer)
{
    if (h->end_fd < 0)
        return;
    (void) session_frame_write (h->end_fd, SF_END, answer, strlen (answer));
    close (h->end_fd);
    h->end_fd = -1;
}

/* --------------------------------------------------------------------------------------------- */

/* The first n bytes of what mc wrote go to the client, if it is still the one mc writes to */

static void
holder_forward (holder_t *h, gsize n)
{
    if (h->forwarding && n > 0)
        holder_queue (h, SF_DATA, h->pending->str, n);
    g_string_erase (h->pending, 0, (gssize) n);
}

/* --------------------------------------------------------------------------------------------- */

/* The output of mc: forwarded to the client up to the mark mc writes when it lets the terminal
   go, nothing after it */

static void
holder_take_output (holder_t *h, const char *data, gsize len)
{
    const gsize head_len = strlen (SESSION_MARK_HEAD);
    const gsize tail_len = strlen (SESSION_MARK_TAIL);
    const gsize token_len = strlen (h->token);

    g_string_append_len (h->pending, data, (gssize) len);

    while (h->pending->len > 0)
    {
        const char *start = h->pending->str;
        const char *mark, *word, *tail;
        gsize before, rest, word_len;

        mark = memmem (start, h->pending->len, SESSION_MARK_HEAD, head_len);
        if (mark == NULL)
        {
            // the end may be the start of a mark that comes in the next read
            gsize k, keep = 0;

            for (k = MIN (head_len - 1, h->pending->len); k > 0; k--)
                if (memcmp (start + h->pending->len - k, SESSION_MARK_HEAD, k) == 0)
                {
                    keep = k;
                    break;
                }
            holder_forward (h, h->pending->len - keep);
            return;
        }

        before = (gsize) (mark - start);
        word = mark + head_len;
        rest = h->pending->len - before - head_len;
        tail = memmem (word, MIN (rest, SESSION_MARK_WORD_MAX + tail_len), SESSION_MARK_TAIL,
                       tail_len);
        if (tail == NULL && rest < SESSION_MARK_WORD_MAX + tail_len)
        {
            // the mark is not complete yet
            holder_forward (h, before);
            return;
        }

        word_len = tail != NULL ? (gsize) (tail - word) : 0;
        if (tail == NULL || word_len <= token_len || memcmp (word, h->token, token_len) != 0
            || word[token_len] != ';')
        {
            // only looks like a mark, as a program may print: it is output as any other
            holder_forward (h, before + head_len);
            continue;
        }

        {
            // what the mark says is taken before the output in front of it leaves the buffer
            const char *what = word + token_len + 1;
            const gsize what_len = word_len - token_len - 1;
            const gsize switch_len = strlen (SESSION_MARK_SWITCH);
            char type = SF_DETACHED;
            GString *to = g_string_new (NULL);

            if (what_len == strlen (SESSION_MARK_NEW)
                && strncmp (what, SESSION_MARK_NEW, what_len) == 0)
                type = SF_SWITCH;
            else if (what_len > switch_len && strncmp (what, SESSION_MARK_SWITCH, switch_len) == 0)
            {
                type = SF_SWITCH;
                g_string_append_len (to, what + switch_len, (gssize) (what_len - switch_len));
            }

            holder_forward (h, before);
            if (h->forwarding)
            {
                holder_queue (h, type, to->str, to->len);
                h->forwarding = FALSE;
                h->client_closing = TRUE;
            }
            g_string_free (to, TRUE);
        }
        g_string_erase (h->pending, 0, (gssize) (head_len + word_len + tail_len));
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
holder_flush_master (holder_t *h)
{
    while (h->master_fd >= 0 && h->master_out->len > 0)
    {
        ssize_t n;

        n = write (h->master_fd, h->master_out->str, h->master_out->len);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                g_string_truncate (h->master_out, 0);
            return;
        }
        g_string_erase (h->master_out, 0, n);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Keys for mc: what its pty does not take now waits, a paste too; mc that takes nothing for long
   loses them rather than stop the holder */

static void
holder_write_master (holder_t *h, const char *data, gsize len)
{
    if (h->master_out->len + len > MASTER_BUFFER_LIMIT)
        return;
    g_string_append_len (h->master_out, data, (gssize) len);
    holder_flush_master (h);
}

/* --------------------------------------------------------------------------------------------- */

static void
holder_read_master (holder_t *h)
{
    char buf[65536];
    ssize_t n;

    while (h->master_fd >= 0)
    {
        n = read (h->master_fd, buf, sizeof (buf));
        if (n > 0)
        {
            holder_take_output (h, buf, (gsize) n);
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        // EIO: nothing has the slave open now; the end of mc comes as SIGCHLD
        close (h->master_fd);
        h->master_fd = -1;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The link of the agent goes to the one of the new terminal, or away if it has none; the
   environment it came with, without the agent, is left in env */

static void
holder_take_environment (holder_t *h, char **lines, GString *env)
{
    const char *agent = NULL;
    const char *name = "SSH_AUTH_SOCK";
    const size_t name_len = strlen (name);

    for (; *lines != NULL; lines++)
        if (strncmp (*lines, name, name_len) == 0
            && ((*lines)[name_len] == '=' || (*lines)[name_len] == '\0'))
        {
            if ((*lines)[name_len] == '=')
                agent = *lines + name_len + 1;
        }
        else if (**lines != '\0')
            g_string_append_printf (env, "%s\n", *lines);

    if (agent != NULL && agent[0] == '/')
    {
        char *tmp = g_strconcat (h->agent_link, ".new", (char *) NULL);

        (void) unlink (tmp);
        if (symlink (agent, tmp) != 0 || rename (tmp, h->agent_link) != 0)
            (void) unlink (tmp);
        g_free (tmp);
    }
    else
        (void) unlink (h->agent_link);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
holder_start_mc (holder_t *h, const struct winsize *ws)
{
    int master, slave;
    int control[2];
    pid_t pid;

    if (openpty (&master, &slave, NULL, NULL, (struct winsize *) ws) != 0)
        return FALSE;
    if (socketpair (AF_UNIX, SOCK_STREAM, 0, control) != 0)
    {
        close (master);
        close (slave);
        return FALSE;
    }

    pid = fork ();
    if (pid < 0)
    {
        close (master);
        close (slave);
        close (control[0]);
        close (control[1]);
        return FALSE;
    }

    if (pid == 0)
    {
        char fd_text[16];

        (void) setsid ();
#ifdef TIOCSCTTY
        (void) ioctl (slave, TIOCSCTTY, 0);
#endif
        (void) dup2 (slave, STDIN_FILENO);
        (void) dup2 (slave, STDOUT_FILENO);
        (void) dup2 (slave, STDERR_FILENO);
        if (slave > STDERR_FILENO)
            close (slave);
        close (master);
        close (control[0]);
        close (h->listen_fd);
        if (h->client_fd >= 0)
            close (h->client_fd);
        close (child_pipe[0]);
        close (child_pipe[1]);

        g_snprintf (fd_text, sizeof (fd_text), "%d", control[1]);
        g_setenv (SESSION_ENV_ID, h->id, TRUE);
        g_setenv (SESSION_ENV_FD, fd_text, TRUE);
        g_setenv (SESSION_ENV_TOKEN, h->token, TRUE);
        g_setenv ("SSH_AUTH_SOCK", h->agent_link, TRUE);

        signal (SIGHUP, SIG_DFL);
        signal (SIGPIPE, SIG_DFL);
        signal (SIGCHLD, SIG_DFL);

        execv (h->mc_argv[0], h->mc_argv + 1);
        _exit (127);
    }

    close (slave);
    close (control[1]);
    set_cloexec (master);
    set_cloexec (control[0]);
    set_nonblock (master);
    set_nonblock (control[0]);
    h->master_fd = master;
    h->control_fd = control[0];
    h->mc_pid = pid;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* A frame to mc on the control socket; a frame that cannot go out whole spoils the stream, which
   is closed then */

static gboolean
holder_write_control (holder_t *h, char type, const char *data, gsize len)
{
    if (h->control_fd < 0)
        return FALSE;
    if (session_frame_write_timed (h->control_fd, type, data, len, CONTROL_WRITE_MS))
        return TRUE;

    close (h->control_fd);
    h->control_fd = -1;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
holder_send_info (holder_t *h, int fd)
{
    char *text;

    text = g_strdup_printf ("id=%s\npid=%ld\nterm=%s\ncwd=%s\nstarted=%ld\nattached=%d\nlost=%d\n",
                            h->id, (long) h->mc_pid, h->term != NULL ? h->term : "",
                            h->cwd != NULL ? h->cwd : "", (long) h->started,
                            h->client_fd >= 0 && h->forwarding ? 1 : 0,
                            holder_client_lost (h) ? 1 : 0);
    (void) session_frame_write (fd, SF_INFO, text, strlen (text));
    g_free (text);
}

/* --------------------------------------------------------------------------------------------- */

static void
holder_refuse (int fd, const char *why)
{
    (void) session_frame_write (fd, SF_REFUSED, why, strlen (why));
    close (fd);
}

/* --------------------------------------------------------------------------------------------- */

static void
holder_refuse_end (int fd, const char *why)
{
    (void) session_frame_write (fd, SF_END, why, strlen (why));
    close (fd);
}

/* --------------------------------------------------------------------------------------------- */

/* A new connection says what it wants in its first frame: who we are, or to be the terminal */

static void
holder_accept (holder_t *h)
{
    session_reader_t reader;
    GString *data;
    char type;
    int fd;

    fd = accept (h->listen_fd, NULL, NULL);
    if (fd < 0)
        return;
    set_cloexec (fd);

    session_reader_init (&reader);
    data = g_string_new (NULL);

    if (!session_read_frame (fd, &reader, &type, data, 1000))
        close (fd);
    else if (type == SF_INFO)
    {
        holder_send_info (h, fd);
        close (fd);
    }
    else if (type == SF_END)
    {
        // mc is asked; the answer goes to this client when it comes
        if (h->end_fd >= 0)
            holder_refuse_end (fd, "busy");
        else
        {
            // mc ignores a request it gets later than the asker waits: it would end unseen
            char deadline[32];

            g_snprintf (deadline, sizeof (deadline), "%ld",
                        (long) time (NULL) + SESSION_END_WAIT_MS / 1000);
            if (!holder_write_control (h, SF_END, deadline, strlen (deadline)))
                holder_refuse_end (fd, "no answer");
            else
            {
                h->end_fd = fd;
                h->end_since = g_get_monotonic_time ();
            }
        }
    }
    else if (type != SF_HELLO)
        close (fd);
    else
    {
        char **lines = g_strsplit (data->str, "\n", -1);
        const guint n = g_strv_length (lines);
        struct winsize ws;
        GString *env;

        memset (&ws, 0, sizeof (ws));
        if (n >= 4)
        {
            ws.ws_row = (unsigned short) atoi (lines[2]);
            ws.ws_col = (unsigned short) atoi (lines[3]);
        }

        if (n < 4 || atoi (lines[0]) != SESSION_PROTO_VERSION)
            holder_refuse (fd, _ ("another version of M-Commander"));
        else if (h->term != NULL && strcmp (h->term, lines[1]) != 0)
            holder_refuse (fd, _ ("another kind of terminal (TERM)"));
        else
        {
            if (h->client_fd >= 0)
            {
                // the old terminal may hang: it is told if it can be, and let go anyway
                set_nonblock (h->client_fd);
                (void) session_frame_write (h->client_fd, SF_KICKED, NULL, 0);
                holder_drop_client (h);
            }

            h->client_fd = fd;
            set_nonblock (fd);
            h->forwarding = TRUE;
            h->client_closing = FALSE;
            // what came with the hello belongs to this client
            g_string_truncate (h->client_reader.buf, 0);
            g_string_append_len (h->client_reader.buf, reader.buf->str, (gssize) reader.buf->len);
            h->client_reader.bad = FALSE;
            env = g_string_new (NULL);
            holder_take_environment (h, lines + 4, env);

            if (h->mc_pid == 0)
            {
                h->term = g_strdup (lines[1]);
                if (!holder_start_mc (h, &ws))
                {
                    holder_queue (h, SF_EXIT, "127", 3);
                    holder_flush_client (h);
                    holder_drop_client (h);
                }
            }
            else
            {
                if (h->master_fd >= 0 && ws.ws_row > 0 && ws.ws_col > 0)
                    (void) ioctl (h->master_fd, TIOCSWINSZ, &ws);
                (void) holder_write_control (h, SF_ATTACHED, env->str, env->len);
            }
            g_string_free (env, TRUE);
            fd = -1;
        }
        g_strfreev (lines);
    }

    g_string_free (data, TRUE);
    session_reader_free (&reader);
}

/* --------------------------------------------------------------------------------------------- */

static void
holder_read_client (holder_t *h)
{
    GString *data;
    gboolean alive;
    char type;

    alive = session_reader_fill (&h->client_reader, h->client_fd);
    data = g_string_new (NULL);
    while (h->client_fd >= 0 && session_reader_next (&h->client_reader, &type, data))
    {
        if (type == SF_DATA && h->master_fd >= 0 && h->forwarding)
            holder_write_master (h, data->str, data->len);
        else if (type == SF_WINCH && h->master_fd >= 0)
        {
            struct winsize ws;
            unsigned int rows = 0, cols = 0;

            memset (&ws, 0, sizeof (ws));
            if (sscanf (data->str, "%u %u", &rows, &cols) == 2 && rows > 0 && cols > 0)
            {
                ws.ws_row = (unsigned short) rows;
                ws.ws_col = (unsigned short) cols;
                (void) ioctl (h->master_fd, TIOCSWINSZ, &ws);
            }
        }
    }
    g_string_free (data, TRUE);

    if (!alive || h->client_reader.bad)
        holder_drop_client (h);
}

/* --------------------------------------------------------------------------------------------- */

static void
holder_read_control (holder_t *h)
{
    GString *data;
    gboolean alive;
    char type;

    alive = session_reader_fill (&h->control_reader, h->control_fd);
    data = g_string_new (NULL);
    while (session_reader_next (&h->control_reader, &type, data))
        if (type == SF_CWD)
        {
            g_free (h->cwd);
            h->cwd = g_strdup (data->str);
        }
        else if (type == SF_END)
        {
            // mc ends: "ok" is said when it is over and the socket is gone, so a list read after
            // the answer does not have this session any more
            if (strcmp (data->str, "ok") != 0)
                holder_answer_end (h, data->str);
        }
    g_string_free (data, TRUE);

    if (!alive || h->control_reader.bad)
    {
        close (h->control_fd);
        h->control_fd = -1;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* mc is over: what it wrote last goes to the client, then its exit code */

static void
holder_finish (holder_t *h, int status)
{
    char code[16];
    int exit_code;

    holder_read_master (h);
    if (h->pending->len > 0 && h->forwarding)
        holder_queue (h, SF_DATA, h->pending->str, h->pending->len);

    if (WIFEXITED (status))
        exit_code = WEXITSTATUS (status);
    else if (WIFSIGNALED (status))
        exit_code = 128 + WTERMSIG (status);
    else
        exit_code = 1;

    g_snprintf (code, sizeof (code), "%d", exit_code);
    if (h->client_fd >= 0 && (h->forwarding || h->client_closing))
    {
        const gint64 deadline = g_get_monotonic_time () + FINISH_WAIT_MS * 1000;

        holder_queue (h, SF_EXIT, code, strlen (code));
        // the last words are worth a short wait, not more: the client may not read at all
        holder_flush_client (h);
        while (h->client_fd >= 0 && h->client_out_sent < h->client_out->len)
        {
            struct pollfd pfd = { h->client_fd, POLLOUT, 0 };
            const gint64 left = (deadline - g_get_monotonic_time ()) / 1000;

            if (left <= 0 || poll (&pfd, 1, (int) left) <= 0)
                break;
            holder_flush_client (h);
        }
    }

    (void) unlink (h->path);
    (void) unlink (h->agent_link);
    (void) unlink (h->lock_path);
    holder_answer_end (h, "ok");
}

/* --------------------------------------------------------------------------------------------- */

/* The socket of a session under path, listening before anyone can see it: a list that came
   between bind() and listen() would find it refusing, and remove it as the socket of a dead
   session. -1 with EADDRINUSE when the name is taken. */

static int
holder_listen (const char *path)
{
    struct sockaddr_un addr;
    mode_t old_umask;
    int fd, saved;

    if (strlen (path) >= sizeof (addr.sun_path))
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    set_cloexec (fd);

    memset (&addr, 0, sizeof (addr));
    addr.sun_family = AF_UNIX;
    strcpy (addr.sun_path, path);

    // no unlink() before: bind() refuses a name that is taken, and it may be a live session
    old_umask = umask (077);
    if (bind (fd, (struct sockaddr *) &addr, sizeof (addr)) != 0)
    {
        saved = errno;
        umask (old_umask);
        close (fd);
        errno = saved;
        return -1;
    }
    umask (old_umask);

    if (listen (fd, 8) != 0)
    {
        saved = errno;
        (void) unlink (path);
        close (fd);
        errno = saved;
        return -1;
    }

    return fd;
}

/* --------------------------------------------------------------------------------------------- */

static void
holder_main (holder_t *h, int ready_fd)
{
    int null_fd;
    gboolean running = TRUE;

    null_fd = open ("/dev/null", O_RDWR);
    if (null_fd >= 0)
    {
        (void) dup2 (null_fd, STDIN_FILENO);
        (void) dup2 (null_fd, STDOUT_FILENO);
        (void) dup2 (null_fd, STDERR_FILENO);
        if (null_fd > STDERR_FILENO)
            close (null_fd);
    }

    signal (SIGHUP, SIG_IGN);
    signal (SIGPIPE, SIG_IGN);
    signal (SIGINT, SIG_IGN);
    signal (SIGQUIT, SIG_IGN);
    signal (SIGWINCH, SIG_IGN);

    if (pipe (child_pipe) != 0)
        _exit (1);
    set_cloexec (child_pipe[0]);
    set_cloexec (child_pipe[1]);
    set_nonblock (child_pipe[0]);
    set_nonblock (child_pipe[1]);
    catch_signal (SIGCHLD, self_pipe_handler);

    if (write (ready_fd, h->id, SESSION_ID_LEN) != SESSION_ID_LEN)
        _exit (1);
    close (ready_fd);

    h->master_fd = -1;
    h->control_fd = -1;
    h->client_fd = -1;
    h->end_fd = -1;
    h->mc_pid = 0;
    h->started = time (NULL);
    h->client_out = g_string_new (NULL);
    h->master_out = g_string_new (NULL);
    h->pending = g_string_new (NULL);
    session_reader_init (&h->client_reader);
    session_reader_init (&h->control_reader);

    while (running)
    {
        struct pollfd pfd[5];
        int n = 0, i_listen, i_master = -1, i_control = -1, i_client = -1, i_child, timeout;

        i_listen = n;
        pfd[n].fd = h->listen_fd;
        pfd[n++].events = POLLIN;
        i_child = n;
        pfd[n].fd = child_pipe[0];
        pfd[n++].events = POLLIN;
        if (h->master_fd >= 0)
        {
            i_master = n;
            pfd[n].fd = h->master_fd;
            pfd[n++].events = POLLIN | (h->master_out->len > 0 ? POLLOUT : 0);
        }
        if (h->control_fd >= 0)
        {
            i_control = n;
            pfd[n].fd = h->control_fd;
            pfd[n++].events = POLLIN;
        }
        if (h->client_fd >= 0)
        {
            i_client = n;
            pfd[n].fd = h->client_fd;
            pfd[n++].events = POLLIN | (h->client_out_sent < h->client_out->len ? POLLOUT : 0);
        }
        for (int i = 0; i < n; i++)
            pfd[i].revents = 0;

        if (h->mc_pid == 0)
            timeout = FIRST_CLIENT_WAIT_MS;
        else if (h->end_fd >= 0)
            timeout = (int) MAX (
                0, SESSION_END_WAIT_MS - (g_get_monotonic_time () - h->end_since) / 1000);
        else
            timeout = -1;

        if (poll (pfd, (nfds_t) n, timeout) < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        if (h->mc_pid == 0 && pfd[i_listen].revents == 0)
            break;  // the client that started us never came

        // the asker has given up: so does the holder, and mc ignores the request when it comes
        if (h->end_fd >= 0
            && g_get_monotonic_time () - h->end_since >= (gint64) SESSION_END_WAIT_MS * 1000)
            holder_answer_end (h, "no answer");

        if ((pfd[i_child].revents & POLLIN) != 0)
        {
            int status;

            drain_pipe (child_pipe[0]);
            if (h->mc_pid > 0 && waitpid (h->mc_pid, &status, WNOHANG) == h->mc_pid)
            {
                holder_finish (h, status);
                return;
            }
        }

        if (i_master >= 0 && (pfd[i_master].revents & (POLLIN | POLLHUP | POLLERR)) != 0)
            holder_read_master (h);
        if (h->master_fd >= 0 && h->master_out->len > 0)
            holder_flush_master (h);
        if (i_control >= 0 && h->control_fd >= 0
            && (pfd[i_control].revents & (POLLIN | POLLHUP | POLLERR)) != 0)
            holder_read_control (h);
        if (i_client >= 0 && h->client_fd >= 0
            && (pfd[i_client].revents & (POLLIN | POLLHUP | POLLERR)) != 0)
            holder_read_client (h);
        if (h->client_fd >= 0 && h->client_out_sent < h->client_out->len)
            holder_flush_client (h);
        if ((pfd[i_listen].revents & POLLIN) != 0)
            holder_accept (h);
    }

    (void) unlink (h->path);
    (void) unlink (h->agent_link);
    (void) unlink (h->lock_path);
}

/* --------------------------------------------------------------------------------------------- */

/* The name, the lock and the socket of a new session. In the holder process: an fcntl() lock is
   not inherited by fork(). Whatever is left on a failure has its lock free: a list removes it. */

static gboolean
holder_publish (holder_t *h, const char *dir)
{
    int attempt;

    // a name that is taken is never reused: another one is chosen
    for (attempt = 0; attempt < 8; attempt++)
    {
        int lock_fd;

        g_free (h->id);
        g_free (h->path);
        g_free (h->lock_path);
        h->id = session_random_hex (SESSION_ID_LEN);
        h->path = g_strdup_printf ("%s/%s.sock", dir, h->id);
        h->lock_path = g_strdup_printf ("%s/%s.lock", dir, h->id);

        // the lock comes first: a socket without its lock held looks dead to a list
        lock_fd = session_lock_create (h->lock_path);
        if (lock_fd < 0)
        {
            if (errno != EEXIST)
                return FALSE;
            continue;
        }
        if (!session_lock_hold (lock_fd))
        {
            (void) unlink (h->lock_path);
            close (lock_fd);
            return FALSE;
        }
        h->listen_fd = holder_listen (h->path);
        if (h->listen_fd >= 0)
        {
            // lock_fd stays open as long as the holder lives: closing it drops the lock
            h->agent_link = g_strdup_printf ("%s/%s.agent", dir, h->id);
            return TRUE;
        }
        {
            const int saved = errno;

            (void) unlink (h->lock_path);
            close (lock_fd);
            if (saved != EADDRINUSE)
                return FALSE;
        }
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* A new holder, away from the terminal: returns the path of its socket */

static char *
start_holder (const char *dir, char **mc_argv)
{
    holder_t h;
    int ready[2];
    pid_t pid;
    char id[SESSION_ID_LEN];
    ssize_t n;
    gsize got = 0, i;

    if (pipe (ready) != 0)
        return NULL;

    memset (&h, 0, sizeof (h));
    h.mc_argv = mc_argv;
    h.token = session_random_hex (SESSION_TOKEN_LEN);
    h.listen_fd = -1;

    pid = fork ();
    if (pid == 0)
    {
        close (ready[0]);
        (void) setsid ();
        // the holder is the grandchild: no one but init waits for it
        if (fork () == 0)
        {
            if (!holder_publish (&h, dir))
                _exit (1);
            holder_main (&h, ready[1]);
            _exit (0);
        }
        _exit (0);
    }

    close (ready[1]);
    g_free (h.token);
    if (pid > 0)
        (void) waitpid (pid, NULL, 0);

    // the holder says the id of the session once it takes connections
    while (got < sizeof (id))
    {
        n = read (ready[0], id + got, sizeof (id) - got);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        got += (gsize) n;
    }
    close (ready[0]);

    if (got != sizeof (id))
        return NULL;
    for (i = 0; i < got; i++)
        if (!g_ascii_isxdigit (id[i]))
            return NULL;

    return g_strdup_printf ("%s/%.*s.sock", dir, (int) sizeof (id), id);
}

/* --------------------------------------------------------------------------------------------- */

static void
print_sessions (const GPtrArray *list, const char *term)
{
    guint i;

    printf ("\n%s\n\n", _ ("M-Commander sessions"));
    for (i = 0; i < list->len; i++)
    {
        const session_info_t *s = (const session_info_t *) g_ptr_array_index (list, i);
        const char *state;

        if (!s->attached)
            state = _ ("detached");
        else if (s->lost)
            state = _ ("attached, looks lost");
        else
            state = _ ("attached");

        printf ("  %u  %-20s %s", i + 1, state, s->cwd[0] != '\0' ? s->cwd : "?");
        if (strcmp (s->term, term) != 0)
        {
            printf ("   ");
            printf (_ ("TERM=%s, cannot be taken here"), s->term);
        }
        printf ("\n");
    }
    printf ("\n  Enter  %s\n  k N    %s\n  r      %s\n  q      %s\n\n", _ ("new session"),
            _ ("end session N"), _ ("refresh"), _ ("shell"));
}

/* --------------------------------------------------------------------------------------------- */

/* The sessions this terminal can take that nobody works in: detached, or their terminal looks
   lost; the first of them goes to first */

static guint
free_sessions (const GPtrArray *list, const char *term, const session_info_t **first)
{
    guint i, n = 0;

    for (i = 0; i < list->len; i++)
    {
        const session_info_t *s = (const session_info_t *) g_ptr_array_index (list, i);

        if (strcmp (s->term, term) == 0 && (!s->attached || s->lost))
        {
            if (n++ == 0 && first != NULL)
                *first = s;
        }
    }

    return n;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
read_line (char *line, size_t size)
{
    if (fgets (line, (int) size, stdin) == NULL)
        return FALSE;
    g_strstrip (line);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ask_yes (const char *question)
{
    char line[16];

    printf ("%s [y/N] ", question);
    fflush (stdout);
    return read_line (line, sizeof (line)) && (line[0] == 'y' || line[0] == 'Y');
}

/* --------------------------------------------------------------------------------------------- */

/* A session that someone works in is taken or ended only when the user says so */

static gboolean
in_use_confirmed (const session_info_t *s, guint number, const char *format)
{
    char *question;
    gboolean yes;

    if (!s->attached || s->lost)
        return TRUE;

    question = g_strdup_printf (format, number);
    yes = ask_yes (question);
    g_free (question);

    return yes;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
lobby_end (const GPtrArray *list, guint number)
{
    gboolean ended;

    const session_info_t *s;
    char *answer;

    if (number < 1 || number > list->len)
        return FALSE;
    s = (const session_info_t *) g_ptr_array_index (list, number - 1);
    if (!in_use_confirmed (s, number, _ ("Session %u is in use in another terminal. End it?")))
        return FALSE;

    answer = session_end (s->path);
    ended = strcmp (answer, "ok") == 0;
    if (ended)
        printf (_ ("Session %u is ended.\n"), number);
    else if (strcmp (answer, "busy") == 0)
        printf (
            _ ("Session %u has an editor, a viewer or a dialog open: take it and end it there.\n"),
            number);
    else
        printf (_ ("Session %u did not end: %s.\n"), number, answer);
    g_free (answer);

    return ended;
}

/* --------------------------------------------------------------------------------------------- */

/* The list of sessions: the path of the one to take, or NULL for a new one. FALSE: back to the
   shell. */

static gboolean
lobby (const char *dir, const char *term, char **path)
{
    gboolean ended = FALSE;

    *path = NULL;

    while (TRUE)
    {
        GPtrArray *list = session_list (dir);
        char line[64];
        guint number;

        // k has ended the last session nobody works in: there is nothing to choose
        if (ended && free_sessions (list, term, NULL) == 0)
        {
            g_ptr_array_free (list, TRUE);
            return TRUE;
        }

        print_sessions (list, term);
        printf ("%s", _ ("Number to take, Enter, k N, r or q: "));
        fflush (stdout);
        if (!read_line (line, sizeof (line)) || strcmp (line, "q") == 0)
        {
            g_ptr_array_free (list, TRUE);
            return FALSE;
        }

        if (line[0] == '\0' || strcmp (line, "n") == 0)
        {
            g_ptr_array_free (list, TRUE);
            return TRUE;
        }

        if (line[0] == 'k')
        {
            ended = lobby_end (list, (guint) atoi (line + 1));
            g_ptr_array_free (list, TRUE);
            continue;
        }

        number = (guint) atoi (line);
        if (number >= 1 && number <= list->len)
        {
            const session_info_t *s = (const session_info_t *) g_ptr_array_index (list, number - 1);

            if (strcmp (s->term, term) != 0)
                printf ("%s\n", _ ("That session was started in another kind of terminal."));
            else if (in_use_confirmed (s, number,
                                       _ ("Session %u is in use in another terminal. Take it?")))
            {
                *path = g_strdup (s->path);
                g_ptr_array_free (list, TRUE);
                return TRUE;
            }
        }
        g_ptr_array_free (list, TRUE);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Exec mc without a session: something is missing that a session needs */

static void
run_plain (char **mc_argv)
{
    g_setenv (SESSION_ENV_ID, "none", TRUE);
    execv (mc_argv[0], mc_argv + 1);
    perror (mc_argv[0]);
    exit (127);
}

/* --------------------------------------------------------------------------------------------- */

static int
client_main (char **mc_argv)
{
    const char *term = g_getenv ("TERM");
    char *dir;
    gboolean show_lobby;
    char *path = NULL;

    if (term == NULL)
        term = "";

    dir = session_dir ();
    if (dir == NULL)
        run_plain (mc_argv);

    if (pipe (winch_pipe) != 0)
        run_plain (mc_argv);
    set_cloexec (winch_pipe[0]);
    set_cloexec (winch_pipe[1]);
    set_nonblock (winch_pipe[0]);
    set_nonblock (winch_pipe[1]);
    catch_signal (SIGWINCH, self_pipe_handler);
    signal (SIGPIPE, SIG_IGN);
    signal (SIGTERM, client_killed);
    signal (SIGHUP, client_killed);

    {
        // the list is for a choice: none to make with no session free, nor with one detached
        GPtrArray *list = session_list (dir);
        const session_info_t *first = NULL;
        const guint n = free_sessions (list, term, &first);

        show_lobby = n > 1 || (n == 1 && first->attached);
        if (n == 1 && !first->attached)
            path = g_strdup (first->path);
        g_ptr_array_free (list, TRUE);
    }

    while (TRUE)
    {
        GString *why;
        int exit_code = 0;
        attach_result_t result;

        if (show_lobby && !lobby (dir, term, &path))
            return 0;

        if (path == NULL)
        {
            path = start_holder (dir, mc_argv);
            if (path == NULL)
                run_plain (mc_argv);
        }

        why = g_string_new (NULL);
        result = attach (path, term, &exit_code, why);
        // the holder of a client that was not read could not say KICKED: it only let it go
        if (result == ATTACH_LOST && session_taken_elsewhere (path))
            result = ATTACH_KICKED;
        g_clear_pointer (&path, g_free);

        switch (result)
        {
        case ATTACH_EXIT:
            // killed by a signal (the holder says 128 + its number): mc did not leave the
            // terminal as it found it
            if (exit_code > 128)
                terminal_reset_modes ();
            g_string_free (why, TRUE);
            return exit_code;
        case ATTACH_DETACHED:
            printf ("\n[%s]\n", _ ("detached; run M-Commander again to take it back"));
            g_string_free (why, TRUE);
            return 0;
        case ATTACH_KICKED:
            terminal_reset_modes ();
            printf ("\r\n[%s]\r\n", _ ("the session was taken by another terminal"));
            g_string_free (why, TRUE);
            return 0;
        case ATTACH_HANGUP:
            g_string_free (why, TRUE);
            return 0;
        case ATTACH_REFUSED:
            printf ("\n");
            printf (_ ("The session cannot be taken: %s\n"), why->str);
            break;
        case ATTACH_LOST:
            terminal_reset_modes ();
            printf ("\n%s\n", _ ("The session is gone."));
            break;
        case ATTACH_SWITCH:
            // an id is hex digits only: nothing else is a name in the directory
            if (why->len > 0 && strspn (why->str, "0123456789abcdef") == why->len)
                path = g_strdup_printf ("%s/%s.sock", dir, why->str);
            g_string_free (why, TRUE);
            show_lobby = FALSE;
            continue;
        default:
            break;
        }
        g_string_free (why, TRUE);
        show_lobby = TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
main (int argc, char *argv[])
{
    // what the mc that exec'ed us had open is not for the holder nor for the next mc
#ifdef HAVE_CLOSEFROM
    closefrom (STDERR_FILENO + 1);
#else
    {
        long fd, max_fd = sysconf (_SC_OPEN_MAX);

        for (fd = STDERR_FILENO + 1; fd < (max_fd > 0 ? max_fd : 1024); fd++)
            (void) close ((int) fd);
    }
#endif

    (void) setlocale (LC_ALL, "");
    (void) bindtextdomain (PACKAGE, LOCALEDIR);
    (void) textdomain (PACKAGE);

    if (argc >= 4 && strcmp (argv[1], "client") == 0)
        return client_main (argv + 2);

    fprintf (stderr, "usage: %s client MC ARGV0 [ARGS...]\n", argv[0]);
    return 2;
}

/* --------------------------------------------------------------------------------------------- */
