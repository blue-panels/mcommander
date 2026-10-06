/*
   Tests for the user menu that edits itself.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

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

#define TEST_SUITE_NAME "/src/usermenu_ini"

#include "tests/mctest.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/mcconfig.h"  // mc_config_get_home_dir()

#include "src/usermenu_ini.h"

/* --------------------------------------------------------------------------------------------- */

static char *
write_temp_file (const char *content)
{
    char *name;
    int fd;
    FILE *file;

    name = g_build_filename (g_get_tmp_dir (), "mc-usermenu-test-XXXXXX", (char *) NULL);
    fd = g_mkstemp (name);
    if (fd == -1)
    {
        g_free (name);
        return NULL;
    }

    file = fdopen (fd, "w");
    if (file == NULL)
    {
        close (fd);
        g_free (name);
        return NULL;
    }

    if (content != NULL)
        fputs (content, file);
    fclose (file);

    return name;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
entries_new (void)
{
    return g_ptr_array_new_with_free_func ((GDestroyNotify) user_menu_entry_free);
}

/* --------------------------------------------------------------------------------------------- */

static user_menu_entry_t *
entry_new (const char *label, char hotkey, const char *command)
{
    user_menu_entry_t *entry;

    entry = g_new0 (user_menu_entry_t, 1);
    entry->label = g_strdup (label);
    entry->hotkey = hotkey;
    entry->command = g_strdup (command);

    return entry;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_entries_are_read_in_the_order_of_the_file)
{
    char *file;
    GPtrArray *entries;
    user_menu_entry_t *entry;

    file = write_temp_file ("# mc menu format 2\n"
                            "[Second]\n"
                            "hotkey=b\n"
                            "command=echo two\n"
                            "\n"
                            "[First]\n"
                            "hotkey=a\n"
                            "command=echo one\n"
                            "view=true\n"
                            "silent=true\n");
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 1, NULL));

    ck_assert_uint_eq (entries->len, 2);

    entry = g_ptr_array_index (entries, 0);
    ck_assert_str_eq (entry->label, "Second");
    ck_assert_int_eq (entry->hotkey, 'b');
    ck_assert_str_eq (entry->command, "echo two");
    ck_assert (!entry->view);
    ck_assert (!entry->silent);
    ck_assert_int_eq (entry->level, 1);

    entry = g_ptr_array_index (entries, 1);
    ck_assert_str_eq (entry->label, "First");
    ck_assert (entry->view);
    ck_assert (entry->silent);

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_what_is_written_is_read_back)
{
    char *file;
    GPtrArray *entries;
    user_menu_entry_t *entry;

    file = write_temp_file (NULL);
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    g_ptr_array_add (entries, entry_new ("Pack it", 'p', "tar caf %{Name} %s"));
    g_ptr_array_add (entries, entry_new ("Two lines", 'l', "echo one\necho two"));

    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));

    ck_assert_uint_eq (entries->len, 2);

    entry = g_ptr_array_index (entries, 0);
    ck_assert_str_eq (entry->label, "Pack it");
    ck_assert_int_eq (entry->hotkey, 'p');
    ck_assert_str_eq (entry->command, "tar caf %{Name} %s");

    // A command of several lines is one value of the file, newlines and all.
    entry = g_ptr_array_index (entries, 1);
    ck_assert_str_eq (entry->command, "echo one\necho two");

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_only_the_level_of_the_file_is_written)
{
    char *file;
    GPtrArray *entries;

    file = write_temp_file (NULL);
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    g_ptr_array_add (entries, entry_new ("Mine", 'm', "echo mine"));
    g_ptr_array_add (entries, entry_new ("Theirs", 't', "echo theirs"));
    ((user_menu_entry_t *) g_ptr_array_index (entries, 1))->level = 2;

    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));

    ck_assert_uint_eq (entries->len, 1);
    ck_assert_str_eq (((user_menu_entry_t *) g_ptr_array_index (entries, 0))->label, "Mine");

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_key_of_a_later_version_is_kept)
{
    char *file;
    GPtrArray *entries;
    GKeyFile *keys;
    char *value;

    file = write_temp_file ("# mc menu format 2\n"
                            "[Entry]\n"
                            "hotkey=e\n"
                            "command=echo old\n"
                            "icon=tools\n");
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));
    ck_assert_uint_eq (entries->len, 1);

    g_free (((user_menu_entry_t *) g_ptr_array_index (entries, 0))->command);
    ((user_menu_entry_t *) g_ptr_array_index (entries, 0))->command = g_strdup ("echo new");

    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);

    keys = g_key_file_new ();
    ck_assert (g_key_file_load_from_file (keys, file, G_KEY_FILE_NONE, NULL));

    value = g_key_file_get_string (keys, "Entry", "command", NULL);
    ck_assert_str_eq (value, "echo new");
    g_free (value);

    // The key this version knows nothing about is still there.
    value = g_key_file_get_string (keys, "Entry", "icon", NULL);
    ck_assert_ptr_ne (value, NULL);
    ck_assert_str_eq (value, "tools");
    g_free (value);

    g_key_file_free (keys);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_file_that_is_not_there_gives_no_entries)
{
    GPtrArray *entries;

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, "/nonexistent/mc6/menu.ini", 0, NULL));
    ck_assert_uint_eq (entries->len, 0);
    g_ptr_array_free (entries, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_file_that_cannot_be_read_is_not_written_over)
{
    static const char *const broken = "# mc menu format 2\n"
                                      "[Diff]\n"
                                      "hotkey=d\n"
                                      "command=diff %f %D/%F\n"
                                      "  | less\n";
    char *file;
    char *content = NULL;
    GPtrArray *entries;
    GError *error = NULL;

    file = write_temp_file (broken);
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    ck_assert (!user_menu_ini_load_file (entries, file, 0, &error));
    ck_assert_ptr_ne (error, NULL);
    g_clear_error (&error);
    ck_assert_uint_eq (entries->len, 0);

    g_ptr_array_add (entries, entry_new ("New", 'n', "echo new"));

    ck_assert (!user_menu_ini_save_file (file, entries, 0, &error));
    ck_assert_ptr_ne (error, NULL);
    g_clear_error (&error);

    ck_assert (g_file_get_contents (file, &content, NULL, NULL));
    ck_assert_str_eq (content, broken);

    g_free (content);
    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_masks_and_types_match)
{
    static const struct
    {
        const char *masks;
        const char *path;
        gboolean is_dir;
        const char *base;
        gboolean result;
    } cases[] = {
        // no '/': the last part of the path, at any level
        { "*.c", "/a/b/main.c", FALSE, NULL, TRUE },
        { "*.c", "/a/b/main.cc", FALSE, NULL, FALSE },
        { "*.1", "/123/4535/rrr.1/uu.2", FALSE, NULL, FALSE },
        { "*.1", "/123/4535/rrr.1", TRUE, NULL, TRUE },
        { "*.h; *.c", "/a/main.c", FALSE, NULL, TRUE },
        { "*.[ch]", "/a/main.h", FALSE, NULL, TRUE },
        { "*.[ch]", "/a/main.c", FALSE, NULL, TRUE },
        { "[abc]", "/a/a", FALSE, NULL, TRUE },
        { "[abc]", "/a/b", FALSE, NULL, TRUE },
        { "[abc]", "/a/d", FALSE, NULL, FALSE },
        { "x[a-cx]y", "/a/xby", FALSE, NULL, TRUE },
        { "x[a-cx]y", "/a/xxy", FALSE, NULL, TRUE },
        { "[!a-c]", "/a/b", FALSE, NULL, FALSE },
        { "[]a]", "/a/]", FALSE, NULL, TRUE },
        { "ttyS*", "/dev/ttyS0", FALSE, NULL, TRUE },
        // a '/' at the start: the whole path; '*' does not go over a '/'
        { "/home/u/mc/*.c", "/home/u/mc/main.c", FALSE, NULL, TRUE },
        { "/home/u/mc/*.c", "/home/u/mc/src/main.c", FALSE, NULL, FALSE },
        { "/home/u/mc/**/*.c", "/home/u/mc/src/main.c", FALSE, NULL, TRUE },
        { "/home/u/mc/**/*.c", "/home/u/mc/main.c", FALSE, NULL, TRUE },
        { "/home/u/mc/**", "/home/u/mc/src/x/y", FALSE, NULL, TRUE },
        { "/home/u/mc/**", "/home/u/mc", TRUE, NULL, FALSE },
        { "/home/u/mc/**", "/home/u/mc-old/x", FALSE, NULL, FALSE },
        { "**/src/*", "/home/u/mc/src/main.c", FALSE, NULL, TRUE },
        { "**/src/*", "/home/u/mc/src/x/main.c", FALSE, NULL, FALSE },
        // a '/' in the middle: from the menu file, or at any level without one
        { "src/*.c", "/home/u/mc/src/main.c", FALSE, "/home/u/mc", TRUE },
        { "src/*.c", "/home/u/other/src/main.c", FALSE, "/home/u/mc", FALSE },
        { "src/*.c", "/home/u/other/src/main.c", FALSE, NULL, TRUE },
        // a '/' at the end: a directory
        { "build/", "/home/u/mc/build", TRUE, NULL, TRUE },
        { "build/", "/home/u/mc/build", FALSE, NULL, FALSE },
        // the last mask that matches decides
        { "*.c;!test_*.c", "/a/test_x.c", FALSE, NULL, FALSE },
        { "*.c;!test_*.c", "/a/x.c", FALSE, NULL, TRUE },
        { "!*.o", "/a/x.c", FALSE, NULL, FALSE },
        { "*;!*.o", "/a/x.c", FALSE, NULL, TRUE },
        { "*;!*.o", "/a/x.o", FALSE, NULL, FALSE },
        { "\\!x", "/a/!x", FALSE, NULL, TRUE },
        { "*.c", NULL, FALSE, NULL, FALSE },
    };
    size_t i;
    char *home_mask;

    for (i = 0; i < G_N_ELEMENTS (cases); i++)
        ck_assert_msg (
            user_menu_ini_path_match (cases[i].masks, cases[i].path, cases[i].is_dir, cases[i].base)
                == cases[i].result,
            "case %zu: %s on %s", i, cases[i].masks, cases[i].path);

    // '~' is the home directory
    home_mask = g_build_filename (mc_config_get_home_dir (), "dev", "x.c", (char *) NULL);
    ck_assert (user_menu_ini_path_match ("~/dev/*.c", home_mask, FALSE, NULL));
    g_free (home_mask);

    ck_assert (user_menu_ini_on_match ("file", S_IFREG | 0644, FALSE, FALSE));
    ck_assert (!user_menu_ini_on_match ("file", S_IFDIR | 0755, FALSE, FALSE));
    ck_assert (user_menu_ini_on_match ("dir", S_IFDIR | 0755, FALSE, FALSE));
    ck_assert (user_menu_ini_on_match ("!dir", S_IFREG | 0644, FALSE, FALSE));
    ck_assert (!user_menu_ini_on_match ("!dir", S_IFDIR | 0755, FALSE, FALSE));
    ck_assert (user_menu_ini_on_match ("dir;fifo;socket", S_IFIFO | 0644, FALSE, FALSE));

    // a link goes by what it points to, and is a link as well
    ck_assert (user_menu_ini_on_match ("dir", S_IFLNK | 0777, TRUE, FALSE));
    ck_assert (user_menu_ini_on_match ("file", S_IFLNK | 0777, FALSE, FALSE));
    ck_assert (user_menu_ini_on_match ("link", S_IFLNK | 0777, TRUE, FALSE));
    ck_assert (user_menu_ini_on_match ("broken", S_IFLNK | 0777, FALSE, TRUE));
    ck_assert (!user_menu_ini_on_match ("file", S_IFLNK | 0777, FALSE, TRUE));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_type_that_is_not_there_is_reported)
{
    char *bad = NULL;
    char *file;
    GPtrArray *entries;
    GError *error = NULL;

    ck_assert (user_menu_ini_on_check ("file;!dir; link", NULL));
    ck_assert (!user_menu_ini_on_check ("file;!dif", &bad));
    ck_assert_str_eq (bad, "dif");
    g_free (bad);

    file = write_temp_file ("# mc menu format 2\n"
                            "[Good]\n"
                            "command=echo good\n"
                            "\n"
                            "[Convert %f to man]\n"
                            "hotkey=m\n"
                            "on=!dif\n"
                            "command=go-md2man -in %f -out %n.1\n");
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    ck_assert (!user_menu_ini_load_file (entries, file, 0, &error));
    ck_assert_ptr_ne (error, NULL);
    ck_assert_ptr_ne (strstr (error->message, "Convert %f to man"), NULL);
    ck_assert_ptr_ne (strstr (error->message, "dif"), NULL);
    g_clear_error (&error);

    // nothing of a file with a wrong value is taken, not even the entries before it
    ck_assert_uint_eq (entries->len, 0);

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_conditions_are_written_and_read_back)
{
    char *file;
    GPtrArray *entries;
    user_menu_entry_t *entry;
    GError *error = NULL;

    file = write_temp_file ("# mc menu format 2\n"
                            "[Compile]\n"
                            "hotkey=c\n"
                            "path=*.c;*.h\n"
                            "on=file\n"
                            "marked=false\n"
                            "other.on=dir\n"
                            "needs=cc\n"
                            "default=true\n"
                            "command=make\n");
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));
    ck_assert_uint_eq (entries->len, 1);

    // a rename in the dialog must not lose them: they travel with the entry
    entry = g_ptr_array_index (entries, 0);
    g_free (entry->label);
    entry->label = g_strdup ("Build");
    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, &error));
    ck_assert_uint_eq (entries->len, 1);

    entry = g_ptr_array_index (entries, 0);
    ck_assert_str_eq (entry->label, "Build");
    ck_assert_str_eq (entry->cond[UM_COND_PATH], "*.c;*.h");
    ck_assert_str_eq (entry->cond[UM_COND_ON], "file");
    ck_assert_str_eq (entry->cond[UM_COND_MARKED], "false");
    ck_assert_str_eq (entry->cond[UM_COND_OTHER_ON], "dir");
    ck_assert_str_eq (entry->cond[UM_COND_NEEDS], "cc");
    ck_assert_ptr_eq (entry->cond[UM_COND_PATH_RE], NULL);
    ck_assert (entry->is_default);

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_flag_that_is_not_true_or_false_is_reported)
{
    char *file;
    GPtrArray *entries;
    GError *error = NULL;

    file = write_temp_file ("# mc menu format 2\n"
                            "[Pack]\n"
                            "marked=yes\n"
                            "command=tar czf x.tgz %s\n");
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    ck_assert (!user_menu_ini_load_file (entries, file, 0, &error));
    ck_assert_ptr_ne (strstr (error->message, "marked"), NULL);
    g_clear_error (&error);
    ck_assert_uint_eq (entries->len, 0);

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static char *
read_back (const char *file)
{
    char *content = NULL;

    ck_assert (g_file_get_contents (file, &content, NULL, NULL));
    return content;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_command_of_several_lines_is_a_block)
{
    char *file;
    char *content;
    GPtrArray *entries;

    file = write_temp_file (NULL);
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    g_ptr_array_add (entries, entry_new ("Two", 't', "echo one\n\techo \\two"));
    g_ptr_array_add (entries, entry_new ("Fence", 'f', "cat <<EOF\n```\nEOF"));
    g_ptr_array_add (entries, entry_new ("One", 'o', "printf \"%s\\n\" %f"));
    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);

    content = read_back (file);
    ck_assert (g_str_has_prefix (content, "# mc menu format 2\n"));
    ck_assert_ptr_ne (strstr (content, "command=```\necho one\n\techo \\two\n```\n"), NULL);
    ck_assert_ptr_ne (strstr (content, "command=````\ncat <<EOF\n```\nEOF\n````\n"), NULL);
    ck_assert_ptr_ne (strstr (content, "command=printf \"%s\\n\" %f\n"), NULL);
    g_free (content);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));
    ck_assert_uint_eq (entries->len, 3);
    ck_assert_str_eq (((user_menu_entry_t *) g_ptr_array_index (entries, 0))->command,
                      "echo one\n\techo \\two");
    ck_assert_str_eq (((user_menu_entry_t *) g_ptr_array_index (entries, 1))->command,
                      "cat <<EOF\n```\nEOF");
    ck_assert_str_eq (((user_menu_entry_t *) g_ptr_array_index (entries, 2))->command,
                      "printf \"%s\\n\" %f");

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_file_without_the_format_line_is_converted)
{
    char *file, *old;
    char *content;
    GPtrArray *entries;
    GError *error = NULL;

    // written by GKeyFile: escapes in the values, comments, no format line
    file = write_temp_file ("# my menu\n"
                            "[Old]\n"
                            "hotkey=o\n"
                            "# two lines\n"
                            "command=echo a\\necho \\\\b\n"
                            "view=false\n");
    ck_assert_ptr_ne (file, NULL);
    old = g_strconcat (file, ".old", (char *) NULL);

    // not read as it is: that would run the commands with other meaning
    ck_assert (user_menu_ini_needs_conversion (file));
    entries = entries_new ();
    ck_assert (!user_menu_ini_load_file (entries, file, 0, &error));
    g_clear_error (&error);
    ck_assert_uint_eq (entries->len, 0);

    ck_assert (user_menu_ini_convert_file (file, NULL));
    ck_assert (!user_menu_ini_needs_conversion (file));

    content = read_back (file);
    ck_assert_str_eq (content,
                      "# mc menu format 2\n"
                      "# my menu\n"
                      "[Old]\n"
                      "hotkey=o\n"
                      "# two lines\n"
                      "command=```\n"
                      "echo a\n"
                      "echo \\b\n"
                      "```\n"
                      "view=false\n");
    g_free (content);

    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));
    ck_assert_str_eq (((user_menu_entry_t *) g_ptr_array_index (entries, 0))->command,
                      "echo a\necho \\b");
    g_ptr_array_free (entries, TRUE);

    // the file as it was stays next to it
    content = read_back (old);
    ck_assert_ptr_ne (strstr (content, "command=echo a\\necho \\\\b"), NULL);
    g_free (content);
    unlink (old);
    unlink (file);
    g_free (file);

    // written by hand in this format, the line forgotten: the line is put in
    file = write_temp_file ("[Two]\n"
                            "command=```\n"
                            "echo one\n"
                            "echo two\n"
                            "```\n");
    ck_assert (user_menu_ini_convert_file (file, NULL));
    content = read_back (file);
    ck_assert_str_eq (content,
                      "# mc menu format 2\n"
                      "[Two]\n"
                      "command=```\n"
                      "echo one\n"
                      "echo two\n"
                      "```\n");
    g_free (content);
    unlink (old);
    g_free (old);
    old = g_strconcat (file, ".old", (char *) NULL);
    unlink (old);
    unlink (file);
    g_free (file);

    // neither: the file is not touched
    file = write_temp_file ("[Bad]\non=!dif\ncommand=x\n");
    ck_assert (!user_menu_ini_convert_file (file, &error));
    g_clear_error (&error);
    content = read_back (file);
    ck_assert_str_eq (content, "[Bad]\non=!dif\ncommand=x\n");
    g_free (content);
    unlink (file);
    g_free (file);
    g_free (old);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_comments_and_what_did_not_change_stay_as_written)
{
    char *file;
    char *content;
    GPtrArray *entries;
    user_menu_entry_t *entry;

    file = write_temp_file ("# mc menu format 2\n"
                            "# my menu\n"
                            "\n"
                            "# about the first one\n"
                            "[First]\n"
                            "hotkey=a\n"
                            "# why it is so\n"
                            "command=```\n"
                            "echo first\n"
                            "```\n"
                            "icon=one\n"
                            "\n"
                            "# about the second one\n"
                            "[Second]\n"
                            "command=echo second\n"
                            "\n"
                            "[Third]\n"
                            "command=echo third\n"
                            "# the end\n");
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));
    ck_assert_uint_eq (entries->len, 3);

    // the second goes first, the third goes away, the first gets a new command
    g_ptr_array_remove_index (entries, 2);
    entry = g_ptr_array_steal_index (entries, 1);
    g_ptr_array_insert (entries, 0, entry);
    entry = g_ptr_array_index (entries, 1);
    g_free (entry->command);
    entry->command = g_strdup ("echo 1st");

    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);

    content = read_back (file);
    ck_assert_str_eq (content,
                      "# mc menu format 2\n"
                      "# my menu\n"
                      "\n"
                      "# about the second one\n"
                      "[Second]\n"
                      "command=echo second\n"
                      "\n"
                      "# about the first one\n"
                      "[First]\n"
                      "hotkey=a\n"
                      "# why it is so\n"
                      "command=echo 1st\n"
                      "icon=one\n"
                      "\n"
                      "# the end\n");
    g_free (content);

    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_an_error_names_the_line)
{
    static const struct
    {
        const char *text;
        const char *line;
    } cases[] = {
        { "# mc menu format 2\n[Diff]\nhotkey=d\ncommand=diff %f %D/%F\n  | less\n", "line 5:" },
        { "# mc menu format 2\n[A]\ncommand=```\necho a\n", "line 3:" },
        { "# mc menu format 2\n[A]\ncommand=a\ncommand=b\n", "line 4:" },
        { "# mc menu format 2\n[A]\ncommand=a\n[A]\ncommand=b\n", "line 4:" },
        { "# mc menu format 2\ncommand=a\n", "line 2:" },
        // no format line: not a menu of this format
        { "[A]\ncommand=a\n", "line 1:" },
    };
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
        char *file;
        GPtrArray *entries;
        GError *error = NULL;

        file = write_temp_file (cases[i].text);
        ck_assert_ptr_ne (file, NULL);

        entries = entries_new ();
        ck_assert (!user_menu_ini_load_file (entries, file, 0, &error));
        ck_assert_ptr_ne (error, NULL);
        ck_assert_msg (strstr (error->message, cases[i].line) != NULL, "case %zu: %s", i,
                       error->message);
        g_clear_error (&error);
        ck_assert_uint_eq (entries->len, 0);

        g_ptr_array_free (entries, TRUE);
        unlink (file);
        g_free (file);
    }
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_regex_becomes_masks_where_it_can)
{
    static const struct
    {
        const char *regex;
        const char *glob;
    } cases[] = {
        { "\\.c$", "*.c" },           { "^ttyS", "ttyS*" },
        { "^Makefile$", "Makefile" }, { "\\.(tar\\.gz|tgz)$", NULL },
        { "^README.*", "README*" },   { "a.b$", "*a?b" },
        { "\\.[ch]$", NULL },         { "^\\d+$", NULL },
        { "(a|b)(c|d)", NULL },
    };
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
        char *glob;

        glob = user_menu_ini_regex_to_glob (cases[i].regex);
        if (cases[i].glob == NULL)
            ck_assert_msg (glob == NULL, "%s gave %s", cases[i].regex, glob);
        else
        {
            ck_assert_msg (glob != NULL, "%s gave nothing", cases[i].regex);
            ck_assert_str_eq (glob, cases[i].glob);
        }
        g_free (glob);
    }
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_conditions_of_a_menu_written_by_hand_become_keys)
{
    char *file, *out;
    char *content;
    GPtrArray *entries;
    user_menu_entry_t *e;
    guint added;

    file = write_temp_file ("shell_patterns=0\n"
                            "+ t r & ! t t\n"
                            "a       File, nothing marked\n"
                            "        echo a\n"
                            "+ f \\.c$ | f \\.h$ & t r\n"
                            "b       C sources\n"
                            "        echo b\n"
                            "+ f \\.c$ & t d | t l\n"
                            "c       Not to be said with keys\n"
                            "        echo c\n"
                            "= f ^ttyS\n"
                            "+ ! t d\n"
                            "d       Default on a port\n"
                            "        echo d\n"
                            "+ T d & F \\.c$\n"
                            "e       The other panel\n"
                            "        echo e\n"
                            "=+ f \\.gz$ & t r\n"
                            "f       Both\n"
                            "        for i in %s; do\n"
                            "            echo \"$i\"\n"
                            "        done\n");
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    added = user_menu_ini_import_file (entries, file, 0, NULL);
    ck_assert_uint_eq (added, 6);

    e = g_ptr_array_index (entries, 0);
    ck_assert_str_eq (e->cond[UM_COND_ON], "file");
    ck_assert_str_eq (e->cond[UM_COND_MARKED], "false");
    ck_assert_ptr_eq (e->comment, NULL);

    e = g_ptr_array_index (entries, 1);
    ck_assert_str_eq (e->cond[UM_COND_PATH], "*.c;*.h");
    ck_assert_str_eq (e->cond[UM_COND_ON], "file");

    e = g_ptr_array_index (entries, 2);
    ck_assert_ptr_eq (e->cond[UM_COND_PATH], NULL);
    ck_assert_ptr_eq (e->cond[UM_COND_ON], NULL);
    ck_assert_str_eq (e->comment,
                      "# imported from mc.menu, condition not converted: "
                      "+ f \\.c$ & t d | t l\n");

    // "=" is where the menu opens on it: default.* keys
    e = g_ptr_array_index (entries, 3);
    ck_assert_str_eq (e->cond[UM_COND_ON], "!dir");
    ck_assert_str_eq (e->dcond[UM_COND_PATH], "ttyS*");
    ck_assert_ptr_eq (e->comment, NULL);

    e = g_ptr_array_index (entries, 4);
    ck_assert_str_eq (e->cond[UM_COND_OTHER_ON], "dir");
    ck_assert_str_eq (e->cond[UM_COND_OTHER_PATH], "*.c");

    // "=+" is both
    e = g_ptr_array_index (entries, 5);
    ck_assert_str_eq (e->cond[UM_COND_PATH], "*.gz");
    ck_assert_str_eq (e->cond[UM_COND_ON], "file");
    ck_assert_str_eq (e->dcond[UM_COND_PATH], "*.gz");
    ck_assert_str_eq (e->dcond[UM_COND_ON], "file");
    ck_assert_ptr_eq (e->comment, NULL);
    ck_assert_str_eq (e->command, "for i in %s; do\n    echo \"$i\"\ndone");

    // the comment stands above the entry in the file
    out = write_temp_file (NULL);
    unlink (out);
    ck_assert (user_menu_ini_save_file (out, entries, 0, NULL));
    content = read_back (out);
    ck_assert_ptr_ne (strstr (content,
                              "# imported from mc.menu, condition not converted: "
                              "+ f \\.c$ & t d | t l\n"
                              "[Not to be said with keys]\n"),
                      NULL);
    g_free (content);

    g_ptr_array_free (entries, TRUE);
    unlink (out);
    g_free (out);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_regular_expressions_of_names)
{
    char *file;
    GPtrArray *entries;
    user_menu_entry_t *e;
    GError *error = NULL;
    guint added;

    // with no '/', the last part of the path; with one, the whole path
    ck_assert (user_menu_ini_regex_match ("^ttyS", "/dev/ttyS0"));
    ck_assert (!user_menu_ini_regex_match ("^ttyS", "/dev/attyS0"));
    ck_assert (user_menu_ini_regex_match ("\\.[ch]$", "/src/main.h"));
    ck_assert (!user_menu_ini_regex_match ("!^ttyS", "/dev/ttyS0"));
    ck_assert (user_menu_ini_regex_match ("!^ttyS", "/dev/tty0"));
    ck_assert (user_menu_ini_regex_match ("^/home/u/mc(/|$)", "/home/u/mc/x.c"));
    ck_assert (!user_menu_ini_regex_match ("^/home/u/mc(/|$)", "/home/u/mc-old/x.c"));

    // a wrong expression names its line
    file = write_temp_file ("# mc menu format 2\n"
                            "[Port]\n"
                            "path~=^tty[S\n"
                            "command=minicom\n");
    ck_assert_ptr_ne (file, NULL);
    entries = entries_new ();
    ck_assert (!user_menu_ini_load_file (entries, file, 0, &error));
    ck_assert_ptr_ne (strstr (error->message, "line 3:"), NULL);
    g_clear_error (&error);
    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);

    // an expression no mask can say is imported as it is
    file = write_temp_file ("shell_patterns=0\n"
                            "+ f \\.[ch]$ & t r\n"
                            "c       C sources\n"
                            "        echo c\n");
    ck_assert_ptr_ne (file, NULL);
    entries = entries_new ();
    added = user_menu_ini_import_file (entries, file, 0, NULL);
    ck_assert_uint_eq (added, 1);
    e = g_ptr_array_index (entries, 0);
    ck_assert_msg (e->comment == NULL, "%s", e->comment);
    ck_assert_ptr_eq (e->cond[UM_COND_PATH], NULL);
    ck_assert_str_eq (e->cond[UM_COND_PATH_RE], "\\.[ch]$");
    ck_assert_str_eq (e->cond[UM_COND_ON], "file");
    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_conditions_are_imported_as_paths)
{
    static const struct
    {
        const char *condition;
        int key;
        const char *value;
    } cases[] = {
        { "+ f \\.[ch]$", UM_COND_PATH_RE, "\\.[ch]$" },
        { "+ ! f \\.o$", UM_COND_PATH, "*;!*.o" },
        { "+ ! f \\.o$ | f ^core$", UM_COND_PATH, "*;!*.o;core" },
        { "+ F \\.c$", UM_COND_OTHER_PATH, "*.c" },
        // the directory of the panel and mixed alternatives stay as comments
        { "+ d ^/home/u/mc", -1, NULL },
        { "+ f \\.[ch]$ | f ^Make", -1, NULL },
    };
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
        char *text, *file;
        GPtrArray *entries;
        user_menu_entry_t *e;

        text = g_strdup_printf ("shell_patterns=0\n%s\nx       X\n        echo x\n",
                                cases[i].condition);
        file = write_temp_file (text);
        g_free (text);
        ck_assert_ptr_ne (file, NULL);

        entries = entries_new ();
        ck_assert_uint_eq (user_menu_ini_import_file (entries, file, 0, NULL), 1);
        e = g_ptr_array_index (entries, 0);

        if (cases[i].value == NULL)
            ck_assert_msg (e->comment != NULL, "case %zu was converted", i);
        else
        {
            ck_assert_msg (e->cond[cases[i].key] != NULL, "case %zu: no key", i);
            ck_assert_str_eq (e->cond[cases[i].key], cases[i].value);
        }

        g_ptr_array_free (entries, TRUE);
        unlink (file);
        g_free (file);
    }
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_default_conditions_are_written_and_read_back)
{
    char *file;
    char *content;
    GPtrArray *entries;
    user_menu_entry_t *e;
    GError *error = NULL;

    file = write_temp_file ("# mc menu format 2\n"
                            "[Connect]\n"
                            "hotkey=m\n"
                            "default.path=ttyS*\n"
                            "default.other.on=dir\n"
                            "command=minicom -D /dev/%f\n");
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));
    e = g_ptr_array_index (entries, 0);
    ck_assert (!e->is_default);
    ck_assert_str_eq (e->dcond[UM_COND_PATH], "ttyS*");
    ck_assert_str_eq (e->dcond[UM_COND_OTHER_ON], "dir");
    ck_assert_ptr_eq (e->cond[UM_COND_PATH], NULL);

    // a value the dialog changes is written under its key, the rest as it was
    g_free (e->dcond[UM_COND_PATH]);
    e->dcond[UM_COND_PATH] = g_strdup ("ttyUSB*");
    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);

    content = read_back (file);
    ck_assert_str_eq (content,
                      "# mc menu format 2\n"
                      "[Connect]\n"
                      "hotkey=m\n"
                      "default.path=ttyUSB*\n"
                      "default.other.on=dir\n"
                      "command=minicom -D /dev/%f\n");
    g_free (content);
    unlink (file);
    g_free (file);

    // Always in the dialog: default=true, and the default.* keys go
    file = write_temp_file ("# mc menu format 2\n"
                            "[Connect]\n"
                            "hotkey=m\n"
                            "default.path=ttyS*\n"
                            "command=minicom\n"
                            "\n"
                            "[Next]\n"
                            "command=echo\n");
    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));
    e = g_ptr_array_index (entries, 0);
    e->is_default = TRUE;
    g_free (e->dcond[UM_COND_PATH]);
    e->dcond[UM_COND_PATH] = NULL;
    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);
    content = read_back (file);
    ck_assert_str_eq (content,
                      "# mc menu format 2\n"
                      "[Connect]\n"
                      "hotkey=m\n"
                      "command=minicom\n"
                      "default=true\n"
                      "\n"
                      "[Next]\n"
                      "command=echo\n");
    g_free (content);
    unlink (file);
    g_free (file);

    // a wrong value under default. is an error as well
    file = write_temp_file ("# mc menu format 2\n[Connect]\ndefault.on=!dif\ncommand=minicom\n");
    entries = entries_new ();
    ck_assert (!user_menu_ini_load_file (entries, file, 0, &error));
    ck_assert_ptr_ne (strstr (error->message, "default.on"), NULL);
    g_clear_error (&error);
    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_menu_written_by_hand_is_imported)
{
    char *file;
    GPtrArray *entries;
    user_menu_entry_t *entry;
    guint added;

    file = write_temp_file ("shell_patterns=0\n"
                            "\n"
                            "# a comment\n"
                            "+ t r & ! t t\n"
                            "c       Compile this file\n"
                            "        make \"`basename %f .c`\"\n"
                            "        echo done\n"
                            "\n"
                            "i       if [] then else\n"
                            "        echo brackets\n"
                            "\n"
                            "d       Same label\n"
                            "        echo one\n"
                            "\n"
                            "D       Same label\n"
                            "        echo two\n");
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();
    added = user_menu_ini_import_file (entries, file, 1, NULL);

    // the directive, the comment and the condition are not entries
    ck_assert_uint_eq (added, 4);
    ck_assert_uint_eq (entries->len, 4);

    entry = g_ptr_array_index (entries, 0);
    ck_assert_int_eq (entry->hotkey, 'c');
    ck_assert_str_eq (entry->label, "Compile this file");
    // the commands lose the indentation and keep their order
    ck_assert_str_eq (entry->command, "make \"`basename %f .c`\"\necho done");

    // a group name holds no brackets, so the label cannot either
    entry = g_ptr_array_index (entries, 1);
    ck_assert_str_eq (entry->label, "if () then else");

    // and two entries cannot share a label
    ck_assert_str_eq (((user_menu_entry_t *) g_ptr_array_index (entries, 2))->label, "Same label");
    ck_assert_str_eq (((user_menu_entry_t *) g_ptr_array_index (entries, 3))->label,
                      "Same label (2)");

    // what was imported is what a file of the new kind can hold
    {
        char *out;

        out = write_temp_file (NULL);
        ck_assert (user_menu_ini_save_file (out, entries, 1, NULL));
        g_ptr_array_free (entries, TRUE);

        entries = entries_new ();
        ck_assert (user_menu_ini_load_file (entries, out, 1, NULL));
        ck_assert_uint_eq (entries->len, 4);

        unlink (out);
        g_free (out);
    }

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_submenu_and_its_entries_are_written_and_read_back)
{
    char *file;
    GPtrArray *entries;
    user_menu_entry_t *box, *child, *top;
    GKeyFile *keys;

    file = write_temp_file (NULL);
    ck_assert_ptr_ne (file, NULL);

    entries = entries_new ();

    box = entry_new ("Archives", 'a', "");
    box->is_submenu = TRUE;
    g_ptr_array_add (entries, box);

    child = entry_new ("Pack it", 'p', "tar caf x.tgz %s");
    child->parent = g_strdup ("Archives");
    g_ptr_array_add (entries, child);

    g_ptr_array_add (entries, entry_new ("Top one", '1', "echo one"));

    ck_assert (user_menu_ini_save_file (file, entries, 0, NULL));
    g_ptr_array_free (entries, TRUE);

    // a submenu holds no command; an entry at the top has no parent
    keys = g_key_file_new ();
    ck_assert (g_key_file_load_from_file (keys, file, G_KEY_FILE_NONE, NULL));
    ck_assert (g_key_file_get_boolean (keys, "Archives", "submenu", NULL));
    ck_assert (!g_key_file_has_key (keys, "Archives", "command", NULL));
    ck_assert (!g_key_file_has_key (keys, "Top one", "parent", NULL));
    g_key_file_free (keys);

    entries = entries_new ();
    ck_assert (user_menu_ini_load_file (entries, file, 0, NULL));
    ck_assert_uint_eq (entries->len, 3);

    box = g_ptr_array_index (entries, 0);
    ck_assert (box->is_submenu);
    ck_assert_ptr_eq (box->parent, NULL);

    child = g_ptr_array_index (entries, 1);
    ck_assert (!child->is_submenu);
    ck_assert_str_eq (child->parent, "Archives");
    ck_assert_str_eq (child->command, "tar caf x.tgz %s");

    top = g_ptr_array_index (entries, 2);
    ck_assert_ptr_eq (top->parent, NULL);

    g_ptr_array_free (entries, TRUE);
    unlink (file);
    g_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_entries_are_read_in_the_order_of_the_file);
    tcase_add_test (tc_core, test_what_is_written_is_read_back);
    tcase_add_test (tc_core, test_only_the_level_of_the_file_is_written);
    tcase_add_test (tc_core, test_a_key_of_a_later_version_is_kept);
    tcase_add_test (tc_core, test_a_file_that_is_not_there_gives_no_entries);
    tcase_add_test (tc_core, test_a_file_that_cannot_be_read_is_not_written_over);
    tcase_add_test (tc_core, test_masks_and_types_match);
    tcase_add_test (tc_core, test_a_type_that_is_not_there_is_reported);
    tcase_add_test (tc_core, test_conditions_are_written_and_read_back);
    tcase_add_test (tc_core, test_a_flag_that_is_not_true_or_false_is_reported);
    tcase_add_test (tc_core, test_a_command_of_several_lines_is_a_block);
    tcase_add_test (tc_core, test_a_file_without_the_format_line_is_converted);
    tcase_add_test (tc_core, test_comments_and_what_did_not_change_stay_as_written);
    tcase_add_test (tc_core, test_an_error_names_the_line);
    tcase_add_test (tc_core, test_a_regex_becomes_masks_where_it_can);
    tcase_add_test (tc_core, test_conditions_of_a_menu_written_by_hand_become_keys);
    tcase_add_test (tc_core, test_regular_expressions_of_names);
    tcase_add_test (tc_core, test_conditions_are_imported_as_paths);
    tcase_add_test (tc_core, test_default_conditions_are_written_and_read_back);
    tcase_add_test (tc_core, test_a_menu_written_by_hand_is_imported);
    tcase_add_test (tc_core, test_a_submenu_and_its_entries_are_written_and_read_back);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
