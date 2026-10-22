/*
   Panel plugin gh for the M-Commander
   Running gh, reading its rows, and the settings of the plugin

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

#include <config.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/glibcompat.h"
#include "lib/mcconfig.h"
#include "lib/plugin-prefs.h"

#include "src/panel-plugins/gh/gh-internal.h"

/*** file scope functions ************************************************************************/

/* The command line of gh: the binary of the settings, then @args. No shell
   sees it, so a title or a branch name goes through as it is. */
static char **
gh_build_argv (const gh_data_t *data, const char *const *args)
{
    GPtrArray *argv;
    guint i;

    argv = g_ptr_array_new ();
    g_ptr_array_add (argv, g_strdup (data->gh_cmd != NULL ? data->gh_cmd : "gh"));
    for (i = 0; args[i] != NULL; i++)
        g_ptr_array_add (argv, g_strdup (args[i]));
    g_ptr_array_add (argv, NULL);

    return (char **) g_ptr_array_free (argv, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* gh asks nothing and paints nothing: its output is read, not shown. */
static char **
gh_build_envp (void)
{
    char **envp;

    envp = g_get_environ ();
    envp = g_environ_setenv (envp, "GH_PROMPT_DISABLED", "1", TRUE);
    envp = g_environ_setenv (envp, "GH_NO_UPDATE_NOTIFIER", "1", TRUE);
    envp = g_environ_setenv (envp, "NO_COLOR", "1", TRUE);
    envp = g_environ_setenv (envp, "GH_PAGER", "cat", TRUE);
    return envp;
}

/* --------------------------------------------------------------------------------------------- */

/* Undo what jq's @tsv does to a field: \t, \n, \r and \\ come back as the
   characters. A carriage return is dropped, GitHub sends CRLF bodies. */
static char *
gh_unescape_field (const char *s, gsize len)
{
    GString *out;
    gsize i;

    out = g_string_sized_new (len);
    for (i = 0; i < len; i++)
    {
        if (s[i] == '\\' && i + 1 < len)
        {
            i++;
            switch (s[i])
            {
            case 't':
                g_string_append_c (out, '\t');
                break;
            case 'n':
                g_string_append_c (out, '\n');
                break;
            case 'r':
                break;
            default:
                g_string_append_c (out, s[i]);
                break;
            }
        }
        else if (s[i] != '\r')
            g_string_append_c (out, s[i]);
    }

    return g_string_free (out, FALSE);
}

/*** public functions ****************************************************************************/

/* Run gh with @args in @cwd (NULL: where mc is). @output gets its standard
   output, @err_text what it said on the standard error, NULL when it said
   nothing there. A gh that exits with
   an error fails, unless @any_status and it printed something: "gh pr checks"
   exits with an error when a check failed or is still running. */
gboolean
gh_run (const gh_data_t *data, const char *cwd, const char *const *args, gboolean any_status,
        char **output, char **err_text)
{
    char **argv;
    char **envp;
    char *std_out = NULL;
    char *std_err = NULL;
    int status = 0;
    GError *error = NULL;
    gboolean ok;

    if (output != NULL)
        *output = NULL;
    if (err_text != NULL)
        *err_text = NULL;

    argv = gh_build_argv (data, args);
    envp = gh_build_envp ();
    ok = g_spawn_sync (cwd, argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, &std_out, &std_err,
                       &status, &error);
    g_strfreev (envp);
    g_strfreev (argv);

    if (!ok)
    {
        if (err_text != NULL)
            *err_text = g_strdup_printf (_ ("Cannot run %s: %s"),
                                         data->gh_cmd != NULL ? data->gh_cmd : "gh",
                                         error != NULL ? error->message : "");
        g_clear_error (&error);
        return FALSE;
    }

#if GLIB_CHECK_VERSION(2, 70, 0)
    ok = g_spawn_check_wait_status (status, NULL);
#else
    ok = g_spawn_check_exit_status (status, NULL);
#endif
    if (!ok && any_status && std_out != NULL && std_out[0] != '\0')
        ok = TRUE;

    if (ok && output != NULL)
        *output = std_out;
    else
        g_free (std_out);

    if (err_text != NULL && std_err != NULL && g_strstrip (std_err)[0] != '\0')
        *err_text = std_err;
    else
        g_free (std_err);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Run gh with @args and write what it prints into @fd, byte for byte: the
   contents of a file may be anything, a NUL too, which a string would cut. */
gboolean
gh_run_to_fd (const gh_data_t *data, const char *const *args, int fd)
{
    char **argv;
    char **envp;
    GError *error = NULL;
    GPid pid;
    int out_fd = -1;
    int status = 0;
    gboolean ok;

    argv = gh_build_argv (data, args);
    envp = gh_build_envp ();
    ok = g_spawn_async_with_pipes (NULL, argv, envp,
                                   G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD
                                       | G_SPAWN_STDERR_TO_DEV_NULL,
                                   NULL, NULL, &pid, NULL, &out_fd, NULL, &error);
    g_strfreev (envp);
    g_strfreev (argv);
    if (!ok)
    {
        g_clear_error (&error);
        return FALSE;
    }

    while (ok)
    {
        char buf[16 * 1024];
        ssize_t got;
        const char *p = buf;

        got = read (out_fd, buf, sizeof (buf));
        if (got == 0)
            break;
        if (got < 0)
        {
            if (errno != EINTR)
                ok = FALSE;
            continue;
        }

        while (got > 0)
        {
            ssize_t written = write (fd, p, (size_t) got);

            if (written < 0 && errno == EINTR)
                continue;
            if (written <= 0)
            {
                ok = FALSE;
                break;
            }
            p += written;
            got -= written;
        }
    }
    close (out_fd);

    while (waitpid ((pid_t) pid, &status, 0) == -1 && errno == EINTR)
        ;
    g_spawn_close_pid (pid);

#if GLIB_CHECK_VERSION(2, 70, 0)
    return ok && g_spawn_check_wait_status (status, NULL);
#else
    return ok && g_spawn_check_exit_status (status, NULL);
#endif
}

/* --------------------------------------------------------------------------------------------- */

/* The first line gh prints, NULL when it fails or prints nothing. */
char *
gh_run_line (const gh_data_t *data, const char *cwd, const char *const *args)
{
    char *output = NULL;
    char *nl;

    if (!gh_run (data, cwd, args, FALSE, &output, NULL) || output == NULL)
    {
        g_free (output);
        return NULL;
    }

    nl = strchr (output, '\n');
    if (nl != NULL)
        *nl = '\0';
    if (output[0] == '\0')
    {
        g_free (output);
        return NULL;
    }
    return output;
}

/* --------------------------------------------------------------------------------------------- */

/* The fields of one row of @tsv, unescaped; NULL-terminated. */
char **
gh_split_row (const char *line, guint *count)
{
    GPtrArray *fields;
    const char *p = line;

    fields = g_ptr_array_new ();
    while (TRUE)
    {
        const char *tab = strchr (p, '\t');
        gsize len = tab != NULL ? (gsize) (tab - p) : strlen (p);

        g_ptr_array_add (fields, gh_unescape_field (p, len));
        if (tab == NULL)
            break;
        p = tab + 1;
    }

    if (count != NULL)
        *count = fields->len;
    g_ptr_array_add (fields, NULL);
    return (char **) g_ptr_array_free (fields, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* 2026-10-20T18:34:36Z to a time; 0 when it is no such time. */
time_t
gh_parse_time (const char *iso)
{
    GDateTime *dt;
    time_t t;

    if (iso == NULL || iso[0] == '\0')
        return 0;

    dt = g_date_time_new_from_iso8601 (iso, NULL);
    if (dt == NULL)
        return 0;
    t = (time_t) g_date_time_to_unix (dt);
    g_date_time_unref (dt);
    return t;
}

/* --------------------------------------------------------------------------------------------- */

char *
gh_format_time (time_t t)
{
    GDateTime *dt;
    char *s;

    if (t == 0)
        return g_strdup ("");

    dt = g_date_time_new_from_unix_local ((gint64) t);
    if (dt == NULL)
        return g_strdup ("");
    s = g_date_time_format (dt, "%Y-%m-%d %H:%M");
    g_date_time_unref (dt);
    return s;
}

/* --------------------------------------------------------------------------------------------- */

/* "433 Fix the wheel" for the panel: a slash would make a directory of it,
   a control character would break the line, and a long title is cut. @head
   is the number, or the start of the hash of a commit. */
char *
gh_entry_label (const char *head, const char *title)
{
    GString *name;
    const char *p;
    gboolean space = FALSE;
    glong chars = 0;

    name = g_string_new (head);
    if (title == NULL)
        return g_string_free (name, FALSE);

    g_string_append_c (name, ' ');
    for (p = title; *p != '\0' && chars < 60; p = g_utf8_next_char (p))
    {
        gunichar c = g_utf8_get_char (p);

        if (c == '/' || c == '\\')
            c = '-';
        if (g_unichar_isspace (c) || g_unichar_iscntrl (c))
        {
            space = TRUE;
            continue;
        }
        if (space && chars > 0)
        {
            g_string_append_c (name, ' ');
            chars++;
        }
        space = FALSE;
        g_string_append_unichar (name, c);
        chars++;
    }

    while (name->len > 0 && name->str[name->len - 1] == ' ')
        g_string_truncate (name, name->len - 1);

    return g_string_free (name, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

char *
gh_entry_name (int number, const char *title)
{
    char head[32];

    g_snprintf (head, sizeof (head), "%d", number);
    return gh_entry_label (head, title);
}

/* --------------------------------------------------------------------------------------------- */

char *
gh_read_config_string (const char *key, const char *fallback)
{
    char *cfg_path;
    char *value;

    cfg_path = g_build_filename (mc_config_get_path (), GH_PANEL_CONFIG_FILE, (char *) NULL);
    value = mc_plugin_prefs_read_config_string (cfg_path, GH_PANEL_CONFIG_GROUP, key);
    g_free (cfg_path);

    if (value == NULL)
        return g_strdup (fallback);
    return value;
}

/* --------------------------------------------------------------------------------------------- */

int
gh_read_config_int (const char *key, int fallback)
{
    char *value;
    long n;

    value = gh_read_config_string (key, NULL);
    if (value == NULL)
        return fallback;
    n = strtol (value, NULL, 10);
    g_free (value);

    return n > 0 && n <= 1000 ? (int) n : fallback;
}

/* --------------------------------------------------------------------------------------------- */

int
gh_load_hotkey (const char *key, const char *fallback_text, int fallback_key)
{
    return mc_plugin_prefs_load_hotkey (GH_PANEL_CONFIG_FILE, GH_PANEL_CONFIG_GROUP, key,
                                        fallback_text, fallback_key, NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* A place as the panel keeps it, owner[/repo[/pulls|issues[/<number>]]], from
   what the user typed or pasted: that, gh:/..., or the address of the page on
   GitHub (https://github.com/owner/repo/pull/433). NULL when it names none. */
char *
gh_normalize_location (const char *text)
{
    char *s;
    char *p;
    char *end;
    char **segs;
    GPtrArray *out;
    guint n, i;

    s = g_strstrip (g_strdup (text));
    p = s;

    if (g_str_has_prefix (p, "gh:"))
        p += 3;
    else
    {
        char *scheme = strstr (p, "://");

        /* the host goes, github.com or an enterprise one */
        if (scheme != NULL)
        {
            p = strchr (scheme + 3, '/');
            if (p == NULL)
                p = s + strlen (s);
        }
        else if (g_str_has_prefix (p, "github.com/"))
            p += strlen ("github.com");
    }

    end = strpbrk (p, "?#");
    if (end != NULL)
        *end = '\0';

    segs = g_strsplit (p, "/", -1);
    g_free (s);

    out = g_ptr_array_new ();
    n = g_strv_length (segs);
    for (i = 0; i < n; i++)
        if (segs[i][0] != '\0')
            g_ptr_array_add (out, segs[i]);

    /* owner/repo/pulls|issues/<number>, the rest of a page address dropped */
    if (out->len >= 2 && g_str_has_suffix ((char *) g_ptr_array_index (out, 1), ".git"))
    {
        char *repo = (char *) g_ptr_array_index (out, 1);

        repo[strlen (repo) - 4] = '\0';
    }
    if (out->len >= 3)
    {
        const char *section = (const char *) g_ptr_array_index (out, 2);

        if (strcmp (section, "pull") == 0 || strcmp (section, "pulls") == 0)
            g_ptr_array_index (out, 2) = (gpointer) "pulls";
        else if (strcmp (section, "issues") != 0)
            g_ptr_array_set_size (out, 2);
    }
    if (out->len >= 4)
    {
        const char *number = (const char *) g_ptr_array_index (out, 3);

        g_ptr_array_set_size (out,
                              number[0] >= '1' && number[0] <= '9'
                                      && strspn (number, "0123456789") == strlen (number)
                                  ? 4
                                  : 3);
    }

    for (i = 0; i < out->len; i++)
        if (strpbrk ((const char *) g_ptr_array_index (out, i), ": \t") != NULL
            || ((const char *) g_ptr_array_index (out, i))[0] == '\0')
            break;

    s = NULL;
    if (out->len > 0 && i == out->len)
    {
        g_ptr_array_add (out, NULL);
        s = g_strjoinv ("/", (char **) out->pdata);
    }
    g_ptr_array_free (out, TRUE);
    g_strfreev (segs);

    return s;
}

/* --------------------------------------------------------------------------------------------- */

GPtrArray *
gh_favorites_load (void)
{
    char *cfg_path;
    mc_config_t *cfg;
    gchar **list;
    gsize len = 0;
    GPtrArray *favs;
    gsize i;

    cfg_path = g_build_filename (mc_config_get_path (), GH_PANEL_CONFIG_FILE, (char *) NULL);
    cfg = mc_config_init (cfg_path, TRUE);
    g_free (cfg_path);

    favs = g_ptr_array_new_with_free_func (g_free);
    list =
        mc_config_get_string_list (cfg, GH_FAVORITES_CONFIG_GROUP, GH_FAVORITES_CONFIG_KEY, &len);
    if (list != NULL)
    {
        for (i = 0; i < len; i++)
            if (list[i] != NULL && list[i][0] != '\0')
            {
                /* a page address saved before it was understood counts too */
                char *location = gh_normalize_location (list[i]);
                guint j;

                for (j = 0; location != NULL && j < favs->len; j++)
                    if (strcmp ((const char *) g_ptr_array_index (favs, j), location) == 0)
                        g_clear_pointer (&location, g_free);
                if (location != NULL)
                    g_ptr_array_add (favs, location);
            }
        g_strfreev (list);
    }
    mc_config_deinit (cfg);
    return favs;
}

/* --------------------------------------------------------------------------------------------- */

void
gh_favorites_save (GPtrArray *favs)
{
    char *cfg_path;
    mc_config_t *cfg;

    cfg_path = g_build_filename (mc_config_get_path (), GH_PANEL_CONFIG_FILE, (char *) NULL);
    cfg = mc_config_init (cfg_path, FALSE);
    g_free (cfg_path);

    if (favs->len == 0)
        mc_config_del_key (cfg, GH_FAVORITES_CONFIG_GROUP, GH_FAVORITES_CONFIG_KEY);
    else
        mc_config_set_string_list (cfg, GH_FAVORITES_CONFIG_GROUP, GH_FAVORITES_CONFIG_KEY,
                                   (const gchar **) favs->pdata, favs->len);
    mc_config_save_file (cfg, NULL);
    mc_config_deinit (cfg);
}

/* --------------------------------------------------------------------------------------------- */

/* The local clone of owner/repo the panel saw last, NULL when it never saw one
   or it is gone. */
/* GitHub takes owner and repository in any case: one key for all of them */
static char *
gh_clone_key (const char *owner, const char *repo)
{
    char *key;
    char *lower;

    key = g_strdup_printf ("%s/%s", owner, repo);
    lower = g_ascii_strdown (key, -1);
    g_free (key);
    return lower;
}

/* --------------------------------------------------------------------------------------------- */

char *
gh_clone_load (const char *owner, const char *repo)
{
    char *cfg_path;
    char *key;
    char *dir;

    cfg_path = g_build_filename (mc_config_get_path (), GH_PANEL_CONFIG_FILE, (char *) NULL);
    key = gh_clone_key (owner, repo);
    dir = mc_plugin_prefs_read_config_string (cfg_path, GH_CLONES_CONFIG_GROUP, key);
    g_free (key);
    g_free (cfg_path);

    if (dir != NULL && !g_file_test (dir, G_FILE_TEST_IS_DIR))
        g_clear_pointer (&dir, g_free);
    return dir;
}

/* --------------------------------------------------------------------------------------------- */

void
gh_clone_save (const char *owner, const char *repo, const char *dir)
{
    char *cfg_path;
    char *key;
    char *old;
    mc_config_t *cfg;

    old = gh_clone_load (owner, repo);
    if (old != NULL && strcmp (old, dir) == 0)
    {
        g_free (old);
        return;
    }
    g_free (old);

    cfg_path = g_build_filename (mc_config_get_path (), GH_PANEL_CONFIG_FILE, (char *) NULL);
    cfg = mc_config_init (cfg_path, FALSE);
    g_free (cfg_path);

    key = gh_clone_key (owner, repo);
    mc_config_set_string (cfg, GH_CLONES_CONFIG_GROUP, key, dir);
    g_free (key);
    mc_config_save_file (cfg, NULL);
    mc_config_deinit (cfg);
}

/* --------------------------------------------------------------------------------------------- */

/* The top of the work tree @dir is in, NULL when it is in none. */
char *
gh_git_toplevel (const char *dir)
{
    const char *argv[] = { "git", "rev-parse", "--show-toplevel", NULL };
    char *out = NULL;
    int status = 0;

    if (!g_spawn_sync (dir, (char **) argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
                       NULL, NULL, &out, NULL, &status, NULL)
#if GLIB_CHECK_VERSION(2, 70, 0)
        || !g_spawn_check_wait_status (status, NULL)
#else
        || !g_spawn_check_exit_status (status, NULL)
#endif
        || out == NULL || g_strstrip (out)[0] == '\0')
    {
        g_free (out);
        return NULL;
    }
    return out;
}

/* --------------------------------------------------------------------------------------------- */

/* The value of --state of gh pr list and gh issue list */
const char *
gh_state_filter_name (gh_state_filter_t state)
{
    switch (state)
    {
    case GH_STATE_CLOSED:
        return "closed";
    case GH_STATE_MERGED:
        return "merged";
    case GH_STATE_ALL:
        return "all";
    default:
        return "open";
    }
}

/* --------------------------------------------------------------------------------------------- */

/* "gh ... --web" starts the browser and returns, so it is waited for like any
   other gh: nothing is left for mc to reap. */
gboolean
gh_open_web (const gh_data_t *data, const char *const *args)
{
    char *err_text = NULL;
    gboolean ok;

    ok = gh_run (data, NULL, args, FALSE, NULL, &err_text);
    if (!ok)
        gh_error (data,
                  err_text != NULL && err_text[0] != '\0' ? err_text
                                                          : _ ("Cannot open the browser"));
    g_free (err_text);
    return ok;
}
