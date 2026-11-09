/*
   lib/widget - tests of the subscriptions of WInput to the events of its dialog

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

#define TEST_SUITE_NAME "/lib/widget/input_lifecycle"

#include "tests/mctest.h"

#include "lib/event.h"
#include "lib/event/internal.h"
#include "lib/fileloc.h"
#include "lib/mcconfig.h"
#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "src/vfs/local/local.c"

/* --------------------------------------------------------------------------------------------- */

static char *tmp_dir;

/* --------------------------------------------------------------------------------------------- */

static guint
subscribers (const WDialog *h, const char *event)
{
    GTree *group;
    GPtrArray *callbacks;

    group = mc_event_get_event_group_by_name (h->event_group, FALSE, NULL);
    ck_assert_ptr_nonnull (group);
    callbacks = mc_event_get_event_by_name (group, event, FALSE, NULL);
    ck_assert_ptr_nonnull (callbacks);
    return callbacks->len;
}

static WDialog *
dialog_new (void)
{
    return dlg_create (FALSE, 0, 0, 10, 40, WPOS_KEEP_DEFAULT, FALSE, NULL, NULL, NULL, NULL,
                       "input");
}

/* @Before */
static void
setup (void)
{
    GError *error = NULL;

    tmp_dir = g_dir_make_tmp ("mc-input-XXXXXX", NULL);
    ck_assert_ptr_nonnull (tmp_dir);
    // a dialog saves the histories where the configuration is
    g_setenv ("HOME", tmp_dir, TRUE);
    g_setenv ("XDG_CONFIG_HOME", tmp_dir, TRUE);
    g_setenv ("XDG_DATA_HOME", tmp_dir, TRUE);
    g_setenv ("XDG_CACHE_HOME", tmp_dir, TRUE);
    str_init_strings (NULL);
    mc_config_init_config_paths (&error);
    ck_assert_ptr_null (error);
    vfs_init ();
    vfs_init_localfs ();
    ck_assert (mc_event_init (&error));
}

/* @After */
static void
teardown (void)
{
    GError *error = NULL;

    char *cmd;

    ck_assert (mc_event_deinit (&error));
    vfs_shut ();
    mc_config_deinit_config_paths ();
    str_uninit_strings ();
    cmd = g_strdup_printf ("rm -rf '%s'", tmp_dir);
    ck_assert_int_eq (system (cmd), 0);
    g_free (cmd);
    g_clear_pointer (&tmp_dir, g_free);
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_destroyed_without_a_dialog)
{
    WInput *in;

    in = input_new (1, 1, NULL, 20, "text", NULL, INPUT_COMPLETE_NONE);
    ck_assert_ptr_null (WIDGET (in)->owner);
    widget_destroy (WIDGET (in));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_destroyed_in_a_living_dialog_unsubscribes)
{
    WDialog *h = dialog_new ();
    WInput *in, *other;
    WGroup *g = GROUP (h);

    in = input_new (1, 1, NULL, 20, "text", NULL, INPUT_COMPLETE_NONE);
    other = input_new (2, 1, NULL, 20, "other", NULL, INPUT_COMPLETE_NONE);
    group_add_widget (g, in);
    group_add_widget (g, other);
    send_message (in, NULL, MSG_INIT, 0, NULL);
    send_message (other, NULL, MSG_INIT, 0, NULL);
    ck_assert_uint_eq (subscribers (h, MCEVENT_HISTORY_LOAD), 2);
    ck_assert_uint_eq (subscribers (h, MCEVENT_HISTORY_SAVE), 2);

    // taken out of the list only, so that it still has its owner when destroyed
    g->widgets = g_list_remove (g->widgets, in);
    g->current = g_list_find (g->widgets, other);
    widget_destroy (WIDGET (in));
    ck_assert_uint_eq (subscribers (h, MCEVENT_HISTORY_LOAD), 1);
    ck_assert_uint_eq (subscribers (h, MCEVENT_HISTORY_SAVE), 1);

    widget_destroy (WIDGET (h));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_destroyed_with_its_dialog)
{
    WDialog *h = dialog_new ();
    WInput *in;
    char *group = g_strdup (h->event_group);

    in = input_new (1, 1, NULL, 20, "text", NULL, INPUT_COMPLETE_NONE);
    group_add_widget (GROUP (h), in);
    send_message (in, NULL, MSG_INIT, 0, NULL);
    ck_assert (mc_event_present (group, MCEVENT_HISTORY_LOAD));

    widget_destroy (WIDGET (h));
    ck_assert (!mc_event_present (group, MCEVENT_HISTORY_LOAD));
    g_free (group);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_destroyed_without_a_dialog);
    tcase_add_test (tc_core, test_destroyed_in_a_living_dialog_unsubscribes);
    tcase_add_test (tc_core, test_destroyed_with_its_dialog);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
