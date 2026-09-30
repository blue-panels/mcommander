/*
   lib - an mc that lost its terminal, and a new mc that takes it over

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

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lib/tty/resurrect.h"

/* --------------------------------------------------------------------------------------------- */

static char *
make_dir (void)
{
    char *dir = g_dir_make_tmp ("mc-resurrect-test-XXXXXX", NULL);

    ck_assert_ptr_nonnull (dir);
    return dir;
}

/* --------------------------------------------------------------------------------------------- */

static void
remove_dir (char *dir)
{
    GDir *d = g_dir_open (dir, 0, NULL);
    const char *name;

    while (d != NULL && (name = g_dir_read_name (d)) != NULL)
    {
        char *path = g_build_filename (dir, name, (char *) NULL);

        (void) unlink (path);
        g_free (path);
    }
    if (d != NULL)
        g_dir_close (d);
    (void) rmdir (dir);
    g_free (dir);
}

/* --------------------------------------------------------------------------------------------- */

/* The waiting mc is seen in the list with its title as one line, and only with its own TERM. */

START_TEST (test_resurrect_list)
{
    char *dir;
    resurrect_server_t *srv;
    GPtrArray *list;
    const resurrect_entry_t *entry;

    if (!resurrect_available ())
        return;

    dir = make_dir ();
    srv = resurrect_server_new (dir, "xterm", "a dir\nwith a line break");
    ck_assert_ptr_nonnull (srv);

    list = resurrect_list (dir, "xterm");
    ck_assert_int_eq ((int) list->len, 1);
    entry = (const resurrect_entry_t *) g_ptr_array_index (list, 0);
    ck_assert_int_eq ((int) entry->pid, (int) getpid ());
    ck_assert_str_eq (entry->term, "xterm");
    ck_assert_str_eq (entry->title, "a dir with a line break");
    resurrect_entries_free (list);

    list = resurrect_list (dir, "vt100");
    ck_assert_int_eq ((int) list->len, 0);
    resurrect_entries_free (list);

    resurrect_server_free (srv);

    // what it left is gone with it
    list = resurrect_list (dir, "xterm");
    ck_assert_int_eq ((int) list->len, 0);
    resurrect_entries_free (list);

    remove_dir (dir);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The files of an mc that died without a word are taken away by the next one that looks. */

START_TEST (test_resurrect_dead_server)
{
    char *dir, *info;
    GPtrArray *list;
    pid_t child;
    int status = 0;

    if (!resurrect_available ())
        return;

    dir = make_dir ();

    child = fork ();
    ck_assert_int_ge ((int) child, 0);
    if (child == 0)
    {
        // made, and gone with no cleanup
        _exit (resurrect_server_new (dir, "xterm", "gone") != NULL ? 0 : 1);
    }
    ck_assert_int_eq ((int) waitpid (child, &status, 0), (int) child);
    ck_assert (WIFEXITED (status) && WEXITSTATUS (status) == 0);

    info = g_strdup_printf ("%s/srv-%ld.info", dir, (long) child);
    ck_assert (g_file_test (info, G_FILE_TEST_EXISTS));

    list = resurrect_list (dir, "xterm");
    ck_assert_int_eq ((int) list->len, 0);
    resurrect_entries_free (list);
    ck_assert (!g_file_test (info, G_FILE_TEST_EXISTS));

    g_free (info);
    remove_dir (dir);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A client of another TERM is refused; one of the same TERM gets the server to use its
   descriptors, and the exit code of the server comes back through the shim. */

START_TEST (test_resurrect_hand_over)
{
    char *dir;
    resurrect_server_t *srv;
    GPtrArray *list;
    pid_t child;
    int status = 0;
    int in_pipe[2], out_pipe[2];

    if (!resurrect_available ())
        return;

    dir = make_dir ();
    srv = resurrect_server_new (dir, "xterm", "there");
    ck_assert_ptr_nonnull (srv);

    ck_assert_int_eq (pipe (in_pipe), 0);
    ck_assert_int_eq (pipe (out_pipe), 0);

    // listing connects and leaves: a probe, which the server has to let go
    list = resurrect_list (dir, "xterm");
    ck_assert_int_eq ((int) list->len, 1);

    child = fork ();
    ck_assert_int_ge ((int) child, 0);
    if (child == 0)
    {
        const resurrect_entry_t *entry = (const resurrect_entry_t *) g_ptr_array_index (list, 0);
        int notify[2];
        pid_t server = 0;
        char buf[8];
        int code;

        if (pipe (notify) != 0)
            _exit (10);
        if (write (in_pipe[1], "ping", 4) != 4)
            _exit (11);

        if (resurrect_visit (entry, "vt100", in_pipe[0], out_pipe[1], notify[1], &server))
            _exit (12);
        if (!resurrect_visit (entry, "xterm", in_pipe[0], out_pipe[1], notify[1], &server))
            _exit (13);
        if (server != getppid ())
            _exit (14);

        close (notify[1]);
        if (read (out_pipe[0], buf, 4) != 4 || memcmp (buf, "pong", 4) != 0)
            _exit (15);

        code = resurrect_shim (notify[0], server);
        _exit (code == 7 ? 0 : 16);
    }

    {
        int in_fd = -1, out_fd = -1, notify_fd = -1;
        char buf[8];

        ck_assert (resurrect_server_wait (srv, &in_fd, &out_fd, &notify_fd));

        // the descriptors of the client are ours now
        ck_assert_int_eq ((int) read (in_fd, buf, 4), 4);
        ck_assert_int_eq (memcmp (buf, "ping", 4), 0);
        ck_assert_int_eq ((int) write (out_fd, "pong", 4), 4);
        resurrect_send_exit_code (notify_fd, 7);
        close (in_fd);
        close (out_fd);
    }

    ck_assert_int_eq ((int) waitpid (child, &status, 0), (int) child);
    ck_assert (WIFEXITED (status));
    ck_assert_int_eq (WEXITSTATUS (status), 0);

    resurrect_entries_free (list);
    resurrect_server_free (srv);
    remove_dir (dir);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A shim whose server is gone without an exit code fails. */

START_TEST (test_resurrect_shim_server_gone)
{
    int fds[2];

    ck_assert_int_eq (pipe (fds), 0);
    close (fds[1]);
    ck_assert_int_eq (resurrect_shim (fds[0], 0), EXIT_FAILURE);
    close (fds[0]);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The directory of the files belongs to the user and is closed to the others. */

START_TEST (test_resurrect_dir)
{
    char *base, *dir, *expected;

    if (!resurrect_available ())
        return;

    base = make_dir ();
    expected = g_build_filename (base, "mc-resurrect", (char *) NULL);
    g_setenv ("XDG_RUNTIME_DIR", base, TRUE);

    dir = resurrect_dir ();
    ck_assert_ptr_nonnull (dir);
    ck_assert_str_eq (dir, expected);
    g_free (dir);

    // somebody else can get in: not a place for sockets
    ck_assert_int_eq (chmod (expected, 0755), 0);
    ck_assert_ptr_null (resurrect_dir ());

    (void) rmdir (expected);
    g_free (expected);
    remove_dir (base);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_resurrect_list);
    tcase_add_test (tc_core, test_resurrect_dead_server);
    tcase_add_test (tc_core, test_resurrect_hand_over);
    tcase_add_test (tc_core, test_resurrect_shim_server_gone);
    tcase_add_test (tc_core, test_resurrect_dir);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
