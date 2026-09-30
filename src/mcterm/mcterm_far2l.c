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
#include <string.h>
#include <unistd.h>

#include "lib/global.h"

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
};

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

static void
serve_stack (mcterm_far2l_t *f, const guint8 *stack, gsize len, GString *out)
{
    far2l_dnd_request_t q;
    guint8 rid = 0, cmd = 0;
    GByteArray *reply = NULL;
    int st;

    if (!far2l_request_head (stack, len, &rid, &cmd))
        return;  // not even a RID: there is nobody to answer

    /* every other family of far2l interactions (the clipboard, images, ...) is not served here:
       the empty answer is the one that says so */
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

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
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
    }
    f->offers = g_ptr_array_new_with_free_func (offer_free);
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
