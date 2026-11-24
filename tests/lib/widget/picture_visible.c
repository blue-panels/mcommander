/*
   lib/widget - when a picture on the screen can be seen

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

#define TEST_SUITE_NAME "lib/widget/picture"

#include "tests/mctest.h"

#include "lib/widget.h"

/* --------------------------------------------------------------------------------------------- */

static WGroup *dlg;    // the dialog: 0,0 24x80
static Widget *owner;  // draws the picture: 1,0 20x80
static Widget *bar;    // below the owner: 22,0 1x80
static WRect cells;    // the picture: 5,10 4x20

/* --------------------------------------------------------------------------------------------- */

static Widget *
widget_new (WGroup *g, int y, int x, int lines, int cols)
{
    Widget *w;
    WRect r;

    w = g_new0 (Widget, 1);
    rect_init (&r, y, x, lines, cols);
    widget_init (w, &r, widget_default_callback, NULL);
    group_add_widget (g, w);

    return w;
}

/* --------------------------------------------------------------------------------------------- */

static WGroup *
group_new (int y, int x, int lines, int cols)
{
    WGroup *g;
    WRect r;

    g = g_new0 (WGroup, 1);
    rect_init (&r, y, x, lines, cols);
    group_init (g, &r, group_default_callback, NULL);

    return g;
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    dlg = group_new (0, 0, 24, 80);
    // as dlg_create() does it
    WIDGET (dlg)->owner = dlg;
    owner = widget_new (dlg, 1, 0, 20, 80);
    bar = widget_new (dlg, 22, 0, 1, 80);
    rect_init (&cells, 5, 10, 4, 20);
    top_dlg = g_list_prepend (NULL, dlg);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    g_clear_list (&top_dlg, NULL);
    widget_destroy (WIDGET (dlg));
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_visible) { ck_assert (widget_picture_visible (owner, &cells)); }
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_owner_hidden)
{
    widget_set_state (owner, WST_VISIBLE, FALSE);
    ck_assert (!widget_picture_visible (owner, &cells));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_group_of_owner_hidden)
{
    WGroup *g;
    Widget *inner;

    g = group_new (1, 0, 20, 80);
    group_add_widget (dlg, g);
    inner = widget_new (g, 1, 0, 20, 80);
    ck_assert (widget_picture_visible (inner, &cells));

    widget_set_state (WIDGET (g), WST_VISIBLE, FALSE);
    ck_assert (!widget_picture_visible (inner, &cells));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_dialog_suspended)
{
    WIDGET (dlg)->state |= WST_SUSPENDED;
    ck_assert (!widget_picture_visible (owner, &cells));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_dialog_above)
{
    WGroup *above;

    // a small dialog that does not even touch the picture hides it too
    above = group_new (20, 60, 3, 10);
    top_dlg = g_list_prepend (top_dlg, above);
    ck_assert (!widget_picture_visible (owner, &cells));

    top_dlg = g_list_remove (top_dlg, above);
    ck_assert (widget_picture_visible (owner, &cells));
    widget_destroy (WIDGET (above));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_later_sibling_over_it)
{
    Widget *popup;

    // a popped up menu is as high as the screen
    popup = widget_new (dlg, 0, 0, 24, 80);
    ck_assert (!widget_picture_visible (owner, &cells));

    widget_set_state (popup, WST_VISIBLE, FALSE);
    ck_assert (widget_picture_visible (owner, &cells));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_later_sibling_beside_it)
{
    (void) widget_new (dlg, 5, 30, 4, 10);
    ck_assert (widget_picture_visible (owner, &cells));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_earlier_sibling_under_it)
{
    WGroup *g;
    Widget *under, *over;

    // drawn before the widget with the picture: that widget is over it
    g = group_new (1, 0, 20, 80);
    group_add_widget (dlg, g);
    under = widget_new (g, 1, 0, 20, 80);
    over = widget_new (g, 1, 0, 20, 80);
    ck_assert (widget_picture_visible (over, &cells));
    ck_assert (!widget_picture_visible (under, &cells));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_not_inside_owner)
{
    WRect r;

    rect_init (&r, 18, 10, 4, 20);
    ck_assert (!widget_picture_visible (owner, &r));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_placements_go_with_owner)
{
    static const char sixel[] = "\033Pq#0~\033\\";
    mc_picture_t *pic;

    pic = mc_picture_new_sixel (sixel, sizeof (sixel) - 1, 1, 6);
    widget_picture_set (owner, 0, pic, &cells);
    widget_picture_set (owner, 1, pic, &cells);
    widget_picture_set (bar, 0, pic, &cells);

    // the compositor holds a reference for every placement
    mc_picture_unref (pic);
    widget_picture_trim (owner, 1);
    widget_picture_clear (bar, 0);
    group_remove_widget (owner);
    widget_destroy (owner);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_visible);
    tcase_add_test (tc_core, test_owner_hidden);
    tcase_add_test (tc_core, test_group_of_owner_hidden);
    tcase_add_test (tc_core, test_dialog_suspended);
    tcase_add_test (tc_core, test_dialog_above);
    tcase_add_test (tc_core, test_later_sibling_over_it);
    tcase_add_test (tc_core, test_later_sibling_beside_it);
    tcase_add_test (tc_core, test_earlier_sibling_under_it);
    tcase_add_test (tc_core, test_not_inside_owner);
    tcase_add_test (tc_core, test_placements_go_with_owner);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
