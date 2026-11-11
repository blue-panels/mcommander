/*
   lib - lazy loading of panel plugins

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

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#include "lib/mcconfig.h"
#include "lib/plugin-prefs.h"
#include "lib/strutil.h"
#include "lib/vfs/vfs.h"

#include "src/vfs/local/local.c"

static char *test_plugins_dir = NULL;

#define MC_PANEL_PLUGINS_DIR  test_plugins_dir
#define MC_EDITOR_PLUGINS_DIR "/nonexistent"

#include "lib/panel-plugin-loader.c"

#define FIXTURE_FILE "panel-plugin-fixture.so"
/* A copy must not have the name of a file in the build directory: dyld looks for the name in
   DYLD_LIBRARY_PATH first, and meson puts the build directory there. */
#define COPY_PREFIX        "copy-"
#define FIXTURE_EAGER_FILE "panel-plugin-eager-fixture.so"
#define FIXTURE_OTHER_FILE "panel-plugin-other-fixture.so"
#define FIXTURE_V2_FILE    "panel-plugin-v2-fixture.so"

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

/* what the fixture module says about itself */
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
    char *package;

    test_root = g_dir_make_tmp ("mc-plugin-lazy-XXXXXX", NULL);
    ck_assert_ptr_nonnull (test_root);

    g_setenv ("HOME", test_root, TRUE);
    g_setenv ("XDG_CONFIG_HOME", test_root, TRUE);
    g_setenv ("XDG_DATA_HOME", test_root, TRUE);
    g_setenv ("XDG_CACHE_HOME", test_root, TRUE);
    g_unsetenv ("FIXTURE_SHUTDOWN");
    g_unsetenv ("FIXTURE_REGISTER_fixture");
    g_unsetenv ("FIXTURE_REGISTER_other");
    mc_config_init_config_paths (NULL);

    test_plugins_dir = g_build_filename (test_root, "system", (char *) NULL);
    ck_assert_int_eq (g_mkdir_with_parents (test_plugins_dir, 0700), 0);

    /* the system directory: user packages are opened at start */
    package = g_build_filename (test_plugins_dir, "fixture", (char *) NULL);
    ck_assert_int_eq (g_mkdir_with_parents (package, 0700), 0);
    test_so = g_build_filename (package, COPY_PREFIX FIXTURE_FILE, (char *) NULL);
    g_free (package);

    test_cache_file = mc_panel_plugins_cache_file ();
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown (void)
{
    char *argv[] = { (char *) "rm", (char *) "-rf", test_root, NULL };

    mc_config_deinit_config_paths ();
    g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
    g_free (stub_op_offers);
    stub_op_offers = NULL;
    g_free (test_cache_file);
    g_free (test_so);
    g_free (test_plugins_dir);
    g_free (test_root);
}

/* --------------------------------------------------------------------------------------------- */

static char *
copy_fixture (const char *built_file, const char *package_dir, const char *package)
{
    char *source, *dir, *dest, *data;
    gsize length;

    source = g_build_filename (TEST_PANEL_PLUGIN_DIR, built_file, (char *) NULL);
    dir = g_build_filename (package_dir, package, (char *) NULL);
    ck_assert_int_eq (g_mkdir_with_parents (dir, 0700), 0);
    dest = g_strconcat (dir, G_DIR_SEPARATOR_S COPY_PREFIX, built_file, (char *) NULL);
    ck_assert (g_file_get_contents (source, &data, &length, NULL));
    ck_assert (g_file_set_contents (dest, data, (gssize) length, NULL));
    g_free (data);
    g_free (dir);
    g_free (source);
    return dest;
}

/* --------------------------------------------------------------------------------------------- */

static char *
user_packages_dir (void)
{
    return g_build_filename (test_root, ".local", "lib", MC_USERCONF_DIR, "panel-plugins",
                             (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
install_fixture (const char *built_file)
{
    char *source, *data;
    gsize length;

    source = g_build_filename (TEST_PANEL_PLUGIN_DIR, built_file, (char *) NULL);
    ck_assert (g_file_get_contents (source, &data, &length, NULL));
    ck_assert (g_file_set_contents (test_so, data, (gssize) length, NULL));
    g_free (data);
    g_free (source);
}

/* --------------------------------------------------------------------------------------------- */

static void
describe_module_at (const char *so, const mc_panel_plugin_t *description)
{
    mc_pp_cache_t *cache;
    struct stat st;

    ck_assert_int_eq (stat (so, &st), 0);
    cache = mc_pp_cache_open (test_cache_file);
    mc_pp_cache_store (cache, so, &st, description);
    mc_pp_cache_close (cache);
}

/* --------------------------------------------------------------------------------------------- */

static void
describe_module (const mc_panel_plugin_t *description)
{
    describe_module_at (test_so, description);
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

#ifdef __linux__
static gboolean
module_is_mapped (void)
{
    char *maps = NULL;
    gboolean found;

    if (!g_file_get_contents ("/proc/self/maps", &maps, NULL, NULL))
        return FALSE;

    found = strstr (maps, test_so) != NULL;
    g_free (maps);
    return found;
}
#endif

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_first_start_opens_and_describes)
{
    const mc_panel_plugin_t *plugin;
    mc_panel_plugin_t *stub;

    install_fixture (FIXTURE_FILE);

    mc_panel_plugins_load ();

    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);
    ck_assert_ptr_nonnull (plugin->open);

    stub = stub_of_module ();
    ck_assert_ptr_nonnull (stub);
    ck_assert_int_eq (stub_offers, MC_PP_OFFERS_CONFIGURE);
    ck_assert_int_eq (stub_op_offers[0],
                      MC_PP_OP_OFFERS_MAY_OPEN_NAME | MC_PP_OP_OFFERS_OPEN_INPUT_STREAM);
    mc_pp_cache_free_stub (stub);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_eager_module_is_not_described)
{
    const mc_panel_plugin_t *plugin;

    install_fixture (FIXTURE_EAGER_FILE);

    mc_panel_plugins_load ();

    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);
    ck_assert_ptr_nonnull (plugin->open);
    ck_assert_ptr_null (stub_of_module ());
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_second_start_does_not_open)
{
    const mc_panel_plugin_t *plugin;
    const mc_pp_file_operation_t *stub_operation;

    install_fixture (FIXTURE_FILE);
    describe_module (&described_plugin);

    mc_panel_plugins_load ();

    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);
    ck_assert_ptr_null (plugin->open);
    ck_assert_ptr_null (plugin->configure);
#ifdef __linux__
    ck_assert (!module_is_mapped ());
#endif

    ck_assert (mc_panel_plugin_offers (plugin, MC_PP_OFFERS_CONFIGURE));
    ck_assert (!mc_panel_plugin_offers (plugin, MC_PP_OFFERS_OPEN_FILE_LIST));
    ck_assert (mc_panel_plugin_operation_offers (
        plugin, 0, MC_PP_OP_OFFERS_MAY_OPEN_NAME | MC_PP_OP_OFFERS_OPEN_INPUT_STREAM));
    ck_assert (!mc_panel_plugin_operation_offers (plugin, 0, MC_PP_OP_OFFERS_SHOW));
    ck_assert (!mc_panel_plugin_operation_offers (plugin, 1, MC_PP_OP_OFFERS_SHOW));

    stub_operation = &plugin->file_operations[0];

    ck_assert (mc_panel_plugin_ensure_loaded (plugin, NULL));

    ck_assert_ptr_eq (mc_panel_plugin_find_by_name ("fixture"), plugin);
    ck_assert_ptr_nonnull (plugin->open);
    ck_assert_ptr_nonnull (plugin->configure);
#ifdef __linux__
    ck_assert (module_is_mapped ());
#endif
    ck_assert_ptr_ne (plugin->file_operations, stub_operation);
    mctest_assert_str_eq (stub_operation->name, "open");
    ck_assert (plugin->file_operations[0].may_open_name ("a.fix"));
    ck_assert (!plugin->file_operations[0].may_open_name ("a.txt"));
    ck_assert (mc_panel_plugin_offers (plugin, MC_PP_OFFERS_CONFIGURE));

    ck_assert (g_file_test (test_cache_file, G_FILE_TEST_EXISTS));

    ck_assert (mc_panel_plugin_ensure_loaded (plugin, NULL));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_broken_module_offers_nothing)
{
    const mc_panel_plugin_t *plugin;
    char *error = NULL;

    g_file_set_contents (test_so, "not a module", -1, NULL);
    describe_module (&described_plugin);

    mc_panel_plugins_load ();
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);
    ck_assert (mc_panel_plugin_offers (plugin, MC_PP_OFFERS_CONFIGURE));

    ck_assert (!mc_panel_plugin_ensure_loaded (plugin, &error));
    ck_assert_ptr_nonnull (error);
    g_free (error);

    ck_assert (!mc_panel_plugin_offers (plugin, MC_PP_OFFERS_CONFIGURE));
    ck_assert_int_eq (plugin->file_operation_count, 0);
    /* the action is still there, so that using it says why nothing happens */
    ck_assert_int_eq (plugin->action_count, 1);
    ck_assert_int_eq (plugin->cmd_menu_entry_count, 1);

    ck_assert (!g_file_test (test_cache_file, G_FILE_TEST_EXISTS));

    error = NULL;
    ck_assert (!mc_panel_plugin_ensure_loaded (plugin, &error));
    ck_assert_ptr_nonnull (error);
    g_free (error);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_module_with_other_actions_is_refused)
{
    const mc_panel_plugin_t *plugin;
    mc_panel_plugin_t description = described_plugin;
    char *error = NULL;

    /* the fixture has one action: the menus of the session were built for two */
    static const mc_pp_action_t two_actions[] = {
        { "One", NULL },
        { "Two", NULL },
    };

    description.actions = two_actions;
    description.action_count = 2;

    install_fixture (FIXTURE_FILE);
    describe_module (&description);

    mc_panel_plugins_load ();
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);

    ck_assert (!mc_panel_plugin_ensure_loaded (plugin, &error));
    ck_assert_ptr_nonnull (strstr (error, "restart"));
    g_free (error);
    ck_assert_ptr_null (plugin->open);

    ck_assert (!g_file_test (test_cache_file, G_FILE_TEST_EXISTS));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_module_with_other_operations_is_refused)
{
    const mc_panel_plugin_t *plugin;
    mc_panel_plugin_t description = described_plugin;
    static const mc_pp_file_operation_t other_operations[] = {
        { .name = "view", .kind = MC_PP_FILE_OPERATION_VIEW },
    };

    description.file_operations = other_operations;

    install_fixture (FIXTURE_FILE);
    describe_module (&description);

    mc_panel_plugins_load ();
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);

    ck_assert (!mc_panel_plugin_ensure_loaded (plugin, NULL));
    ck_assert_int_eq (plugin->file_operation_count, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_running_module_behind_the_path_is_left_alone)
{
    const mc_panel_plugin_t *plugin, *other;
    char *packages, *other_so;

    packages = user_packages_dir ();
    other_so = copy_fixture (FIXTURE_OTHER_FILE, packages, "other");
    install_fixture (FIXTURE_FILE);
    describe_module (&described_plugin);

    mc_panel_plugins_load ();
    other = mc_panel_plugin_find_by_name ("other");
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (other);
    ck_assert_ptr_nonnull (other->open);
    ck_assert_ptr_nonnull (plugin);

    /* the path of the stub now leads to the module "other" runs from */
    ck_assert_int_eq (unlink (test_so), 0);
    ck_assert_int_eq (symlink (other_so, test_so), 0);

    ck_assert (!mc_panel_plugin_ensure_loaded (plugin, NULL));
    ck_assert_ptr_null (g_getenv ("FIXTURE_SHUTDOWN"));
    mctest_assert_str_eq (g_getenv ("FIXTURE_REGISTER_other"), "1");
    ck_assert_ptr_nonnull (other->open);

    g_free (other_so);
    g_free (packages);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* the stub was made for that very file: the module is open, register() must not run again */
START_TEST (test_stub_for_a_running_module_does_not_register)
{
    const mc_panel_plugin_t *plugin, *other;
    char *packages, *other_so;

    packages = user_packages_dir ();
    other_so = copy_fixture (FIXTURE_OTHER_FILE, packages, "other");
    ck_assert_int_eq (symlink (other_so, test_so), 0);
    describe_module (&described_plugin);

    mc_panel_plugins_load ();
    other = mc_panel_plugin_find_by_name ("other");
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (other);
    ck_assert_ptr_nonnull (plugin);
    ck_assert_ptr_null (plugin->open);

    ck_assert (!mc_panel_plugin_ensure_loaded (plugin, NULL));
    mctest_assert_str_eq (g_getenv ("FIXTURE_REGISTER_other"), "1");
    ck_assert_ptr_null (g_getenv ("FIXTURE_SHUTDOWN"));
    ck_assert_ptr_nonnull (other->open);

    g_free (other_so);
    g_free (packages);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* a package update while mc runs: another file, the same plugin */
START_TEST (test_module_replaced_with_the_same_interface_is_used)
{
    const mc_panel_plugin_t *plugin;

    install_fixture (FIXTURE_FILE);
    describe_module (&described_plugin);

    mc_panel_plugins_load ();
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);
    ck_assert_ptr_null (plugin->open);

    install_fixture (FIXTURE_FILE);

    ck_assert (mc_panel_plugin_ensure_loaded (plugin, NULL));
    ck_assert_ptr_nonnull (plugin->open);
    ck_assert (!g_file_test (test_cache_file, G_FILE_TEST_EXISTS));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_module_replaced_with_another_interface_is_not_used)
{
    const mc_panel_plugin_t *plugin;
    char *error = NULL;

    install_fixture (FIXTURE_FILE);
    describe_module (&described_plugin);

    mc_panel_plugins_load ();
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);

    /* the new version has one more action: the menus of the run were built for one */
    install_fixture (FIXTURE_V2_FILE);

    ck_assert (!mc_panel_plugin_ensure_loaded (plugin, &error));
    ck_assert_ptr_nonnull (strstr (error, "restart"));
    g_free (error);
    ck_assert_ptr_null (plugin->open);
    ck_assert (!g_file_test (test_cache_file, G_FILE_TEST_EXISTS));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cache_of_each_install)
{
    char *dir, *stale, *recent;
    struct utimbuf old;

    dir = g_path_get_dirname (test_cache_file);
    ck_assert_int_eq (g_mkdir_with_parents (dir, 0700), 0);
    stale = g_build_filename (dir, "panel-plugins-0000000000000000.cache", (char *) NULL);
    recent = g_build_filename (dir, "panel-plugins-1111111111111111.cache", (char *) NULL);
    g_file_set_contents (stale, "", 0, NULL);
    g_file_set_contents (recent, "", 0, NULL);
    old.actime = old.modtime = time (NULL) - (PANEL_PLUGINS_CACHE_DAYS + 1) * 24 * 60 * 60;
    ck_assert_int_eq (utime (stale, &old), 0);

    install_fixture (FIXTURE_FILE);
    mc_panel_plugins_load ();

    ck_assert (g_str_has_prefix (test_cache_file, dir));
    {
        char *saved_dir, *other_install;

        saved_dir = test_plugins_dir;
        test_plugins_dir = (char *) "/another/install/panel-plugins";
        other_install = mc_panel_plugins_cache_file ();
        test_plugins_dir = saved_dir;
        ck_assert_str_ne (other_install, test_cache_file);
        g_free (other_install);
    }
    ck_assert (g_file_test (test_cache_file, G_FILE_TEST_EXISTS));
    ck_assert (g_file_test (recent, G_FILE_TEST_EXISTS));
    ck_assert (!g_file_test (stale, G_FILE_TEST_EXISTS));

    g_free (recent);
    g_free (stale);
    g_free (dir);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* the package directory is not named as the plugin: one pass must not take it twice */
START_TEST (test_user_dir_as_system_dir_is_read_once)
{
    const mc_panel_plugin_t *plugin;
    GKeyFile *kf;
    char *so, **groups;
    gsize n;

    g_free (test_plugins_dir);
    test_plugins_dir = user_packages_dir ();
    /* the name of the cache depends on the plugin directory */
    g_free (test_cache_file);
    test_cache_file = mc_panel_plugins_cache_file ();
    so = copy_fixture (FIXTURE_FILE, test_plugins_dir, "fixture-pkg");
    describe_module_at (so, &described_plugin);

    mc_panel_plugins_load ();
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);
    ck_assert_ptr_null (plugin->open);

    kf = g_key_file_new ();
    ck_assert (g_key_file_load_from_file (kf, test_cache_file, G_KEY_FILE_NONE, NULL));
    groups = g_key_file_get_groups (kf, &n);
    ck_assert_int_eq (n, 2); /* the header and one module */
    g_strfreev (groups);
    g_key_file_free (kf);
    g_free (so);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_user_package_is_opened_at_start)
{
    const mc_panel_plugin_t *plugin;
    char *packages, *user_so;

    packages = user_packages_dir ();
    user_so = copy_fixture (FIXTURE_FILE, packages, "fixture");
    describe_module_at (user_so, &described_plugin);

    mc_panel_plugins_load ();
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);
    ck_assert_ptr_nonnull (plugin->open);

    {
        mc_pp_cache_t *cache;
        struct stat st;
        int offers, *op_offers;

        ck_assert_int_eq (stat (user_so, &st), 0);
        cache = mc_pp_cache_open (test_cache_file);
        ck_assert_ptr_null (mc_pp_cache_make_stub (cache, user_so, &st, &offers, &op_offers, NULL));
        mc_pp_cache_close (cache);
    }

    g_free (user_so);
    g_free (packages);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* the plugin is the same, the directories are not */
START_TEST (test_broken_user_copy_leaves_the_system_one)
{
    const mc_panel_plugin_t *plugin;
    char *packages, *user_so, *system_so;

    packages = user_packages_dir ();
    user_so = copy_fixture (FIXTURE_FILE, packages, "fixture");
    /* the dependency of the user copy is gone, its file and entry are as they were */
    g_file_set_contents (user_so, "not a module", -1, NULL);
    describe_module_at (user_so, &described_plugin);
    system_so = copy_fixture (FIXTURE_FILE, test_plugins_dir, "fixture-dist");

    mc_panel_plugins_load ();
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);
    ck_assert_ptr_nonnull (plugin->open);

    g_free (system_so);
    g_free (user_so);
    g_free (packages);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* a build installed into the user's directory */
START_TEST (test_user_dir_as_system_dir_is_lazy)
{
    const mc_panel_plugin_t *plugin;
    char *so;

    g_free (test_plugins_dir);
    test_plugins_dir = user_packages_dir ();
    /* the name of the cache depends on the plugin directory */
    g_free (test_cache_file);
    test_cache_file = mc_panel_plugins_cache_file ();
    so = copy_fixture (FIXTURE_FILE, test_plugins_dir, "fixture");
    describe_module_at (so, &described_plugin);

    mc_panel_plugins_load ();
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);
    ck_assert_ptr_null (plugin->open);

    g_free (so);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_module_with_other_menu_drops_cache)
{
    const mc_panel_plugin_t *plugin;
    mc_panel_plugin_t description = described_plugin;

    /* say, a hotkey register() took from the user's config */
    static const mc_pp_cmd_menu_entry_t other_menu[] = {
        { "&Do it", 0, "S-F2", 4321, MC_PP_MENU_PANEL },
    };

    description.cmd_menu_entries = other_menu;

    install_fixture (FIXTURE_FILE);
    describe_module (&description);

    mc_panel_plugins_load ();
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);
    ck_assert_int_eq (plugin->cmd_menu_entries[0].key, 4321);

    ck_assert (mc_panel_plugin_ensure_loaded (plugin, NULL));
    ck_assert_int_eq (plugin->cmd_menu_entries[0].key, 1234);
    ck_assert (!g_file_test (test_cache_file, G_FILE_TEST_EXISTS));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_module_with_other_menu_entries_is_refused)
{
    const mc_panel_plugin_t *plugin;
    mc_panel_plugin_t description = described_plugin;

    /* the menus of the session were built for two entries */
    static const mc_pp_cmd_menu_entry_t two_entries[] = {
        { "&Do it", 0, "S-F1", 1234, MC_PP_MENU_PANEL },
        { "Do it &again", 0, NULL, 0, MC_PP_MENU_PANEL },
    };

    description.cmd_menu_entries = two_entries;
    description.cmd_menu_entry_count = 2;

    install_fixture (FIXTURE_FILE);
    describe_module (&description);

    mc_panel_plugins_load ();
    plugin = mc_panel_plugin_find_by_name ("fixture");
    ck_assert_ptr_nonnull (plugin);

    ck_assert (!mc_panel_plugin_ensure_loaded (plugin, NULL));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_disabled_module_is_described)
{
    mc_panel_plugin_t *stub;

    install_fixture (FIXTURE_FILE);
    /* plugin preferences are kept only once there is a main config, and saved through the VFS */
    str_init_strings ("UTF-8");
    vfs_init ();
    vfs_init_localfs ();
    mc_global.main_config = mc_config_init (NULL, FALSE);
    mc_plugin_prefs_set_disabled (MC_PLUGIN_KIND_PANEL, "fixture", TRUE);
    ck_assert (mc_plugin_prefs_is_disabled (MC_PLUGIN_KIND_PANEL, "fixture"));

    mc_panel_plugins_load ();
    ck_assert_ptr_null (mc_panel_plugin_find_by_name ("fixture"));

    stub = stub_of_module ();
    ck_assert_ptr_nonnull (stub);
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

    tcase_add_test (tc_core, test_first_start_opens_and_describes);
    tcase_add_test (tc_core, test_eager_module_is_not_described);
    tcase_add_test (tc_core, test_second_start_does_not_open);
    tcase_add_test (tc_core, test_broken_module_offers_nothing);
    tcase_add_test (tc_core, test_module_with_other_actions_is_refused);
    tcase_add_test (tc_core, test_module_with_other_operations_is_refused);
    tcase_add_test (tc_core, test_running_module_behind_the_path_is_left_alone);
    tcase_add_test (tc_core, test_stub_for_a_running_module_does_not_register);
    tcase_add_test (tc_core, test_module_replaced_with_the_same_interface_is_used);
    tcase_add_test (tc_core, test_module_replaced_with_another_interface_is_not_used);
    tcase_add_test (tc_core, test_cache_of_each_install);
    tcase_add_test (tc_core, test_user_dir_as_system_dir_is_read_once);
    tcase_add_test (tc_core, test_user_package_is_opened_at_start);
    tcase_add_test (tc_core, test_broken_user_copy_leaves_the_system_one);
    tcase_add_test (tc_core, test_user_dir_as_system_dir_is_lazy);
    tcase_add_test (tc_core, test_module_with_other_menu_entries_is_refused);
    tcase_add_test (tc_core, test_disabled_module_is_described);
    tcase_add_test (tc_core, test_module_with_other_menu_drops_cache);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
