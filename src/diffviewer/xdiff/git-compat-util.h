/*
   What the xdiff files of git take from git-compat-util.h, written for the diff viewer.

   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MC__DIFFVIEW_XDIFF_GIT_COMPAT_UTIL_H
#define MC__DIFFVIEW_XDIFF_GIT_COMPAT_UTIL_H

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <regex.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

/* xemit.h declares xdl_emit_diff a second time, after xdiffi.h; this header comes
   first, so the build with -Werror keeps every other warning */
#pragma GCC diagnostic ignored "-Wredundant-decls"

#define UNUSED G_GNUC_UNUSED

#define BUG(...) g_error (__VA_ARGS__)

#define bitsizeof(x) (CHAR_BIT * sizeof (x))
#define maximum_signed_value_of_type(a) (INTMAX_MAX >> (bitsizeof (intmax_t) - bitsizeof (a)))
/* for a and b not negative, as git uses it */
#define signed_add_overflows(a, b) ((b) > maximum_signed_value_of_type (a) - (a))

/* Not g_malloc(): git's allocators die when memory runs out, these return NULL, which
   every caller in xdiff checks, so xdl_diff() fails and mc goes on.  A zero size still
   gives memory, as in git.  xdl_free() is free(). */
static inline void *
xmalloc (size_t size)
{
    return malloc (size != 0 ? size : 1);
}

static inline void *
xcalloc (size_t nmemb, size_t size)
{
    return calloc (nmemb != 0 ? nmemb : 1, size != 0 ? size : 1);
}

static inline void *
xrealloc (void *ptr, size_t size)
{
    return realloc (ptr, size != 0 ? size : 1);
}

/* the diff viewer passes no -I regular expressions */
static inline int
regexec_buf (const regex_t *preg, const char *buf, size_t size, size_t nmatch,
             regmatch_t pmatch[], int eflags)
{
    (void) preg;
    (void) buf;
    (void) size;
    (void) nmatch;
    (void) pmatch;
    (void) eflags;
    return REG_NOMATCH;
}

#endif
