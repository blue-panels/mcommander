/*
   Search text engine for the M-Commander
   A search of plain bytes in data read by blocks

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
#include <string.h>  // memchr(), memcmp()

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/util.h"  // unix_error_string()
#include "lib/search.h"

#include "internal.h"

/*** file scope macro definitions ****************************************************************/

// the longest of the UTF-8 sequences that fold to an ASCII letter
#define FOLD_LEN 3

/*** file scope type declarations ****************************************************************/

struct mc_search_plain
{
    GString *needle;
    gsize *fail;
    gboolean caseless;
    gboolean fold;  // the needle can also match a non-ASCII character
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** The first @c in [@p, @end), or @end if there is none. */
static inline const char *
next_byte (const char *p, const char *end, char c)
{
    const char *q;

    q = memchr (p, c, (size_t) (end - p));
    return q != NULL ? q : end;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Find @needle in @hay, like memmem() (which isn't available everywhere). While nothing is matched
 * the first byte is looked for with memchr(), in both cases if @caseless (then @needle must be in
 * lower case); after that it is a KMP scan by the failure table @fail, so a needle that keeps
 * almost matching costs one pass over @hay.
 */
static const char *
find_bytes (const char *hay, gsize hay_len, const char *needle, gsize needle_len, const gsize *fail,
            gboolean caseless)
{
    const char *const end = hay + hay_len;
    const char c_lower = needle[0];
    const char c_upper = caseless ? g_ascii_toupper (c_lower) : c_lower;
    const char *p = hay;
    const char *lower, *upper;
    gsize state = 0;

    lower = next_byte (p, end, c_lower);
    upper = c_upper != c_lower ? next_byte (p, end, c_upper) : end;

    while (TRUE)
    {
        if (state == 0)
        {
            if (lower < p)
                lower = next_byte (p, end, c_lower);
            if (upper < p)
                upper = next_byte (p, end, c_upper);

            p = MIN (lower, upper);
            if (p == end)
                return NULL;

            state = 1;
            p++;
        }
        else
        {
            char c;

            if (p == end)
                return NULL;

            c = caseless ? g_ascii_tolower (*p) : *p;
            while (state != 0 && c != needle[state])
                state = fail[state - 1];
            if (c == needle[state])
                state++;
            p++;
        }

        if (state == needle_len)
            return p - needle_len;
    }
}

/* --------------------------------------------------------------------------------------------- */

/** The KMP failure table of @needle: the longest proper border of every prefix. */
static gsize *
failure_table (const GString *needle)
{
    gsize *fail;
    gsize i, k = 0;

    fail = g_new0 (gsize, needle->len);

    for (i = 1; i < needle->len; i++)
    {
        while (k != 0 && needle->str[i] != needle->str[k])
            k = fail[k - 1];
        if (needle->str[i] == needle->str[k])
            k++;
        fail[i] = k;
    }

    return fail;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Whether @needle can also match a non-ASCII character: in UTF-8 the regex search folds case by
 * Unicode (see mc_search__cond_struct_new_init_regex()), and U+212A folds to k, U+017F to s.
 */
static gboolean
may_fold (const mc_search_t *lc_mc_search, const GString *needle, gboolean caseless)
{
    return caseless && mc_global.utf8_display && str_isutf8 (lc_mc_search->original.charset)
        && (memchr (needle->str, 'k', needle->len) != NULL
            || memchr (needle->str, 's', needle->len) != NULL);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
has_fold (const char *hay, gsize hay_len)
{
    static const gsize no_border[FOLD_LEN] = { 0, 0, 0 };

    return find_bytes (hay, hay_len, "\xe2\x84\xaa", 3, no_border, FALSE) != NULL
        || find_bytes (hay, hay_len, "\xc5\xbf", 2, no_border, FALSE) != NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Whether case folding of @str is that of ASCII. Where the regex search does not fold by Unicode
 * it goes by the rules of the locale: in a Turkish one I goes with a dotless i.
 */
static gboolean
ascii_case_rules (const char *charset, const GString *str)
{
    GString *lower, *upper;
    char *ascii_lower, *ascii_upper;
    gboolean same;

    lower = mc_search__tolower_case_str (charset, str);
    upper = mc_search__toupper_case_str (charset, str);
    ascii_lower = g_ascii_strdown (str->str, (gssize) str->len);
    ascii_upper = g_ascii_strup (str->str, (gssize) str->len);

    same = strcmp (lower->str, ascii_lower) == 0 && strcmp (upper->str, ascii_upper) == 0;

    g_string_free (lower, TRUE);
    g_string_free (upper, TRUE);
    g_free (ascii_lower);
    g_free (ascii_upper);

    return same;
}

/* --------------------------------------------------------------------------------------------- */

static struct mc_search_plain *
plain_new (const mc_search_t *lc_mc_search)
{
    struct mc_search_plain *plain;
    gboolean caseless;
    GString *needle;

    needle = mc_search__plain_needle (lc_mc_search, &caseless);
    if (needle == NULL)
        return NULL;

    plain = g_new (struct mc_search_plain, 1);
    plain->needle = needle;
    plain->fail = failure_table (needle);
    plain->caseless = caseless;
    plain->fold = may_fold (lc_mc_search, needle, caseless);

    return plain;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * The bytes a plain search looks for, NULL when this search is not a matter of plain bytes.
 *
 * The search string is in the charset of the data, so a case sensitive string is a byte match. A
 * case insensitive one is a byte match for ASCII letters only. A string with a newline stays with
 * the regex search, which works on lines. Hex numbers are always matched exactly.
 *
 * @param lc_mc_search the search
 * @param caseless     set to TRUE if ASCII letters must be matched case insensitively
 */
GString *
mc_search__plain_needle (const mc_search_t *lc_mc_search, gboolean *caseless)
{
    const GString *str = lc_mc_search->original.str;
    GString *needle;
    gsize i;

    *caseless = FALSE;

    if (lc_mc_search->is_all_charsets || lc_mc_search->whole_words || str == NULL || str->len == 0)
        return NULL;

    if (lc_mc_search->search_type == MC_SEARCH_T_HEX)
        return mc_search__hex_numbers_to_bytes (str);

    if (lc_mc_search->search_type != MC_SEARCH_T_NORMAL)
        return NULL;

    for (i = 0; i < str->len; i++)
    {
        if (str->str[i] == '\n')
            return NULL;
        if ((unsigned char) str->str[i] >= 0x80 && !lc_mc_search->is_case_sensitive)
            return NULL;
    }

    if (!lc_mc_search->is_case_sensitive
        && !(str_isutf8 (lc_mc_search->original.charset) && mc_global.utf8_display)
        && !ascii_case_rules (lc_mc_search->original.charset, str))
        return NULL;

    *caseless = !lc_mc_search->is_case_sensitive;
    needle = mc_g_string_dup (str);
    if (*caseless)
        g_string_ascii_down (needle);

    return needle;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_search__plain_free (struct mc_search_plain *plain)
{
    if (plain == NULL)
        return;

    g_string_free (plain->needle, TRUE);
    g_free (plain->fail);
    g_free (plain);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Search the bytes got from block_fn. Like in the regex search, the byte at @end_search is still
 * in the range.
 *
 * Data with a character that the needle can match only by folding is left to the regex search.
 *
 * @param start_search where to start; set to where the regex search has to go on from
 * @param finished     set to TRUE if the result stands, FALSE if the regex search has to run
 * @return whether a match is found
 */
gboolean
mc_search__run_plain (mc_search_t *lc_mc_search, const void *user_data, off_t *start_search,
                      off_t end_search, gsize *found_len, gboolean *finished)
{
    const struct mc_search_plain *plain;
    const char *needle;
    gsize needle_len, keep_len, keep = 0;
    char *buf;
    off_t pos = *start_search;
    off_t limit, buf_size;
    gboolean found = FALSE;
    mc_search_error_t error = MC_SEARCH_E_NOTFOUND;
    int read_errno = 0;

    if (!lc_mc_search->prepared.plain_tried)
    {
        lc_mc_search->prepared.plain_tried = TRUE;
        lc_mc_search->prepared.plain = plain_new (lc_mc_search);
    }

    plain = lc_mc_search->prepared.plain;
    if (plain == NULL)
        return FALSE;

    needle = plain->needle->str;
    needle_len = plain->needle->len;
    /* A character split by the end of a chunk is not seen yet, and what the needle matched before
       it has to be there when the regex search goes on from the kept bytes. */
    keep_len = needle_len - 1 + (plain->fold ? FOLD_LEN - 1 : 0);
    limit = end_search + 1;
    buf_size = MIN (limit - pos, (off_t) MC_SEARCH__PLAIN_CHUNK);
    buf = g_malloc ((gsize) MAX (buf_size, 0) + keep_len + 1);

    *finished = TRUE;

    while (pos < limit)
    {
        const size_t want = (size_t) MIN ((off_t) MC_SEARCH__PLAIN_CHUNK, limit - pos);
        const ssize_t got = lc_mc_search->block_fn (user_data, pos, buf + keep, want);
        const char *hit;
        gsize total;

        if (got == -1)
        {
            read_errno = errno;
            error = MC_SEARCH_E_INPUT;
            break;
        }
        if (got == 0)
            break;

        total = keep + (gsize) got;

        if (plain->fold && has_fold (buf, total))
        {
            *start_search = pos - (off_t) keep;
            *finished = FALSE;
            break;
        }

        hit = find_bytes (buf, total, needle, needle_len, plain->fail, plain->caseless);
        if (hit != NULL)
        {
            lc_mc_search->normal_offset = pos - (off_t) keep + (hit - buf);
            lc_mc_search->start_buffer = lc_mc_search->normal_offset;
            if (found_len != NULL)
                *found_len = needle_len;
            found = TRUE;
            break;
        }

        pos += got;
        keep = MIN (total, keep_len);
        memmove (buf, buf + total - keep, keep);

        if (lc_mc_search->update_fn != NULL
            && lc_mc_search->update_fn (user_data, pos) == MC_SEARCH_CB_ABORT)
        {
            error = MC_SEARCH_E_ABORT;
            break;
        }
    }

    g_free (buf);

    if (!*finished || found)
        return found;

    if (error == MC_SEARCH_E_NOTFOUND)
        mc_search_set_error (lc_mc_search, MC_SEARCH_E_NOTFOUND, "%s", _ (STR_E_NOTFOUND));
    else if (error == MC_SEARCH_E_ABORT)
        mc_search_set_error (lc_mc_search, MC_SEARCH_E_ABORT, NULL);
    else
        mc_search_set_error (lc_mc_search, error, "%s", unix_error_string (read_errno));

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
