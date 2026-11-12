/*
   Panel plugin duplicates for the M-Commander
   The dialog that starts a search for duplicates

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

/** \file dup_dialog.c
 *  \brief Source: the dialog that starts a search for duplicates
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lib/global.h"
#include "lib/mcconfig.h"
#include "lib/strutil.h"  // str_trunc()
#include "lib/util.h"
#include "lib/widget.h"

#include "dup_dialog.h"

/*** file scope macro definitions ****************************************************************/

#define CONFIG_GROUP "Duplicates"

/*** file scope type declarations ****************************************************************/

/* What the user typed and ticked, kept between searches */
typedef struct
{
    char *start;
    gboolean other_panel;
    gboolean other_reference;
    gboolean ignore_enable;
    char *ignore;
    char *name;
    gboolean shell_patterns;
    gboolean case_sens;
    gboolean skip_hidden;
    char *size_min;
    char *size_max;
    char *mtime_from;
    char *mtime_to;
    gboolean same_size;
    gboolean same_content;
    gboolean same_name;
    gboolean name_case;
    gboolean similar;
    int method;  // dup_similar_method_t
    char *similarity;
    gboolean recursive;
    gboolean follow_symlinks;
} dup_options_t;

/*** file scope variables ************************************************************************/

static dup_options_t options;
static gboolean options_loaded = FALSE;

static unsigned long same_size_id, same_name_id, name_case_id, same_content_id;
static unsigned long similar_id, method_id, similarity_id;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static char *
get_string (const char *key, const char *def)
{
    return mc_config_get_string (mc_global.main_config, CONFIG_GROUP, key, def);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
get_bool (const char *key, gboolean def)
{
    return mc_config_get_bool (mc_global.main_config, CONFIG_GROUP, key, def);
}

/* --------------------------------------------------------------------------------------------- */

static void
options_load (void)
{
    if (options_loaded)
        return;

    options.start = get_string ("start", ".");
    options.other_panel = get_bool ("other_panel", FALSE);
    options.other_reference = get_bool ("other_reference", TRUE);
    options.ignore_enable = get_bool ("ignore_dirs_enable", TRUE);
    options.ignore = get_string ("ignore_dirs", ".git:.svn:.hg");
    options.name = get_string ("name", "");
    options.shell_patterns = get_bool ("shell_patterns", TRUE);
    options.case_sens = get_bool ("case_sens", FALSE);
    options.skip_hidden = get_bool ("skip_hidden", FALSE);
    options.size_min = get_string ("size_min", "");
    options.size_max = get_string ("size_max", "");
    options.mtime_from = get_string ("mtime_from", "");
    options.mtime_to = get_string ("mtime_to", "");
    options.same_size = get_bool ("same_size", TRUE);
    options.same_content = get_bool ("same_content", TRUE);
    options.same_name = get_bool ("same_name", FALSE);
    options.name_case = get_bool ("name_case", FALSE);
    options.similar = get_bool ("similar", FALSE);
    options.method = mc_config_get_int (mc_global.main_config, CONFIG_GROUP, "similar_method",
                                        DUP_SIMILAR_BY_TYPE);
    options.similarity = get_string ("similarity", "90");
    options.recursive = get_bool ("recursive", TRUE);
    options.follow_symlinks = get_bool ("follow_symlinks", FALSE);
    options_loaded = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
options_save (void)
{
    mc_config_t *c = mc_global.main_config;

    mc_config_set_string (c, CONFIG_GROUP, "start", options.start);
    mc_config_set_bool (c, CONFIG_GROUP, "other_panel", options.other_panel);
    mc_config_set_bool (c, CONFIG_GROUP, "other_reference", options.other_reference);
    mc_config_set_bool (c, CONFIG_GROUP, "ignore_dirs_enable", options.ignore_enable);
    mc_config_set_string (c, CONFIG_GROUP, "ignore_dirs", options.ignore);
    mc_config_set_string (c, CONFIG_GROUP, "name", options.name);
    mc_config_set_bool (c, CONFIG_GROUP, "shell_patterns", options.shell_patterns);
    mc_config_set_bool (c, CONFIG_GROUP, "case_sens", options.case_sens);
    mc_config_set_bool (c, CONFIG_GROUP, "skip_hidden", options.skip_hidden);
    mc_config_set_string (c, CONFIG_GROUP, "size_min", options.size_min);
    mc_config_set_string (c, CONFIG_GROUP, "size_max", options.size_max);
    mc_config_set_string (c, CONFIG_GROUP, "mtime_from", options.mtime_from);
    mc_config_set_string (c, CONFIG_GROUP, "mtime_to", options.mtime_to);
    mc_config_set_bool (c, CONFIG_GROUP, "same_size", options.same_size);
    mc_config_set_bool (c, CONFIG_GROUP, "same_content", options.same_content);
    mc_config_set_bool (c, CONFIG_GROUP, "same_name", options.same_name);
    mc_config_set_bool (c, CONFIG_GROUP, "name_case", options.name_case);
    mc_config_set_bool (c, CONFIG_GROUP, "similar", options.similar);
    mc_config_set_int (c, CONFIG_GROUP, "similar_method", options.method);
    mc_config_set_string (c, CONFIG_GROUP, "similarity", options.similarity);
    mc_config_set_bool (c, CONFIG_GROUP, "recursive", options.recursive);
    mc_config_set_bool (c, CONFIG_GROUP, "follow_symlinks", options.follow_symlinks);
}

/* --------------------------------------------------------------------------------------------- */

static void
take_string (char **field, char *value)
{
    g_free (*field);
    *field = value != NULL ? value : g_strdup ("");
}

/* --------------------------------------------------------------------------------------------- */

static char *
resolve_dir (const char *panel_dir, const char *text)
{
    char *s, *dir;

    if (text == NULL || *text == '\0')
        text = ".";

    s = tilde_expand (text);
    if (g_path_is_absolute (s))
        dir = s;
    else
    {
        dir = g_build_filename (panel_dir, s, (char *) NULL);
        g_free (s);
    }
    canonicalize_pathname (dir);
    return dir;
}

/* --------------------------------------------------------------------------------------------- */

/* The directory with no link in its path: a link to the reference directory is that directory */
static char *
physical_dir (const char *dir)
{
    char *real, *copy;

    real = realpath (dir, NULL);
    copy = g_strdup (real != NULL ? real : dir);
    free (real);
    return copy;
}

/* --------------------------------------------------------------------------------------------- */

/* NULL when the options make a search, or what is wrong with them */
static char *
options_check (const char *panel_dir, const char *other_dir, dup_search_t *search)
{
    GError *error = NULL;
    time_t now;
    char *real;

    memset (search, 0, sizeof (*search));
    now = time (NULL);

    search->start_dir = resolve_dir (panel_dir, options.start);
    if (!g_file_test (search->start_dir, G_FILE_TEST_IS_DIR))
        return g_strdup_printf (_ ("No such directory: %s"), search->start_dir);
    real = physical_dir (search->start_dir);
    g_free (search->start_dir);
    search->start_dir = real;

    if (options.other_panel)
    {
        if (other_dir == NULL)
            return g_strdup (_ ("The other panel shows no local directory"));
        search->other_dir = physical_dir (other_dir);
        search->other_reference = options.other_reference;
        if (search->other_reference && dup_path_is_below (search->start_dir, search->other_dir)
            && dup_path_is_below (search->other_dir, search->start_dir))
            return g_strdup (_ ("The reference directory is the start directory"));
    }

    if (options.same_content && options.similar)
    {
        char *end;
        long percent;

        /* the similar content alone: sizes and names of similar files differ */
        search->criteria = DUPLICATES_BY_CONTENT | DUPLICATES_SIMILAR;
        search->method = (dup_similar_method_t) options.method;
        if (search->method == DUP_SIMILAR_IMAGES && !dup_images_readable ())
            return g_strdup (_ ("This build reads no images: libjpeg and libpng were not found"));
        percent = strtol (options.similarity, &end, 10);
        while (*end == ' ' || *end == '%')
            end++;
        if (search->method != DUP_SIMILAR_TEXT && (*end != '\0' || percent < 1 || percent > 100))
            return g_strdup (_ ("The similarity is a percent from 1 to 100"));
        search->similarity = (int) percent;
    }
    else
    {
        if (!options.same_size && !options.same_content && !options.same_name)
            return g_strdup (_ ("Choose what the duplicates have in common"));
        if (options.same_size)
            search->criteria |= DUPLICATES_BY_SIZE;
        if (options.same_content)
            search->criteria |= DUPLICATES_BY_CONTENT;
        if (options.same_name)
            search->criteria |= DUPLICATES_BY_NAME;
        if (options.name_case)
            search->criteria |= DUPLICATES_NAME_CASE_SENS;
    }

    if (!dup_parse_size (options.size_min, &search->size_min)
        || !dup_parse_size (options.size_max, &search->size_max))
        return g_strdup (_ ("Malformed size"));
    if (search->size_max != 0 && search->size_min > search->size_max)
        return g_strdup (_ ("The lower size limit is above the upper one"));

    if (!dup_parse_date (options.mtime_from, FALSE, now, &search->mtime_from)
        || !dup_parse_date (options.mtime_to, TRUE, now, &search->mtime_to))
        return g_strdup (_ ("Malformed date"));
    if (search->mtime_to != 0 && search->mtime_from > search->mtime_to)
        return g_strdup (_ ("The first date is after the last one"));

    search->names =
        dup_name_filter_new (options.name, options.shell_patterns, options.case_sens, &error);
    if (search->names == NULL)
    {
        char *msg;

        msg = g_strdup_printf (_ ("Malformed file name pattern: %s"), error->message);
        g_error_free (error);
        return msg;
    }

    if (options.ignore_enable && options.ignore != NULL && *options.ignore != '\0')
        search->ignore_dirs = g_strsplit (options.ignore, ":", -1);

    search->skip_hidden = options.skip_hidden;
    search->recursive = options.recursive;
    search->follow_symlinks = options.follow_symlinks;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
update_states (Widget *w)
{
    const gboolean content = CHECK (widget_find_by_id (w, same_content_id))->state;
    const gboolean similar = content && CHECK (widget_find_by_id (w, similar_id))->state;
    const gboolean text = RADIO (widget_find_by_id (w, method_id))->sel == DUP_SIMILAR_TEXT;

    widget_disable (widget_find_by_id (w, similar_id), !content);
    widget_disable (widget_find_by_id (w, method_id), !similar);
    widget_disable (widget_find_by_id (w, similarity_id), !similar || text);
    widget_disable (widget_find_by_id (w, same_size_id), similar);
    widget_disable (widget_find_by_id (w, same_name_id), similar);
    widget_disable (widget_find_by_id (w, name_case_id), similar);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dialog_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_INIT:
        update_states (w);
        return dlg_default_callback (w, sender, msg, parm, data);

    case MSG_NOTIFY:
        if (sender != NULL
            && (sender->id == same_content_id || sender->id == similar_id
                || sender->id == method_id))
        {
            update_states (w);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
dup_dialog_run (const char *panel_dir, const char *other_dir, dup_search_t *search)
{
    char *other_label;

    static const char *method_items[] = {
        N_ ("By &file type"),
        N_ ("Ima&ges: perceptual hash"),
        N_ ("Te&xt: ignore line endings and spaces"),
    };

    options_load ();

    other_label = other_dir != NULL
        ? g_strdup_printf (_ ("Also in the other &panel (%s)"), str_trunc (other_dir, 30))
        : g_strdup (_ ("Also in the other &panel"));

    while (TRUE)
    {
        char *start = NULL, *ignore = NULL, *name = NULL, *size_min = NULL, *size_max = NULL;
        char *mtime_from = NULL, *mtime_to = NULL, *similarity = NULL;
        char *error;
        int ret;

        {
            /* clang-format off */
            quick_widget_t quick_widgets[] = {
                QUICK_LABELED_INPUT (N_ ("Start at:"), input_label_left, options.start,
                                     "duplicates-start", &start, NULL, FALSE, FALSE,
                                     INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD),
                QUICK_CHECKBOX (other_label, &options.other_panel, NULL),
                QUICK_CHECKBOX (N_ ("    The other panel is a &reference: never move or delete there"),
                                &options.other_reference, NULL),
                QUICK_CHECKBOX (N_ ("Enable &ignore directories:"), &options.ignore_enable, NULL),
                QUICK_INPUT (options.ignore, "duplicates-ignore", &ignore, NULL, FALSE, FALSE,
                             INPUT_COMPLETE_FILENAMES),
                QUICK_SEPARATOR (TRUE),
                QUICK_LABELED_INPUT (N_ ("File mask:"), input_label_left, options.name,
                                     "duplicates-name", &name, NULL, FALSE, FALSE,
                                     INPUT_COMPLETE_FILENAMES),
                QUICK_START_COLUMNS,
                    QUICK_CHECKBOX (N_ ("&Using shell patterns"), &options.shell_patterns, NULL),
                    QUICK_CHECKBOX (N_ ("Case sensi&tive"), &options.case_sens, NULL),
                QUICK_NEXT_COLUMN,
                    QUICK_CHECKBOX (N_ ("Skip h&idden"), &options.skip_hidden, NULL),
                QUICK_STOP_COLUMNS,
                QUICK_START_COLUMNS,
                    QUICK_LABELED_INPUT (N_ ("Size from (e.g. 10M):"), input_label_left,
                                         options.size_min, "duplicates-size-min", &size_min, NULL,
                                         FALSE, FALSE, INPUT_COMPLETE_NONE),
                    QUICK_LABELED_INPUT (N_ ("Modified from (e.g. 2024-01-01 or 30d):"),
                                         input_label_above, options.mtime_from,
                                         "duplicates-mtime-from", &mtime_from, NULL, FALSE, FALSE,
                                         INPUT_COMPLETE_NONE),
                QUICK_NEXT_COLUMN,
                    QUICK_LABELED_INPUT (N_ ("to:"), input_label_left, options.size_max,
                                         "duplicates-size-max", &size_max, NULL, FALSE, FALSE,
                                         INPUT_COMPLETE_NONE),
                    QUICK_LABELED_INPUT (N_ ("to:"), input_label_above, options.mtime_to,
                                         "duplicates-mtime-to", &mtime_to, NULL, FALSE, FALSE,
                                         INPUT_COMPLETE_NONE),
                QUICK_STOP_COLUMNS,
                QUICK_SEPARATOR (TRUE),
                QUICK_START_COLUMNS,
                    QUICK_CHECKBOX (N_ ("Same si&ze"), &options.same_size, &same_size_id),
                    QUICK_CHECKBOX (N_ ("Same na&me"), &options.same_name, &same_name_id),
                QUICK_NEXT_COLUMN,
                    QUICK_CHECKBOX (N_ ("Same co&ntent"), &options.same_content,
                                    &same_content_id),
                    QUICK_CHECKBOX (N_ ("Name case sensiti&ve"), &options.name_case,
                                    &name_case_id),
                QUICK_STOP_COLUMNS,
                QUICK_CHECKBOX (N_ ("Simi&lar content, not only the same bytes"),
                                &options.similar, &similar_id),
                QUICK_START_COLUMNS,
                    QUICK_RADIO (3, method_items, &options.method, &method_id),
                QUICK_NEXT_COLUMN,
                    QUICK_LABELED_INPUT (N_ ("Similarity (e.g. 90%):"), input_label_left,
                                         options.similarity, "duplicates-similarity",
                                         &similarity, &similarity_id, FALSE, FALSE,
                                         INPUT_COMPLETE_NONE),
                QUICK_STOP_COLUMNS,
                QUICK_START_COLUMNS,
                    QUICK_CHECKBOX (N_ ("Find &recursively"), &options.recursive, NULL),
                QUICK_NEXT_COLUMN,
                    QUICK_CHECKBOX (N_ ("Follow s&ymlinks"), &options.follow_symlinks, NULL),
                QUICK_STOP_COLUMNS,
                QUICK_BUTTONS_OK_CANCEL,
                QUICK_END,
            };
            /* clang-format on */

            WRect r = { -1, -1, 0, 72 };

            quick_dialog_t qdlg = {
                .rect = r,
                .title = N_ ("Find duplicates"),
                .help = "[Find duplicates]",
                .help_file = dup_help_file (),
                .widgets = quick_widgets,
                .callback = dialog_callback,
                .mouse_callback = NULL,
            };

            ret = quick_dialog (&qdlg);
        }

        if (ret != B_ENTER)
        {
            g_free (start);
            g_free (ignore);
            g_free (name);
            g_free (size_min);
            g_free (size_max);
            g_free (mtime_from);
            g_free (mtime_to);
            g_free (similarity);
            g_free (other_label);
            return FALSE;
        }

        take_string (&options.start, start);
        take_string (&options.ignore, ignore);
        take_string (&options.name, name);
        take_string (&options.size_min, size_min);
        take_string (&options.size_max, size_max);
        take_string (&options.mtime_from, mtime_from);
        take_string (&options.mtime_to, mtime_to);
        take_string (&options.similarity, similarity);
        options_save ();

        error = options_check (panel_dir, other_dir, search);
        if (error == NULL)
            break;

        message (D_ERROR, MSG_ERROR, "%s", error);
        g_free (error);
        dup_search_clear (search);
    }

    g_free (other_label);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
dup_search_clear (dup_search_t *search)
{
    g_free (search->start_dir);
    g_free (search->other_dir);
    g_strfreev (search->ignore_dirs);
    dup_name_filter_free (search->names);
    memset (search, 0, sizeof (*search));
}

/* --------------------------------------------------------------------------------------------- */

const char *
dup_help_file (void)
{
    static char *path = NULL;

    if (path == NULL)
        path = g_build_filename (MC_PLUGIN_DIR, "duplicates_panel.md", (char *) NULL);

    return g_file_test (path, G_FILE_TEST_IS_REGULAR) ? path : NULL;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dup_path_is_below (const char *path, const char *dir)
{
    size_t len;

    len = strlen (dir);
    while (len > 1 && dir[len - 1] == PATH_SEP)
        len--;

    return strncmp (path, dir, len) == 0
        && (path[len] == '\0' || path[len] == PATH_SEP || (len == 1 && dir[0] == PATH_SEP));
}

/* --------------------------------------------------------------------------------------------- */

/* A reference directory inside the start directory keeps its files; the start directory inside
   the reference one is what is searched, so its files are not reference ones. */
gboolean
dup_search_is_reference (const dup_search_t *search, const char *path)
{
    if (search->other_dir == NULL || !search->other_reference
        || !dup_path_is_below (path, search->other_dir))
        return FALSE;

    return !(dup_path_is_below (path, search->start_dir)
             && dup_path_is_below (search->start_dir, search->other_dir));
}

/* --------------------------------------------------------------------------------------------- */
