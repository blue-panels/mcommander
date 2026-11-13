/** \file dup_dialog.h
 *  \brief Header: the dialog that starts a search for duplicates
 */

#ifndef MC__PANEL_PLUGIN_DUP_DIALOG_H
#define MC__PANEL_PLUGIN_DUP_DIALOG_H

#include <sys/types.h>
#include <time.h>

#include "lib/global.h"

#include "dup_engine.h"
#include "dup_filter.h"

/*** structures declarations (and typedefs of structures) ****************************************/

/* What the search looks at, checked and ready to use */
typedef struct
{
    char *start_dir;              // absolute
    char *other_dir;              // absolute, or NULL
    gboolean other_reference;     // the files of other_dir are never moved or deleted
    char **ignore_dirs;           // names and absolute paths, or NULL
    dup_name_filter_t *names;     // never NULL
    off_t size_min, size_max;     // 0 is no limit
    time_t mtime_from, mtime_to;  // 0 is no limit
    gboolean skip_hidden;
    gboolean recursive;
    gboolean follow_symlinks;
    duplicates_criteria_t criteria;
    dup_similar_method_t method;  // with DUPLICATES_SIMILAR
    int similarity;               // percent, for images
} dup_search_t;

/*** declarations of public functions ************************************************************/

/* @panel_dir: the directory of the panel the command runs in; @other_dir: the one of the other
   panel, NULL when it shows no local directory.  FALSE when the user cancels. */
gboolean dup_dialog_run (const char *panel_dir, const char *other_dir, dup_search_t *search);
void dup_search_clear (dup_search_t *search);

/* Is @path the directory @dir or below it */
gboolean dup_path_is_below (const char *path, const char *dir);

/* The help of the plugin, or NULL when it is not installed */
const char *dup_help_file (void);

/* Is @path a file of the reference directory of @search */
gboolean dup_search_is_reference (const dup_search_t *search, const char *path);

/*** inline functions ****************************************************************************/

#endif
