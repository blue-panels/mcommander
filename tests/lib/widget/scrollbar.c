/*
   lib/widget - tests for the scrollbar

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

#define TEST_SUITE_NAME "/lib/widget"

#include "tests/mctest.h"

#include "lib/widget.h"

/* --------------------------------------------------------------------------------------------- */

/* A client that counts what the bar tells it, and where the bar was then */
static int notified;
static long notified_pos;
static Widget client;

static cb_ret_t
client_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    if (msg == MSG_NOTIFY && sender != NULL)
    {
        notified++;
        notified_pos = scrollbar_get_pos (SCROLLBAR (sender));
        return MSG_HANDLED;
    }
    return widget_default_callback (w, sender, msg, parm, data);
}

/* --------------------------------------------------------------------------------------------- */

/* A vertical bar of 12 cells: arrows at 0 and 11, a track of 10 cells, a thumb of 4 */
static WScrollBar *
new_bar (void)
{
    static const scrollbar_look_t look = { '^', 'v', '|', '#', 4 };
    WRect r = { 0, 0, 1, 1 };
    WScrollBar *b;

    widget_init (&client, &r, client_callback, NULL);
    notified = 0;
    notified_pos = -1;

    b = scrollbar_new (0, 0, 12, SCROLLBAR_VERTICAL);
    scrollbar_set_look (b, &look);
    scrollbar_set_client (b, &client);
    scrollbar_set_range (b, 100, 10, 0);
    return b;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_scrollbar_fits)
{
    WScrollBar *b = new_bar ();

    // a view that holds all of the text leaves nothing to show
    scrollbar_set_range (b, 10, 10, 0);
    mctest_assert_false (scrollbar_needed (b));
    scrollbar_set_range (b, 11, 10, 0);
    mctest_assert_true (scrollbar_needed (b));
    // the position stays within what the view can reach
    scrollbar_set_range (b, 100, 10, 500);
    ck_assert_int_eq (scrollbar_get_pos (b), 90);

    widget_destroy (WIDGET (b));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_scrollbar_steps_and_pages)
{
    WScrollBar *b = new_bar ();

    // the arrow at the end steps one forward, and the client is told
    scrollbar_mouse (b, MSG_MOUSE_DOWN, 11);
    ck_assert_int_eq (scrollbar_get_pos (b), 1);
    ck_assert_int_eq (notified, 1);
    ck_assert_int_eq (notified_pos, 1);

    // the track past the thumb pages: the view less one
    scrollbar_mouse (b, MSG_MOUSE_DOWN, 9);
    ck_assert_int_eq (scrollbar_get_pos (b), 10);

    // the arrow at the start steps back
    scrollbar_mouse (b, MSG_MOUSE_DOWN, 0);
    ck_assert_int_eq (scrollbar_get_pos (b), 9);

    // at the start, the arrow moves nothing and tells nothing
    scrollbar_set_range (b, 100, 10, 0);
    notified = 0;
    scrollbar_mouse (b, MSG_MOUSE_DOWN, 0);
    ck_assert_int_eq (notified, 0);

    widget_destroy (WIDGET (b));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_scrollbar_short_steps)
{
    WScrollBar *b = new_bar ();

    // three cells: no room for a thumb, but there is something to scroll and the arrows step
    widget_set_size (WIDGET (b), 0, 0, 3, 1);
    scrollbar_set_range (b, 20, 3, 0);
    mctest_assert_true (scrollbar_needed (b));

    scrollbar_mouse (b, MSG_MOUSE_DOWN, 2);
    ck_assert_int_eq (scrollbar_get_pos (b), 1);
    ck_assert_int_eq (notified, 1);
    scrollbar_mouse (b, MSG_MOUSE_DOWN, 0);
    ck_assert_int_eq (scrollbar_get_pos (b), 0);

    // the track between the arrows does nothing, the wheel scrolls
    scrollbar_mouse (b, MSG_MOUSE_DOWN, 1);
    ck_assert_int_eq (scrollbar_get_pos (b), 0);
    scrollbar_mouse (b, MSG_MOUSE_SCROLL_DOWN, 1);
    ck_assert_int_eq (scrollbar_get_pos (b), 2);

    widget_destroy (WIDGET (b));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_scrollbar_drags_the_thumb)
{
    WScrollBar *b = new_bar ();

    // the thumb at the start takes cells 1-4; it is held by its second cell
    scrollbar_mouse (b, MSG_MOUSE_DOWN, 2);
    ck_assert_int_eq (scrollbar_get_pos (b), 0);
    ck_assert_int_eq (notified, 0);

    // dragged to the end of the track: the end of the text
    scrollbar_mouse (b, MSG_MOUSE_DRAG, 9);
    ck_assert_int_eq (scrollbar_get_pos (b), 90);

    // halfway: 3 of the 6 cells the thumb moves in
    scrollbar_mouse (b, MSG_MOUSE_DRAG, 5);
    ck_assert_int_eq (scrollbar_get_pos (b), 45);

    // let go, the next drag moves nothing
    scrollbar_mouse (b, MSG_MOUSE_UP, 5);
    notified = 0;
    scrollbar_mouse (b, MSG_MOUSE_DRAG, 9);
    ck_assert_int_eq (notified, 0);
    ck_assert_int_eq (scrollbar_get_pos (b), 45);

    widget_destroy (WIDGET (b));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_scrollbar_fits);
    tcase_add_test (tc_core, test_scrollbar_steps_and_pages);
    tcase_add_test (tc_core, test_scrollbar_short_steps);
    tcase_add_test (tc_core, test_scrollbar_drags_the_thumb);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
