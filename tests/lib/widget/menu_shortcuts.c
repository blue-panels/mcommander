/*
   lib/widget - unit tests for the shortcuts of the menu entries

   Copyright (C) 2026
   Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "lib/widget/menu_shortcuts"

#include "tests/mctest.h"

#include "lib/tty/tty.h"

/* the screen size of the screen library is not linked in */
#undef COLS
#define COLS test_cols
static int test_cols = 80;

#include "lib/widget/menu.c"

/* --------------------------------------------------------------------------------------------- */

/* The key bound to the command of an entry now, NULL for none */
static const char *bound = NULL;

static char *
test_get_shortcut (long command)
{
    (void) command;
    return g_strdup (bound);
}

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    str_init_strings ("UTF-8");
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown (void)
{
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_update_shortcuts)
{
    WRect r = { 0, 0, 25, 80 };
    WDialog *d;
    WMenuBar *menubar;
    menu_entry_t *from_keymap, *own;
    menu_t *menu;
    GList *entries = NULL;

    d = g_new0 (WDialog, 1);
    group_init (GROUP (d), &r, NULL, NULL);
    d->get_shortcut = test_get_shortcut;
    menubar = menubar_new (NULL);
    group_add_widget (GROUP (d), menubar);

    from_keymap = menu_entry_new ("&Compare directories", CK_CompareDirs);
    own = menu_entry_new ("&Plugin", CK_IgnoreKey);
    menu_entry_set_shortcut (own, "F11");
    entries = g_list_append (entries, from_keymap);
    entries = g_list_append (entries, own);
    menu = menu_new ("&Command", entries, NULL);

    bound = "Alt-x d";
    menubar_add_menu (menubar, menu);
    ck_assert_str_eq (from_keymap->shortcut, "Alt-x d");
    ck_assert_str_eq (own->shortcut, "F11");

    /* the key went to another thing: the entry shows none, the width follows */
    bound = NULL;
    menubar_update_shortcuts (menubar);
    ck_assert_ptr_null (from_keymap->shortcut);
    ck_assert_str_eq (own->shortcut, "F11");
    ck_assert_uint_eq (menu->max_entry_len, menu->max_hotkey_len + 2 + 3);

    /* a key bound later shows up */
    bound = "Ctrl-x d";
    menubar_update_shortcuts (menubar);
    ck_assert_str_eq (from_keymap->shortcut, "Ctrl-x d");
    ck_assert_str_eq (own->shortcut, "F11");
    ck_assert_uint_eq (menu->max_entry_len, menu->max_hotkey_len + 2 + 8);

    widget_destroy (WIDGET (d));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_update_shortcuts_without_owner)
{
    WMenuBar *menubar;

    /* not in a dialog yet: there is no keymap to take the shortcuts from */
    menubar = menubar_new (NULL);
    menubar_update_shortcuts (menubar);
    widget_destroy (WIDGET (menubar));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_update_shortcuts);
    tcase_add_test (tc_core, test_update_shortcuts_without_owner);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
