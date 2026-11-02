/*
   SQLite panel plugin tests.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.
 */

#define TEST_SUITE_NAME "/src/panel-plugins/sqlite"

#include "tests/mctest.h"

#include <string.h>
#include <unistd.h>

#include <glib/gstdio.h>
#include <sqlite3.h>

#include "lib/panel-plugin.h"
#include "lib/strutil.h"
#include "lib/tty/tty.h"  // COLS
#include "lib/vfs/vfs.h"
#include "src/vfs/local/local.c"
#include "src/filemanager/dir.h"
#include "src/panel-plugins/sqlite/sqlite_json.h"
#include "src/viewer/mcviewer.h"

const mc_panel_plugin_t *mc_panel_plugin_register (void);

/* What the last F3 showed */
static char *viewed = NULL;

/* --------------------------------------------------------------------------------------------- */

/* The plugin's F3 handler calls the real viewer in MC.  These model tests do
   not open a terminal UI, so they provide that one boundary: it keeps the
   file the plugin gave it. */
gboolean
mcview_viewer (const char *command, const vfs_path_t *file_vpath, int start_line,
               off_t search_start, off_t search_end)
{
    (void) command;
    (void) start_line;
    (void) search_start;
    (void) search_end;

    g_free (viewed);
    viewed = NULL;
    if (!g_file_get_contents (vfs_path_as_str (file_vpath), &viewed, NULL, NULL))
        viewed = NULL;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    str_init_strings ("UTF-8");
    // the width F3 fits a table of rows into
    COLS = 80;
    // the viewer of F3 is given a vfs path
    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown (void)
{
    g_free (viewed);
    viewed = NULL;
    vfs_shut ();
    str_uninit_strings ();
}

static void
sqlite_test_list_clear (dir_list *list)
{
    int i;

    for (i = 0; i < list->len; i++)
    {
        g_string_free (list->list[i].fname, TRUE);
        g_free (list->list[i].name_sort_key);
        g_free (list->list[i].extension_sort_key);
    }
    g_free (list->list);
    memset (list, 0, sizeof (*list));
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sqlite_test_list_has (const dir_list *list, const char *name)
{
    int i;

    for (i = 0; i < list->len; i++)
        if (strcmp (list->list[i].fname->str, name) == 0)
            return TRUE;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static char *
sqlite_test_create_database (void)
{
    GError *error = NULL;
    char *path = NULL;
    sqlite3 *db = NULL;
    int fd;
    int rc;

    fd = g_file_open_tmp ("mc-sqlite-test-XXXXXX", &path, &error);
    ck_assert_int_ne (fd, -1);
    close (fd);

    rc = sqlite3_open (path, &db);
    ck_assert_int_eq (rc, SQLITE_OK);
    rc = sqlite3_exec (db,
                       "CREATE TABLE contacts (id INTEGER PRIMARY KEY, name TEXT, photo BLOB, "
                       "content TEXT, email TEXT UNIQUE);"
                       "INSERT INTO contacts (name, photo, content) VALUES "
                       "('Ada', X'00ff', '{\"IndexText\":{\"LineStart\":0}}');"
                       "INSERT INTO contacts (name, photo, content) VALUES ('Grace', NULL, "
                       "'plain text');",
                       NULL, NULL, NULL);
    ck_assert_int_eq (rc, SQLITE_OK);
    sqlite3_close (db);
    g_clear_error (&error);

    return path;
}

/* --------------------------------------------------------------------------------------------- */

static char *
sqlite_test_create_rowid_database (void)
{
    GError *error = NULL;
    char *path = NULL;
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    int fd;
    int i;
    int rc;

    fd = g_file_open_tmp ("mc-sqlite-test-XXXXXX", &path, &error);
    ck_assert_int_ne (fd, -1);
    close (fd);

    rc = sqlite3_open (path, &db);
    ck_assert_int_eq (rc, SQLITE_OK);
    rc = sqlite3_exec (db, "CREATE TABLE entries (value TEXT);", NULL, NULL, NULL);
    ck_assert_int_eq (rc, SQLITE_OK);
    rc = sqlite3_prepare_v2 (db, "INSERT INTO entries (rowid, value) VALUES (?1, ?2)", -1, &stmt,
                             NULL);
    ck_assert_int_eq (rc, SQLITE_OK);

    /* One transaction for all of them: a commit per row is slow enough on a
       synchronous file system to run into the test timeout. */
    rc = sqlite3_exec (db, "BEGIN", NULL, NULL, NULL);
    ck_assert_int_eq (rc, SQLITE_OK);

    for (i = 401; i >= 1; i--)
    {
        char *value = g_strdup_printf ("row-%03d", i);

        sqlite3_bind_int (stmt, 1, i);
        sqlite3_bind_text (stmt, 2, value, -1, SQLITE_TRANSIENT);
        if (sqlite3_step (stmt) != SQLITE_DONE || sqlite3_reset (stmt) != SQLITE_OK)
            ck_abort_msg ("Cannot insert test row %d", i);
        sqlite3_clear_bindings (stmt);
        g_free (value);
    }

    rc = sqlite3_exec (db, "COMMIT", NULL, NULL, NULL);
    ck_assert_int_eq (rc, SQLITE_OK);

    sqlite3_finalize (stmt);
    sqlite3_close (db);
    g_clear_error (&error);
    return path;
}

/* --------------------------------------------------------------------------------------------- */

static const mc_pp_file_operation_t *
sqlite_test_find_operation (const mc_panel_plugin_t *plugin, const char *name)
{
    int i;

    for (i = 0; i < plugin->file_operation_count; i++)
        if (strcmp (plugin->file_operations[i].name, name) == 0)
            return &plugin->file_operations[i];

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* A stream with no file of its own: what a database inside another panel
   plugin arrives as. */

typedef struct
{
    mc_pp_input_stream_t base;
    char *path;
    gboolean freed;
} sqlite_test_stream_t;

static mc_pp_result_t
sqlite_test_stream_open (mc_pp_input_stream_t *stream, void **handle, GError **error)
{
    sqlite_test_stream_t *source = (sqlite_test_stream_t *) stream;
    FILE *file;

    (void) error;

    file = fopen (source->path, "r");
    *handle = file;
    return file != NULL ? MC_PPR_OK : MC_PPR_FAILED;
}

static gssize
sqlite_test_stream_read (mc_pp_input_stream_t *stream, void *handle, void *buf, gsize size,
                         GError **error)
{
    (void) stream;
    (void) error;

    return (gssize) fread (buf, 1, size, (FILE *) handle);
}

static void
sqlite_test_stream_close (mc_pp_input_stream_t *stream, void *handle)
{
    (void) stream;

    fclose ((FILE *) handle);
}

static void
sqlite_test_stream_free (mc_pp_input_stream_t *stream)
{
    sqlite_test_stream_t *source = (sqlite_test_stream_t *) stream;

    source->freed = TRUE;
}

static const mc_pp_input_stream_ops_t sqlite_test_stream_ops = {
    .open = sqlite_test_stream_open,
    .read = sqlite_test_stream_read,
    .seek = NULL,
    .close = sqlite_test_stream_close,
    .free = sqlite_test_stream_free,
};

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_browse_database_and_view_row)
{
    const mc_panel_plugin_t *plugin;
    void *data;
    dir_list list = { 0 };
    char *db_path;
    char *local_path = NULL;
    char *content = NULL;
    char *schema_path = NULL;
    char *schema = NULL;
    char *location;
    void *restored;

    db_path = sqlite_test_create_database ();
    plugin = mc_panel_plugin_register ();
    data = plugin->open (NULL, db_path);
    mctest_assert_not_null (data);

    ck_assert_int_eq (plugin->get_items (data, &list), MC_PPR_OK);
    mctest_assert_true (sqlite_test_list_has (&list, "contacts"));
    mctest_assert_true (sqlite_test_list_has (&list, "schema.sql"));
    sqlite_test_list_clear (&list);
    ck_assert_int_eq (plugin->get_local_copy (data, "schema.sql", &schema_path), MC_PPR_OK);
    mctest_assert_true (g_file_get_contents (schema_path, &schema, NULL, NULL));
    mctest_assert_null (strstr (schema, "sqlite_autoindex"));
    unlink (schema_path);
    g_free (schema_path);
    g_free (schema);

    ck_assert_int_eq (plugin->chdir (data, "contacts"), MC_PPR_OK);
    ck_assert_int_eq (plugin->get_items (data, &list), MC_PPR_OK);
    mctest_assert_true (sqlite_test_list_has (&list, "schema.sql"));
    mctest_assert_true (sqlite_test_list_has (&list, "rows-000000000001-000000000002"));
    sqlite_test_list_clear (&list);

    ck_assert_int_eq (plugin->chdir (data, "rows-000000000001-000000000002"), MC_PPR_OK);
    ck_assert_int_eq (plugin->get_items (data, &list), MC_PPR_OK);
    mctest_assert_true (sqlite_test_list_has (&list, "row-000000000001.json"));
    sqlite_test_list_clear (&list);

    ck_assert_int_eq (plugin->chdir (data, ".."), MC_PPR_OK);
    mctest_assert_str_eq (plugin->get_focus_name (data), "rows-000000000001-000000000002");
    ck_assert_int_eq (plugin->chdir (data, ".."), MC_PPR_OK);
    mctest_assert_str_eq (plugin->get_focus_name (data), "contacts");

    ck_assert_int_eq (plugin->chdir (data, "contacts"), MC_PPR_OK);
    ck_assert_int_eq (plugin->chdir (data, "rows-000000000001-000000000002"), MC_PPR_OK);
    ck_assert_int_eq (plugin->get_local_copy (data, "row-000000000001.json", &local_path),
                      MC_PPR_OK);
    mctest_assert_true (g_file_get_contents (local_path, &content, NULL, NULL));
    ck_assert_ptr_ne (strstr (content, "\"name\": \"Ada\""), NULL);
    ck_assert_ptr_ne (strstr (content, "\"$blob\": \"00ff\""), NULL);
    ck_assert_ptr_ne (strstr (content, "\\\"IndexText\\\""), NULL);

    location = plugin->get_location (data);
    mctest_assert_not_null (location);
    restored = plugin->open (NULL, location);
    mctest_assert_not_null (restored);
    plugin->close (restored);

    unlink (local_path);
    g_free (local_path);
    g_free (content);
    g_free (location);
    plugin->close (data);
    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_rowid_page_mapping)
{
    const mc_panel_plugin_t *plugin;
    void *data;
    dir_list list = { 0 };
    char *db_path;
    char *local_path = NULL;
    char *content = NULL;

    db_path = sqlite_test_create_rowid_database ();
    plugin = mc_panel_plugin_register ();
    data = plugin->open (NULL, db_path);
    mctest_assert_not_null (data);

    ck_assert_int_eq (plugin->chdir (data, "entries"), MC_PPR_OK);
    ck_assert_int_eq (plugin->get_items (data, &list), MC_PPR_OK);
    mctest_assert_true (sqlite_test_list_has (&list, "rows-000000000001-000000000200"));
    mctest_assert_true (sqlite_test_list_has (&list, "rows-000000000201-000000000400"));
    mctest_assert_true (sqlite_test_list_has (&list, "rows-000000000401-000000000401"));
    sqlite_test_list_clear (&list);

    ck_assert_int_eq (plugin->chdir (data, "rows-000000000201-000000000400"), MC_PPR_OK);
    ck_assert_int_eq (plugin->get_local_copy (data, "row-000000000201.json", &local_path),
                      MC_PPR_OK);
    mctest_assert_true (g_file_get_contents (local_path, &content, NULL, NULL));
    ck_assert_ptr_ne (strstr (content, "\"value\": \"row-201\""), NULL);
    unlink (local_path);
    g_free (local_path);
    g_free (content);
    local_path = NULL;
    content = NULL;

    ck_assert_int_eq (plugin->chdir (data, ".."), MC_PPR_OK);
    ck_assert_int_eq (plugin->chdir (data, "rows-000000000401-000000000401"), MC_PPR_OK);
    ck_assert_int_eq (plugin->get_local_copy (data, "row-000000000401.json", &local_path),
                      MC_PPR_OK);
    mctest_assert_true (g_file_get_contents (local_path, &content, NULL, NULL));
    ck_assert_ptr_ne (strstr (content, "\"value\": \"row-401\""), NULL);

    unlink (local_path);
    g_free (local_path);
    g_free (content);
    plugin->close (data);
    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_location_with_colon_in_database_path)
{
    const mc_panel_plugin_t *plugin;
    GError *error = NULL;
    char *directory;
    char *prefix;
    char *colon_directory;
    char *db_path;
    char *location;
    sqlite3 *db = NULL;
    void *data;
    void *restored;
    dir_list list = { 0 };

    directory = g_dir_make_tmp ("mc-sqlite-location-XXXXXX", &error);
    mctest_assert_not_null (directory);
    prefix = g_build_filename (directory, "prefix", NULL);
    colon_directory = g_strconcat (prefix, ":", NULL);
    db_path = g_build_filename (colon_directory, "database.sqlite", NULL);
    mctest_assert_true (g_file_set_contents (prefix, "x", 1, &error));
    ck_assert_int_eq (g_mkdir (colon_directory, 0700), 0);

    ck_assert_int_eq (sqlite3_open (db_path, &db), SQLITE_OK);
    ck_assert_int_eq (sqlite3_exec (db, "CREATE TABLE records (value TEXT);", NULL, NULL, NULL),
                      SQLITE_OK);
    sqlite3_close (db);

    plugin = mc_panel_plugin_register ();
    data = plugin->open (NULL, db_path);
    mctest_assert_not_null (data);
    location = plugin->get_location (data);
    mctest_assert_not_null (location);
    restored = plugin->open (NULL, location);
    mctest_assert_not_null (restored);
    ck_assert_int_eq (plugin->get_items (restored, &list), MC_PPR_OK);
    mctest_assert_true (sqlite_test_list_has (&list, "schema.sql"));

    sqlite_test_list_clear (&list);
    plugin->close (restored);
    plugin->close (data);
    unlink (db_path);
    ck_assert_int_eq (g_rmdir (colon_directory), 0);
    unlink (prefix);
    ck_assert_int_eq (g_rmdir (directory), 0);
    g_free (location);
    g_free (db_path);
    g_free (colon_directory);
    g_free (prefix);
    g_free (directory);
    g_clear_error (&error);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fallback_order_for_view_and_without_rowid_table)
{
    const mc_panel_plugin_t *plugin;
    char *db_path;
    char *local_path = NULL;
    char *content = NULL;
    sqlite3 *db = NULL;
    void *data;

    db_path = sqlite_test_create_rowid_database ();
    ck_assert_int_eq (sqlite3_open (db_path, &db), SQLITE_OK);
    ck_assert_int_eq (sqlite3_exec (db,
                                    "CREATE TABLE keyed (id INTEGER PRIMARY KEY, value TEXT) "
                                    "WITHOUT ROWID;"
                                    "INSERT INTO keyed VALUES (2, 'z');"
                                    "INSERT INTO keyed VALUES (1, 'a');"
                                    "CREATE VIEW keyed_view AS SELECT value FROM keyed;",
                                    NULL, NULL, NULL),
                      SQLITE_OK);
    sqlite3_close (db);

    plugin = mc_panel_plugin_register ();
    data = plugin->open (NULL, db_path);
    mctest_assert_not_null (data);
    ck_assert_int_eq (plugin->chdir (data, "keyed"), MC_PPR_OK);
    ck_assert_int_eq (plugin->chdir (data, "rows-000000000001-000000000002"), MC_PPR_OK);
    ck_assert_int_eq (plugin->get_local_copy (data, "row-000000000001.json", &local_path),
                      MC_PPR_OK);
    mctest_assert_true (g_file_get_contents (local_path, &content, NULL, NULL));
    ck_assert_ptr_ne (strstr (content, "\"id\": 1"), NULL);
    unlink (local_path);
    g_free (local_path);
    g_free (content);

    ck_assert_int_eq (plugin->chdir (data, ".."), MC_PPR_OK);
    ck_assert_int_eq (plugin->chdir (data, ".."), MC_PPR_OK);
    ck_assert_int_eq (plugin->chdir (data, "keyed_view"), MC_PPR_OK);
    ck_assert_int_eq (plugin->chdir (data, "rows-000000000001-000000000002"), MC_PPR_OK);
    local_path = NULL;
    content = NULL;
    ck_assert_int_eq (plugin->get_local_copy (data, "row-000000000001.json", &local_path),
                      MC_PPR_OK);
    mctest_assert_true (g_file_get_contents (local_path, &content, NULL, NULL));
    ck_assert_ptr_ne (strstr (content, "\"value\": \"a\""), NULL);

    unlink (local_path);
    g_free (local_path);
    g_free (content);
    plugin->close (data);
    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_view_table_as_text)
{
    const mc_panel_plugin_t *plugin;
    void *data;
    char *db_path;

    db_path = sqlite_test_create_database ();
    plugin = mc_panel_plugin_register ();
    data = plugin->open (NULL, db_path);
    mctest_assert_not_null (data);

    ck_assert_int_eq (plugin->view (data, "contacts", NULL, FALSE), MC_PPR_OK);
    mctest_assert_str_eq (viewed,
                          "-- contacts\n"
                          "\n"
                          " id | name  | photo      | content                       | email \n"
                          "----+-------+------------+-------------------------------+-------\n"
                          "  1 | Ada   | <blob 2 B> | {\"IndexText\":{\"LineStart\":0}} |       \n"
                          "  2 | Grace |            | plain text                    |       \n"
                          "(2 rows)\n");

    // Shift-F3 shows the same, there is no raw form of a table
    g_free (viewed);
    viewed = NULL;
    ck_assert_int_eq (plugin->view (data, "contacts", NULL, TRUE), MC_PPR_OK);
    ck_assert_ptr_nonnull (strstr (viewed, "(2 rows)"));

    // a name that is no table is not ours to show
    ck_assert_int_eq (plugin->view (data, "missing", NULL, FALSE), MC_PPR_NOT_SUPPORTED);

    plugin->close (data);
    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_view_table_fits_the_screen)
{
    const mc_panel_plugin_t *plugin;
    sqlite3 *db = NULL;
    void *data;
    char *db_path;
    char **lines;

    db_path = sqlite_test_create_rowid_database ();
    ck_assert_int_eq (sqlite3_open (db_path, &db), SQLITE_OK);
    ck_assert_int_eq (sqlite3_exec (db,
                                    "CREATE TABLE notes (id INTEGER PRIMARY KEY, body TEXT);"
                                    "INSERT INTO notes (body) VALUES "
                                    "('0123456789abcdefghijklmnopqrstuvwxyz0123456789');"
                                    "INSERT INTO notes (body) VALUES (printf('%.300c', 'x'));",
                                    NULL, NULL, NULL),
                      SQLITE_OK);
    {
        // 300 bytes of 0x80 as a text: no UTF-8 at all
        GString *sql = g_string_new ("INSERT INTO notes (body) VALUES (CAST (X'");

        for (int i = 0; i < 300; i++)
            g_string_append (sql, "80");
        g_string_append (sql, "' AS TEXT));");
        ck_assert_int_eq (sqlite3_exec (db, sql->str, NULL, NULL, NULL), SQLITE_OK);
        g_string_free (sql, TRUE);
    }
    sqlite3_close (db);

    plugin = mc_panel_plugin_register ();
    data = plugin->open (NULL, db_path);
    mctest_assert_not_null (data);

    // on a wide screen the long value is cut at 256 bytes only
    COLS = 300;
    ck_assert_int_eq (plugin->view (data, "notes", NULL, FALSE), MC_PPR_OK);
    ck_assert_ptr_nonnull (strstr (viewed, " 0123456789abcdefghijklmnopqrstuvwxyz0123456789 "));
    lines = g_strsplit (viewed, "\n", -1);
    // "-- notes", "", head, rule, row 1, row 2: "  2 | " + 256 x + "~ "
    ck_assert_int_eq (strlen (lines[5]), 6 + 256 + 2);
    ck_assert_ptr_nonnull (strstr (lines[5], "xx~ "));
    g_strfreev (lines);

    // a long text of bytes that are no UTF-8 is not lost to the cut at 256 bytes
    ck_assert_ptr_nonnull (strstr (viewed, "  3 | \xef\xbf\xbd\xef\xbf\xbd"));

    // on a narrow one the widest column gives way, from the end of its values
    COLS = 30;
    ck_assert_int_eq (plugin->view (data, "notes", NULL, FALSE), MC_PPR_OK);
    lines = g_strsplit (viewed, "\n", -1);
    ck_assert_int_eq (strlen (lines[2]), 30);
    mctest_assert_str_eq (lines[4], "  1 | 0123456789abcdefghijkl~ ");
    ck_assert_int_eq (strlen (lines[5]), 30);
    g_strfreev (lines);

    plugin->close (data);
    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_view_long_column_names_fit)
{
    const mc_panel_plugin_t *plugin;
    sqlite3 *db = NULL;
    void *data;
    char *db_path;
    char **lines;

    db_path = sqlite_test_create_rowid_database ();
    ck_assert_int_eq (sqlite3_open (db_path, &db), SQLITE_OK);
    ck_assert_int_eq (sqlite3_exec (db,
                                    "CREATE VIEW named AS SELECT value AS a_rather_long_name_one, "
                                    "value AS a_rather_long_name_two FROM entries;",
                                    NULL, NULL, NULL),
                      SQLITE_OK);
    sqlite3_close (db);

    plugin = mc_panel_plugin_register ();
    data = plugin->open (NULL, db_path);
    mctest_assert_not_null (data);

    // the names give way too, the table fits
    COLS = 30;
    ck_assert_int_eq (plugin->view (data, "named", NULL, FALSE), MC_PPR_OK);
    lines = g_strsplit (viewed, "\n", -1);
    mctest_assert_str_eq (lines[2], " a_rather_lo~ | a_rather_lon~ ");
    g_strfreev (lines);

    plugin->close (data);
    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_view_rows_by_page)
{
    const mc_panel_plugin_t *plugin;
    void *data;
    char *db_path;

    db_path = sqlite_test_create_rowid_database ();
    plugin = mc_panel_plugin_register ();
    data = plugin->open (NULL, db_path);
    mctest_assert_not_null (data);

    // the table: its first page, in rowid order
    ck_assert_int_eq (plugin->view (data, "entries", NULL, FALSE), MC_PPR_OK);
    ck_assert_ptr_nonnull (strstr (viewed, "-- entries: the first 200 rows\n"));
    ck_assert_ptr_nonnull (strstr (viewed, " row-001 \n"));
    ck_assert_ptr_nonnull (strstr (viewed, " row-200 \n"));
    mctest_assert_null (strstr (viewed, "row-201"));
    ck_assert_ptr_nonnull (strstr (viewed, "(200 rows)\n"));

    ck_assert_int_eq (plugin->chdir (data, "entries"), MC_PPR_OK);
    ck_assert_int_eq (plugin->view (data, "rows-000000000201-000000000400", NULL, FALSE),
                      MC_PPR_OK);
    ck_assert_ptr_nonnull (strstr (viewed, "-- entries, rows 201-400\n"));
    ck_assert_ptr_nonnull (strstr (viewed, " row-201 \n"));
    ck_assert_ptr_nonnull (strstr (viewed, " row-400 \n"));
    mctest_assert_null (strstr (viewed, "row-200 "));
    mctest_assert_null (strstr (viewed, "row-401"));

    ck_assert_int_eq (plugin->view (data, "rows-000000000401-000000000401", NULL, FALSE),
                      MC_PPR_OK);
    ck_assert_ptr_nonnull (strstr (viewed, "-- entries, rows 401-401\n"));
    ck_assert_ptr_nonnull (strstr (viewed, " row-401 \n(1 row)\n"));

    // schema.sql is a file, F3 on it is the usual one
    ck_assert_int_eq (plugin->view (data, "schema.sql", NULL, FALSE), MC_PPR_NOT_SUPPORTED);

    plugin->close (data);
    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_view_page_of_a_view)
{
    const mc_panel_plugin_t *plugin;
    sqlite3 *db = NULL;
    void *data;
    char *db_path;

    db_path = sqlite_test_create_rowid_database ();
    ck_assert_int_eq (sqlite3_open (db_path, &db), SQLITE_OK);
    ck_assert_int_eq (sqlite3_exec (db,
                                    "CREATE VIEW late AS SELECT value, rowid AS n FROM entries "
                                    "WHERE rowid > 395;",
                                    NULL, NULL, NULL),
                      SQLITE_OK);
    sqlite3_close (db);

    plugin = mc_panel_plugin_register ();
    data = plugin->open (NULL, db_path);
    mctest_assert_not_null (data);

    // a view has no rowid: its rows are sorted by every column
    ck_assert_int_eq (plugin->chdir (data, "late"), MC_PPR_OK);
    ck_assert_int_eq (plugin->view (data, "rows-000000000001-000000000006", NULL, FALSE),
                      MC_PPR_OK);
    mctest_assert_str_eq (viewed,
                          "-- late, rows 1-6\n"
                          "\n"
                          " value   | n   \n"
                          "---------+-----\n"
                          " row-396 | 396 \n"
                          " row-397 | 397 \n"
                          " row-398 | 398 \n"
                          " row-399 | 399 \n"
                          " row-400 | 400 \n"
                          " row-401 | 401 \n"
                          "(6 rows)\n");

    plugin->close (data);
    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reload_sees_external_changes)
{
    const mc_panel_plugin_t *plugin;
    sqlite3 *db = NULL;
    dir_list list = { 0 };
    void *data;
    char *db_path;

    db_path = sqlite_test_create_rowid_database ();
    plugin = mc_panel_plugin_register ();
    data = plugin->open (NULL, db_path);
    mctest_assert_not_null (data);

    ck_assert_int_eq (plugin->chdir (data, "entries"), MC_PPR_OK);
    ck_assert_int_eq (plugin->reload (data), MC_PPR_OK);
    ck_assert_int_eq (plugin->get_items (data, &list), MC_PPR_OK);
    mctest_assert_false (sqlite_test_list_has (&list, "rows-000000000401-000000000402"));
    sqlite_test_list_clear (&list);

    ck_assert_int_eq (sqlite3_open (db_path, &db), SQLITE_OK);
    ck_assert_int_eq (sqlite3_exec (db,
                                    "INSERT INTO entries (rowid, value) VALUES (402, 'row-402');",
                                    NULL, NULL, NULL),
                      SQLITE_OK);
    sqlite3_close (db);

    ck_assert_int_eq (plugin->reload (data), MC_PPR_OK);
    ck_assert_int_eq (plugin->get_items (data, &list), MC_PPR_OK);
    mctest_assert_true (sqlite_test_list_has (&list, "rows-000000000401-000000000402"));

    sqlite_test_list_clear (&list);
    plugin->close (data);
    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_pretty_embedded_json)
{
    const char input[] = "{\"IndexText\":{\"LineStart\":0},\"IndexContent\":\"\\n\"}";
    const char expected[] = "{\n"
                            "    \"IndexText\": {\n"
                            "      \"LineStart\": 0\n"
                            "    },\n"
                            "    \"IndexContent\": \"\\n\"\n"
                            "  }";
    char *pretty;

    pretty = sqlite_json_pretty (input, strlen (input), 2);
    mctest_assert_not_null (pretty);
    ck_assert_str_eq (pretty, expected);
    g_free (pretty);

    mctest_assert_null (sqlite_json_pretty ("{\"missing\": ]", strlen ("{\"missing\": ]"), 2));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_non_database_is_declined)
{
    const mc_panel_plugin_t *plugin;
    GError *error = NULL;
    char *path = NULL;
    int fd;

    fd = g_file_open_tmp ("mc-sqlite-test-XXXXXX", &path, &error);
    ck_assert_int_ne (fd, -1);
    close (fd);

    plugin = mc_panel_plugin_register ();
    mctest_assert_null (plugin->open (NULL, path));

    unlink (path);
    g_free (path);
    g_clear_error (&error);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_closing_database_focuses_its_file)
{
    const mc_panel_plugin_t *plugin;
    mc_panel_host_t host = { 0 };
    void *data;
    char *db_path;
    char *base;

    db_path = sqlite_test_create_database ();
    base = g_path_get_basename (db_path);
    plugin = mc_panel_plugin_register ();
    data = plugin->open (&host, db_path);
    mctest_assert_not_null (data);

    ck_assert_int_eq (plugin->chdir (data, ".."), MC_PPR_CLOSE);
    mctest_assert_str_eq (host.focus_after, base);

    plugin->close (data);
    g_free (host.focus_after);
    g_free (base);
    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_open_operation_browses_a_local_file)
{
    const mc_panel_plugin_t *plugin;
    const mc_pp_file_operation_t *operation;
    mc_pp_input_stream_t *stream;
    dir_list list = { 0 };
    void *data;
    char *db_path;
    char *location;

    db_path = sqlite_test_create_database ();
    plugin = mc_panel_plugin_register ();
    operation = sqlite_test_find_operation (plugin, "open");
    mctest_assert_not_null (operation);
    ck_assert_int_eq (operation->kind, MC_PP_FILE_OPERATION_OPEN);

    stream = mc_pp_input_stream_new_for_file (db_path, FALSE);
    data = operation->open_input_stream (NULL, "contacts.sqlite", stream);
    mctest_assert_not_null (data);

    ck_assert_int_eq (plugin->get_items (data, &list), MC_PPR_OK);
    ck_assert (sqlite_test_list_has (&list, "contacts"));
    sqlite_test_list_clear (&list);

    /* The stream names a file that stays, so the panel keeps its location. */
    location = plugin->get_location (data);
    mctest_assert_not_null (location);
    ck_assert (strstr (location, db_path) != NULL);
    g_free (location);

    plugin->close (data);
    ck_assert (g_file_test (db_path, G_FILE_TEST_EXISTS));

    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_open_operation_copies_a_stream_without_a_file)
{
    const mc_panel_plugin_t *plugin;
    const mc_pp_file_operation_t *operation;
    sqlite_test_stream_t stream;
    dir_list list = { 0 };
    void *data;
    char *db_path;

    db_path = sqlite_test_create_database ();
    plugin = mc_panel_plugin_register ();
    operation = sqlite_test_find_operation (plugin, "open");

    stream.base.ops = &sqlite_test_stream_ops;
    stream.base.data = NULL;
    stream.path = db_path;
    stream.freed = FALSE;

    data = operation->open_input_stream (NULL, "inner.sqlite", &stream.base);
    mctest_assert_not_null (data);

    ck_assert_int_eq (plugin->get_items (data, &list), MC_PPR_OK);
    ck_assert (sqlite_test_list_has (&list, "contacts"));
    sqlite_test_list_clear (&list);

    /* The copy is the panel's own, so it is named after the entry it came
       from and offers no location to come back to. */
    mctest_assert_str_eq (plugin->get_title (data), "inner.sqlite");
    mctest_assert_null (plugin->get_location (data));

    plugin->close (data);
    ck_assert (stream.freed);

    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_view_operation_renders_the_schema)
{
    const mc_panel_plugin_t *plugin;
    const mc_pp_file_operation_t *operation;
    mc_pp_input_stream_t *stream;
    char *db_path;
    char *view_path = NULL;
    char *content = NULL;

    db_path = sqlite_test_create_database ();
    plugin = mc_panel_plugin_register ();
    operation = sqlite_test_find_operation (plugin, "view");
    mctest_assert_not_null (operation);
    ck_assert_int_eq (operation->kind, MC_PP_FILE_OPERATION_VIEW);

    stream = mc_pp_input_stream_new_for_file (db_path, FALSE);
    ck_assert_int_eq (operation->view_input_stream (NULL, "contacts.sqlite", stream, &view_path),
                      MC_PPR_OK);
    mctest_assert_not_null (view_path);
    ck_assert (g_str_has_suffix (view_path, ".sql"));
    ck_assert (g_file_get_contents (view_path, &content, NULL, NULL));
    ck_assert (strstr (content, "CREATE TABLE contacts") != NULL);

    g_free (content);
    unlink (view_path);
    g_free (view_path);
    unlink (db_path);
    g_free (db_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_view_operation_leaves_a_foreign_stream_alone)
{
    const mc_panel_plugin_t *plugin;
    const mc_pp_file_operation_t *operation;
    sqlite_test_stream_t stream;
    GError *error = NULL;
    char *path = NULL;
    char *view_path = NULL;
    int fd;

    fd = g_file_open_tmp ("mc-sqlite-test-XXXXXX", &path, &error);
    ck_assert_int_ne (fd, -1);
    close (fd);
    g_clear_error (&error);

    plugin = mc_panel_plugin_register ();
    operation = sqlite_test_find_operation (plugin, "view");

    stream.base.ops = &sqlite_test_stream_ops;
    stream.base.data = NULL;
    stream.path = path;
    stream.freed = FALSE;

    ck_assert_int_ne (operation->view_input_stream (NULL, "empty.sqlite", &stream.base, &view_path),
                      MC_PPR_OK);
    mctest_assert_null (view_path);
    ck_assert (!stream.freed);

    unlink (path);
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_name_gate_of_the_file_operations)
{
    const mc_panel_plugin_t *plugin;
    const mc_pp_file_operation_t *operation;

    plugin = mc_panel_plugin_register ();
    operation = sqlite_test_find_operation (plugin, "open");

    ck_assert (operation->may_open_name ("places.sqlite"));
    ck_assert (operation->may_open_name ("archive/BACKUP.DB3"));
    ck_assert (!operation->may_open_name ("notes.txt"));
    ck_assert (!operation->may_open_name (".db"));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_browse_database_and_view_row);
    tcase_add_test (tc_core, test_rowid_page_mapping);
    tcase_add_test (tc_core, test_location_with_colon_in_database_path);
    tcase_add_test (tc_core, test_fallback_order_for_view_and_without_rowid_table);
    tcase_add_test (tc_core, test_view_table_as_text);
    tcase_add_test (tc_core, test_view_table_fits_the_screen);
    tcase_add_test (tc_core, test_view_long_column_names_fit);
    tcase_add_test (tc_core, test_view_rows_by_page);
    tcase_add_test (tc_core, test_view_page_of_a_view);
    tcase_add_test (tc_core, test_reload_sees_external_changes);
    tcase_add_test (tc_core, test_pretty_embedded_json);
    tcase_add_test (tc_core, test_non_database_is_declined);
    tcase_add_test (tc_core, test_closing_database_focuses_its_file);
    tcase_add_test (tc_core, test_open_operation_browses_a_local_file);
    tcase_add_test (tc_core, test_open_operation_copies_a_stream_without_a_file);
    tcase_add_test (tc_core, test_view_operation_renders_the_schema);
    tcase_add_test (tc_core, test_view_operation_leaves_a_foreign_stream_alone);
    tcase_add_test (tc_core, test_name_gate_of_the_file_operations);

    return mctest_run_all (tc_core);
}
