/*
   Internal file viewer for the M-Commander
   Function for search data

   Copyright (C) 1994-2025
   Free Software Foundation, Inc.
   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

   Written by:
   Miguel de Icaza, 1994, 1995, 1998
   Janne Kukonlehto, 1994, 1995
   Jakub Jelinek, 1995
   Joseph M. Hinkle, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Roland Illig <roland.illig@gmx.de>, 2004, 2005
   Slava Zanko <slavazanko@google.com>, 2009
   Andrew Borodin <aborodin@vmail.ru>, 2009-2022
   Ilia Maslakov <il.smind@gmail.com>, 2009
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the M-Commander
   a fork of GNU Midnight Commander.

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

#include <config.h>

#include <errno.h>
#include <stdio.h>   // sscanf()
#include <string.h>  // memchr(), memcmp()

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/charsets.h"  // cp_source
#include "lib/util.h"      // unix_error_string()
#include "lib/widget.h"

#include "src/setup.h"

#include "internal.h"

/*** global variables ****************************************************************************/

mcview_search_options_t mcview_search_options = {
    .type = MC_SEARCH_T_NORMAL,
    .case_sens = FALSE,
    .backwards = FALSE,
    .whole_words = FALSE,
    .all_codepages = FALSE,
};

/*** file scope macro definitions ****************************************************************/

/* The fast search reads this much at a time and looks at Esc in between: not more, on a slow VFS
   the wait for one chunk is the wait for Esc. */
#define MCVIEW_FAST_SEARCH_CHUNK (128 * 1024)

/*** file scope type declarations ****************************************************************/

typedef struct
{
    simple_status_msg_t status_msg;  // base class

    gboolean first;
    WView *view;
    off_t offset;
} mcview_search_status_msg_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static int search_cb_char_curr_index = -1;
static char search_cb_char_buffer[6];

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
mcview_search_status_update_cb (status_msg_t *sm)
{
    simple_status_msg_t *ssm = SIMPLE_STATUS_MSG (sm);
    mcview_search_status_msg_t *vsm = (mcview_search_status_msg_t *) sm;
    Widget *wd = WIDGET (sm->dlg);
    int percent = -1;

    if (verbose)
        percent = mcview_calc_percent (vsm->view, vsm->offset);

    if (percent >= 0)
        label_set_textv (ssm->label, _ ("Searching %s: %3d%%"), vsm->view->last_search_string,
                         percent);
    else
        label_set_textv (ssm->label, _ ("Searching %s"), vsm->view->last_search_string);

    if (vsm->first)
    {
        Widget *lw = WIDGET (ssm->label);
        WRect r;

        r = wd->rect;
        r.cols = MAX (r.cols, lw->rect.cols + 6);
        widget_set_size_rect (wd, &r);
        r = lw->rect;
        r.x = wd->rect.x + (wd->rect.cols - r.cols) / 2;
        widget_set_size_rect (lw, &r);
        vsm->first = FALSE;
    }

    return status_msg_common_update (sm);
}

/* --------------------------------------------------------------------------------------------- */

static inline off_t
mcview_calculate_start_of_previous_line (WView *view, const off_t current_pos)
{
    const off_t bol = mcview_bol (view, current_pos, 0);

    // Are we in the 1st line?
    if (bol == 0)
        return (-1);

    return mcview_bol (view, bol - 1, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_search_update_steps (WView *view)
{
    off_t filesize;

    filesize = mcview_get_filesize (view);

    if (filesize != 0)
        view->update_steps = filesize / 100;
    else  // viewing a data stream, not a file
        view->update_steps = 40000;

    // Do not update the percent display but every 20 kb
    if (view->update_steps < 20000)
        view->update_steps = 20000;

    // Make interrupt more responsive
    if (view->update_steps > 40000)
        view->update_steps = 40000;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * The bytes of a hex search made of numbers only, NULL for anything else: a quoted string goes to
 * the generic search, and so does an error, to be reported there.
 * The syntax is the one of mc_search__hex_translate_to_regex().
 */

static GString *
mcview_fast_search_hex_needle (const GString *str)
{
    GString *needle;
    gsize i = 0;

    needle = g_string_sized_new (str->len / 2);

    while (i < str->len)
    {
        unsigned int val;
        int n;

        if (g_ascii_isspace (str->str[i]))
            i++;
        else if (sscanf (str->str + i, "%x%n", &val, &n) == 1 && val <= 255)
        {
            g_string_append_c (needle, (char) val);
            i += (gsize) n;
        }
        else
        {
            g_string_free (needle, TRUE);
            return NULL;
        }
    }

    if (needle->len == 0)
    {
        g_string_free (needle, TRUE);
        return NULL;
    }

    return needle;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * The bytes the fast path looks for, NULL when this search is not a matter of plain bytes.
 *
 * The search string is already in the charset of the file, so a case sensitive string is a byte
 * match. A case insensitive one is a byte match for ASCII letters only: others need Unicode case
 * folding. A string with a newline stays with the generic search, which works on lines.
 * Hex numbers are always matched exactly.
 *
 * @param search   the search
 * @param caseless set to TRUE if ASCII letters must be matched case insensitively
 */

static GString *
mcview_fast_search_needle (const mc_search_t *search, gboolean *caseless)
{
    const GString *str = search->original.str;
    GString *needle;
    gsize i;

    *caseless = FALSE;

    if (str == NULL || str->len == 0)
        return NULL;

    if (search->search_type == MC_SEARCH_T_HEX)
        return mcview_fast_search_hex_needle (str);

    if (search->search_type != MC_SEARCH_T_NORMAL)
        return NULL;

    for (i = 0; i < str->len; i++)
    {
        if (str->str[i] == '\n')
            return NULL;
        if ((unsigned char) str->str[i] >= 0x80 && !search->is_case_sensitive)
            return NULL;
    }

    *caseless = !search->is_case_sensitive;
    needle = mc_g_string_dup (str);
    if (*caseless)
        g_string_ascii_down (needle);

    return needle;
}

/* --------------------------------------------------------------------------------------------- */
/** The first @c in [@p, @end), or @end if there is none. */

static inline const char *
mcview_fast_search_next (const char *p, const char *end, char c)
{
    const char *q;

    q = memchr (p, c, (size_t) (end - p));
    return q != NULL ? q : end;
}

/* --------------------------------------------------------------------------------------------- */
/** Whether the bytes at @p after the first one, already matched, are those of @needle. */

static gboolean
mcview_fast_search_rest_matches (const char *p, const char *needle, gsize needle_len,
                                 gboolean caseless)
{
    gsize i;

    if (!caseless)
        return memcmp (p + 1, needle + 1, needle_len - 1) == 0;

    for (i = 1; i < needle_len; i++)
        if (g_ascii_tolower (p[i]) != needle[i])
            return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Find @needle in @hay, like memmem() (which isn't available everywhere). If @caseless, @needle
 * must be in lower case: both cases of its first byte are looked for with memchr(), which is much
 * faster than folding the whole @hay.
 */

static const char *
mcview_fast_search_find (const char *hay, gsize hay_len, const char *needle, gsize needle_len,
                         gboolean caseless)
{
    const char *end, *lower, *upper;
    const char c_lower = needle[0];
    const char c_upper = caseless ? g_ascii_toupper (c_lower) : c_lower;

    if (hay_len < needle_len)
        return NULL;

    end = hay + hay_len - needle_len + 1;  // the last possible start of a match + 1
    lower = mcview_fast_search_next (hay, end, c_lower);
    upper = c_upper != c_lower ? mcview_fast_search_next (hay, end, c_upper) : end;

    while (TRUE)
    {
        const char *p = MIN (lower, upper);

        if (p == end)
            return NULL;

        if (mcview_fast_search_rest_matches (p, needle, needle_len, caseless))
            return p;

        if (p == lower)
            lower = mcview_fast_search_next (p + 1, end, c_lower);
        else
            upper = mcview_fast_search_next (p + 1, end, c_upper);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Read up to @want bytes, as many as there are. -1 on an error, with errno set. */

static ssize_t
mcview_fast_search_read (int fd, char *buf, size_t want)
{
    size_t got = 0;

    while (got < want)
    {
        const ssize_t n = mc_read (fd, buf + got, want - got);

        if (n == -1 && errno == EINTR)
            continue;
        if (n == -1)
            return -1;
        if (n == 0)
            break;
        got += (size_t) n;
    }

    return (ssize_t) got;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Fast path of mcview_find(): read the file in chunks and search the bytes directly. Only a match
 * that lies wholly in [@search_start, @search_end) is found. Sets the same result fields
 * (normal_offset, error) as mc_search_run().
 *
 * The descriptor is left at an arbitrary place: mcview_file_load_data() seeks before every read.
 */

static gboolean
mcview_fast_search (mcview_search_status_msg_t *ssm, off_t search_start, off_t search_end,
                    gsize *len, const GString *needle_str, gboolean caseless)
{
    WView *view = ssm->view;
    mc_search_t *search = view->search;
    const char *const needle = needle_str->str;
    const gsize needle_len = needle_str->len;
    char *buf;
    gsize keep = 0;  // bytes kept from the previous chunk for matches across chunks
    off_t pos = search_start;
    gboolean found = FALSE;
    mc_search_error_t error = MC_SEARCH_E_NOTFOUND;
    int read_errno = 0;

    if (mc_lseek (view->ds_file_fd, pos, SEEK_SET) != pos)
    {
        mc_search_set_error (search, MC_SEARCH_E_INPUT, "%s", unix_error_string (errno));
        return FALSE;
    }

    buf = g_malloc (MCVIEW_FAST_SEARCH_CHUNK + needle_len);

    while (pos < search_end)
    {
        const size_t want = (size_t) MIN ((off_t) MCVIEW_FAST_SEARCH_CHUNK, search_end - pos);
        const ssize_t got = mcview_fast_search_read (view->ds_file_fd, buf + keep, want);
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
        hit = mcview_fast_search_find (buf, total, needle, needle_len, caseless);
        if (hit != NULL)
        {
            search->normal_offset = pos - (off_t) keep + (hit - buf);
            search->start_buffer = search->normal_offset;
            *len = needle_len;
            found = TRUE;
            break;
        }

        pos += got;
        keep = MIN (total, needle_len - 1);
        memmove (buf, buf + total - keep, keep);

        if (search->update_fn != NULL && search->update_fn (ssm, pos) == MC_SEARCH_CB_ABORT)
        {
            error = MC_SEARCH_E_ABORT;
            break;
        }
    }

    g_free (buf);

    if (found)
        mc_search_set_error (search, MC_SEARCH_E_OK, NULL);
    else if (error == MC_SEARCH_E_NOTFOUND)
        mc_search_set_error (search, MC_SEARCH_E_NOTFOUND, "%s", _ (STR_E_NOTFOUND));
    else if (error == MC_SEARCH_E_ABORT)
        mc_search_set_error (search, MC_SEARCH_E_ABORT, NULL);
    else
        mc_search_set_error (search, error, "%s", unix_error_string (read_errno));

    return found;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_find (mcview_search_status_msg_t *ssm, off_t search_start, off_t search_end, gsize *len)
{
    WView *view = ssm->view;

    view->search_numNeedSkipChar = 0;
    search_cb_char_curr_index = -1;

    if (mcview_search_options.backwards)
    {
        search_end = mcview_get_filesize (view);

        if ((view->search_line_type & MC_SEARCH_LINE_BEGIN) != 0)
            search_start = mcview_bol (view, search_start, 0);

        while (search_start >= 0)
        {
            gboolean ok;

            view->search_nroff_seq->index = search_start;
            mcview_nroff_seq_info (view->search_nroff_seq);

            if (search_end > search_start + (off_t) view->search->original.str->len
                && mc_search_is_fixed_search_str (view->search))
                search_end = search_start + view->search->original.str->len;

            ok = mc_search_run (view->search, (void *) ssm, search_start, search_end, len);
            if (ok && view->search->normal_offset == search_start)
            {
                if (view->mode_flags.nroff)
                    view->search->normal_offset++;
                return TRUE;
            }

            /* We abort the search in case of a pattern error, or if the user aborts
               the search. In other words: in all cases except "string not found". */
            if (!ok && view->search->error != MC_SEARCH_E_NOTFOUND)
                return FALSE;

            if ((view->search_line_type & MC_SEARCH_LINE_BEGIN) != 0)
                search_start = mcview_calculate_start_of_previous_line (view, search_start);
            else
                search_start--;
        }

        mc_search_set_error (view->search, MC_SEARCH_E_NOTFOUND, "%s", _ (STR_E_NOTFOUND));
        return FALSE;
    }

    if ((view->search_line_type & MC_SEARCH_LINE_BEGIN) != 0 && search_start != 0)
        search_start = mcview_eol (view, search_start);

    if (view->datasource == DS_FILE && !view->mode_flags.nroff && view->search_line_type == 0
        && !view->search->is_all_charsets && !view->search->whole_words)
    {
        gboolean caseless;
        GString *needle;

        needle = mcview_fast_search_needle (view->search, &caseless);
        if (needle != NULL)
        {
            gboolean found;

            found = mcview_fast_search (ssm, search_start, search_end, len, needle, caseless);
            g_string_free (needle, TRUE);
            return found;
        }
    }

    view->search_nroff_seq->index = search_start;
    mcview_nroff_seq_info (view->search_nroff_seq);

    return mc_search_run (view->search, (void *) ssm, search_start, search_end, len);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_search_show_result (WView *view, size_t match_len)
{
    int nroff_len;

    nroff_len = view->mode_flags.nroff
        ? mcview__get_nroff_real_len (view, view->search->start_buffer,
                                      view->search->normal_offset - view->search->start_buffer)
        : 0;
    view->search_start = view->search->normal_offset + nroff_len;

    if (!view->mode_flags.hex)
        view->search_start++;

    nroff_len = view->mode_flags.nroff
        ? mcview__get_nroff_real_len (view, view->search_start - 1, match_len)
        : 0;
    view->search_end = view->search_start + match_len + nroff_len;

    mcview_moveto_match (view);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Search for the current pattern within the already-indexed filter_offsets.
 * Each filter line is searched independently so that matches are restricted to
 * visible (filtered) lines.  Handles forward, backward, and wrap-around.
 */
static void
mcview_do_search_in_filter (WView *view, off_t want_search_start)
{
    mcview_search_status_msg_t vsm;
    gboolean found = FALSE;
    size_t match_len;
    guint n;
    off_t orig_search_start = view->search_start;

    if (view->filter_offsets == NULL || view->filter_offsets->len == 0)
    {
        message (D_NORMAL, _ ("Search"), "%s", _ (STR_E_NOTFOUND));
        view->dirty++;
        return;
    }

    n = view->filter_offsets->len;
    mcview_search_update_steps (view);
    view->update_activate = want_search_start;

    vsm.first = TRUE;
    vsm.view = view;
    vsm.offset = want_search_start;

    status_msg_init (STATUS_MSG (&vsm), _ ("Search"), 1.0, simple_status_msg_init_cb,
                     mcview_search_status_update_cb, NULL);

    if (!mcview_search_options.backwards)
    {
        /* Forward: start from the filter line that contains want_search_start. */
        guint start_idx = mcview_filter_idx (view, want_search_start);
        guint i;

        for (i = start_idx; i < n; i++)
        {
            off_t bol = g_array_index (view->filter_offsets, off_t, i);
            off_t eol = mcview_eol (view, bol);
            /* On the first line, start from want_search_start to skip the
               previous match; on subsequent lines search from the beginning. */
            off_t from = (i == start_idx && want_search_start > bol) ? want_search_start : bol;

            if (from >= eol)
                continue;

            view->search_numNeedSkipChar = 0;
            search_cb_char_curr_index = -1;
            view->search_nroff_seq->index = from;
            mcview_nroff_seq_info (view->search_nroff_seq);

            if (mc_search_run (view->search, (void *) &vsm, from, eol, &match_len))
            {
                mcview_search_show_result (view, match_len);
                found = TRUE;
                break;
            }
            if (view->search->error != MC_SEARCH_E_NOTFOUND)
                break;
        }

        /* Offer wrap-around (same convention as normal forward search). */
        if (orig_search_start != 0 && !found && view->search->error == MC_SEARCH_E_NOTFOUND)
        {
            view->search_start = orig_search_start;
            mcview_update (view);

            if (query_dialog (_ ("Search done"), _ ("Continue from beginning?"), D_NORMAL, 2,
                              _ ("&Yes"), _ ("&No"))
                != 0)
                found = TRUE; /* User chose No -- suppress "not found". */
            else
            {
                /* Wrap: re-search filter lines from 0 up to and including start_idx. */
                for (i = 0; i <= start_idx && !found; i++)
                {
                    off_t bol = g_array_index (view->filter_offsets, off_t, i);
                    off_t eol = mcview_eol (view, bol);

                    view->search_numNeedSkipChar = 0;
                    search_cb_char_curr_index = -1;
                    view->search_nroff_seq->index = bol;
                    mcview_nroff_seq_info (view->search_nroff_seq);

                    if (mc_search_run (view->search, (void *) &vsm, bol, eol, &match_len))
                    {
                        mcview_search_show_result (view, match_len);
                        found = TRUE;
                    }
                    else if (view->search->error != MC_SEARCH_E_NOTFOUND)
                        break;
                }
            }
        }
    }
    else
    {
        /* Backward: find the rightmost match before want_search_start.
           Use the same offset adjustment as the non-filter backward search. */
        off_t back_limit = (want_search_start >= 2) ? want_search_start - 2 : 0;
        guint start_idx = mcview_filter_idx (view, back_limit);
        gint j;

        for (j = (gint) start_idx; j >= 0 && !found; j--)
        {
            off_t bol = g_array_index (view->filter_offsets, off_t, (guint) j);
            off_t eol = mcview_eol (view, bol);
            /* On the first line limit the right edge to back_limit; on
               earlier lines search the whole line right-to-left. */
            off_t try_start = (j == (gint) start_idx) ? MIN (eol - 1, back_limit) : eol - 1;

            while (try_start >= bol)
            {
                view->search_numNeedSkipChar = 0;
                search_cb_char_curr_index = -1;
                view->search_nroff_seq->index = try_start;
                mcview_nroff_seq_info (view->search_nroff_seq);

                if (mc_search_run (view->search, (void *) &vsm, try_start, eol, &match_len)
                    && view->search->normal_offset == try_start)
                {
                    mcview_search_show_result (view, match_len);
                    found = TRUE;
                    break;
                }
                if (view->search->error != MC_SEARCH_E_NOTFOUND)
                    goto search_done;

                if (try_start == 0)
                    break;
                try_start--;
            }
        }
    }

search_done:
    status_msg_deinit (STATUS_MSG (&vsm));

    if (!found)
    {
        view->search_start = orig_search_start;
        mcview_update (view);

        if (view->search->error == MC_SEARCH_E_NOTFOUND)
            message (D_NORMAL, _ ("Search"), "%s", _ (STR_E_NOTFOUND));
        else if (view->search->error_str != NULL)
            message (D_NORMAL, _ ("Search"), "%s", view->search->error_str);
    }

    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_do_search (WView *view, off_t want_search_start)
{
    mcview_search_status_msg_t vsm;

    off_t search_start = 0;
    off_t orig_search_start = view->search_start;
    gboolean found = FALSE;
    gboolean incomplete = FALSE;

    size_t match_len;

    /* In filter mode text search is restricted to the indexed filtered lines. */
    if (view->filter_active && !view->mode_flags.hex)
    {
        mcview_do_search_in_filter (view, want_search_start);
        return;
    }

    view->search_start = want_search_start;
    // to avoid infinite search loop we need to increase or decrease start offset of search

    if (view->search_start != 0)
    {
        if (!view->mode_flags.nroff)
            search_start = view->search_start + (mcview_search_options.backwards ? -2 : 0);
        else
        {
            if (mcview_search_options.backwards)
            {
                mcview_nroff_t *nroff;

                nroff = mcview_nroff_seq_new_num (view, view->search_start);
                if (mcview_nroff_seq_prev (nroff) != -1)
                    search_start = -(mcview__get_nroff_real_len (view, nroff->index - 1, 2)
                                     + nroff->char_length + 1);
                else
                    search_start = -2;

                mcview_nroff_seq_free (&nroff);
            }
            else
            {
                search_start = mcview__get_nroff_real_len (view, view->search_start + 1, 2);
            }
            search_start += view->search_start;
        }
    }

    if (mcview_search_options.backwards && search_start < 0)
        search_start = 0;

    // Compute the percent steps
    mcview_search_update_steps (view);

    view->update_activate = search_start;

    vsm.first = TRUE;
    vsm.view = view;
    vsm.offset = search_start;

    status_msg_init (STATUS_MSG (&vsm), _ ("Search"), 1.0, simple_status_msg_init_cb,
                     mcview_search_status_update_cb, NULL);

    do
    {
        off_t growbufsize;

        if (view->growbuf_in_use)
            growbufsize = mcview_growbuf_filesize (view);
        else
            growbufsize = view->search->original.str->len;

        if (mcview_find (&vsm, search_start, mcview_get_filesize (view), &match_len))
        {
            mcview_search_show_result (view, match_len);
            found = TRUE;
            break;
        }

        /* Search error is here.
         * MC_SEARCH_E_NOTFOUND: continue search
         * others: stop
         */
        if (view->search->error != MC_SEARCH_E_NOTFOUND)
            break;

        search_start = growbufsize - view->search->original.str->len;
        /* A cooperative source advances through input events, not reads. Do not
           busy-loop over the same snapshot while waiting for its next portion. */
        if (view->datasource == DS_GENERATOR && mcview_may_still_grow (view)
            && mcview_get_filesize (view) == growbufsize)
        {
            incomplete = TRUE;
            break;
        }
    }
    while (search_start > 0 && mcview_may_still_grow (view));

    // After mcview_may_still_grow (view) == FALSE, last chunk remains. Search there.
    if (view->growbuf_in_use && !found && view->search->error == MC_SEARCH_E_NOTFOUND
        && !mcview_search_options.backwards
        && mcview_find (&vsm, search_start, mcview_get_filesize (view), &match_len))
    {
        mcview_search_show_result (view, match_len);
        found = TRUE;
    }

    status_msg_deinit (STATUS_MSG (&vsm));

    if (orig_search_start != 0 && (!found && view->search->error == MC_SEARCH_E_NOTFOUND)
        && !mcview_search_options.backwards)
    {
        view->search_start = orig_search_start;
        mcview_update (view);

        if (query_dialog (_ ("Search done"), _ ("Continue from beginning?"), D_NORMAL, 2,
                          _ ("&Yes"), _ ("&No"))
            != 0)
            found = TRUE;
        else
        {
            // continue search from beginning
            view->update_activate = 0;

            vsm.first = TRUE;
            vsm.view = view;
            vsm.offset = 0;

            status_msg_init (STATUS_MSG (&vsm), _ ("Search"), 1.0, simple_status_msg_init_cb,
                             mcview_search_status_update_cb, NULL);

            // search from file begin up to initial search start position
            if (mcview_find (&vsm, 0, orig_search_start, &match_len))
            {
                mcview_search_show_result (view, match_len);
                found = TRUE;
            }

            status_msg_deinit (STATUS_MSG (&vsm));
        }
    }

    if (!found)
    {
        view->search_start = orig_search_start;
        mcview_update (view);

        if (view->search->error == MC_SEARCH_E_NOTFOUND)
            message (D_NORMAL, _ ("Search"), "%s",
                     incomplete ? _ ("Not found in the text rendered so far.")
                                : _ (STR_E_NOTFOUND));
        else if (view->search->error_str != NULL)
            message (D_NORMAL, _ ("Search"), "%s", view->search->error_str);
    }

    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_search_init (WView *view)
{
    view->search = mc_search_new (view->last_search_string, cp_source);

    view->search_nroff_seq = mcview_nroff_seq_new (view);

    if (view->search == NULL)
        return FALSE;

    view->search->search_type = mcview_search_options.type;
    view->search->is_all_charsets = mcview_search_options.all_codepages;
    view->search->is_case_sensitive = mcview_search_options.case_sens;
    view->search->whole_words = mcview_search_options.whole_words;
    view->search->search_fn = mcview_search_cmd_callback;
    view->search->update_fn = mcview_search_update_cmd_callback;

    view->search_line_type = mc_search_get_line_type (view->search);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_search_deinit (WView *view)
{
    mc_search_free (view->search);
    g_free (view->last_search_string);
    mcview_nroff_seq_free (&view->search_nroff_seq);
}

/* --------------------------------------------------------------------------------------------- */

mc_search_cbret_t
mcview_search_cmd_callback (const void *user_data, off_t char_offset, int *current_char)
{
    WView *view = ((const mcview_search_status_msg_t *) user_data)->view;

    // AB: FIXME
    //    view_read_continue (view, &view->search_onechar_info);
    if (!view->mode_flags.nroff)
    {
        mcview_get_byte (view, char_offset, current_char);
        return MC_SEARCH_CB_OK;
    }

    if (view->search_numNeedSkipChar != 0)
    {
        view->search_numNeedSkipChar--;
        return MC_SEARCH_CB_SKIP;
    }

    if (search_cb_char_curr_index == -1
        || search_cb_char_curr_index >= view->search_nroff_seq->char_length)
    {
        if (search_cb_char_curr_index != -1)
            mcview_nroff_seq_next (view->search_nroff_seq);

        search_cb_char_curr_index = 0;
        if (view->search_nroff_seq->char_length > 1)
            g_unichar_to_utf8 (view->search_nroff_seq->current_char, search_cb_char_buffer);
        else
            search_cb_char_buffer[0] = (char) view->search_nroff_seq->current_char;

        switch (view->search_nroff_seq->type)
        {
        case NROFF_TYPE_BOLD:
            view->search_numNeedSkipChar =
                view->search_nroff_seq->char_length + 1;  // letter + '\b'
            break;
        case NROFF_TYPE_UNDERLINE:
        case NROFF_TYPE_OVERSTRIKE:
            view->search_numNeedSkipChar = 2;  // '_' or the first character + '\b'
            break;
        case NROFF_TYPE_BOLD_UNDERLINE:
            view->search_numNeedSkipChar =
                2 + view->search_nroff_seq->char_length + 1;  // '_' + '\b' + letter + '\b'
            break;
        case NROFF_TYPE_HEADING:
            view->search_numNeedSkipChar =
                (view->search_nroff_seq->char_length + 1) * 2;  // letter + '\b' + letter + '\b'
            break;
        default:
            break;
        }

        return MC_SEARCH_CB_INVALID;
    }

    *current_char = search_cb_char_buffer[search_cb_char_curr_index];
    search_cb_char_curr_index++;

    return (*current_char != -1) ? MC_SEARCH_CB_OK : MC_SEARCH_CB_INVALID;
}

/* --------------------------------------------------------------------------------------------- */

mc_search_cbret_t
mcview_search_update_cmd_callback (const void *user_data, off_t char_offset)
{
    status_msg_t *sm = STATUS_MSG (user_data);
    mcview_search_status_msg_t *vsm = (mcview_search_status_msg_t *) user_data;
    WView *view = vsm->view;
    gboolean do_update = FALSE;
    mc_search_cbret_t result = MC_SEARCH_CB_OK;

    vsm->offset = char_offset;

    if (mcview_search_options.backwards)
    {
        if (vsm->offset <= view->update_activate)
        {
            view->update_activate -= view->update_steps;

            do_update = TRUE;
        }
    }
    else
    {
        if (vsm->offset >= view->update_activate)
        {
            view->update_activate += view->update_steps;

            do_update = TRUE;
        }
    }

    if (do_update && sm->update (sm) == B_CANCEL)
        result = MC_SEARCH_CB_ABORT;

    // may be in future return from this callback will change current position in searching block.

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/* Both views */
void
mcview_search (WView *view, gboolean start_search)
{
    off_t want_search_start = view->search_start;

    if (start_search)
    {
        if (mcview_dialog_search (view))
        {
            if (view->mode_flags.hex)
                want_search_start = view->hex_cursor;

            mcview_do_search (view, want_search_start);
        }
    }
    else
    {
        if (view->mode_flags.hex)
        {
            if (!mcview_search_options.backwards)
                want_search_start = view->hex_cursor + 1;
            else if (view->hex_cursor > 0)
                want_search_start = view->hex_cursor - 1;
            else
                want_search_start = 0;
        }

        mcview_do_search (view, want_search_start);
    }
}

/* --------------------------------------------------------------------------------------------- */
