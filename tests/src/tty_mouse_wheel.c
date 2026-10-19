/*
   src - unit tests for the mouse wheel in the xterm mouse decoder

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

#define TEST_SUITE_NAME "/src/tty_mouse_wheel"

#include "tests/mctest.h"

/* Run the real decoder on bytes given by the test. */
#define tty_lowlevel_getch test_tty_lowlevel_getch
#include "lib/tty/key.c"
#undef tty_lowlevel_getch

static const char *test_input;

int
test_tty_lowlevel_getch (void)
{
    return *test_input != '\0' ? (unsigned char) *test_input++ : -1;
}

/* --------------------------------------------------------------------------------------------- */

/* One SGR report, without the "ESC [ <" the caller has read */
static Gpm_Event
report (const char *seq)
{
    Gpm_Event ev;

    memset (&ev, 0, sizeof (ev));
    test_input = seq;
    xmouse_get_event (&ev, TRUE);
    return ev;
}

/* --------------------------------------------------------------------------------------------- */

/* The decoder keeps its state between reports: each case starts with none */
static void
setup (void)
{
    double_click_speed = 200;
    xmouse_reset ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_wheel_is_no_press)
{
    Gpm_Event ev;

    ev = report ("64;5;5M");
    ck_assert_int_eq (ev.type & (GPM_DOWN | GPM_UP), GPM_DOWN);
    ck_assert_int_eq (ev.buttons, GPM_B_UP);

    // a release after it ends no button: it is dropped
    ev = report ("0;5;5m");
    ck_assert_int_eq (ev.type, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_release_after_wheel_ends_the_button)
{
    Gpm_Event ev;

    ev = report ("0;5;5M");
    ck_assert_int_eq (ev.buttons, GPM_B_LEFT);

    // the wheel turned while the button is held
    ev = report ("65;5;6M");
    ck_assert_int_eq (ev.buttons, GPM_B_DOWN);

    // the release is the button's
    ev = report ("0;5;6m");
    ck_assert ((ev.type & GPM_UP) != 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_click_after_wheel_is_single)
{
    Gpm_Event ev;

    // the click comes within the time of a double click after the wheel
    report ("64;5;5M");

    ev = report ("0;5;5M");
    ck_assert ((ev.type & GPM_DOWN) != 0);
    ck_assert ((ev.type & GPM_SINGLE) != 0);
    report ("0;5;5m");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_wheel_ends_a_series_of_clicks)
{
    Gpm_Event ev;

    report ("0;5;5M");
    report ("0;5;5m");
    // the wheel between two clicks, with the release some terminals send after it
    report ("65;5;5M");
    report ("0;5;5m");

    ev = report ("0;5;5M");
    ck_assert ((ev.type & GPM_SINGLE) != 0);
    report ("0;5;5m");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, NULL);

    tcase_add_test (tc_core, test_wheel_is_no_press);
    tcase_add_test (tc_core, test_release_after_wheel_ends_the_button);
    tcase_add_test (tc_core, test_click_after_wheel_is_single);
    tcase_add_test (tc_core, test_wheel_ends_a_series_of_clicks);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
