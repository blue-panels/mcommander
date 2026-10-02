/*
   lib/widget - unit tests for a paste taken as one block

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

#define TEST_SUITE_NAME "lib/widget/paste"

#include "tests/mctest.h"

#include <string.h>

#include "lib/widget.h"

/* --------------------------------------------------------------------------------------------- */

static Widget *pasted_to = NULL;
static const GString *pasted_text = NULL;

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
taker_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    if (msg == MSG_PASTE)
    {
        pasted_to = w;
        pasted_text = (const GString *) data;
        return MSG_HANDLED;
    }
    return widget_default_callback (w, sender, msg, parm, data);
}

/* --------------------------------------------------------------------------------------------- */

static Widget *
add_widget (WGroup *g, widget_cb_fn callback)
{
    Widget *w;
    WRect r;

    w = g_new0 (Widget, 1);
    rect_init (&r, 0, 0, 5, 5);
    widget_init (w, &r, callback, NULL);
    w->options |= WOP_SELECTABLE;
    group_add_widget (g, w);
    return w;
}

/* --------------------------------------------------------------------------------------------- */

static void
check_line (const char *text, const char *expected)
{
    char *s = g_strdup (text);
    size_t len;

    len = input_text_to_line (s, strlen (s));
    ck_assert_str_eq (s, expected);
    ck_assert_uint_eq (len, strlen (expected));
    g_free (s);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_text_to_line)
{
    check_line ("ls\ncd /tmp\n", "ls cd /tmp");
    check_line ("a\r\nb\rc\td\n\n", "a b c d");
    check_line ("привет\nмир", "привет мир");
    check_line ("\n\r\n", "");
    check_line ("", "");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_group_passes_paste_to_current)
{
    WGroup *g, *g0;
    Widget *w;
    WRect r;
    GString *text = g_string_new ("one\ntwo");

    g = g_new0 (WGroup, 1);
    rect_init (&r, 0, 0, 20, 20);
    group_init (g, &r, group_default_callback, NULL);

    // no widget to take it
    pasted_to = NULL;
    ck_assert_int_eq (send_message (g, NULL, MSG_PASTE, 0, text), MSG_NOT_HANDLED);

    g0 = g_new0 (WGroup, 1);
    rect_init (&r, 0, 0, 10, 10);
    group_init (g0, &r, group_default_callback, NULL);
    WIDGET (g0)->options |= WOP_SELECTABLE;
    group_add_widget (g, g0);
    (void) add_widget (g0, widget_default_callback);
    w = add_widget (g0, taker_callback);

    // through the inner group to its current widget
    ck_assert_ptr_eq (g0->current->data, w);
    ck_assert_int_eq (send_message (g, NULL, MSG_PASTE, 0, text), MSG_HANDLED);
    ck_assert_ptr_eq (pasted_to, w);
    ck_assert_ptr_eq (pasted_text, text);

    // a current widget that does not take it
    pasted_to = NULL;
    g0->current = g0->widgets;
    ck_assert_int_eq (send_message (g, NULL, MSG_PASTE, 0, text), MSG_NOT_HANDLED);
    ck_assert_ptr_null (pasted_to);

    widget_destroy (WIDGET (g));
    g_string_free (text, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_text_to_line);
    tcase_add_test (tc_core, test_group_passes_paste_to_current);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
