/*
   src/panel-plugins/arcmc - RPM dispatch, browsing and extraction

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

#define TEST_SUITE_NAME "/src/panel-plugins/arcmc/rpm"

#include "tests/mctest.h"

#include <archive.h>
#include <archive_entry.h>
#include <gmodule.h>
#include <string.h>
#include <unistd.h>

#include "lib/mcconfig.h"
#include "src/setup.h"
#include "src/filemanager/dir.h"
#include "src/filemanager/mcmagic.h"
#include "src/panel-plugins/arcmc/arcmc-types.h"
#include "src/vfs/local/local.c"

static const char payload[] = "Name: arcmc-rpm-test\nVersion: 1\n";
static char *test_root;
static GModule *module;
static const mc_panel_plugin_t *plugin;

/* A cpio payload compressed with gzip, preceded by the RPM lead, signature
   header and main header. No rpm program or host package database is needed. */
static void
make_rpm (const char *path)
{
    unsigned char headers[128] = { 0xed, 0xab, 0xee, 0xdb, 3, 0 };
    char compressed[8192];
    size_t used;
    struct archive *writer;
    struct archive_entry *entry;
    GByteArray *rpm;

    memcpy (headers + 96, "\216\255\350\001", 4);
    memcpy (headers + 112, "\216\255\350\001", 4);
    writer = archive_write_new ();
    ck_assert_int_eq (archive_write_set_format_cpio_newc (writer), ARCHIVE_OK);
    ck_assert_int_eq (archive_write_add_filter_gzip (writer), ARCHIVE_OK);
    ck_assert_int_eq (archive_write_open_memory (writer, compressed, sizeof (compressed), &used),
                      ARCHIVE_OK);
    entry = archive_entry_new ();
    archive_entry_set_pathname (entry, "package.spec");
    archive_entry_set_filetype (entry, AE_IFREG);
    archive_entry_set_perm (entry, 0644);
    archive_entry_set_size (entry, sizeof (payload) - 1);
    ck_assert_int_eq (archive_write_header (writer, entry), ARCHIVE_OK);
    ck_assert_int_eq (archive_write_data (writer, payload, sizeof (payload) - 1),
                      sizeof (payload) - 1);
    archive_entry_free (entry);
    ck_assert_int_eq (archive_write_close (writer), ARCHIVE_OK);
    archive_write_free (writer);

    rpm = g_byte_array_new ();
    g_byte_array_append (rpm, headers, sizeof (headers));
    g_byte_array_append (rpm, (const guint8 *) compressed, used);
    ck_assert (g_file_set_contents (path, (const char *) rpm->data, rpm->len, NULL));
    g_byte_array_unref (rpm);
}

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    mc_panel_plugin_register_fn register_plugin;

    test_root = g_dir_make_tmp ("mc-arcmc-rpm-XXXXXX", NULL);
    ck_assert_ptr_nonnull (test_root);
    g_setenv ("MC_PROFILE_ROOT", test_root, TRUE);
    str_init_strings (NULL);
    vfs_init ();
    vfs_init_localfs ();
    mc_config_init_config_paths (NULL);
    mc_global.sysconfig_dir = g_strdup (TEST_MAGIC_DIR);
    mc_global.share_data_dir = g_strdup (TEST_MAGIC_DIR);
#ifdef USE_FILE_CMD
    use_file_to_check_type = FALSE;
#endif

    module = g_module_open (TEST_ARCMC_MODULE, G_MODULE_BIND_LAZY);
    ck_assert_msg (module != NULL, "%s", g_module_error ());
    ck_assert (g_module_symbol (module, MC_PANEL_PLUGIN_ENTRY, (gpointer *) &register_plugin));
    plugin = register_plugin ();
}

static void
teardown (void)
{
    char *argv[] = { (char *) "rm", (char *) "-rf", test_root, NULL };

    plugin->shutdown ();
    g_module_close (module);
    mc_magic_flush ();
    mc_config_deinit_config_paths ();
    vfs_shut ();
    str_uninit_strings ();
    g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
    g_free (test_root);
}

/* --------------------------------------------------------------------------------------------- */

/* Both opening by path (menu/history) and by stream (magic.ini) must read the
   payload with libarchive, without leaving an extfs helper active. */
START_TEST (test_rpm_payload)
{
    static const char *const names[] = { "package.rpm", "package.src.rpm", "package.SPM" };
    char *path = g_build_filename (test_root, names[_i / 2], (char *) NULL);
    mc_magic_source_t source = { .display_name = names[_i / 2], .local_path = path };
    mc_magic_action_t action = { 0 };
    mc_panel_host_t host = { 0 };
    arcmc_data_t *data;
    dir_list list = { 0 };
    char *copy = NULL, *contents = NULL, *local_copy = NULL;
    char *before = NULL, *after = NULL;
    gsize before_len, after_len;
    const char *remove_names[] = { "package.spec" };

    make_rpm (path);
    ck_assert (g_file_get_contents (path, &before, &before_len, NULL));
    ck_assert_int_eq (mc_magic_find_action (&source, "Open", &local_copy, &action),
                      MC_MAGIC_ACTION_FOUND);
    ck_assert_str_eq (action.plugin_id, "arcmc");
    ck_assert_str_eq (action.operation_id, "open");
    ck_assert_ptr_null (local_copy);
    mc_magic_action_clear (&action);
    ck_assert (plugin->file_operations[0].may_open_name (names[_i / 2]));

    if (_i % 2 == 0)
        data = plugin->open (&host, path);
    else
        data = plugin->open_input_stream (&host, names[_i / 2],
                                          mc_pp_input_stream_new_for_file (path, FALSE));
    ck_assert_ptr_nonnull (data);
    ck_assert_ptr_null (data->extfs_helper);
    ck_assert_int_eq (plugin->get_items (data, &list), MC_PPR_OK);
    ck_assert_int_eq (list.len, 1);
    ck_assert_str_eq (list.list[0].fname->str, "package.spec");
    dir_list_free_list (&list);

    ck_assert_int_eq (plugin->get_local_copy (data, "package.spec", &copy), MC_PPR_OK);
    ck_assert (g_file_get_contents (copy, &contents, NULL, NULL));
    ck_assert_str_eq (contents, payload);
    ck_assert_int_eq (plugin->put_file (data, copy, "added.txt"), MC_PPR_NOT_SUPPORTED);
    ck_assert_int_eq (plugin->delete_items (data, remove_names, 1), MC_PPR_NOT_SUPPORTED);
    plugin->close (data);

    ck_assert_int_eq (mc_magic_find_action (&source, "View", &local_copy, &action),
                      MC_MAGIC_ACTION_FOUND);
    ck_assert_str_eq (action.plugin_id, "arcmc");
    ck_assert_str_eq (action.operation_id, "view");
    mc_magic_action_clear (&action);
    g_free (contents);
    contents = NULL;
    unlink (copy);
    g_free (copy);
    copy = NULL;
    ck_assert_int_eq (
        plugin->file_operations[1].view_input_stream (
            &host, names[_i / 2], mc_pp_input_stream_new_for_file (path, FALSE), &copy),
        MC_PPR_OK);
    ck_assert (g_file_get_contents (copy, &contents, NULL, NULL));
    ck_assert_ptr_nonnull (strstr (contents, "package.spec"));

    ck_assert (g_file_get_contents (path, &after, &after_len, NULL));
    ck_assert_uint_eq (before_len, after_len);
    ck_assert (memcmp (before, after, before_len) == 0);
    unlink (copy);
    g_free (copy);
    g_free (contents);
    g_free (before);
    g_free (after);
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_loop_test (tc_core, test_rpm_payload, 0, 6);
    return mctest_run_all (tc_core);
}
