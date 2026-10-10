/*
   tests/src/keymap_reload.c -- test keymap reload correctness

   Copyright (C) 2026
   Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "/src/keymap_reload"

#include "tests/mctest.h"

#include "lib/keybind.h"
#include "lib/mcconfig.h"
#include "lib/tty/key.h"
#include "lib/widget.h"

#include "src/keymap.h"

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keymap_load_defaults)
{
    const global_keymap_t *map;

    keymap_load (FALSE); /* load from C defaults only, no files */

    /* LF keypad Enter must work in menus without losing Ctrl-Enter actions
       in the panels or the editor. */
    ck_assert_int_eq (keybind_lookup_keymap_command (menu_map, '\n'), CK_Enter);
    ck_assert_int_eq (keybind_lookup_keymap_command (menu_map, KEY_M_CTRL | '\n'), CK_Enter);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_M_CTRL | '\n'),
                      CK_PutCurrentSelected);
#ifdef USE_INTERNAL_EDIT
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, KEY_M_CTRL | '\n'), CK_Return);
#endif

    map = filemanager_map;
    ck_assert_ptr_ne (map, NULL);

    /* default panel keymap should have CK_Copy bound to F5 */
    {
        long cmd;

        cmd = keybind_lookup_keymap_command (map, KEY_F (5));
        ck_assert_int_eq (cmd, CK_Copy);
    }

    /* default panel keymap should have CK_Move bound to F6 */
    {
        long cmd;

        cmd = keybind_lookup_keymap_command (map, KEY_F (6));
        ck_assert_int_eq (cmd, CK_Move);
    }

    /* Keep both traditional quick-search keys and give quick filter a distinct
       shifted Alt-S binding for switching modes. */
    {
        long cmd;

        cmd = keybind_lookup_keymap_command (panel_map, XCTRL ('s'));
        ck_assert_int_eq (cmd, CK_Search);

        cmd = keybind_lookup_keymap_command (panel_map, ALT ('s'));
        ck_assert_int_eq (cmd, CK_Search);

        cmd = keybind_lookup_keymap_command (panel_map, ALT ('S'));
        ck_assert_int_eq (cmd, CK_QuickFilter);

        ck_assert_uint_eq (keybind_lookup_action_flags (CK_CycleListingFormat),
                           KEYBIND_ACTION_KEEP_PANEL_FILTER);
        ck_assert_uint_eq (keybind_lookup_action_flags (CK_Mark),
                           KEYBIND_ACTION_KEEP_PANEL_FILTER | KEYBIND_ACTION_PANEL_SELECTION);
        ck_assert_uint_eq (keybind_lookup_action_flags (CK_Copy), KEYBIND_ACTION_NONE);
    }

    keymap_free ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keymap_reload_pointer_changes)
{
    const global_keymap_t *old_map;

    keymap_load (FALSE);
    old_map = filemanager_map;
    ck_assert_ptr_ne (old_map, NULL);

    keymap_free ();
    keymap_load (FALSE);

    /* Valid after reload, but may point to different memory; a widget that
       cached old_map would then hold a dangling pointer. */
    ck_assert_ptr_ne (filemanager_map, NULL);

    {
        long cmd;

        cmd = keybind_lookup_keymap_command (filemanager_map, KEY_F (5));
        ck_assert_int_eq (cmd, CK_Copy);
    }

    keymap_free ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keymap_data_pointer_stability)
{
    long cmd;

    keymap_load (FALSE);
    keymap_free ();
    keymap_load (FALSE);

    cmd = keybind_lookup_keymap_command (filemanager_map, KEY_F (5));
    ck_assert_int_eq (cmd, CK_Copy);

    keymap_free ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_widget_keymap_dangling)
{
    const global_keymap_t *widget_keymap;
    long cmd;

    keymap_load (FALSE);

    /* A widget caches the keymap pointer at init time. */
    widget_keymap = filemanager_map;
    ck_assert_ptr_ne (widget_keymap, NULL);

    cmd = keybind_lookup_keymap_command (widget_keymap, KEY_F (5));
    ck_assert_int_eq (cmd, CK_Copy);

    keymap_free ();
    keymap_load (FALSE);

    /* filemanager_map is valid and updated after the reload. */
    cmd = keybind_lookup_keymap_command (filemanager_map, KEY_F (5));
    ck_assert_int_eq (cmd, CK_Copy);

    /* widget_keymap may now point to freed memory; do not dereference it. */
    keymap_free ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keymap_far_mode)
{
    keymap_far_mode = TRUE;
    keymap_load (FALSE);

    /* the keys that Far Manager gives to the panel actions */
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_M_ALT | KEY_F (7)),
                      CK_Find);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_M_ALT | KEY_F (8)),
                      CK_History);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_M_ALT | KEY_F (11)),
                      CK_EditorViewerHistory);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, XCTRL ('l')), CK_PanelInfo);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, XCTRL ('q')),
                      CK_PanelQuickView);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, XCTRL ('t')), CK_PanelTree);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (3)),
                      CK_SortByName);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (4)),
                      CK_SortByExt);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (5)),
                      CK_SortByMTime);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (6)),
                      CK_SortBySize);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (12)), CK_Sort);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_ALT | KEY_F (12)),
                      CK_History);

    /* Ctrl-T went to the tree panel; the marking keys of Far Manager stay */
    ck_assert_int_ne (keybind_lookup_keymap_command (panel_map, XCTRL ('t')), CK_Mark);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_IC), CK_Mark);

    /* what M-Commander has always had is still there */
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_F (5)), CK_Copy);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, ALT ('?')), CK_Find);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, ALT ('h')), CK_History);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, ALT ('H')), CK_History);

    keymap_free ();
    keymap_far_mode = FALSE;
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keymap_far_mode_off_by_default)
{
    ck_assert (!keymap_far_mode);

    keymap_load (FALSE);

    /* none of the keys of Far mode is bound while the mode is off ... */
    ck_assert_int_ne (keybind_lookup_keymap_command (filemanager_map, KEY_M_ALT | KEY_F (7)),
                      CK_Find);
    ck_assert_int_ne (keybind_lookup_keymap_command (filemanager_map, XCTRL ('l')), CK_PanelInfo);
    ck_assert_int_ne (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (3)),
                      CK_SortByName);

    /* ... and Ctrl-T marks a file, as it always did */
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, XCTRL ('t')), CK_Mark);

    keymap_free ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keymap_far_mode_switch)
{
    /* switching the mode on and off is a reload of the keymap, as the options dialog does it */
    keymap_load (FALSE);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, XCTRL ('t')), CK_Mark);

    keymap_far_mode = TRUE;
    keymap_free ();
    keymap_load (FALSE);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (3)),
                      CK_SortByName);
    ck_assert_int_ne (keybind_lookup_keymap_command (panel_map, XCTRL ('t')), CK_Mark);

    keymap_far_mode = FALSE;
    keymap_free ();
    keymap_load (FALSE);
    ck_assert_int_ne (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (3)),
                      CK_SortByName);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, XCTRL ('t')), CK_Mark);

    keymap_free ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keymap_far_mode_editor_viewer)
{
    keymap_far_mode = TRUE;
    keymap_load (FALSE);

#ifdef USE_INTERNAL_EDIT
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, KEY_M_CTRL | KEY_F (7)),
                      CK_Replace);
    ck_assert_int_ne (keybind_lookup_keymap_command (editor_map, KEY_F (4)), CK_Replace);
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, KEY_F (4)), CK_Quit);
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, KEY_F (10)), CK_Quit);
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, KEY_M_ALT | KEY_F (8)), CK_Goto);
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, KEY_M_CTRL | KEY_F (3)),
                      CK_ShowNumbers);
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, KEY_M_ALT | KEY_F (11)),
                      CK_History);
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, XCTRL ('z')), CK_Undo);
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, XCTRL ('u')), CK_Unmark);
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, XCTRL ('a')), CK_MarkAll);
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, KEY_F (7)), CK_Search);
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, KEY_F (2)), CK_Save);
#endif
    ck_assert_int_eq (keybind_lookup_keymap_command (viewer_map, KEY_M_ALT | KEY_F (8)), CK_Goto);
    ck_assert_int_eq (keybind_lookup_keymap_command (viewer_map, KEY_M_ALT | KEY_F (7)),
                      CK_SearchOppositeContinue);
    ck_assert_int_eq (keybind_lookup_keymap_command (viewer_map, KEY_M_ALT | KEY_F (11)),
                      CK_History);
    ck_assert_int_eq (keybind_lookup_keymap_command (viewer_map, KEY_F (5)), CK_Goto);

    keymap_free ();

    /* off again: the editor and the viewer are what they were */
    keymap_far_mode = FALSE;
    keymap_load (FALSE);
#ifdef USE_INTERNAL_EDIT
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, KEY_F (4)), CK_Replace);
    ck_assert_int_eq (keybind_lookup_keymap_command (editor_map, XCTRL ('u')), CK_Undo);
#endif
    ck_assert_int_ne (keybind_lookup_keymap_command (viewer_map, KEY_M_ALT | KEY_F (8)), CK_Goto);
    keymap_free ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keymap_far_mode_listing_modes)
{
    int i;

    keymap_far_mode = TRUE;
    keymap_load (FALSE);

    /* Ctrl-1 .. Ctrl-9 and Ctrl-0 are the digit with the modifier, not the control characters */
    for (i = 1; i <= 10; i++)
        ck_assert_int_eq (
            keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | (i == 10 ? '0' : '0' + i)),
            CK_PanelListingMode1 + i - 1);

    /* the control characters of the same codes keep their meaning */
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, XCTRL ('q')),
                      CK_PanelQuickView);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, XCTRL ('t')), CK_PanelTree);

    /* a key name carries the digit and the modifier, as the keymap files write it */
    ck_assert_int_eq (tty_keyname_to_keycode ("ctrl-1", NULL), KEY_M_CTRL | '1');
    ck_assert_int_eq (tty_keyname_to_keycode ("ctrl-q", NULL), XCTRL ('q'));

    keymap_free ();

    /* off: the keys are not bound */
    keymap_far_mode = FALSE;
    keymap_load (FALSE);
    ck_assert_int_ne (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | '1'),
                      CK_PanelListingMode1);
    keymap_free ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keymap_far_mode_command_line)
{
    keymap_far_mode = TRUE;
    keymap_load (FALSE);

    /* the history of the command line, the deleting of the line and the name of the file */
    ck_assert_int_eq (keybind_lookup_keymap_command (input_map, XCTRL ('e')), CK_HistoryPrev);
    ck_assert_int_eq (keybind_lookup_keymap_command (input_map, XCTRL ('x')), CK_HistoryNext);
    ck_assert_int_eq (keybind_lookup_keymap_command (input_map, XCTRL ('y')), CK_Clear);
    ck_assert_int_eq (keybind_lookup_keymap_command (input_map, KEY_M_CTRL | KEY_BACKSPACE),
                      CK_DeleteToWordBegin);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, XCTRL ('f')),
                      CK_PutCurrentFullSelected);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, XCTRL ('a')), CK_ChangeMode);

    /* the keys they took are given away or kept elsewhere */
    ck_assert_int_eq (keybind_lookup_keymap_command (input_map, KEY_END), CK_End);
    ck_assert_int_eq (keybind_lookup_keymap_command (input_map, ALT ('p')), CK_HistoryPrev);
    ck_assert_int_eq (keybind_lookup_keymap_command (input_map, ALT ('y')), CK_Yank);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, ALT ('x')),
                      CK_ExtendedKeyMap);
    ck_assert_int_ne (keybind_lookup_keymap_command (filemanager_map, XCTRL ('x')),
                      CK_ExtendedKeyMap);
    ck_assert_int_eq (
        keybind_lookup_keymap_command (filemanager_map, KEY_M_CTRL | KEY_M_SHIFT | '\n'),
        CK_PutCurrentFullSelected);

    keymap_free ();

    /* off: the keys of the Emacs are back */
    keymap_far_mode = FALSE;
    keymap_load (FALSE);
    ck_assert_int_eq (keybind_lookup_keymap_command (input_map, XCTRL ('e')), CK_End);
    ck_assert_int_eq (keybind_lookup_keymap_command (input_map, XCTRL ('y')), CK_Yank);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, XCTRL ('x')),
                      CK_ExtendedKeyMap);
    ck_assert_int_ne (keybind_lookup_keymap_command (filemanager_map, XCTRL ('a')), CK_ChangeMode);
    keymap_free ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keymap_far_mode_dialogs_and_workflows)
{
    ck_assert (!check_far_keys);

    keymap_far_mode = TRUE;
    keymap_load (FALSE);

    /* dialogs: Ctrl-Enter is OK, and the check boxes take + and - */
    ck_assert_int_eq (keybind_lookup_keymap_command (dialog_map, KEY_M_CTRL | '\n'), CK_Ok);
    ck_assert_int_eq (keybind_lookup_keymap_command (dialog_map, '\n'), CK_Ok);
    ck_assert (check_far_keys);

    /* Ctrl-\ goes to the root directory and the hotlist moved to Alt-\ */
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, XCTRL ('\\')), CK_CdRoot);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, ALT ('\\')), CK_HotList);
    ck_assert_int_ne (keybind_lookup_keymap_command (filemanager_map, XCTRL ('\\')), CK_HotList);

    /* the sort keys that Far has besides the four of the first part */
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (7)),
                      CK_SortByUnsorted);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (8)),
                      CK_SortByCTime);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (9)),
                      CK_SortByATime);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (11)),
                      CK_IgnoreKey);

    /* Ctrl-G, Alt-F6, Alt-F10, Shift-F9 and Shift-F10 */
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, XCTRL ('g')),
                      CK_ApplyCommand);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_x_map, 'g'), CK_ApplyCommand);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_M_ALT | KEY_F (6)),
                      CK_Link);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_M_ALT | KEY_F (10)),
                      CK_Tree);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_F (21)), CK_SaveSetup);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_F (22)),
                      CK_MenuLastSelected);

    /* the keys of mc that the mode leaves alone */
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_F (19)),
                      CK_MenuLastSelected);

    keymap_free ();

    /* off: all of it is as it was */
    keymap_far_mode = FALSE;
    keymap_load (FALSE);
    ck_assert (!check_far_keys);
    ck_assert_int_ne (keybind_lookup_keymap_command (dialog_map, KEY_M_CTRL | '\n'), CK_Ok);
    ck_assert_int_eq (keybind_lookup_keymap_command (dialog_map, '\n'), CK_Ok);
    ck_assert_int_ne (keybind_lookup_keymap_command (panel_map, XCTRL ('\\')), CK_CdRoot);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, XCTRL ('\\')), CK_HotList);
    ck_assert_int_ne (keybind_lookup_keymap_command (panel_map, KEY_M_CTRL | KEY_F (7)),
                      CK_SortByUnsorted);
    ck_assert_int_ne (keybind_lookup_keymap_command (filemanager_map, XCTRL ('g')),
                      CK_ApplyCommand);
    ck_assert_int_ne (keybind_lookup_keymap_command (filemanager_x_map, 'g'), CK_ApplyCommand);
    ck_assert_int_ne (keybind_lookup_keymap_command (filemanager_map, KEY_M_ALT | KEY_F (10)),
                      CK_Tree);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_F (19)),
                      CK_MenuLastSelected);
    keymap_free ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keymap_far_mode_select_all_default_button)
{
    keymap_far_mode = TRUE;
    keymap_load (FALSE);

    /* Shift-gray plus and minus select and unselect all files; PgDn focuses the default button */
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_SHIFT | KEY_KP_ADD),
                      CK_SelectAll);
    ck_assert_int_eq (keybind_lookup_keymap_command (panel_map, KEY_M_SHIFT | KEY_KP_SUBTRACT),
                      CK_UnselectAll);
    ck_assert_int_eq (keybind_lookup_keymap_command (dialog_map, KEY_NPAGE), CK_DefaultButton);

    /* the plain gray keys keep asking for a pattern */
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_KP_ADD), CK_Select);
    ck_assert_int_eq (keybind_lookup_keymap_command (filemanager_map, KEY_KP_SUBTRACT),
                      CK_Unselect);

    /* Shift-Enter and Ctrl-Shift-Enter put the name of a file in the edit lines of dialogs */
    ck_assert_int_eq (keybind_lookup_keymap_command (input_map, KEY_M_SHIFT | '\n'),
                      CK_PutPanelFile);
    ck_assert_int_eq (keybind_lookup_keymap_command (input_map, KEY_M_CTRL | KEY_M_SHIFT | '\n'),
                      CK_PutOtherPanelFile);

    keymap_free ();

    /* off: none of the three is bound */
    keymap_far_mode = FALSE;
    keymap_load (FALSE);
    ck_assert_int_ne (keybind_lookup_keymap_command (panel_map, KEY_M_SHIFT | KEY_KP_ADD),
                      CK_SelectAll);
    ck_assert_int_ne (keybind_lookup_keymap_command (dialog_map, KEY_NPAGE), CK_DefaultButton);
    ck_assert_int_ne (keybind_lookup_keymap_command (input_map, KEY_M_SHIFT | '\n'),
                      CK_PutPanelFile);
    keymap_free ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_keymap_load_defaults);
    tcase_add_test (tc_core, test_keymap_reload_pointer_changes);
    tcase_add_test (tc_core, test_keymap_data_pointer_stability);
    /* test_keymap_user_override requires mc_global init -- run manually */
    /* tcase_add_test (tc_core, test_keymap_user_override); */
    tcase_add_test (tc_core, test_widget_keymap_dangling);
    tcase_add_test (tc_core, test_keymap_far_mode);
    tcase_add_test (tc_core, test_keymap_far_mode_off_by_default);
    tcase_add_test (tc_core, test_keymap_far_mode_switch);
    tcase_add_test (tc_core, test_keymap_far_mode_editor_viewer);
    tcase_add_test (tc_core, test_keymap_far_mode_listing_modes);
    tcase_add_test (tc_core, test_keymap_far_mode_command_line);
    tcase_add_test (tc_core, test_keymap_far_mode_dialogs_and_workflows);
    tcase_add_test (tc_core, test_keymap_far_mode_select_all_default_button);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
