/*
   src/runtime-host - unit tests for the descriptor watch and the terminal suspend services

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

#define TEST_SUITE_NAME "/src/runtime-host/services"

#include "tests/mctest.h"

#include <unistd.h>

#include "src/runtime-host.c"

/* --------------------------------------------------------------------------------------------- */

static int
ready_never (int fd, void *data)
{
    (void) fd;
    (void) data;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
between_never (void *data)
{
    (void) data;

    ck_abort_msg ("the terminal was given away before the screen was set up");
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fd_watch_refuses_what_select_cannot_watch)
{
    const char *reason = NULL;

    mctest_assert_false (runtime_host_fd_watch (-1, ready_never, NULL, &reason));
    ck_assert_str_eq (reason, "bad_fd");

    reason = NULL;
    mctest_assert_false (runtime_host_fd_watch (FD_SETSIZE, ready_never, NULL, &reason));
    ck_assert_str_eq (reason, "bad_fd");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fd_watch_takes_a_descriptor)
{
    const char *reason = NULL;
    int fds[2];

    ck_assert_int_eq (pipe (fds), 0);
    mctest_assert_true (runtime_host_fd_watch (fds[0], ready_never, NULL, &reason));
    ck_assert_ptr_null (reason);
    runtime_host_fd_unwatch (fds[0]);
    close (fds[0]);
    close (fds[1]);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_suspend_refuses_before_the_screen)
{
    const char *reason = NULL;

    ck_assert_ptr_null (top_dlg);
    mctest_assert_false (runtime_host_tty_suspend (between_never, NULL, TRUE, &reason));
    ck_assert_str_eq (reason, "not_ready");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_fd_watch_refuses_what_select_cannot_watch);
    tcase_add_test (tc_core, test_fd_watch_takes_a_descriptor);
    tcase_add_test (tc_core, test_tty_suspend_refuses_before_the_screen);

    return mctest_run_all (tc_core);
}
