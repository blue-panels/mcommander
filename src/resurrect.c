/*
   M-Commander that survives the loss of its terminal

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
 *  \brief Source: M-Commander that survives the loss of its terminal
 *
 *  When the terminal is hung up (the window closed, the SSH connection dropped) the mc in it
 *  does not exit: it waits in a socket, and a new mc started in another terminal offers to take
 *  over. The new one hands its terminal to the old one and stays as a shim (window size changes
 *  and the interrupt key are passed on, the exit code is the old one's), and the old one redraws
 *  its screen on the terminal it has got and goes on. It is what far2l does, and it is on by
 *  default: --mortal, or immortal=false in [Midnight-Commander], leaves it out.
 *
 *  Unlike far2l mc does not fork at start: nothing changes for a terminal that stays. The
 *  descriptors 0 and 1 (and 2, if it was the same terminal) are replaced by dup2(), so every
 *  part of mc that writes to them, the screen library included, is on the new terminal without
 *  knowing that. The terminal of a new client must have the same $TERM: the tables of the
 *  screen library were made for that one. What the old terminal could do is asked again of
 *  the new one, as at start.
 */

#include <config.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/tty/mouse.h"
#include "lib/tty/resurrect.h"

#include "src/args.h"
#include "src/execute.h"             // post_exec()
#include "src/filemanager/layout.h"  // update_xterm_title_path()

#include "src/resurrect.h"

/*** global variables ****************************************************************************/

gboolean resurrect_immortal = TRUE;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static char *waiting_dir = NULL;
static char *own_term = NULL;
/* the write end of the pipe to the shim of the terminal that took this mc last */
static int notify_fd = -1;
static gboolean stderr_on_terminal = FALSE;
static gboolean in_hangup = FALSE;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* SIGHUP is taken so that it does not kill mc and is not inherited by what mc runs; the terminal
   is found to be gone by reading it (tty_set_hangup_hook) */

static void
hangup_signal (int sig)
{
    (void) sig;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
same_terminal (int fd_a, int fd_b)
{
    struct stat a, b;

    return fstat (fd_a, &a) == 0 && fstat (fd_b, &b) == 0 && S_ISCHR (a.st_mode)
        && S_ISCHR (b.st_mode) && a.st_rdev == b.st_rdev;
}

/* --------------------------------------------------------------------------------------------- */

/* What a mortal mc does when its terminal is hung up. */

static void
die_of_hangup (void)
{
    struct sigaction sa;

    memset (&sa, 0, sizeof (sa));
    sigemptyset (&sa.sa_mask);
    sa.sa_handler = SIG_DFL;
    (void) sigaction (SIGHUP, &sa, NULL);
    (void) kill (getpid (), SIGHUP);
    _exit (EXIT_FAILURE);
}

/* --------------------------------------------------------------------------------------------- */

/* The screen on the terminal that came: as at start, asked what it can do, and drawn again */

static void
attach_new_terminal (void)
{
    // what was switched on for the old terminal is asked again; the switches are off for that
    disable_mouse ();
    disable_bracketed_paste ();
    disable_win32_input ();
    disable_kitty_keyboard ();
    disable_far2l_input ();

    tty_raw_mode ();
    tty_probe_graphics ();
    tty_clear_screen ();

    post_exec ();

    // the size of the new terminal is taken the way a change of it is
    (void) raise (SIGWINCH);
    update_xterm_title_path ();
}

/* --------------------------------------------------------------------------------------------- */

/* Called when the terminal is gone. Returns when there is another. */

static void
resurrect_hangup (void)
{
    resurrect_server_t *srv;
    char *cwd;
    int in_fd = -1, out_fd = -1, new_notify_fd = -1;

    if (in_hangup)
        return;
    in_hangup = TRUE;

    cwd = g_get_current_dir ();
    srv = resurrect_server_new (waiting_dir, own_term, cwd);
    g_free (cwd);

    if (srv == NULL || !resurrect_server_wait (srv, &in_fd, &out_fd, &new_notify_fd))
    {
        resurrect_server_free (srv);
        die_of_hangup ();
        return;
    }
    resurrect_server_free (srv);

    if (dup2 (in_fd, STDIN_FILENO) < 0 || dup2 (out_fd, STDOUT_FILENO) < 0)
        die_of_hangup ();
    if (stderr_on_terminal && dup2 (out_fd, STDERR_FILENO) < 0)
        die_of_hangup ();

    /* The screen library reads the terminal from a descriptor of its own (the one of /dev/tty)
       if it has not been given the standard input: that one is the new terminal as well */
    if (tty_input_fd () > STDERR_FILENO && dup2 (in_fd, tty_input_fd ()) < 0)
        die_of_hangup ();

    if (in_fd > STDERR_FILENO)
        close (in_fd);
    if (out_fd > STDERR_FILENO)
        close (out_fd);

    if (notify_fd >= 0)
        close (notify_fd);
    notify_fd = new_notify_fd;

    attach_new_terminal ();
    in_hangup = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* Give the terminal to a waiting mc; if that works, this process is the shim and does not return.
 */

static void
take_over (const resurrect_entry_t *entry)
{
    int fds[2];
    pid_t pid = 0;

    if (pipe (fds) != 0)
        return;

    if (!resurrect_visit (entry, own_term, STDIN_FILENO, STDOUT_FILENO, fds[1], &pid))
    {
        close (fds[0]);
        close (fds[1]);
        printf ("%s\n", _ ("That M-Commander did not take the terminal."));
        return;
    }

    close (fds[1]);
    exit (resurrect_shim (fds[0], pid));
}

/* --------------------------------------------------------------------------------------------- */

static void
offer_waiting (void)
{
    GPtrArray *list;
    char line[64];
    guint i;

    list = resurrect_list (waiting_dir, own_term);
    if (list->len == 0)
    {
        resurrect_entries_free (list);
        return;
    }

    printf ("%s\n", _ ("Some M-Commanders have lost their terminals and wait nearby:"));
    for (i = 0; i < list->len; i++)
    {
        const resurrect_entry_t *entry = (const resurrect_entry_t *) g_ptr_array_index (list, i);

        printf ("  %u) %s (pid %ld)\n", i + 1, entry->title, (long) entry->pid);
    }
    printf ("%s ", _ ("Number of the one to take here, or Enter to start a new M-Commander:"));
    (void) fflush (stdout);

    if (fgets (line, sizeof (line), stdin) != NULL)
    {
        char *end = NULL;
        const long n = strtol (line, &end, 10);

        if (end != line && n >= 1 && n <= (long) list->len)
            take_over ((const resurrect_entry_t *) g_ptr_array_index (list, (guint) (n - 1)));
    }

    resurrect_entries_free (list);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
resurrect_start (void)
{
    const char *term = getenv ("TERM");
    struct sigaction sa;

    // Only the file manager, in a real terminal of a kind that can be taken over: not the
    // console of Linux (it cannot be hung up), and not the terminal of another mc
    if (!resurrect_immortal || mc_args__mortal || !resurrect_available ()
        || mc_global.mc_run_mode != MC_RUN_FULL || mc_global.run_from_parent_mc
        || !isatty (STDIN_FILENO) || !isatty (STDOUT_FILENO) || term == NULL || term[0] == '\0'
        || strncmp (term, "linux", 5) == 0 || getenv ("MC_TTY") != NULL)
        return;

    waiting_dir = resurrect_dir ();
    if (waiting_dir == NULL)
        return;
    own_term = g_strdup (term);

    offer_waiting ();

    memset (&sa, 0, sizeof (sa));
    sigemptyset (&sa.sa_mask);
    sa.sa_handler = hangup_signal;
    (void) sigaction (SIGHUP, &sa, NULL);

    stderr_on_terminal =
        same_terminal (STDERR_FILENO, STDOUT_FILENO) || same_terminal (STDERR_FILENO, STDIN_FILENO);

    tty_set_hangup_hook (resurrect_hangup);
}

/* --------------------------------------------------------------------------------------------- */

void
resurrect_finish (int exit_code)
{
    (void) fflush (stdout);
    resurrect_send_exit_code (notify_fd, exit_code);
    notify_fd = -1;

    g_free (waiting_dir);
    waiting_dir = NULL;
    g_free (own_term);
    own_term = NULL;
}

/* --------------------------------------------------------------------------------------------- */
