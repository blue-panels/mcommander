/*
   mcterm as a far2l terminal: drag and drop for the programs in it.

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

/** \file mcterm_far2l.c
 *  \brief Source: mcterm as a far2l terminal
 */

#include <config.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/event.h"
#include "lib/fileloc.h"
#include "lib/mcconfig.h"
#include "lib/tty/key.h"  // KEY_M_*, KEY_IC

#include "mcterm_far2l.h"

/*** file scope macro definitions ****************************************************************/

/* The profile mc grants to a program in its terminal, as the specification proposes it. */
#define MCTERM_F2L_MAX_FRAME    65536u
#define MCTERM_F2L_MAX_CHUNK    32768u
#define MCTERM_F2L_IDLE_SECONDS 600u
#define MCTERM_F2L_MAX_OFFERS   8u
#define MCTERM_F2L_PAGE         64u
/* a whole APC of a request is at most this (the introducer, base64 and the terminator) */
#define MCTERM_F2L_MAX_REQUEST 65536u

/* The clipboard sub-commands of the extensions (FarTTY.h of far2l) */
#define CLIP_OPEN            'o'
#define CLIP_CLOSE           'c'
#define CLIP_EMPTY           'e'
#define CLIP_ISAVAIL         'a'
#define CLIP_SETDATACHUNK    'S'
#define CLIP_SETDATA         's'
#define CLIP_GETDATA         'g'
#define CLIP_GETDATAID       'i'
#define CLIP_REGISTER_FORMAT 'r'
/* what this terminal can: data IDs and data set in chunks */
#define CLIP_FEAT_DATA_ID     0x1u
#define CLIP_FEAT_CHUNKED_SET 0x2u
#define CLIP_CF_TEXT          1u
#define CLIP_CF_UNICODETEXT   13u
#define CLIP_FIRST_FORMAT     0xC000u
/* a read is allowed this long after a paste key, and each read lets the next come as late, a
   few times (far2l's own terminal does the same) */
#define CLIP_READ_USEC     (5 * G_USEC_PER_SEC)
#define CLIP_READ_PROLONGS 3
#define CLIP_MAX_DATA      (16u * 1024u * 1024u)

/*** file scope type declarations ****************************************************************/

typedef struct
{
    guint8 id[FAR2L_ID_LEN];     /* the offer of the program's binding */
    guint8 parent[FAR2L_ID_LEN]; /* the offer of the outer terminal it stands for */
    GPtrArray *entries;          /* far2l_dnd_entry_t *, NULL until the first LIST */
    gint64 used;                 /* the lease runs from here */
} offer_t;

struct mcterm_far2l_struct
{
    mcterm_far2l_parent_t parent;
    gboolean on;
    gboolean bound;
    far2l_dnd_grant_t grant;
    /* what the program asked for in BIND, to tell a repeat from a different request */
    guint32 asked_frame, asked_chunk, asked_features;
    guint16 asked_window;
    GPtrArray *offers; /* offer_t * */
    /* the clipboard */
    gboolean clip_open;
    GHashTable *clip_allowed; /* client IDs the user allowed while the extensions are on */
    char *ask_client;         /* an OPEN that waits for the user's answer */
    guint8 ask_rid;
    gint64 read_since; /* the last paste key, 0 for none */
    int read_prolongs;
    GByteArray *chunks; /* data set in chunks, for the SETDATA that ends it */
    gboolean chunks_overflow;
};

/* a clipboard request, popped from its end */
typedef struct
{
    const guint8 *b;
    gsize len;
    gboolean err;
} clip_reader_t;

/*** file scope variables ************************************************************************/

/* the formats registered by name, and the data of every format but text: mc's clipboard holds
   text only, the rest is kept here for as long as mc runs */
static GHashTable *clip_format_ids = NULL;  /* char * -> GUINT_TO_POINTER (id) */
static GHashTable *clip_format_data = NULL; /* GUINT_TO_POINTER (id) -> GByteArray * */
static guint clip_next_format = CLIP_FIRST_FORMAT;

/*** file scope functions ************************************************************************/

static gboolean
real_is_bound (void)
{
    return far2l_dnd_is_bound ();
}

static int
real_list (const guint8 *offer, GPtrArray *entries)
{
    return far2l_dnd_list (offer, entries);
}

static int
real_read (const guint8 *offer, guint64 item_id, guint64 offset, guint32 length, guint8 **data,
           gsize *data_len, gboolean *eof)
{
    return far2l_dnd_read (offer, item_id, offset, length, data, data_len, eof);
}

static void
real_close (const guint8 *offer, guint8 reason)
{
    far2l_dnd_close (offer, reason);
}

static guint32
real_max_chunk (void)
{
    return far2l_dnd_max_chunk ();
}

/* --------------------------------------------------------------------------------------------- */

/* mc's clipboard: the clipfile, after clipboard_paste or the far2l terminal outside filled it */

static char *
real_clip_get (gsize *len)
{
    char *text = NULL;
    ev_clipboard_text_from_file_t ev = { &text, FALSE, 0 };

    *len = 0;
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_file_from_ext_clip", NULL);
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_text_from_file", &ev);
    if (!ev.ret || text == NULL)
    {
        g_free (text);
        return NULL;
    }
    *len = ev.len;
    return text;
}

/* --------------------------------------------------------------------------------------------- */

static void
real_clip_set (const char *text, gsize len)
{
    (void) len;  // the text ends at its NUL
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_text_to_file", (gpointer) text);
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_file_to_ext_clip", NULL);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
real_clip_has_text (void)
{
    char *fname = mc_config_get_full_path (EDIT_HOME_CLIP_FILE);
    struct stat st;
    const gboolean has = fname != NULL && stat (fname, &st) == 0 && st.st_size > 0;

    g_free (fname);
    return has;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
real_clip_remembered (const char *client_id)
{
    char *fname = mc_config_get_full_path (MC_FAR2L_CLIENTS_FILE);
    char *text = NULL;
    gboolean found = FALSE;

    if (fname != NULL && g_file_get_contents (fname, &text, NULL, NULL))
    {
        char **lines = g_strsplit (text, "\n", -1);
        char **l;

        for (l = lines; *l != NULL && !found; l++)
            found = strcmp (*l, client_id) == 0;
        g_strfreev (lines);
    }
    g_free (text);
    g_free (fname);
    return found;
}

/* --------------------------------------------------------------------------------------------- */

static void
real_clip_remember (const char *client_id)
{
    char *fname = mc_config_get_full_path (MC_FAR2L_CLIENTS_FILE);
    char *text = NULL;
    char *all;

    if (fname == NULL)
        return;
    if (!g_file_get_contents (fname, &text, NULL, NULL))
        text = g_strdup ("");
    all = g_strconcat (text, client_id, "\n", (char *) NULL);
    if (g_file_set_contents (fname, all, -1, NULL))
        (void) chmod (fname, 0600);
    g_free (all);
    g_free (text);
    g_free (fname);
}

/* --------------------------------------------------------------------------------------------- */

static void
offer_free (gpointer p)
{
    offer_t *o = p;

    if (o->entries != NULL)
        g_ptr_array_free (o->entries, TRUE);
    g_free (o);
}

/* --------------------------------------------------------------------------------------------- */

static offer_t *
offer_find (mcterm_far2l_t *f, const guint8 *id)
{
    guint i;

    for (i = 0; i < f->offers->len; i++)
    {
        offer_t *o = g_ptr_array_index (f->offers, i);

        if (memcmp (o->id, id, FAR2L_ID_LEN) == 0)
            return o;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* The offer is over: the outer one is released with the reason, and nothing is left of it. */

static void
offer_end (mcterm_far2l_t *f, offer_t *o, guint8 reason)
{
    f->parent.close (o->parent, reason);
    g_ptr_array_remove (f->offers, o);
}

/* --------------------------------------------------------------------------------------------- */

static void
offers_end_all (mcterm_far2l_t *f, guint8 reason)
{
    while (f->offers->len > 0)
        offer_end (f, g_ptr_array_index (f->offers, f->offers->len - 1), reason);
}

/* --------------------------------------------------------------------------------------------- */

/* An offer nobody has touched for its lease is given up. */

static void
offers_expire (mcterm_far2l_t *f)
{
    const gint64 now = g_get_monotonic_time ();
    guint i = 0;

    while (i < f->offers->len)
    {
        offer_t *o = g_ptr_array_index (f->offers, i);

        if (now - o->used > (gint64) MCTERM_F2L_IDLE_SECONDS * G_USEC_PER_SEC)
            offer_end (f, o, FAR2L_DND_CLOSE_CANCELLED);
        else
            i++;
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
random_id (guint8 *id)
{
    int fd = open ("/dev/urandom", O_RDONLY);
    gsize got = 0;

    if (fd < 0)
        return FALSE;
    while (got < FAR2L_ID_LEN)
    {
        ssize_t r = read (fd, id + got, FAR2L_ID_LEN - got);

        if (r <= 0)
            break;
        got += (gsize) r;
    }
    close (fd);
    return got == FAR2L_ID_LEN;
}

/* --------------------------------------------------------------------------------------------- */

/* What the protocol calls the length of a whole APC of a reply that carries n bytes of stack. */

static gsize
reply_frame_len (gsize n)
{
    return 7 + ((n + 2) / 3) * 4 + 1;
}

/* --------------------------------------------------------------------------------------------- */

/* The most data a READ reply can carry so that all of it still fits max_frame; the terminator
   counted is ST, the longer one. */

static guint32
max_read_data (guint32 frame)
{
    const guint32 fixed = 7 + 2;
    guint32 stack;

    if (frame <= fixed)
        return 0;
    stack = (frame - fixed) / 4 * 3;
    /* RID, status, observed size, flags and length */
    return stack > 15 ? stack - 15 : 0;
}

/* --------------------------------------------------------------------------------------------- */

/* The status the program is told when the outer terminal failed a request. */

static int
outer_status (int status)
{
    switch (status)
    {
    case FAR2L_E_DENIED:
    case FAR2L_E_OFFER_GONE:
    case FAR2L_E_UNKNOWN_ITEM:
    case FAR2L_E_CHANGED:
    case FAR2L_E_LIMIT:
    case FAR2L_E_CANCELLED:
        return status;
    default:
        /* nothing the program could do about a terminal that does not answer */
        return FAR2L_E_IO;
    }
}

/* --------------------------------------------------------------------------------------------- */

static GByteArray *
serve_bind (mcterm_far2l_t *f, const far2l_dnd_request_t *q)
{
    far2l_dnd_grant_t g;
    guint32 frame, chunk;

    if (!q->enable)
    {
        /* only the binding that is current is switched off; a late word of an old one is not */
        if (f->bound && memcmp (q->binding, f->grant.binding, FAR2L_ID_LEN) == 0)
        {
            offers_end_all (f, FAR2L_DND_CLOSE_CANCELLED);
            f->bound = FALSE;
        }
        return q->rid != 0 ? far2l_reply_ok (q->rid) : NULL;
    }

    if (!f->parent.is_bound ())
        return far2l_reply_error (q->rid, FAR2L_E_UNSUPPORTED, "no drag and drop here");
    if (q->max_frame < 4096 || q->max_chunk < 1 || q->window < 1)
        return far2l_reply_error (q->rid, FAR2L_E_BAD_REQUEST, "limits below the minimum");
    if ((q->wanted_features & FAR2L_DND_FEATURE_STREAM) == 0)
        return far2l_reply_error (q->rid, FAR2L_E_UNSUPPORTED, "STREAM is the only representation");

    frame = MIN (q->max_frame, MCTERM_F2L_MAX_FRAME);
    chunk = MIN (MIN (q->max_chunk, MCTERM_F2L_MAX_CHUNK), max_read_data (frame));
    if (chunk == 0)
        return far2l_reply_error (q->rid, FAR2L_E_LIMIT, "max_frame too small");

    memset (&g, 0, sizeof (g));
    g.version = 1;
    memcpy (g.binding, q->binding, FAR2L_ID_LEN);
    g.max_frame = frame;
    g.max_chunk = chunk;
    g.window = 1;
    g.idle_seconds = MCTERM_F2L_IDLE_SECONDS;
    g.features = FAR2L_DND_FEATURE_STREAM;

    if (f->bound && memcmp (q->binding, f->grant.binding, FAR2L_ID_LEN) == 0)
    {
        /* the same again is safe; something else for the same binding is not accepted */
        if (q->max_frame != f->asked_frame || q->max_chunk != f->asked_chunk
            || q->window != f->asked_window || q->wanted_features != f->asked_features)
            return far2l_reply_error (q->rid, FAR2L_E_BAD_REQUEST,
                                      "different parameters for the same binding");
        return far2l_reply_bind (q->rid, &f->grant);
    }

    /* a new binding takes the place of the old one, and its offers go */
    offers_end_all (f, FAR2L_DND_CLOSE_CANCELLED);
    f->grant = g;
    f->bound = TRUE;
    f->asked_frame = q->max_frame;
    f->asked_chunk = q->max_chunk;
    f->asked_window = q->window;
    f->asked_features = q->wanted_features;
    return far2l_reply_bind (q->rid, &f->grant);
}

/* --------------------------------------------------------------------------------------------- */

/* The outer offer's list, once: kept as what the program is told of it. */

static int
list_outer (mcterm_far2l_t *f, offer_t *o)
{
    GPtrArray *all;
    guint i;
    int st;

    if (o->entries != NULL)
        return FAR2L_OK;

    all = g_ptr_array_new_with_free_func (far2l_dnd_entry_free);
    st = f->parent.list (o->parent, all);
    if (st != FAR2L_OK)
    {
        g_ptr_array_free (all, TRUE);
        return st;
    }

    o->entries = g_ptr_array_new_with_free_func (far2l_dnd_entry_free);
    for (i = 0; i < all->len; i++)
    {
        const far2l_dnd_entry_t *e = g_ptr_array_index (all, i);
        far2l_dnd_entry_t *c;

        /* files that can be read from the start: the only kind there is in version 1 */
        if (e->kind != FAR2L_DND_KIND_FILE || (e->flags & FAR2L_DND_ITEM_STREAM) == 0
            || !far2l_dnd_name_is_safe (e->name))
            continue;
        c = g_new0 (far2l_dnd_entry_t, 1);
        c->item_id = e->item_id;
        c->kind = FAR2L_DND_KIND_FILE;
        c->flags = (guint16) (FAR2L_DND_ITEM_STREAM | (e->flags & FAR2L_DND_ITEM_SIZE_KNOWN));
        c->size = e->size;
        c->name = g_strdup (e->name);
        g_ptr_array_add (o->entries, c);
    }
    g_ptr_array_free (all, TRUE);
    return FAR2L_OK;
}

/* --------------------------------------------------------------------------------------------- */

static GByteArray *
serve_list (mcterm_far2l_t *f, const far2l_dnd_request_t *q)
{
    offer_t *o = offer_find (f, q->offer);
    GByteArray *reply = NULL;
    guint from, count, total;
    int st;

    if (o == NULL)
        return far2l_reply_error (q->rid, FAR2L_E_OFFER_GONE, "unknown offer");
    o->used = g_get_monotonic_time ();

    /* parent_id is 0 only in base version 1 */
    if (q->parent_id != 0)
        return far2l_reply_error (q->rid, FAR2L_E_UNKNOWN_ITEM, "no directories");

    st = list_outer (f, o);
    if (st != FAR2L_OK)
        return far2l_reply_error (q->rid, outer_status (st), "the terminal outside cannot list it");

    total = o->entries->len;
    if (q->cursor > total)
        return far2l_reply_error (q->rid, FAR2L_E_UNKNOWN_ITEM, "unknown cursor");
    from = (guint) q->cursor;
    count = MIN (total - from, MCTERM_F2L_PAGE);

    /* a page is what fits max_frame; an entry that does not fit even alone is an error, never
       a truncated name */
    while (count > 0)
    {
        const guint next = from + count;

        reply = far2l_reply_list (q->rid, next >= total ? FAR2L_DND_LAST_PAGE : next, o->entries,
                                  from, count);
        if (reply_frame_len (reply->len) <= f->grant.max_frame)
            break;
        g_byte_array_free (reply, TRUE);
        reply = NULL;
        count--;
    }
    if (reply == NULL)
    {
        if (total == 0 || from >= total)
            return far2l_reply_list (q->rid, FAR2L_DND_LAST_PAGE, o->entries, 0, 0);
        return far2l_reply_error (q->rid, FAR2L_E_LIMIT, "an entry does not fit max_frame");
    }
    return reply;
}

/* --------------------------------------------------------------------------------------------- */

static GByteArray *
serve_read (mcterm_far2l_t *f, const far2l_dnd_request_t *q)
{
    offer_t *o = offer_find (f, q->offer);
    const far2l_dnd_entry_t *entry = NULL;
    guint8 *data = NULL;
    gsize len = 0;
    gboolean eof = FALSE;
    guint8 flags = 0;
    GByteArray *reply;
    guint i;
    int st;

    if (o == NULL)
        return far2l_reply_error (q->rid, FAR2L_E_OFFER_GONE, "unknown offer");
    o->used = g_get_monotonic_time ();

    /* zero is not "everything", as it is in FISH+ */
    if (q->length == 0 || q->length > f->grant.max_chunk || q->offset + q->length < q->offset)
        return far2l_reply_error (q->rid, FAR2L_E_BAD_REQUEST, "bad range");

    if (o->entries != NULL)
        for (i = 0; i < o->entries->len && entry == NULL; i++)
            if (((const far2l_dnd_entry_t *) g_ptr_array_index (o->entries, i))->item_id
                == q->item_id)
                entry = g_ptr_array_index (o->entries, i);
    if (entry == NULL)
        return far2l_reply_error (q->rid, FAR2L_E_UNKNOWN_ITEM, "unknown item");

    st = f->parent.read (o->parent, entry->item_id, q->offset,
                         MIN (q->length, f->parent.max_chunk ()), &data, &len, &eof);
    if (st != FAR2L_OK)
        return far2l_reply_error (q->rid, outer_status (st), "the terminal outside cannot read it");

    if (eof)
        flags |= FAR2L_DND_READ_EOF;
    if ((entry->flags & FAR2L_DND_ITEM_SIZE_KNOWN) != 0)
        flags |= FAR2L_DND_READ_SIZE_KNOWN;
    reply = far2l_reply_read (q->rid, (flags & FAR2L_DND_READ_SIZE_KNOWN) != 0 ? entry->size : 0,
                              flags, data, len);
    g_free (data);
    return reply;
}

/* --------------------------------------------------------------------------------------------- */

static GByteArray *
serve_close (mcterm_far2l_t *f, const far2l_dnd_request_t *q)
{
    offer_t *o = offer_find (f, q->offer);

    /* the reason is for the accounting, it does not delete anything; a repeat is a success */
    if (o != NULL)
        offer_end (f, o, q->reason <= FAR2L_DND_CLOSE_FAILED ? q->reason : FAR2L_DND_CLOSE_FAILED);
    return q->rid != 0 ? far2l_reply_ok (q->rid) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
append_frame (GString *out, GByteArray *reply)
{
    char *frame;

    if (reply == NULL)
        return;
    frame = far2l_frame_reply (reply);
    g_string_append (out, frame);
    g_free (frame);
    g_byte_array_free (reply, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/* The clipboard, served as far2l's own terminal serves it (VTFar2lExtensios.cpp of far2l) */

static const guint8 *
clip_pop (clip_reader_t *r, gsize n)
{
    if (r->err || n > r->len)
    {
        r->err = TRUE;
        return NULL;
    }
    r->len -= n;
    return r->b + r->len;
}

/* --------------------------------------------------------------------------------------------- */

static guint32
clip_pop_le (clip_reader_t *r, gsize n)
{
    const guint8 *p = clip_pop (r, n);
    guint32 v = 0;

    while (p != NULL && n-- > 0)
        v = (v << 8) | p[n];
    return v;
}

/* --------------------------------------------------------------------------------------------- */

/* pushed in the order the program pops them last to first: the value goes on top */

static void
clip_push_le (GByteArray *a, guint64 v, gsize n)
{
    gsize i;

    for (i = 0; i < n; i++)
    {
        const guint8 b = (guint8) (v >> (8 * i));

        g_byte_array_append (a, &b, 1);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The client ID, if it is one the protocol allows */

static char *
clip_client_id (const guint8 *id, gsize len)
{
    return far2l_clip_id_is_valid ((const char *) id, len) ? g_strndup ((const char *) id, len)
                                                           : NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* Any stable value that is never 0 serves as the ID of the data */

static guint64
clip_data_id (const guint8 *data, gsize len)
{
    guint64 h = 0xcbf29ce484222325ULL ^ (guint64) len;
    gsize i;

    for (i = 0; i < len; i++)
        h = (h ^ data[i]) * 0x100000001b3ULL;
    return h != 0 ? h : 1;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
clip_read_allowed (const mcterm_far2l_t *f)
{
    return f->read_since != 0 && g_get_monotonic_time () - f->read_since < CLIP_READ_USEC;
}

/* --------------------------------------------------------------------------------------------- */

/* The data of @fmt on the clipboard now; NULL for none */

static GByteArray *
clip_data (mcterm_far2l_t *f, guint32 fmt)
{
    GByteArray *a = NULL;

    if (fmt == CLIP_CF_TEXT || fmt == CLIP_CF_UNICODETEXT)
    {
        gsize len = 0;
        char *text = f->parent.clip_get (&len);

        if (text != NULL && len > 0)
        {
            if (fmt == CLIP_CF_TEXT)
                a = g_byte_array_new_take ((guint8 *) text, len);
            else
            {
                glong n = 0, i;
                gunichar *u = g_utf8_to_ucs4 (text, (glong) len, NULL, &n, NULL);

                if (u != NULL && n > 0)
                {
                    a = g_byte_array_new ();
                    for (i = 0; i < n; i++)
                        clip_push_le (a, u[i], 4);
                }
                g_free (u);
                g_free (text);
            }
        }
        else
            g_free (text);
    }
    else if (clip_format_data != NULL)
    {
        const GByteArray *d = g_hash_table_lookup (clip_format_data, GUINT_TO_POINTER (fmt));

        if (d != NULL)
            a = g_byte_array_append (g_byte_array_new (), d->data, d->len);
    }

    return a;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
clip_set_data (mcterm_far2l_t *f, guint32 fmt, const guint8 *data, gsize len)
{
    if (fmt == CLIP_CF_TEXT || fmt == CLIP_CF_UNICODETEXT)
    {
        char *text;
        gsize n;

        if (fmt == CLIP_CF_TEXT)
        {
            text = g_strndup ((const char *) data, len);
            n = strlen (text);  // up to the first NUL, as far2l counts a text
        }
        else
        {
            gunichar *u = g_new (gunichar, len / 4 + 1);
            gsize i, k = 0;
            glong written = 0;

            for (i = 0; i + 4 <= len; i += 4)
            {
                const gunichar c = (gunichar) data[i] | ((gunichar) data[i + 1] << 8)
                    | ((gunichar) data[i + 2] << 16) | ((gunichar) data[i + 3] << 24);

                if (c == 0)
                    break;
                u[k++] = c;
            }
            text = g_ucs4_to_utf8 (u, (glong) k, NULL, &written, NULL);
            g_free (u);
            if (text == NULL)
                return FALSE;
            n = (gsize) written;
        }
        f->parent.clip_set (text, n);
        g_free (text);
        return TRUE;
    }

    if (clip_format_data == NULL)
        clip_format_data = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
                                                  (GDestroyNotify) g_byte_array_unref);
    g_hash_table_replace (clip_format_data, GUINT_TO_POINTER (fmt),
                          g_byte_array_append (g_byte_array_new (), data, len));
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The answer to OPEN: the status, and below it the features of this terminal */

static GByteArray *
clip_open_reply (guint8 rid, gint8 status)
{
    GByteArray *a = g_byte_array_new ();

    clip_push_le (a, CLIP_FEAT_DATA_ID | CLIP_FEAT_CHUNKED_SET, 8);
    clip_push_le (a, (guint8) status, 1);
    clip_push_le (a, rid, 1);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

static gint8
clip_open_now (mcterm_far2l_t *f)
{
    f->clip_open = TRUE;
    f->read_prolongs = 0;
    g_byte_array_set_size (f->chunks, 0);
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static GByteArray *
serve_clipboard (mcterm_far2l_t *f, const guint8 *stack, gsize len)
{
    clip_reader_t r = { stack, len, FALSE };
    const guint8 rid = (guint8) clip_pop_le (&r, 1);
    guint8 sub;
    GByteArray *a = g_byte_array_new ();

    (void) clip_pop_le (&r, 1);  // the command, 'c'
    sub = (guint8) clip_pop_le (&r, 1);

    if (f->parent.clip_get == NULL)
    {
        // no clipboard to give: the program is to use its own
        g_byte_array_free (a, TRUE);
        if (sub == CLIP_OPEN)
            return clip_open_reply (rid, -1);
        a = g_byte_array_new ();
        clip_push_le (a, rid, 1);
        return a;
    }

    switch (r.err ? 0 : sub)
    {
    case CLIP_OPEN:
    {
        const guint32 n = clip_pop_le (&r, 4);
        char *client = clip_client_id (clip_pop (&r, n), n);

        g_byte_array_free (a, TRUE);
        g_byte_array_set_size (f->chunks, 0);
        // opens do not nest, as in far2l: the second one fails
        if (client == NULL || f->ask_client != NULL || f->clip_open)
        {
            g_free (client);
            return clip_open_reply (rid, 0);
        }
        if (g_hash_table_contains (f->clip_allowed, client) || f->parent.clip_remembered (client))
        {
            const gint8 st = clip_open_now (f);

            g_free (client);
            return clip_open_reply (rid, st);
        }
        // the user says it, from the idle loop: the program waits for the answer
        f->ask_client = client;
        f->ask_rid = rid;
        return NULL;
    }

    case CLIP_CLOSE:
        clip_push_le (a, f->clip_open ? 1 : (guint8) -1, 1);
        f->clip_open = FALSE;
        f->read_prolongs = 0;
        g_byte_array_set_size (f->chunks, 0);
        break;

    case CLIP_EMPTY:
        if (f->clip_open && clip_format_data != NULL)
            g_hash_table_remove_all (clip_format_data);
        // the text of mc's clipboard is replaced by the SETDATA that follows, not cleared now
        clip_push_le (a, f->clip_open ? 1 : (guint8) -1, 1);
        break;

    case CLIP_ISAVAIL:
    {
        const guint32 fmt = clip_pop_le (&r, 4);
        gboolean avail;

        if (fmt == CLIP_CF_TEXT || fmt == CLIP_CF_UNICODETEXT)
            avail = f->parent.clip_has_text ();
        else
            avail = clip_format_data != NULL
                && g_hash_table_contains (clip_format_data, GUINT_TO_POINTER (fmt));
        clip_push_le (a, avail && !r.err ? 1 : 0, 1);
        break;
    }

    case CLIP_SETDATACHUNK:
    {
        const gsize n = (gsize) clip_pop_le (&r, 2) << 8;
        const guint8 *data = clip_pop (&r, n);

        if (!f->clip_open || r.err)
            break;
        if (n == 0)
            g_byte_array_set_size (f->chunks, 0);
        else if (f->chunks->len + n <= CLIP_MAX_DATA)
            g_byte_array_append (f->chunks, data, (guint) n);
        else
            f->chunks_overflow = TRUE;
        break;
    }

    case CLIP_SETDATA:
    {
        const guint32 fmt = clip_pop_le (&r, 4);
        const guint32 n = clip_pop_le (&r, 4);
        const guint8 *data = clip_pop (&r, n);
        gint8 st = -1;

        if (f->clip_open)
        {
            st = 0;
            if (!r.err && !f->chunks_overflow && f->chunks->len + n <= CLIP_MAX_DATA)
            {
                g_byte_array_append (f->chunks, data, n);
                if (clip_set_data (f, fmt, f->chunks->data, f->chunks->len))
                    st = 1;
            }
            if (st == 1)
                clip_push_le (a, clip_data_id (f->chunks->data, f->chunks->len), 8);
            g_byte_array_set_size (f->chunks, 0);
            f->chunks_overflow = FALSE;
        }
        clip_push_le (a, (guint8) st, 1);
        break;
    }

    case CLIP_GETDATA:
    {
        const guint32 fmt = clip_pop_le (&r, 4);

        if (!f->clip_open)
        {
            clip_push_le (a, 0xFFFFFFFFu, 4);
            break;
        }
        {
            const gboolean allowed = clip_read_allowed (f);
            GByteArray *d = allowed && !r.err ? clip_data (f, fmt) : NULL;

            /* not allowed is told as no data: a program is not to learn that it was refused */
            clip_push_le (a, d != NULL ? clip_data_id (d->data, d->len) : 0, 8);
            if (d != NULL)
                g_byte_array_append (a, d->data, d->len);
            clip_push_le (a, d != NULL ? d->len : 0, 4);
            if (d != NULL)
                g_byte_array_free (d, TRUE);
            // each read lets the next come a while later, a few times only
            if (allowed && f->read_prolongs < CLIP_READ_PROLONGS)
            {
                f->read_prolongs++;
                f->read_since = g_get_monotonic_time ();
            }
        }
        break;
    }

    case CLIP_GETDATAID:
    {
        const guint32 fmt = clip_pop_le (&r, 4);
        GByteArray *d = f->clip_open && clip_read_allowed (f) && !r.err ? clip_data (f, fmt) : NULL;

        clip_push_le (a, d != NULL ? clip_data_id (d->data, d->len) : 0, 8);
        if (d != NULL)
            g_byte_array_free (d, TRUE);
        break;
    }

    case CLIP_REGISTER_FORMAT:
    {
        const guint32 n = clip_pop_le (&r, 4);
        const guint8 *name = clip_pop (&r, n);
        guint id = 0;

        if (!r.err && n > 0 && n <= 256)
        {
            char *key = g_strndup ((const char *) name, n);
            gpointer known;

            if (clip_format_ids == NULL)
                clip_format_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
            known = g_hash_table_lookup (clip_format_ids, key);
            id = GPOINTER_TO_UINT (known);
            if (id == 0 && clip_next_format <= 0xFFFF)
            {
                id = clip_next_format++;
                g_hash_table_insert (clip_format_ids, key, GUINT_TO_POINTER (id));
            }
            else
                g_free (key);
        }
        clip_push_le (a, id, 4);
        break;
    }

    default:
        // an unknown sub-command: the empty answer says so
        break;
    }

    clip_push_le (a, rid, 1);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

/* The program gave the extensions up, or went: a question it asked is not answered, and the
   clipboard it held is closed */

static void
clip_end (mcterm_far2l_t *f)
{
    g_free (f->ask_client);
    f->ask_client = NULL;
    f->clip_open = FALSE;
    f->read_since = 0;
    f->read_prolongs = 0;
    f->chunks_overflow = FALSE;
    g_byte_array_set_size (f->chunks, 0);
    g_hash_table_remove_all (f->clip_allowed);
}

/* --------------------------------------------------------------------------------------------- */

static void
serve_stack (mcterm_far2l_t *f, const guint8 *stack, gsize len, GString *out)
{
    far2l_dnd_request_t q;
    guint8 rid = 0, cmd = 0;
    GByteArray *reply = NULL;
    int st;

    if (!far2l_request_head (stack, len, &rid, &cmd))
        return;  // not even a RID: there is nobody to answer

    if (cmd == FAR2L_INTERACT_CLIPBOARD)
    {
        reply = serve_clipboard (f, stack, len);
        if (rid != 0)
            append_frame (out, reply);
        else if (reply != NULL)
            g_byte_array_free (reply, TRUE);
        return;
    }

    /* every other family of far2l interactions (images, ...) is not served here: the empty
       answer is the one that says so */
    if (cmd != FAR2L_INTERACT_DND)
    {
        if (rid != 0)
            append_frame (out, far2l_reply_empty (rid));
        return;
    }

    st = far2l_dnd_decode_request (stack, len, &q);
    if (st != FAR2L_OK)
    {
        if (rid != 0)
            append_frame (out, far2l_reply_error (rid, st, "cannot take this request"));
        return;
    }

    offers_expire (f);

    switch (q.sub)
    {
    case 'b':
        reply = serve_bind (f, &q);
        break;
    case 'l':
        reply = f->bound ? serve_list (f, &q)
                         : far2l_reply_error (rid, FAR2L_E_OFFER_GONE, "not bound");
        break;
    case 'r':
        reply = f->bound ? serve_read (f, &q)
                         : far2l_reply_error (rid, FAR2L_E_OFFER_GONE, "not bound");
        break;
    case 'c':
        reply = serve_close (f, &q);
        break;
    default:
        break;
    }

    /* RID 0 is a request nobody answers: BIND off and CLOSE only, as best-effort cleanup */
    if (rid != 0)
        append_frame (out, reply);
    else if (reply != NULL)
        g_byte_array_free (reply, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_far2l_allowed (void)
{
    const char *env = getenv ("MC_FAR2L");

    return env == NULL || env[0] != '0';
}

/* --------------------------------------------------------------------------------------------- */

mcterm_far2l_t *
mcterm_far2l_new (const mcterm_far2l_parent_t *parent)
{
    mcterm_far2l_t *f = g_new0 (mcterm_far2l_t, 1);

    if (parent != NULL)
        f->parent = *parent;
    else
    {
        f->parent.is_bound = real_is_bound;
        f->parent.list = real_list;
        f->parent.read = real_read;
        f->parent.close = real_close;
        f->parent.max_chunk = real_max_chunk;
        f->parent.clip_get = real_clip_get;
        f->parent.clip_set = real_clip_set;
        f->parent.clip_has_text = real_clip_has_text;
        f->parent.clip_remembered = real_clip_remembered;
        f->parent.clip_remember = real_clip_remember;
    }
    f->offers = g_ptr_array_new_with_free_func (offer_free);
    f->clip_allowed = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    f->chunks = g_byte_array_new ();
    return f;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_far2l_free (mcterm_far2l_t *f)
{
    if (f == NULL)
        return;
    offers_end_all (f, FAR2L_DND_CLOSE_CANCELLED);
    g_ptr_array_free (f->offers, TRUE);
    clip_end (f);
    g_hash_table_destroy (f->clip_allowed);
    g_byte_array_free (f->chunks, TRUE);
    g_free (f);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_far2l_on (const mcterm_far2l_t *f)
{
    return f != NULL && f->on;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_far2l_bound (const mcterm_far2l_t *f)
{
    return f != NULL && f->on && f->bound;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcterm_far2l_apc (mcterm_far2l_t *f, const char *body, gsize len)
{
    GString *out;

    if (f == NULL || len < 6 || strncmp (body, "far2l", 5) != 0)
        return NULL;

    out = g_string_new (NULL);

    switch (body[5])
    {
    case '1':
        /* The emulator has answered it (and gives the program its keys as far2l events); here it
           only becomes the program's to bind drop reception. */
        if (len == 6)
            f->on = TRUE;
        break;

    case '0':
        if (len == 6)
        {
            offers_end_all (f, FAR2L_DND_CLOSE_CANCELLED);
            clip_end (f);
            f->bound = FALSE;
            f->on = FALSE;
        }
        break;

    case ':':
        if (f->on && len > 6 && len + 8 <= MCTERM_F2L_MAX_REQUEST)
        {
            GByteArray *st = g_byte_array_new ();

            if (far2l_frame_decode (body + 6, len - 6, st))
                serve_stack (f, st->data, st->len, out);
            g_byte_array_free (st, TRUE);
        }
        break;

    default:
        break;
    }

    return g_string_free (out, out->len == 0);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_far2l_drop (mcterm_far2l_t *f, const far2l_drop_t *drop, int x, int y, char **out)
{
    offer_t *o;
    GByteArray *ev;

    *out = NULL;
    if (f == NULL || !f->on || !f->bound || !f->parent.is_bound ())
        return FALSE;

    offers_expire (f);
    if (f->offers->len >= MCTERM_F2L_MAX_OFFERS)
        return FALSE;

    o = g_new0 (offer_t, 1);
    if (!random_id (o->id))
    {
        g_free (o);
        return FALSE;
    }
    memcpy (o->parent, drop->offer, FAR2L_ID_LEN);
    o->used = g_get_monotonic_time ();
    g_ptr_array_add (f->offers, o);

    /* a position that is inside the terminal is told, one that is not is not made up */
    ev = far2l_dnd_encode_event (f->grant.binding, o->id, x >= 0 && y >= 0 ? (gint16) x : -1,
                                 x >= 0 && y >= 0 ? (gint16) y : -1, drop->modifiers, drop->flags);
    *out = far2l_frame_event (ev);
    g_byte_array_free (ev, TRUE);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_far2l_key (mcterm_far2l_t *f, int key)
{
    const int mods = key & KEY_M_MASK;
    const int base = key & ~KEY_M_MASK;
    gboolean paste;

    if (f == NULL || !f->on || (mods & KEY_M_ALT) != 0)
        return;

    // Ctrl+V, with Shift or without, and Shift+Insert: the keys of a paste
    paste = ((mods & KEY_M_CTRL) != 0 && (base == 0x16 || base == 'v' || base == 'V'))
        || (base == KEY_IC && (mods & KEY_M_SHIFT) != 0 && (mods & KEY_M_CTRL) == 0);
    if (paste)
    {
        f->read_since = g_get_monotonic_time ();
        f->read_prolongs = 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_far2l_question_pending (const mcterm_far2l_t *f)
{
    return f != NULL && f->ask_client != NULL;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcterm_far2l_answer (mcterm_far2l_t *f, int answer)
{
    GByteArray *reply;
    char *frame;
    gint8 st;

    if (f == NULL || f->ask_client == NULL)
        return NULL;

    switch (answer)
    {
    case MCTERM_F2L_CLIP_ALWAYS:
        f->parent.clip_remember (f->ask_client);
        MC_FALLTHROUGH;
    case MCTERM_F2L_CLIP_ALLOW:
        g_hash_table_add (f->clip_allowed, f->ask_client);
        f->ask_client = NULL;
        st = clip_open_now (f);
        break;
    case MCTERM_F2L_CLIP_OWN:
        st = -1;
        break;
    default:
        st = 0;
        break;
    }
    g_free (f->ask_client);
    f->ask_client = NULL;

    if (f->ask_rid == 0)
        return NULL;
    reply = clip_open_reply (f->ask_rid, st);
    frame = far2l_frame_reply (reply);
    g_byte_array_free (reply, TRUE);
    return frame;
}

/* --------------------------------------------------------------------------------------------- */
