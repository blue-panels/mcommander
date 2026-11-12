/** \file dup_filter.h
 *  \brief Header: which files a search for duplicates looks at
 */

#ifndef MC__PANEL_PLUGIN_DUP_FILTER_H
#define MC__PANEL_PLUGIN_DUP_FILTER_H

#include <time.h>

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

typedef struct dup_name_filter_s dup_name_filter_t;

/*** declarations of public functions ************************************************************/

/* @patterns: masks separated by ';' or ',', or one regular expression.  NULL with @error set when a
   pattern is malformed; an empty @patterns takes every name. */
dup_name_filter_t *dup_name_filter_new (const char *patterns, gboolean shell_patterns,
                                        gboolean case_sens, GError **error);
gboolean dup_name_filter_match (const dup_name_filter_t *filter, const char *name);
void dup_name_filter_free (dup_name_filter_t *filter);

/* A number with K, M, G or T as in the rest of mc: "10K" and "10KiB" are powers of 1024,
   "10KB" of 1000.  An empty text is no limit (0). */
gboolean dup_parse_size (const char *text, off_t *size);

/* A date "2024-01-01", or a time back from @now: "30d", "2w", "3m", "1y".  An empty text is no
   limit (0).  @end_of_day: a date is the end of that day, for the upper bound. */
gboolean dup_parse_date (const char *text, gboolean end_of_day, time_t now, time_t *t);

/* @ignore: names and absolute paths, as split from "a:b:/c" */
gboolean dup_dir_ignored (char *const *ignore, const char *path, const char *name);

/*** inline functions ****************************************************************************/

#endif
