/*
   lib - tests for the runtime extension ABI and loader

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"

#include <gmodule.h>

#include "lib/event.h"
#include "lib/extension-runtime.h"
#include "lib/runtime-events.h"

/*** global variables ****************************************************************************/

static GError *error = NULL;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef guint (*runtime_fixture_count_fn_t) (void);
typedef const char *(*runtime_fixture_string_fn_t) (void);
typedef gboolean (*runtime_fixture_watch_fn_t) (int fd, const char **error);
typedef gboolean (*runtime_fixture_suspend_fn_t) (gboolean terminal_changed, const char **error);

/*** file scope variables ************************************************************************/

/* what the host services of the fd watch test were asked */
static int host_watched_fd = -1;
static int (*host_ready) (int fd, void *data) = NULL;
static void *host_ready_data = NULL;
static int host_unwatched_fd = -1;
static int host_suspend_calls = 0;
static gboolean host_suspend_changed = FALSE;

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    error = NULL;

    ck_assert_msg (mc_event_init (&error), "Failed to initialize event transport: %s",
                   error != NULL ? error->message : "unknown error");
    ck_assert_msg (mc_runtime_events_init (&error), "Failed to initialize runtime events: %s",
                   error != NULL ? error->message : "unknown error");
    mc_runtime_plugins_set_directory_for_tests (TEST_RUNTIME_PLUGIN_DIR);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    mc_runtime_plugins_shutdown ();
    mc_runtime_events_deinit ();
    g_clear_error (&error);
    ck_assert_msg (mc_event_deinit (&error), "Failed to deinitialize event transport: %s",
                   error != NULL ? error->message : "unknown error");
    g_clear_error (&error);
}

/* --------------------------------------------------------------------------------------------- */

static mc_runtime_event_snapshot_t *
startup_snapshot_new (void)
{
    mc_runtime_event_snapshot_t *snapshot;

    snapshot = mc_runtime_event_snapshot_new (MC_RUNTIME_EVENT_STARTUP);
    snapshot->data.startup.run_mode = g_strdup ("full");
    snapshot->data.startup.config_dir = g_strdup ("/tmp/config");
    snapshot->data.startup.data_dir = g_strdup ("/tmp/data");

    return snapshot;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
host_fd_watch (int fd, int (*ready) (int fd, void *data), void *data, const char **reason)
{
    (void) reason;

    host_watched_fd = fd;
    host_ready = ready;
    host_ready_data = data;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
host_fd_unwatch (int fd)
{
    host_unwatched_fd = fd;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
host_tty_suspend (void (*between) (void *data), void *data, gboolean terminal_changed,
                  const char **reason)
{
    (void) reason;

    host_suspend_calls++;
    host_suspend_changed = terminal_changed;
    between (data);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_runtime_plugin_loads_and_receives_events)
{
    GModule *fixture_module;
    char *fixture_path;
    runtime_fixture_count_fn_t init_count;
    runtime_fixture_count_fn_t startup_count;
    runtime_fixture_count_fn_t shutdown_count;
    mc_runtime_event_snapshot_t *snapshot;

    fixture_path =
        g_build_filename (TEST_RUNTIME_PLUGIN_DIR, "runtime-plugin-fixture.so", (char *) NULL);
    fixture_module = g_module_open (fixture_path, 0);
    ck_assert_msg (fixture_module != NULL, "Failed to open runtime fixture: %s", g_module_error ());
    g_free (fixture_path);

    mctest_assert_true (g_module_symbol (fixture_module, "mc_runtime_plugin_fixture_init_count",
                                         (gpointer *) &init_count));
    mctest_assert_true (g_module_symbol (fixture_module, "mc_runtime_plugin_fixture_startup_count",
                                         (gpointer *) &startup_count));
    mctest_assert_true (g_module_symbol (fixture_module, "mc_runtime_plugin_fixture_shutdown_count",
                                         (gpointer *) &shutdown_count));

    mctest_assert_true (mc_runtime_plugins_load (&error));
    mctest_assert_true (mc_runtime_plugins_are_loaded ());
    ck_assert_int_eq ((int) mc_runtime_plugins_count (), 1);
    ck_assert_int_eq ((int) init_count (), 1);

    snapshot = startup_snapshot_new ();
    mctest_assert_true (mc_runtime_event_publish (snapshot, &error));
    mc_runtime_event_snapshot_free (snapshot);
    ck_assert_int_eq ((int) startup_count (), 1);

    mc_runtime_plugins_shutdown ();
    mctest_assert_false (mc_runtime_plugins_are_loaded ());
    ck_assert_int_eq ((int) mc_runtime_plugins_count (), 0);
    ck_assert_int_eq ((int) shutdown_count (), 1);

    g_module_close (fixture_module);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_runtime_plugin_pre_tty)
{
    GModule *fixture_module;
    char *fixture_path;
    runtime_fixture_count_fn_t pre_tty_count;
    runtime_fixture_string_fn_t pre_tty_seen;
    char *argv[] = { (char *) "mcommander", (char *) "--nomouse", (char *) "/tmp", NULL };
    mc_runtime_pre_tty_info_t info;

    fixture_path =
        g_build_filename (TEST_RUNTIME_PLUGIN_DIR, "runtime-plugin-fixture.so", (char *) NULL);
    fixture_module = g_module_open (fixture_path, 0);
    ck_assert_msg (fixture_module != NULL, "Failed to open runtime fixture: %s", g_module_error ());
    g_free (fixture_path);

    mctest_assert_true (g_module_symbol (fixture_module, "mc_runtime_plugin_fixture_pre_tty_count",
                                         (gpointer *) &pre_tty_count));
    mctest_assert_true (g_module_symbol (fixture_module, "mc_runtime_plugin_fixture_pre_tty_seen",
                                         (gpointer *) &pre_tty_seen));

    info.struct_size = sizeof (info);
    info.argc = 3;
    info.argv = argv;
    info.run_mode = "full";
    info.nested = FALSE;

    // nothing is loaded yet: nobody is called
    mc_runtime_plugins_pre_tty (&info);
    ck_assert_int_eq ((int) pre_tty_count (), 0);

    mctest_assert_true (mc_runtime_plugins_load (&error));
    mc_runtime_plugins_pre_tty (&info);
    ck_assert_int_eq ((int) pre_tty_count (), 1);
    mctest_assert_str_eq (pre_tty_seen (), "full 3 /tmp 0");

    mc_runtime_plugins_shutdown ();
    g_module_close (fixture_module);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_runtime_plugin_fd_watch)
{
    static mc_runtime_host_services_v1_t services;
    GModule *fixture_module;
    char *fixture_path;
    runtime_fixture_watch_fn_t watch;
    runtime_fixture_count_fn_t ready_count;
    const char *watch_error = NULL;

    memset (&services, 0, sizeof (services));
    services.abi_version = MC_RUNTIME_PLUGIN_ABI_VERSION;
    services.struct_size = sizeof (services);
    services.fd_watch = host_fd_watch;
    services.fd_unwatch = host_fd_unwatch;
    mc_runtime_plugins_set_host_services (&services);

    fixture_path =
        g_build_filename (TEST_RUNTIME_PLUGIN_DIR, "runtime-plugin-fixture.so", (char *) NULL);
    fixture_module = g_module_open (fixture_path, 0);
    ck_assert_msg (fixture_module != NULL, "Failed to open runtime fixture: %s", g_module_error ());
    g_free (fixture_path);

    mctest_assert_true (
        g_module_symbol (fixture_module, "mc_runtime_plugin_fixture_watch", (gpointer *) &watch));
    mctest_assert_true (g_module_symbol (fixture_module, "mc_runtime_plugin_fixture_fd_ready_count",
                                         (gpointer *) &ready_count));

    mctest_assert_true (mc_runtime_plugins_load (&error));

    mctest_assert_true (watch (7, &watch_error));
    ck_assert_int_eq (host_watched_fd, 7);
    ck_assert_ptr_nonnull (host_ready);

    // the main loop finds the fd readable: the plugin is called, and asks for a redraw
    ck_assert_int_eq (host_ready (7, host_ready_data), 1);
    ck_assert_int_eq ((int) ready_count (), 1);

    // one watch per fd
    mctest_assert_false (watch (7, &watch_error));
    mctest_assert_str_eq (watch_error, "already_watched");

    // an unloaded runtime loses its watches
    mc_runtime_plugins_shutdown ();
    ck_assert_int_eq (host_unwatched_fd, 7);

    mc_runtime_plugins_set_host_services (NULL);
    g_module_close (fixture_module);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_runtime_plugin_tty_suspend)
{
    static mc_runtime_host_services_v1_t services;
    GModule *fixture_module;
    char *fixture_path;
    runtime_fixture_suspend_fn_t suspend;
    runtime_fixture_count_fn_t between_count;
    const char *suspend_error = NULL;

    fixture_path =
        g_build_filename (TEST_RUNTIME_PLUGIN_DIR, "runtime-plugin-fixture.so", (char *) NULL);
    fixture_module = g_module_open (fixture_path, 0);
    ck_assert_msg (fixture_module != NULL, "Failed to open runtime fixture: %s", g_module_error ());
    g_free (fixture_path);

    mctest_assert_true (g_module_symbol (fixture_module, "mc_runtime_plugin_fixture_suspend",
                                         (gpointer *) &suspend));
    mctest_assert_true (g_module_symbol (fixture_module, "mc_runtime_plugin_fixture_between_count",
                                         (gpointer *) &between_count));

    // a host without the service says so
    mctest_assert_true (mc_runtime_plugins_load (&error));
    mctest_assert_false (suspend (FALSE, &suspend_error));
    mctest_assert_str_eq (suspend_error, "not_supported");
    mc_runtime_plugins_shutdown ();

    memset (&services, 0, sizeof (services));
    services.abi_version = MC_RUNTIME_PLUGIN_ABI_VERSION;
    services.struct_size = sizeof (services);
    services.tty_suspend = host_tty_suspend;
    mc_runtime_plugins_set_host_services (&services);

    mctest_assert_true (mc_runtime_plugins_load (&error));
    mctest_assert_true (suspend (TRUE, &suspend_error));
    ck_assert_int_eq (host_suspend_calls, 1);
    mctest_assert_true (host_suspend_changed);
    ck_assert_int_eq ((int) between_count (), 1);

    mc_runtime_plugins_shutdown ();
    mc_runtime_plugins_set_host_services (NULL);
    g_module_close (fixture_module);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static void
collect_found (const char *runtime_name, const char *display_name, gboolean started,
               gboolean enabled_by_default, gpointer user_data)
{
    GString *seen = (GString *) user_data;

    (void) display_name;

    g_string_append_printf (seen, "%s%s:%d:%d", seen->len > 0 ? " " : "", runtime_name,
                            (int) started, (int) enabled_by_default);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_runtime_plugin_failed_init_does_not_hide_the_next)
{
    GString *seen;

    // runtime-plugin-badinit-fixture is found first, by the same name, and does not start
    mctest_assert_true (mc_runtime_plugins_load (&error));
    ck_assert_int_eq ((int) mc_runtime_plugins_count (), 1);

    seen = g_string_new (NULL);
    mc_runtime_plugins_enumerate_found (collect_found, seen);
    ck_assert_ptr_nonnull (strstr (seen->str, "test-runtime:1:1"));
    ck_assert_ptr_null (strstr (seen->str, "test-runtime:0:"));
    g_string_free (seen, TRUE);

    mc_runtime_plugins_shutdown ();
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_runtime_plugin_off_by_default)
{
    GString *seen;

    mctest_assert_true (mc_runtime_plugins_load (&error));

    // the plugin that is off until turned on is found, and not started
    ck_assert_int_eq ((int) mc_runtime_plugins_count (), 1);

    seen = g_string_new (NULL);
    mc_runtime_plugins_enumerate_found (collect_found, seen);
    ck_assert_ptr_nonnull (strstr (seen->str, "test-runtime:1:1"));
    ck_assert_ptr_nonnull (strstr (seen->str, "test-runtime-off:0:0"));
    g_string_free (seen, TRUE);

    mc_runtime_plugins_shutdown ();

    // nothing is left of them once the plugins are shut down
    seen = g_string_new (NULL);
    mc_runtime_plugins_enumerate_found (collect_found, seen);
    mctest_assert_str_eq (seen->str, "");
    g_string_free (seen, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_runtime_plugin_loads_and_receives_events);
    tcase_add_test (tc_core, test_runtime_plugin_pre_tty);
    tcase_add_test (tc_core, test_runtime_plugin_fd_watch);
    tcase_add_test (tc_core, test_runtime_plugin_tty_suspend);
    tcase_add_test (tc_core, test_runtime_plugin_failed_init_does_not_hide_the_next);
    tcase_add_test (tc_core, test_runtime_plugin_off_by_default);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
