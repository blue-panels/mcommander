/*
   tests/src/mcterm_pty_send.c -- unit tests for the paste into a pty master

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

#define TEST_SUITE_NAME "/src/mcterm_pty_send"

#include "tests/mctest.h"

#include <fcntl.h>
#ifdef HAVE_PTY_H
#include <pty.h>
#endif
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "src/mcterm/mcterm.h"

// More than the kernel keeps for a pty nobody reads.
#define TEXT_LEN   (300 * 1024)
#define STALL_USEC (200 * G_USEC_PER_SEC / 1000)

/*** file scope variables ************************************************************************/

static int master = -1;
static int slave = -1;
static pid_t child = -1;
static char *text = NULL;
static size_t drained = 0;

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

// The slave in raw mode with no echo: what is written stays in its input queue.
static void
raw_slave (void)
{
    struct termios tio;

    tcgetattr (slave, &tio);
    // What cfmakeraw () does, spelled out: Solaris has no cfmakeraw ().
    tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tio.c_oflag &= ~OPOST;
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_cflag &= ~(CSIZE | PARENB);
    tio.c_cflag |= CS8;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    tcsetattr (slave, TCSANOW, &tio);
}

/* --------------------------------------------------------------------------------------------- */

// A child that reads the slave and throws the bytes away.
static void
start_reader (void)
{
    child = fork ();
    if (child == 0)
    {
        char buf[4096];

        close (master);
        while (read (slave, buf, sizeof (buf)) > 0)
            ;
        _exit (0);
    }
}

/* --------------------------------------------------------------------------------------------- */

// A child that writes to the slave without end and never reads it.
static void
start_writer (void)
{
    child = fork ();
    if (child == 0)
    {
        char buf[1024];

        close (master);
        memset (buf, 'o', sizeof (buf));
        while (write (slave, buf, sizeof (buf)) > 0)
            ;
        _exit (0);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
drain (int fd, void *data)
{
    char buf[65536];
    ssize_t n;

    (void) data;

    n = read (fd, buf, sizeof (buf));
    if (n > 0)
        drained += (size_t) n;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
drain_gone (int fd, void *data)
{
    (void) data;

    close (fd);
    master = -1;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    ck_assert_int_eq (openpty (&master, &slave, NULL, NULL, NULL), 0);
    raw_slave ();
    text = g_malloc (TEXT_LEN);
    memset (text, 'x', TEXT_LEN);
    drained = 0;
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    if (child > 0)
    {
        kill (child, SIGKILL);
        waitpid (child, NULL, 0);
        child = -1;
    }
    // What nobody read is dropped first: the close of a STREAMS pty (Solaris) waits for it.
    if (slave >= 0)
    {
        tcflush (slave, TCIOFLUSH);
        close (slave);
    }
    if (master >= 0)
    {
        tcflush (master, TCIOFLUSH);
        close (master);
    }
    master = slave = -1;
    g_free (text);
    text = NULL;
}

/* --------------------------------------------------------------------------------------------- */
/*** tests ***************************************************************************************/
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_all_goes_through_when_the_slave_is_read)
{
    gint64 t0;

    start_reader ();
    t0 = g_get_monotonic_time ();
    mctest_assert_true (mcterm_pty_send (master, text, TEXT_LEN, STALL_USEC, drain, NULL));
    // The reader takes it as fast as it comes; nothing waited out the stall.
    ck_assert_int_lt (g_get_monotonic_time () - t0, 5 * STALL_USEC);
    // The master is left blocking, as it was.
    ck_assert_int_eq (fcntl (master, F_GETFL) & O_NONBLOCK, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_gives_up_when_nobody_reads)
{
    gint64 t0, took;

    t0 = g_get_monotonic_time ();
    mctest_assert_false (mcterm_pty_send (master, text, TEXT_LEN, STALL_USEC, drain, NULL));
    took = g_get_monotonic_time () - t0;
    ck_assert_int_ge (took, STALL_USEC);
    ck_assert_int_lt (took, 5 * STALL_USEC);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_output_alone_does_not_hold_it)
{
    gint64 t0, took;

    // The far side prints without end and reads nothing: the stall is counted all the same.
    start_writer ();
    t0 = g_get_monotonic_time ();
    mctest_assert_false (mcterm_pty_send (master, text, TEXT_LEN, STALL_USEC, drain, NULL));
    took = g_get_monotonic_time () - t0;
    ck_assert_int_ge (took, STALL_USEC);
    ck_assert_int_lt (took, 5 * STALL_USEC);
    ck_assert_int_gt (drained, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_stops_when_the_drain_says_gone)
{
    // Output waits on the master, so the drain is called on the first round.
    ck_assert_int_eq (write (slave, "gone", 4), 4);
    mctest_assert_false (mcterm_pty_send (master, text, TEXT_LEN, STALL_USEC, drain_gone, NULL));
    ck_assert_int_eq (master, -1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_nothing_to_send)
{
    mctest_assert_true (mcterm_pty_send (master, "", 0, STALL_USEC, drain, NULL));
    mctest_assert_false (mcterm_pty_send (-1, "x", 1, STALL_USEC, drain, NULL));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_set_timeout (tc_core, 30);

    tcase_add_test (tc_core, test_all_goes_through_when_the_slave_is_read);
    tcase_add_test (tc_core, test_gives_up_when_nobody_reads);
    tcase_add_test (tc_core, test_output_alone_does_not_hold_it);
    tcase_add_test (tc_core, test_stops_when_the_drain_says_gone);
    tcase_add_test (tc_core, test_nothing_to_send);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
