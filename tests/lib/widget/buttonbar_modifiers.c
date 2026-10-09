/*
   lib/widget - unit tests for the key bar of a held modifier

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

#define TEST_SUITE_NAME "lib/widget/buttonbar_modifiers"

#include "tests/mctest.h"

#include "lib/tty/tty.h"

/* the screen size of the screen library is not linked in */
#undef COLS
#undef LINES
#define COLS  test_cols
#define LINES test_lines
static int test_cols = 80;
static int test_lines = 25;

#define tty_kitty_modifiers test_tty_kitty_modifiers
#include "lib/widget/buttonbar.c"
#undef tty_kitty_modifiers

/* --------------------------------------------------------------------------------------------- */

static guint held = 0;
static long action = CK_IgnoreKey;

static const global_keymap_t own_map[] = {
    { KEY_F (5), CK_Copy, "" },
    { KEY_M_CTRL | KEY_F (1), CK_PanelToggleLeft, "" },
    { KEY_M_SHIFT | KEY_F (8), CK_SyntaxOnOff, "" },  // as "shift-f8" is loaded
    { 0, CK_IgnoreKey, "" },
};

static const global_keymap_t panel_map_a[] = {
    { KEY_F (15), CK_CopySingle, "" },  // Shift-F5
    { 0, CK_IgnoreKey, "" },
};

static const global_keymap_t panel_map_b[] = {
    { KEY_F (16), CK_MoveSingle, "" },  // Shift-F6
    { 0, CK_IgnoreKey, "" },
};

static const global_keymap_t *first_map = own_map;
static const global_keymap_t *second_map = panel_map_a;

static const buttonbar_command_label_t labels[] = {
    { CK_CopySingle, N_ ("ButtonBar|Copy") },
    { CK_PanelToggleLeft, N_ ("ButtonBar|Left") },
    { CK_IgnoreKey, NULL },
};

static WButtonBar *bb;
static Widget *receiver;

/* --------------------------------------------------------------------------------------------- */

guint
test_tty_kitty_modifiers (void)
{
    return held;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
receiver_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    if (msg == MSG_ACTION)
    {
        action = parm;
        return MSG_HANDLED;
    }
    return widget_default_callback (w, sender, msg, parm, data);
}

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    WRect r = { 0, 0, 1, 1 };

    held = 0;
    action = CK_IgnoreKey;
    first_map = own_map;
    second_map = panel_map_a;
    bb = buttonbar_new ();
    receiver = g_new0 (Widget, 1);
    widget_init (receiver, &r, receiver_callback, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown (void)
{
    widget_destroy (WIDGET (bb));
    widget_destroy (receiver);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
label (int idx, int mod)
{
    const char *text;

    (void) buttonbar_mod_command (bb, idx - 1, mod, &text);
    return text == NULL ? "" : text;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_no_modifier_labels)
{
    held = TTY_KITTY_MOD_CTRL;
    ck_assert_int_eq (buttonbar_held_modifiers (bb), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_held_modifiers)
{
    buttonbar_set_modifier_labels (bb, &first_map, &second_map, labels, receiver);

    held = 0;
    ck_assert_int_eq (buttonbar_held_modifiers (bb), 0);
    held = TTY_KITTY_MOD_SHIFT | TTY_KITTY_MOD_CTRL;
    ck_assert_int_eq (buttonbar_held_modifiers (bb), KEY_M_SHIFT | KEY_M_CTRL);
    held = TTY_KITTY_MOD_META;
    ck_assert_int_eq (buttonbar_held_modifiers (bb), KEY_M_ALT);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_labels)
{
    buttonbar_set_modifier_labels (bb, &first_map, &second_map, labels, receiver);

    ck_assert_str_eq (label (1, KEY_M_CTRL), "Left");          // own label
    ck_assert_str_eq (label (5, KEY_M_SHIFT), "Copy");         // from the second keymap
    ck_assert_str_eq (label (8, KEY_M_SHIFT), "SyntaxOnOff");  // no label: the command name
    ck_assert_str_eq (label (2, KEY_M_CTRL), "");              // nothing bound
    ck_assert_str_eq (label (5, KEY_M_ALT), "");

    // the keymaps are read through their variables: a reload is seen
    second_map = panel_map_b;
    ck_assert_str_eq (label (5, KEY_M_SHIFT), "");
    ck_assert_str_eq (label (6, KEY_M_SHIFT), "MoveSingle");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_call)
{
    buttonbar_set_modifier_labels (bb, &first_map, &second_map, labels, receiver);

    ck_assert (buttonbar_call (bb, 4, KEY_M_SHIFT));
    ck_assert_int_eq (action, CK_CopySingle);

    action = CK_IgnoreKey;
    ck_assert (!buttonbar_call (bb, 3, KEY_M_SHIFT));
    ck_assert_int_eq (action, CK_IgnoreKey);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_no_modifier_labels);
    tcase_add_test (tc_core, test_held_modifiers);
    tcase_add_test (tc_core, test_labels);
    tcase_add_test (tc_core, test_call);

    return mctest_run_all (tc_core);
}
