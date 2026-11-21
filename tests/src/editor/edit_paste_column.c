/*
   tests/src/editor - unit tests for a column block that comes back from the terminal

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

#define TEST_SUITE_NAME "/src/editor"

#include "tests/mctest.h"

#include <unistd.h>

#include "lib/charsets.h"
#include "lib/fileloc.h"
#include "lib/mcconfig.h"
#include "src/vfs/local/local.c"
#include "src/selcodepage.h"
#include "src/clipboard.h"

#include "src/editor/edit-impl.h"
#include "src/editor/editwidget.h"

/* A dialog, not a bare group: the editor reaches its owner as one */
static WDialog owner;
static WEdit *test_edit;
static char *clip;

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    WRect r;
    char *dir;

    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    edit_options.filesize_threshold = (char *) "64M";
    edit_options.group_undo = FALSE;
    edit_options.persistent_selections = FALSE;
    edit_options.cursor_beyond_eol = FALSE;

    rect_init (&r, 0, 0, 24, 80);
    test_edit = edit_init (NULL, &r, NULL);
    memset (&owner, 0, sizeof (owner));
    group_add_widget (&owner.group, WIDGET (test_edit));

    mc_global.source_codepage = 0;
    mc_global.display_codepage = 0;
    cp_source = "ASCII";
    cp_display = "ASCII";
    do_set_codepage (0);
    edit_set_codeset (test_edit);

    clip = mc_config_get_full_path (EDIT_HOME_CLIP_FILE);
    dir = g_path_get_dirname (clip);
    g_mkdir_with_parents (dir, 0700);
    g_free (dir);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    clipboard_info_drop (clip);
    unlink (clip);
    g_free (clip);

    edit_clean (test_edit);
    group_remove_widget (test_edit);
    g_free (test_edit);

    free_codepages_list ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* the clipfile of a column block as the editor leaves it */
static void
put_column (const char *data)
{
    char *sum;

    g_file_set_contents (clip, data, -1, NULL);
    sum = g_compute_checksum_for_string (CLIP_DIGEST_TYPE, data, -1);
    clipboard_info_write (clip, sum, TRUE, "ASCII");
    g_free (sum);
}

/* --------------------------------------------------------------------------------------------- */

static void
load_text (const char *text)
{
    for (; *text != '\0'; text++)
        edit_insert (test_edit, *text);
    edit_cursor_move (test_edit, -test_edit->buffer.curs1);
}

/* --------------------------------------------------------------------------------------------- */

static void
paste (const char *text)
{
    GString *s = g_string_new (text);

    edit_paste_text (test_edit, s);
    g_string_free (s, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
assert_text (const char *expected)
{
    GString *actual = g_string_new ("");
    off_t i;

    for (i = 0; i < test_edit->buffer.size; i++)
        g_string_append_c (actual, (gchar) edit_buffer_get_byte (&test_edit->buffer, i));

    mctest_assert_str_eq (actual->str, expected);
    g_string_free (actual, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_column_goes_in_as_column)
{
    load_text ("1234\n5678\n");
    put_column ("ab\ncd\n");
    paste ("ab\rcd");
    assert_text ("ab1234\ncd5678\n");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* the paste is a new change: the undone typing is gone for Redo */
START_TEST (test_column_paste_drops_redo)
{
    load_text ("1234\n5678\n");
    edit_execute_key_command (test_edit, CK_InsertChar, 'x');
    edit_execute_key_command (test_edit, CK_InsertChar, 'y');
    edit_execute_key_command (test_edit, CK_Undo, -1);
    edit_execute_key_command (test_edit, CK_Undo, -1);
    assert_text ("1234\n5678\n");

    put_column ("ab\ncd\n");
    paste ("ab\ncd");
    assert_text ("ab1234\ncd5678\n");

    edit_execute_key_command (test_edit, CK_Redo, -1);
    edit_execute_key_command (test_edit, CK_Redo, -1);
    assert_text ("ab1234\ncd5678\n");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* a Shift-arrow selection ends with the paste: the next keys do not mark or delete anything */
START_TEST (test_column_paste_ends_shift_selection)
{
    load_text ("1234\n5678\n");
    edit_execute_key_command (test_edit, CK_MarkRight, -1);
    edit_execute_key_command (test_edit, CK_MarkRight, -1);
    ck_assert_int_ne (test_edit->highlight, 0);

    put_column ("ab\ncd\n");
    paste ("ab\ncd");
    ck_assert_int_eq (test_edit->highlight, 0);
    assert_text ("ab34\ncd5678\n");

    edit_execute_key_command (test_edit, CK_Right, -1);
    edit_execute_key_command (test_edit, CK_InsertChar, 'z');
    assert_text ("azb34\ncd5678\n");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* a line break pasted while the clipfile holds a column of blanks is a line break */
START_TEST (test_blank_paste_is_typed)
{
    load_text ("1234\n");
    put_column ("  \n  \n");
    paste ("\n");
    assert_text ("\n1234\n");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;
    char *home;

    // the clipfile of the tests is in a directory of their own, not in that of the user
    home = g_dir_make_tmp ("mc-test-paste-column-XXXXXX", NULL);
    g_setenv ("HOME", home, TRUE);
    g_setenv ("XDG_CONFIG_HOME", home, TRUE);
    g_setenv ("XDG_CACHE_HOME", home, TRUE);
    g_setenv ("XDG_DATA_HOME", home, TRUE);

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_column_goes_in_as_column);
    tcase_add_test (tc_core, test_column_paste_drops_redo);
    tcase_add_test (tc_core, test_column_paste_ends_shift_selection);
    tcase_add_test (tc_core, test_blank_paste_is_typed);

    return mctest_run_all (tc_core);
}
