/*
   lib - the cache of what is known about panel plugin modules

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

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include "lib/panel_plugin_cache.h"

/* --------------------------------------------------------------------------------------------- */

static char *test_root = NULL;
static char *test_so = NULL;
static char *test_cache_file = NULL;

static int stub_offers = 0;
static int *stub_op_offers = NULL;

/* --------------------------------------------------------------------------------------------- */

static gboolean
dummy_may_open_name (const char *name)
{
    (void) name;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void *
dummy_open_input_stream (mc_panel_host_t *host, const char *display_name,
                         mc_pp_input_stream_t *stream)
{
    (void) host;
    (void) display_name;
    (void) stream;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
dummy_configure (void)
{
}

/* --------------------------------------------------------------------------------------------- */

static const mc_pp_file_operation_t described_operations[] = {
    {
        .name = "open",
        .kind = MC_PP_FILE_OPERATION_OPEN,
        .may_open_name = dummy_may_open_name,
        .open_input_stream = dummy_open_input_stream,
    },
};

static const mc_pp_action_t described_actions[] = {
    { "Do it", NULL },
};

static const mc_pp_cmd_menu_entry_t described_menu[] = {
    { "&Do it", 0, "S-F1", 1234, MC_PP_MENU_PANEL },
};

static const mc_panel_plugin_t described_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "fixture",
    .display_name = "Fixture",
    .prefix = "fixture:",
    .flags = MC_PPF_NAVIGATE,
    .default_sort_id = "mtime",
    .default_sort_reverse = TRUE,
    .file_operations = described_operations,
    .file_operation_count = 1,
    .actions = described_actions,
    .action_count = 1,
    .cmd_menu_entries = described_menu,
    .cmd_menu_entry_count = 1,
    .configure = dummy_configure,
};

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    test_root = g_dir_make_tmp ("mc-plugin-cache-XXXXXX", NULL);
    ck_assert_ptr_nonnull (test_root);
    test_so = g_build_filename (test_root, "mc-panel-fixture.so", (char *) NULL);
    test_cache_file = g_build_filename (test_root, "cache", "panel-plugins.cache", (char *) NULL);
    ck_assert (g_file_set_contents (test_so, "x", 1, NULL));
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown (void)
{
    char *argv[] = { (char *) "rm", (char *) "-rf", test_root, NULL };

    g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
    g_free (stub_op_offers);
    stub_op_offers = NULL;
    g_free (test_cache_file);
    g_free (test_so);
    g_free (test_root);
}

/* --------------------------------------------------------------------------------------------- */

static void
describe_module (const mc_panel_plugin_t *description)
{
    mc_pp_cache_t *cache;
    struct stat st;

    ck_assert_int_eq (stat (test_so, &st), 0);
    cache = mc_pp_cache_open (test_cache_file);
    mc_pp_cache_store (cache, test_so, &st, description);
    mc_pp_cache_close (cache);
}

/* --------------------------------------------------------------------------------------------- */

static mc_panel_plugin_t *
stub_of_module (void)
{
    mc_pp_cache_t *cache;
    mc_panel_plugin_t *stub;
    struct stat st;

    g_free (stub_op_offers);
    ck_assert_int_eq (stat (test_so, &st), 0);
    cache = mc_pp_cache_open (test_cache_file);
    stub = mc_pp_cache_make_stub (cache, test_so, &st, &stub_offers, &stub_op_offers, NULL);
    mc_pp_cache_close (cache);
    return stub;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_stub_has_what_was_stored)
{
    mc_panel_plugin_t *stub;

    describe_module (&described_plugin);
    stub = stub_of_module ();

    ck_assert_ptr_nonnull (stub);
    mctest_assert_str_eq (stub->name, "fixture");
    mctest_assert_str_eq (stub->display_name, "Fixture");
    mctest_assert_str_eq (stub->prefix, "fixture:");
    ck_assert_ptr_null (stub->proto);
    ck_assert_int_eq (stub->flags, MC_PPF_NAVIGATE);
    mctest_assert_str_eq (stub->default_sort_id, "mtime");
    ck_assert (stub->default_sort_reverse);

    ck_assert_int_eq (stub->file_operation_count, 1);
    mctest_assert_str_eq (stub->file_operations[0].name, "open");
    ck_assert_int_eq (stub->file_operations[0].kind, MC_PP_FILE_OPERATION_OPEN);
    ck_assert_int_eq (stub_op_offers[0],
                      MC_PP_OP_OFFERS_MAY_OPEN_NAME | MC_PP_OP_OFFERS_OPEN_INPUT_STREAM);

    ck_assert_int_eq (stub->action_count, 1);
    mctest_assert_str_eq (stub->actions[0].label, "Do it");

    ck_assert_int_eq (stub->cmd_menu_entry_count, 1);
    mctest_assert_str_eq (stub->cmd_menu_entries[0].label, "&Do it");
    mctest_assert_str_eq (stub->cmd_menu_entries[0].shortcut, "S-F1");
    mctest_assert_str_eq (stub->cmd_menu_entries[0].menu_name, MC_PP_MENU_PANEL);
    ck_assert_int_eq (stub->cmd_menu_entries[0].key, 1234);
    ck_assert_int_eq (stub->cmd_menu_entries[0].action_index, 0);

    ck_assert_int_eq (stub_offers, MC_PP_OFFERS_CONFIGURE);

    ck_assert_ptr_null (stub->open);
    ck_assert_ptr_null (stub->configure);
    ck_assert_ptr_null (stub->file_operations[0].may_open_name);
    ck_assert_ptr_null (stub->file_operations[0].open_input_stream);

    mc_pp_cache_free_stub (stub);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_changed_file_has_no_stub)
{
    describe_module (&described_plugin);

    g_file_set_contents (test_so, "longer", 6, NULL);

    ck_assert_ptr_null (stub_of_module ());
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_changed_mtime_has_no_stub)
{
    mc_pp_cache_t *cache;
    struct stat st;
    mc_panel_plugin_t *stub;

    describe_module (&described_plugin);

    ck_assert_int_eq (stat (test_so, &st), 0);
    st.st_mtime += 10;
    cache = mc_pp_cache_open (test_cache_file);
    stub = mc_pp_cache_make_stub (cache, test_so, &st, &stub_offers, &stub_op_offers, NULL);
    mc_pp_cache_close (cache);

    ck_assert_ptr_null (stub);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* what a package manager does: the same size, and the mtime of the file in the package */
START_TEST (test_replaced_file_with_old_mtime_has_no_stub)
{
    struct stat st;
    struct timespec times[2];

    describe_module (&described_plugin);
    ck_assert_int_eq (stat (test_so, &st), 0);

    g_file_set_contents (test_so, "y", 1, NULL);
    times[0].tv_sec = st.st_atime;
    times[0].tv_nsec = 0;
    times[1].tv_sec = st.st_mtime;
#ifdef HAVE_STRUCT_STAT_ST_MTIM
    times[1].tv_nsec = st.st_mtim.tv_nsec;
#else
    times[1].tv_nsec = 0;
#endif
    ck_assert_int_eq (utimensat (AT_FDCWD, test_so, times, 0), 0);

    ck_assert_ptr_null (stub_of_module ());
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_other_api_has_no_stub)
{
    mc_panel_plugin_t description = described_plugin;

    description.api_version = MC_PANEL_PLUGIN_API_VERSION - 1;
    describe_module (&description);

    ck_assert_ptr_null (stub_of_module ());
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_other_program_has_no_stub)
{
    GKeyFile *kf;

    describe_module (&described_plugin);

    kf = g_key_file_new ();
    ck_assert (g_key_file_load_from_file (kf, test_cache_file, G_KEY_FILE_NONE, NULL));
    g_key_file_set_string (kf, "Cache", "program", "v0.0-other");
    ck_assert (g_key_file_save_to_file (kf, test_cache_file, NULL));
    g_key_file_free (kf);

    ck_assert_ptr_null (stub_of_module ());
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_damaged_cache_is_empty)
{
    char *dir;

    dir = g_path_get_dirname (test_cache_file);
    g_mkdir_with_parents (dir, 0700);
    g_free (dir);
    g_file_set_contents (test_cache_file, "[Cache\nversion", -1, NULL);

    ck_assert_ptr_null (stub_of_module ());
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_unused_entry_is_dropped)
{
    mc_pp_cache_t *cache;
    struct stat st;
    char *other;
    int offers, *op_offers;

    ck_assert_int_eq (stat (test_so, &st), 0);
    other = g_strconcat (test_so, ".other", (char *) NULL);

    cache = mc_pp_cache_open (test_cache_file);
    mc_pp_cache_store (cache, test_so, &st, &described_plugin);
    mc_pp_cache_store (cache, other, &st, &described_plugin);
    mc_pp_cache_close (cache);

    /* only test_so is asked for */
    mc_pp_cache_free_stub (stub_of_module ());

    cache = mc_pp_cache_open (test_cache_file);
    ck_assert_ptr_null (mc_pp_cache_make_stub (cache, other, &st, &offers, &op_offers, NULL));
    mc_pp_cache_close (cache);

    g_free (other);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static void
remove_key (const char *key)
{
    GKeyFile *kf;

    kf = g_key_file_new ();
    ck_assert (g_key_file_load_from_file (kf, test_cache_file, G_KEY_FILE_NONE, NULL));
    ck_assert (g_key_file_remove_key (kf, "Module0", key, NULL));
    ck_assert (g_key_file_save_to_file (kf, test_cache_file, NULL));
    g_key_file_free (kf);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
cache_has_entry (void)
{
    GKeyFile *kf;
    gboolean has;

    kf = g_key_file_new ();
    has = g_key_file_load_from_file (kf, test_cache_file, G_KEY_FILE_NONE, NULL)
        && g_key_file_has_group (kf, "Module0");
    g_key_file_free (kf);
    return has;
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_missing_key_ds") */
static const struct test_missing_key_ds
{
    const char *key;
} test_missing_key_ds[] = {
    { "action_count" }, { "menu_count" },   { "op_count" }, { "flags" },
    { "offers" },       { "sort_reverse" }, { "op0_kind" }, { "op0_offers" },
    { "menu0_action" }, { "menu0_key" },    { "name" },
};

/* @Test(dataSource = "test_missing_key_ds") */
START_PARAMETRIZED_TEST (test_missing_key_is_damaged, test_missing_key_ds)
{
    describe_module (&described_plugin);
    remove_key (data->key);

    ck_assert_ptr_null (stub_of_module ());
    ck_assert (!cache_has_entry ());
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_menu_of_other_action_is_damaged)
{
    mc_panel_plugin_t description = described_plugin;
    static const mc_pp_cmd_menu_entry_t menu[] = {
        { "&Do it", 17, "S-F1", 1234, MC_PP_MENU_PANEL },
    };

    description.cmd_menu_entries = menu;
    describe_module (&description);

    ck_assert_ptr_null (stub_of_module ());
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* a separator has no label, but its key would still run the action */
START_TEST (test_key_of_other_action_is_damaged)
{
    mc_panel_plugin_t description = described_plugin;
    static const mc_pp_cmd_menu_entry_t menu[] = {
        { NULL, 99, NULL, 1234, MC_PP_MENU_PANEL },
    };

    description.cmd_menu_entries = menu;
    describe_module (&description);

    ck_assert_ptr_null (stub_of_module ());
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_unknown_operation_kind_is_damaged)
{
    mc_panel_plugin_t description = described_plugin;
    static const mc_pp_file_operation_t ops[] = {
        { .name = "open", .kind = (mc_pp_file_operation_kind_t) 7 },
    };

    description.file_operations = ops;
    describe_module (&description);

    ck_assert_ptr_null (stub_of_module ());
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_eager_entry_is_dropped)
{
    mc_panel_plugin_t description = described_plugin;

    description.flags |= MC_PPF_EAGER_LOAD;
    describe_module (&description);

    ck_assert_ptr_null (stub_of_module ());
    ck_assert (!cache_has_entry ());
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_entry_gives_its_description)
{
    mc_pp_cache_t *cache;
    mc_panel_plugin_t *stub;
    mc_panel_plugin_t other = described_plugin;
    struct stat st;
    char *stored, *expected, *changed;

    describe_module (&described_plugin);

    ck_assert_int_eq (stat (test_so, &st), 0);
    cache = mc_pp_cache_open (test_cache_file);
    stub = mc_pp_cache_make_stub (cache, test_so, &st, &stub_offers, &stub_op_offers, &stored);
    mc_pp_cache_close (cache);
    ck_assert_ptr_nonnull (stub);

    expected = mc_pp_cache_describe (&described_plugin);
    mctest_assert_str_eq (stored, expected);

    other.display_name = "Another";
    changed = mc_pp_cache_describe (&other);
    ck_assert_str_ne (stored, changed);

    g_free (changed);
    g_free (expected);
    g_free (stored);
    mc_pp_cache_free_stub (stub);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_stub_has_what_was_stored);
    tcase_add_test (tc_core, test_changed_file_has_no_stub);
    tcase_add_test (tc_core, test_changed_mtime_has_no_stub);
    tcase_add_test (tc_core, test_replaced_file_with_old_mtime_has_no_stub);
    tcase_add_test (tc_core, test_other_api_has_no_stub);
    tcase_add_test (tc_core, test_other_program_has_no_stub);
    tcase_add_test (tc_core, test_damaged_cache_is_empty);
    tcase_add_test (tc_core, test_unused_entry_is_dropped);
    mctest_add_parameterized_test (tc_core, test_missing_key_is_damaged, test_missing_key_ds);
    tcase_add_test (tc_core, test_menu_of_other_action_is_damaged);
    tcase_add_test (tc_core, test_key_of_other_action_is_damaged);
    tcase_add_test (tc_core, test_unknown_operation_kind_is_damaged);
    tcase_add_test (tc_core, test_eager_entry_is_dropped);
    tcase_add_test (tc_core, test_entry_gives_its_description);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
