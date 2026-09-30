/*
   far2l terminal extensions and drag and drop over them.

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

/** \file far2l.c
 *  \brief Source: far2l terminal extensions and drag and drop over them
 *
 *  The wire format is the far2l stack.  Its fields are listed in the specification in the
 *  order they are popped, and a stack is popped from its end: a field is written in front of
 *  the ones already written, and a string is its bytes followed by its length, so that the
 *  length comes out first.  Integers are little endian.
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/unixcompat.h"

#include "tty.h"
#include "tty-internal.h"
#include "far2l.h"

/*** file scope macro definitions ****************************************************************/

#define F2L_INTERACT_DND 'd'
#define F2L_INPUT_DND    'D'
#define F2L_SUB_BIND     'b'
#define F2L_SUB_LIST     'l'
#define F2L_SUB_READ     'r'
#define F2L_SUB_CLOSE    'c'

#define F2L_MAX_MESSAGE  1024u
#define F2L_MAX_ENTRY    (16u * 1024u)
#define F2L_MAX_PAGE     64u
#define F2L_MAX_FRAME    (2 * 65536u)
/* every request has to be answered within this, a hung provider must not hold mc forever */
#define F2L_REQUEST_TIMEOUT_MS 20000
#define F2L_BIND_TIMEOUT_MS    500
#define F2L_MAX_ENTRIES        4096

/*** file scope type declarations ****************************************************************/

typedef struct
{
    const guint8 *b;
    gsize len;
    gboolean err;
} reader_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static gboolean dnd_bound = FALSE;
static far2l_dnd_grant_t dnd_grant;
static guint8 next_rid = 0;
static far2l_drop_handler_fn drop_handler = NULL;
static gboolean drop_pending = FALSE;
static far2l_drop_t pending_drop;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* writer: fields are given in pop order and land in front of the ones already there */

static void
wr_raw (GByteArray *a, const guint8 *data, guint len)
{
    g_byte_array_prepend (a, data, len);
}

/* --------------------------------------------------------------------------------------------- */

static void
wr_u8 (GByteArray *a, guint8 v)
{
    wr_raw (a, &v, 1);
}

/* --------------------------------------------------------------------------------------------- */

static void
wr_u16 (GByteArray *a, guint16 v)
{
    guint8 b[2];

    b[0] = (guint8) (v & 0xff);
    b[1] = (guint8) (v >> 8);
    wr_raw (a, b, sizeof (b));
}

/* --------------------------------------------------------------------------------------------- */

static void
wr_u32 (GByteArray *a, guint32 v)
{
    guint8 b[4];
    int i;

    for (i = 0; i < 4; i++)
        b[i] = (guint8) ((v >> (8 * i)) & 0xff);
    wr_raw (a, b, sizeof (b));
}

/* --------------------------------------------------------------------------------------------- */

static void
wr_u64 (GByteArray *a, guint64 v)
{
    guint8 b[8];
    int i;

    for (i = 0; i < 8; i++)
        b[i] = (guint8) ((v >> (8 * i)) & 0xff);
    wr_raw (a, b, sizeof (b));
}

/* --------------------------------------------------------------------------------------------- */

static void
wr_id (GByteArray *a, const guint8 *id)
{
    wr_raw (a, id, FAR2L_ID_LEN);
}

/* --------------------------------------------------------------------------------------------- */

static GByteArray *
wr_begin (guint8 rid, guint8 sub)
{
    GByteArray *a = g_byte_array_new ();

    /* the first fields popped are RID, command and sub-command: they are written first */
    wr_u8 (a, rid);
    wr_u8 (a, F2L_INTERACT_DND);
    wr_u8 (a, sub);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

/* reader: fields are popped from the end */

static const guint8 *
rd_take (reader_t *r, gsize n)
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

static guint8
rd_u8 (reader_t *r)
{
    const guint8 *p = rd_take (r, 1);

    return p != NULL ? p[0] : 0;
}

/* --------------------------------------------------------------------------------------------- */

static guint16
rd_u16 (reader_t *r)
{
    const guint8 *p = rd_take (r, 2);

    return p != NULL ? (guint16) (p[0] | (p[1] << 8)) : 0;
}

/* --------------------------------------------------------------------------------------------- */

static guint32
rd_u32 (reader_t *r)
{
    const guint8 *p = rd_take (r, 4);
    guint32 v = 0;
    int i;

    if (p == NULL)
        return 0;
    for (i = 3; i >= 0; i--)
        v = (v << 8) | p[i];
    return v;
}

/* --------------------------------------------------------------------------------------------- */

static guint64
rd_u64 (reader_t *r)
{
    const guint8 *p = rd_take (r, 8);
    guint64 v = 0;
    int i;

    if (p == NULL)
        return 0;
    for (i = 7; i >= 0; i--)
        v = (v << 8) | p[i];
    return v;
}

/* --------------------------------------------------------------------------------------------- */

static void
rd_id (reader_t *r, guint8 *id)
{
    const guint8 *p = rd_take (r, FAR2L_ID_LEN);

    if (p != NULL)
        memcpy (id, p, FAR2L_ID_LEN);
    else
        memset (id, 0, FAR2L_ID_LEN);
}

/* --------------------------------------------------------------------------------------------- */

/* A length is checked against the limit and against what is left before anything is allocated. */

static const guint8 *
rd_blob (reader_t *r, gsize limit, gsize *n)
{
    guint32 len = rd_u32 (r);

    *n = 0;
    if (r->err)
        return NULL;
    if (len > limit || len > r->len)
    {
        r->err = TRUE;
        return NULL;
    }
    *n = len;
    return rd_take (r, len);
}

/* --------------------------------------------------------------------------------------------- */

static char *
rd_str (reader_t *r, gsize limit)
{
    gsize n;
    const guint8 *p = rd_blob (r, limit, &n);

    if (r->err)
        return NULL;
    if (n != 0 && !g_utf8_validate ((const char *) p, (gssize) n, NULL))
    {
        r->err = TRUE;
        return NULL;
    }
    return g_strndup ((const char *) p, n);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
b64_charset_ok (const char *s, gsize len)
{
    gsize i;

    for (i = 0; i < len; i++)
        if (!g_ascii_isalnum (s[i]) && s[i] != '+' && s[i] != '/' && s[i] != '=')
            return FALSE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
far2l_write (const char *data, gsize len)
{
    tty_raw_write (data, len);
}

/* --------------------------------------------------------------------------------------------- */

static guint8
new_rid (void)
{
    /* 0 is the RID of a request nobody answers */
    next_rid = (guint8) (next_rid % 255 + 1);
    return next_rid;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
random_bytes (guint8 *out, gsize n)
{
    int fd = open ("/dev/urandom", O_RDONLY);
    gsize got = 0;

    if (fd < 0)
        return FALSE;
    while (got < n)
    {
        ssize_t r = read (fd, out + got, n - got);

        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0)
            break;
        got += (gsize) r;
    }
    close (fd);
    return got == n;
}

/* --------------------------------------------------------------------------------------------- */

/* Read what the terminal sends until keep_reading says enough or the time is out.
   Returns FALSE on timeout. */

typedef int (*scan_fn) (GString *buf, void *data);

static gboolean
read_until (GString *buf, gint64 timeout_ms, scan_fn scan, void *data)
{
    gint64 deadline = g_get_monotonic_time () + timeout_ms * 1000;

    while (TRUE)
    {
        gint64 left = deadline - g_get_monotonic_time ();
        fd_set fds;
        struct timeval tv;
        char chunk[512];
        ssize_t n;
        int r;

        r = scan (buf, data);
        if (r != 0)
            return r > 0;
        if (left <= 0)
            return FALSE;

        FD_ZERO (&fds);
        FD_SET (STDIN_FILENO, &fds);
        /* the scan is asked again every so often: the grace after DA1 has to run out by itself */
        if (left > 20000)
            left = 20000;
        tv.tv_sec = left / G_USEC_PER_SEC;
        tv.tv_usec = left % G_USEC_PER_SEC;
        r = select (STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0)
            continue;  // the deadline check above ends it
        n = read (STDIN_FILENO, chunk, sizeof (chunk));
        if (n <= 0)
            return FALSE;
        g_string_append_len (buf, chunk, n);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The end of the APC that starts at buf[start]: the offset just past its terminator, or -1. */

static gssize
apc_end (const GString *buf, gsize start, gsize *body_end)
{
    gsize i;

    for (i = start; i < buf->len; i++)
    {
        if (buf->str[i] == '\a')
        {
            *body_end = i;
            return (gssize) (i + 1);
        }
        if (buf->str[i] == ESC_CHAR && i + 1 < buf->len && buf->str[i + 1] == '\\')
        {
            *body_end = i;
            return (gssize) (i + 2);
        }
    }
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    guint8 rid;
    GByteArray *stack; /* the matching reply, when found */
} reply_wait_t;

/* 1: our reply is in; 0: wait for more.  Stale replies (of a request given up) are dropped. */

static int
scan_reply (GString *buf, void *data)
{
    reply_wait_t *w = data;
    gsize i = 0;

    while (i + 7 <= buf->len)
    {
        gsize body_end = 0;
        gssize end;

        if (buf->str[i] != ESC_CHAR || buf->str[i + 1] != '_'
            || strncmp (buf->str + i + 2, "far2l", 5) != 0)
        {
            i++;
            continue;
        }

        end = apc_end (buf, i + 7, &body_end);
        if (end < 0)
        {
            if (buf->len - i > F2L_MAX_FRAME)
            {
                /* not a frame we can ever take whole: cut it off, the rest is not keys either */
                g_string_erase (buf, i, -1);
                return 0;
            }
            return 0;  // still arriving
        }

        {
            const char *body = buf->str + i + 7;
            gsize blen = body_end - (i + 7);
            gboolean ours = FALSE;

            if (!(blen == 2 && strncmp (body, "ok", 2) == 0) && b64_charset_ok (body, blen))
            {
                GByteArray *st = g_byte_array_new ();

                if (far2l_frame_decode (body, blen, st) && st->len > 0
                    && st->data[st->len - 1] == w->rid)
                {
                    w->stack = st;
                    ours = TRUE;
                }
                else
                    g_byte_array_free (st, TRUE);
            }
            g_string_erase (buf, i, (gssize) ((gsize) end - i));
            if (ours)
                return 1;
        }
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Send the request and wait for the reply of its RID.  What else arrives is put back. */

static int
roundtrip (GByteArray *request, guint8 rid, gint64 timeout_ms, far2l_reply_t *reply,
           GByteArray **keep)
{
    char *frame = far2l_frame_request (request);
    GString *buf = g_string_new (NULL);
    reply_wait_t w = { rid, NULL };
    gboolean got;
    int status;

    g_byte_array_free (request, TRUE);
    far2l_write (frame, strlen (frame));
    g_free (frame);

    got = read_until (buf, timeout_ms, scan_reply, &w);

    if (buf->len > 0)
        tty_unget_input ((const guint8 *) buf->str, buf->len);
    g_string_free (buf, TRUE);

    if (!got || w.stack == NULL)
        return FAR2L_E_TIMEOUT;

    memset (reply, 0, sizeof (*reply));
    if (!far2l_dnd_decode_reply (w.stack->data, w.stack->len, reply))
    {
        far2l_reply_clear (reply);
        g_byte_array_free (w.stack, TRUE);
        return FAR2L_E_PROTOCOL;
    }
    status = reply->status;
    /* the reply body points into the stack, which the caller frees with the reply */
    *keep = w.stack;
    return status;
}

/* --------------------------------------------------------------------------------------------- */

/* a request without a reply: RID 0 */

static void
send_no_reply (GByteArray *request)
{
    char *frame = far2l_frame_request (request);

    g_byte_array_free (request, TRUE);
    far2l_write (frame, strlen (frame));
    g_free (frame);
}

/* --------------------------------------------------------------------------------------------- */

static int
call (GByteArray *request, guint8 rid, gint64 timeout_ms, far2l_reply_t *reply, GByteArray **keep)
{
    int st = roundtrip (request, rid, timeout_ms, reply, keep);

    if (st == FAR2L_E_NO_DND)
        far2l_reply_clear (reply);
    return st;
}

/* --------------------------------------------------------------------------------------------- */

static void
dnd_reset (void)
{
    dnd_bound = FALSE;
    memset (&dnd_grant, 0, sizeof (dnd_grant));
    drop_pending = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
far2l_dnd_bind (void)
{
    guint8 binding[FAR2L_ID_LEN];
    guint8 rid = new_rid ();
    far2l_reply_t reply;
    GByteArray *keep = NULL;
    GByteArray *req;
    int st;

    if (!random_bytes (binding, sizeof (binding)))
        return;

    req = far2l_dnd_encode_bind (rid, TRUE, binding, FAR2L_DND_MAX_FRAME, FAR2L_DND_MAX_CHUNK,
                                 (guint16) FAR2L_DND_WINDOW, FAR2L_DND_FEATURE_STREAM);

    st = call (req, rid, F2L_BIND_TIMEOUT_MS, &reply, &keep);
    if (st == FAR2L_OK
        && far2l_dnd_decode_bind_reply (&reply, 1, binding, FAR2L_DND_MAX_FRAME,
                                        FAR2L_DND_MAX_CHUNK, (guint16) FAR2L_DND_WINDOW,
                                        FAR2L_DND_FEATURE_STREAM, &dnd_grant)
        && (dnd_grant.features & FAR2L_DND_FEATURE_STREAM) != 0)
        dnd_bound = TRUE;
    else
        dnd_reset ();

    if (st != FAR2L_E_TIMEOUT && st != FAR2L_E_PROTOCOL)
        far2l_reply_clear (&reply);
    if (keep != NULL)
        g_byte_array_free (keep, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
far2l_dnd_unbind (void)
{
    GByteArray *req;

    if (!dnd_bound)
        return;
    /* only the matching binding is disabled; best effort, the terminal answers nobody */
    req = far2l_dnd_encode_bind (0, FALSE, dnd_grant.binding, 0, 0, 0, 0);
    send_no_reply (req);
    dnd_reset ();
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

GByteArray *
far2l_dnd_encode_bind (guint8 rid, gboolean enable, const guint8 *binding, guint32 max_frame,
                       guint32 max_chunk, guint16 window, guint32 wanted_features)
{
    GByteArray *a = wr_begin (rid, F2L_SUB_BIND);
    guint8 zero[FAR2L_ID_LEN] = { 0 };

    /* pop order: version, enable, binding, max_frame, max_chunk, window, wanted_features */
    wr_u16 (a, 1);
    wr_u8 (a, enable ? 1 : 0);
    wr_id (a, binding != NULL ? binding : zero);
    wr_u32 (a, max_frame);
    wr_u32 (a, max_chunk);
    wr_u16 (a, window);
    wr_u32 (a, wanted_features);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

GByteArray *
far2l_dnd_encode_list (guint8 rid, const guint8 *offer, guint64 cursor)
{
    GByteArray *a = wr_begin (rid, F2L_SUB_LIST);

    wr_id (a, offer);
    wr_u64 (a, 0);  // parent_id: the root; only 0 in base v1
    wr_u64 (a, cursor);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

GByteArray *
far2l_dnd_encode_read (guint8 rid, const guint8 *offer, guint64 item_id, guint64 offset,
                       guint32 length)
{
    GByteArray *a = wr_begin (rid, F2L_SUB_READ);

    wr_id (a, offer);
    wr_u64 (a, item_id);
    wr_u64 (a, offset);
    wr_u32 (a, length);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

GByteArray *
far2l_dnd_encode_close (guint8 rid, const guint8 *offer, guint8 reason)
{
    GByteArray *a = wr_begin (rid, F2L_SUB_CLOSE);

    wr_id (a, offer);
    wr_u8 (a, reason);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

char *
far2l_frame_request (const GByteArray *stack)
{
    char *b64 = g_base64_encode (stack->data, stack->len);
    char *frame = g_strconcat (ESC_STR "_far2l:", b64, "\a", NULL);

    g_free (b64);
    return frame;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
far2l_frame_decode (const char *b64, gsize len, GByteArray *out)
{
    GString *padded;
    guchar *data;
    gsize n = 0;

    if (len == 0 || len % 4 == 1 || !b64_charset_ok (b64, len))
        return FALSE;

    padded = g_string_new_len (b64, (gssize) len);
    while (padded->len % 4 != 0)
        g_string_append_c (padded, '=');
    data = g_base64_decode (padded->str, &n);
    g_string_free (padded, TRUE);

    if (data == NULL || n == 0)
    {
        g_free (data);
        return FALSE;
    }
    g_byte_array_append (out, data, (guint) n);
    g_free (data);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
far2l_dnd_decode_reply (const guint8 *stack, gsize len, far2l_reply_t *reply)
{
    reader_t r = { stack, len, FALSE };

    memset (reply, 0, sizeof (*reply));
    reply->rid = rd_u8 (&r);
    if (r.err)
        return FALSE;
    if (r.len == 0)
    {
        reply->status = FAR2L_E_NO_DND;
        return TRUE;
    }
    reply->status = (gint8) rd_u8 (&r);
    if (reply->status != FAR2L_OK)
    {
        reply->message = rd_str (&r, F2L_MAX_MESSAGE);
        if (r.err || r.len != 0)
            return FALSE;
        reply->body = NULL;
        reply->body_len = 0;
        return TRUE;
    }
    reply->body = stack;
    reply->body_len = r.len;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
far2l_reply_clear (far2l_reply_t *reply)
{
    g_free (reply->message);
    reply->message = NULL;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
far2l_dnd_decode_bind_reply (const far2l_reply_t *reply, guint16 version, const guint8 *binding,
                             guint32 max_frame, guint32 max_chunk, guint16 window, guint32 wanted,
                             far2l_dnd_grant_t *grant)
{
    reader_t r = { reply->body, reply->body_len, FALSE };

    memset (grant, 0, sizeof (*grant));
    grant->version = rd_u16 (&r);
    rd_id (&r, grant->binding);
    grant->max_frame = rd_u32 (&r);
    grant->max_chunk = rd_u32 (&r);
    grant->window = rd_u16 (&r);
    grant->idle_seconds = rd_u32 (&r);
    grant->features = rd_u32 (&r);
    if (r.err || r.len != 0)
        return FALSE;

    /* the version and the binding are echoed, the limits are the asked ones or lower */
    if (grant->version != version || memcmp (grant->binding, binding, FAR2L_ID_LEN) != 0)
        return FALSE;
    if (grant->max_frame > max_frame || grant->max_chunk > max_chunk || grant->window > window)
        return FALSE;
    if (grant->max_frame < 4096 || grant->max_chunk < 1 || grant->window < 1)
        return FALSE;
    if ((grant->features & ~wanted) != 0
        || (grant->features & (FAR2L_DND_FEATURE_STREAM | FAR2L_DND_FEATURE_REFERENCE)) == 0)
        return FALSE;
    /* the reply of a full chunk (RID, status, size, flags, length, data) has to fit: base64
       makes it 4/3 and the introducer and the terminator (ST is the longer one) come on top */
    if ((gsize) grant->max_chunk + 15 > ((gsize) grant->max_frame - 8) / 4 * 3)
        return FALSE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
far2l_dnd_entry_free (gpointer entry)
{
    far2l_dnd_entry_t *e = entry;

    if (e == NULL)
        return;
    g_free (e->name);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

static far2l_dnd_entry_t *
decode_entry (const guint8 *b, gsize len)
{
    reader_t r = { b, len, FALSE };
    far2l_dnd_entry_t e;
    gsize n;
    char *ref, *ns;

    memset (&e, 0, sizeof (e));
    e.item_id = rd_u64 (&r);
    e.kind = rd_u8 (&r);
    e.flags = rd_u16 (&r);
    e.size = rd_u64 (&r);
    e.name = rd_str (&r, F2L_MAX_ENTRY);
    (void) rd_u8 (&r);  // native_encoding
    (void) rd_blob (&r, F2L_MAX_ENTRY, &n);
    ref = rd_str (&r, F2L_MAX_ENTRY);
    ns = rd_str (&r, F2L_MAX_ENTRY);
    g_free (ref);
    g_free (ns);
    /* a longer entry of a later version is length delimited: what is left grants nothing */
    if (r.err || e.item_id == 0)
    {
        g_free (e.name);
        return NULL;
    }
    return g_memdup2 (&e, sizeof (e));
}

/* --------------------------------------------------------------------------------------------- */

gboolean
far2l_dnd_decode_list_reply (const far2l_reply_t *reply, guint64 *next_cursor, GPtrArray *entries)
{
    reader_t r = { reply->body, reply->body_len, FALSE };
    guint32 count, i;

    *next_cursor = rd_u64 (&r);
    count = rd_u32 (&r);
    if (r.err || count > F2L_MAX_PAGE)
        return FALSE;
    /* a page that is not the last one has at least one entry */
    if (*next_cursor != FAR2L_DND_LAST_PAGE && count == 0)
        return FALSE;

    for (i = 0; i < count; i++)
    {
        gsize n;
        const guint8 *b = rd_blob (&r, F2L_MAX_ENTRY, &n);
        far2l_dnd_entry_t *e;

        if (r.err)
            return FALSE;
        /* one unusable entry does not cost the page */
        e = decode_entry (b, n);
        if (e != NULL)
            g_ptr_array_add (entries, e);
    }
    return r.len == 0;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
far2l_dnd_decode_read_reply (const far2l_reply_t *reply, guint32 asked, guint64 *observed_size,
                             guint8 *flags, const guint8 **data, gsize *data_len)
{
    reader_t r = { reply->body, reply->body_len, FALSE };

    *observed_size = rd_u64 (&r);
    *flags = rd_u8 (&r);
    *data = rd_blob (&r, asked, data_len);
    if (r.err || r.len != 0)
        return FALSE;
    /* an empty success without EOF would stop the reader dead */
    if (*data_len == 0 && (*flags & FAR2L_DND_READ_EOF) == 0)
        return FALSE;
    if ((*flags & FAR2L_DND_READ_SIZE_KNOWN) == 0)
        *observed_size = 0;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
far2l_dnd_decode_event (const guint8 *stack, gsize len, far2l_dnd_event_t *event)
{
    reader_t r = { stack, len, FALSE };

    memset (event, 0, sizeof (*event));
    if (rd_u8 (&r) != F2L_INPUT_DND)
        return FALSE;
    rd_id (&r, event->binding);
    rd_id (&r, event->offer);
    event->x = (gint16) rd_u16 (&r);
    event->y = (gint16) rd_u16 (&r);
    event->modifiers = rd_u32 (&r);
    event->flags = rd_u16 (&r);
    if (r.err || r.len != 0)
        return FALSE;
    /* other bits are zero in v1; a position is both coordinates or neither */
    if ((event->flags & ~1u) != 0
        || ((event->x < 0 || event->y < 0) && (event->x != -1 || event->y != -1)))
        return FALSE;
    if ((event->flags & 1u) == 0)
        event->modifiers = 0;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
far2l_dnd_name_is_safe (const char *name)
{
    const char *p;

    if (name == NULL || name[0] == '\0' || strcmp (name, ".") == 0 || strcmp (name, "..") == 0)
        return FALSE;
    for (p = name; *p != '\0'; p++)
        if (*p == '/' || *p == '\\' || (guchar) *p < 0x20 || *p == 0x7f)
            return FALSE;
    return g_utf8_validate (name, -1, NULL);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
far2l_dnd_is_bound (void)
{
    return dnd_bound;
}

/* --------------------------------------------------------------------------------------------- */

void
far2l_dnd_set_handler (far2l_drop_handler_fn handler)
{
    drop_handler = handler;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
far2l_dnd_event (const guint8 *stack, gsize len)
{
    far2l_dnd_event_t ev;

    /* an event of a binding that is not the current one is somebody else's */
    if (!dnd_bound || !far2l_dnd_decode_event (stack, len, &ev)
        || memcmp (ev.binding, dnd_grant.binding, FAR2L_ID_LEN) != 0)
        return FALSE;

    pending_drop = ev;
    drop_pending = TRUE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
far2l_dnd_dispatch (void)
{
    far2l_drop_t drop;

    if (!drop_pending)
        return;
    drop = pending_drop;
    drop_pending = FALSE;

    if (drop_handler != NULL)
        drop_handler (&drop);
    else
        far2l_dnd_close (drop.offer, FAR2L_DND_CLOSE_REJECTED);
}

/* --------------------------------------------------------------------------------------------- */

guint32
far2l_dnd_max_chunk (void)
{
    return dnd_bound ? dnd_grant.max_chunk : 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
status_or_protocol (int st, far2l_reply_t *reply)
{
    if (st == FAR2L_E_TIMEOUT || st == FAR2L_E_PROTOCOL)
        return st;
    far2l_reply_clear (reply);
    return st;
}

/* --------------------------------------------------------------------------------------------- */

int
far2l_dnd_list (const guint8 *offer, GPtrArray *entries)
{
    guint64 cursor = 0;
    guint pages = 0;

    if (!dnd_bound)
        return FAR2L_E_NO_DND;

    while (TRUE)
    {
        guint8 rid = new_rid ();
        far2l_reply_t reply;
        GByteArray *keep = NULL;
        guint64 next = 0;
        int st = call (far2l_dnd_encode_list (rid, offer, cursor), rid, F2L_REQUEST_TIMEOUT_MS,
                       &reply, &keep);

        if (st != FAR2L_OK)
        {
            st = status_or_protocol (st, &reply);
            if (keep != NULL)
                g_byte_array_free (keep, TRUE);
            return st;
        }
        if (!far2l_dnd_decode_list_reply (&reply, &next, entries))
        {
            g_byte_array_free (keep, TRUE);
            return FAR2L_E_PROTOCOL;
        }
        g_byte_array_free (keep, TRUE);

        if (next == FAR2L_DND_LAST_PAGE)
            return FAR2L_OK;
        /* the cursor has to advance, and a drop of thousands of files is not one we take */
        if (next <= cursor || ++pages > F2L_MAX_ENTRIES || entries->len > F2L_MAX_ENTRIES)
            return FAR2L_E_LIMIT;
        cursor = next;
    }
}

/* --------------------------------------------------------------------------------------------- */

int
far2l_dnd_read (const guint8 *offer, guint64 item_id, guint64 offset, guint32 length, guint8 **data,
                gsize *data_len, gboolean *eof)
{
    guint8 rid = new_rid ();
    far2l_reply_t reply;
    GByteArray *keep = NULL;
    guint64 observed;
    guint8 flags;
    const guint8 *d;
    int st;

    *data = NULL;
    *data_len = 0;
    *eof = FALSE;
    if (!dnd_bound)
        return FAR2L_E_NO_DND;
    if (length == 0 || length > dnd_grant.max_chunk)
        return FAR2L_E_BAD_REQUEST;

    st = call (far2l_dnd_encode_read (rid, offer, item_id, offset, length), rid,
               F2L_REQUEST_TIMEOUT_MS, &reply, &keep);
    if (st != FAR2L_OK)
    {
        st = status_or_protocol (st, &reply);
        if (keep != NULL)
            g_byte_array_free (keep, TRUE);
        return st;
    }
    if (!far2l_dnd_decode_read_reply (&reply, length, &observed, &flags, &d, data_len))
    {
        g_byte_array_free (keep, TRUE);
        return FAR2L_E_PROTOCOL;
    }
    *data = g_memdup2 (d, *data_len > 0 ? *data_len : 1);
    *eof = (flags & FAR2L_DND_READ_EOF) != 0;
    g_byte_array_free (keep, TRUE);
    return FAR2L_OK;
}

/* --------------------------------------------------------------------------------------------- */

void
far2l_dnd_close (const guint8 *offer, guint8 reason)
{
    guint8 rid;
    far2l_reply_t reply;
    GByteArray *keep = NULL;
    int st;

    if (!dnd_bound)
        return;
    rid = new_rid ();
    /* the reply is the barrier after which the offer is released */
    st = call (far2l_dnd_encode_close (rid, offer, reason), rid, F2L_REQUEST_TIMEOUT_MS, &reply,
               &keep);
    (void) status_or_protocol (st, &reply);
    if (keep != NULL)
        g_byte_array_free (keep, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
far2l_request_head (const guint8 *stack, gsize len, guint8 *rid, guint8 *cmd)
{
    reader_t r = { stack, len, FALSE };

    *rid = rd_u8 (&r);
    *cmd = rd_u8 (&r);
    return !r.err;
}

/* --------------------------------------------------------------------------------------------- */

int
far2l_dnd_decode_request (const guint8 *stack, gsize len, far2l_dnd_request_t *q)
{
    reader_t r = { stack, len, FALSE };

    memset (q, 0, sizeof (*q));
    q->rid = rd_u8 (&r);
    q->cmd = rd_u8 (&r);
    if (r.err)
        return FAR2L_E_BAD_REQUEST;
    if (q->cmd != F2L_INTERACT_DND)
        return FAR2L_E_UNSUPPORTED;

    q->sub = rd_u8 (&r);
    if (r.err)
        return FAR2L_E_BAD_REQUEST;

    switch (q->sub)
    {
    case F2L_SUB_BIND:
    {
        guint8 enable;

        q->version = rd_u16 (&r);
        if (r.err)
            return FAR2L_E_BAD_REQUEST;
        /* the layout of the rest belongs to that version: it is not guessed */
        if (q->version != 1)
            return FAR2L_E_UNSUPPORTED;
        enable = rd_u8 (&r);
        if (enable > 1)
            return FAR2L_E_BAD_REQUEST;
        q->enable = enable == 1;
        rd_id (&r, q->binding);
        q->max_frame = rd_u32 (&r);
        q->max_chunk = rd_u32 (&r);
        q->window = rd_u16 (&r);
        q->wanted_features = rd_u32 (&r);
        break;
    }
    case F2L_SUB_LIST:
        rd_id (&r, q->offer);
        q->parent_id = rd_u64 (&r);
        q->cursor = rd_u64 (&r);
        break;
    case F2L_SUB_READ:
        rd_id (&r, q->offer);
        q->item_id = rd_u64 (&r);
        q->offset = rd_u64 (&r);
        q->length = rd_u32 (&r);
        break;
    case F2L_SUB_CLOSE:
        rd_id (&r, q->offer);
        q->reason = rd_u8 (&r);
        break;
    default:
        return FAR2L_E_UNSUPPORTED;
    }

    if (r.err || r.len != 0)
        return FAR2L_E_BAD_REQUEST;
    return FAR2L_OK;
}

/* --------------------------------------------------------------------------------------------- */

static void
wr_str (GByteArray *a, const char *s)
{
    wr_u32 (a, (guint32) strlen (s));
    wr_raw (a, (const guint8 *) s, (guint) strlen (s));
}

/* --------------------------------------------------------------------------------------------- */

GByteArray *
far2l_reply_empty (guint8 rid)
{
    GByteArray *a = g_byte_array_new ();

    wr_u8 (a, rid);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

GByteArray *
far2l_reply_ok (guint8 rid)
{
    GByteArray *a = far2l_reply_empty (rid);

    wr_u8 (a, FAR2L_OK);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

GByteArray *
far2l_reply_error (guint8 rid, int status, const char *message)
{
    GByteArray *a = far2l_reply_empty (rid);
    char *msg = g_strndup (message != NULL ? message : "", 512);

    /* the diagnostic is text: what is cut in the middle of a character is cut once more */
    while (msg[0] != '\0' && !g_utf8_validate (msg, -1, NULL))
        msg[strlen (msg) - 1] = '\0';
    wr_u8 (a, (guint8) (gint8) status);
    wr_str (a, msg);
    g_free (msg);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

GByteArray *
far2l_reply_bind (guint8 rid, const far2l_dnd_grant_t *g)
{
    GByteArray *a = far2l_reply_ok (rid);

    wr_u16 (a, g->version);
    wr_id (a, g->binding);
    wr_u32 (a, g->max_frame);
    wr_u32 (a, g->max_chunk);
    wr_u16 (a, g->window);
    wr_u32 (a, g->idle_seconds);
    wr_u32 (a, g->features);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

GByteArray *
far2l_reply_list (guint8 rid, guint64 next_cursor, GPtrArray *entries, guint from, guint count)
{
    GByteArray *a = far2l_reply_ok (rid);
    guint i;

    wr_u64 (a, next_cursor);
    wr_u32 (a, count);
    for (i = from; i < from + count; i++)
    {
        const far2l_dnd_entry_t *e = g_ptr_array_index (entries, i);
        GByteArray *b = g_byte_array_new ();

        wr_u64 (b, e->item_id);
        wr_u8 (b, e->kind);
        wr_u16 (b, e->flags);
        wr_u64 (b, e->size);
        wr_str (b, e->name);
        wr_u8 (b, 0);    // native_encoding: none
        wr_u32 (b, 0);   // native_name: empty
        wr_str (b, "");  // reference_uri
        wr_str (b, "");  // source_namespace
        wr_u32 (a, b->len);
        wr_raw (a, b->data, b->len);
        g_byte_array_free (b, TRUE);
    }
    return a;
}

/* --------------------------------------------------------------------------------------------- */

GByteArray *
far2l_reply_read (guint8 rid, guint64 observed_size, guint8 flags, const guint8 *data, gsize len)
{
    GByteArray *a = far2l_reply_ok (rid);

    wr_u64 (a, observed_size);
    wr_u8 (a, flags);
    wr_u32 (a, (guint32) len);
    wr_raw (a, data, (guint) len);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

GByteArray *
far2l_dnd_encode_event (const guint8 *binding, const guint8 *offer, gint16 x, gint16 y,
                        guint32 modifiers, guint16 flags)
{
    GByteArray *a = g_byte_array_new ();

    wr_u8 (a, F2L_INPUT_DND);
    wr_id (a, binding);
    wr_id (a, offer);
    wr_u16 (a, (guint16) x);
    wr_u16 (a, (guint16) y);
    wr_u32 (a, modifiers);
    wr_u16 (a, flags);
    return a;
}

/* --------------------------------------------------------------------------------------------- */

static char *
frame_with (const char *intro, const GByteArray *stack)
{
    char *b64 = g_base64_encode (stack->data, stack->len);
    char *frame = g_strconcat (intro, b64, "\a", NULL);

    g_free (b64);
    return frame;
}

char *
far2l_frame_reply (const GByteArray *stack)
{
    return frame_with (ESC_STR "_far2l", stack);
}

char *
far2l_frame_event (const GByteArray *stack)
{
    return frame_with (ESC_STR "_f2l", stack);
}

/* --------------------------------------------------------------------------------------------- */

GByteArray *
far2l_encode_key (gboolean down, guint32 ch, guint32 control_state, guint16 scan, guint16 vk,
                  guint16 repeat)
{
    GByteArray *a = g_byte_array_new ();

    wr_u8 (a, down ? 'K' : 'k');
    wr_u32 (a, ch);
    wr_u32 (a, control_state);
    wr_u16 (a, scan);
    wr_u16 (a, vk);
    wr_u16 (a, repeat);
    return a;
}

/* --------------------------------------------------------------------------------------------- */
