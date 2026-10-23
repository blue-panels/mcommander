/*
   src/mcterm - mcterm as a far2l terminal: drag and drop passed on, keys as events

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

#define TEST_SUITE_NAME "/src/mcterm"

#include "tests/mctest.h"

#include <string.h>

#include "lib/tty/key.h"
#include "src/mcterm/mcterm_far2l.h"

/* --------------------------------------------------------------------------------------------- */

/* the terminal outside, as far as the test needs it */
static gboolean outer_bound = TRUE;
static int outer_closed = -1;
static guint8 outer_closed_offer[FAR2L_ID_LEN];
static const char outer_data[] = "hello";

static gboolean
fake_is_bound (void)
{
    return outer_bound;
}

static int
fake_list (const guint8 *offer, GPtrArray *entries)
{
    static const struct
    {
        guint64 id;
        guint8 kind;
        guint16 flags;
        const char *name;
    } files[] = { { 10, 1, FAR2L_DND_ITEM_STREAM | FAR2L_DND_ITEM_SIZE_KNOWN, "a.txt" },
                  { 11, 2, FAR2L_DND_ITEM_STREAM, "a directory" },
                  { 12, 1, FAR2L_DND_ITEM_STREAM, "../evil" },
                  { 13, 1, 0, "not readable from the start" },
                  { 14, 1, FAR2L_DND_ITEM_STREAM, "b.bin" } };
    gsize i;

    (void) offer;
    for (i = 0; i < G_N_ELEMENTS (files); i++)
    {
        far2l_dnd_entry_t *e = g_new0 (far2l_dnd_entry_t, 1);

        e->item_id = files[i].id;
        e->kind = files[i].kind;
        e->flags = files[i].flags;
        e->size = 5;
        e->name = g_strdup (files[i].name);
        g_ptr_array_add (entries, e);
    }
    return FAR2L_OK;
}

static int
fake_read (const guint8 *offer, guint64 item_id, guint64 offset, guint32 length, guint8 **data,
           gsize *data_len, gboolean *eof)
{
    gsize left = offset >= 5 ? 0 : 5 - (gsize) offset;
    gsize n = MIN (left, (gsize) length);

    (void) offer;
    (void) item_id;
    *data = g_memdup2 (outer_data + offset, n > 0 ? n : 1);
    *data_len = n;
    *eof = offset + n >= 5;
    return FAR2L_OK;
}

static void
fake_close (const guint8 *offer, guint8 reason)
{
    memcpy (outer_closed_offer, offer, FAR2L_ID_LEN);
    outer_closed = reason;
}

static guint32
fake_max_chunk (void)
{
    return 3;  // the outer terminal gives less than the program asked for
}

static const mcterm_far2l_parent_t fake_parent = {
    .is_bound = fake_is_bound,
    .list = fake_list,
    .read = fake_read,
    .close = fake_close,
    .max_chunk = fake_max_chunk,
};

/* mc's clipboard, as far as the test needs it */
static GString *fake_clip = NULL;
static GPtrArray *fake_remembered = NULL;

static char *
fake_clip_get (gsize *len)
{
    *len = fake_clip->len;
    return fake_clip->len > 0 ? g_strndup (fake_clip->str, fake_clip->len) : NULL;
}

static void
fake_clip_set (const char *text, gsize len)
{
    g_string_assign (fake_clip, "");
    g_string_append_len (fake_clip, text, (gssize) len);
}

static gboolean
fake_clip_has_text (void)
{
    return fake_clip->len > 0;
}

static gboolean
fake_clip_remembered (const char *client_id)
{
    guint i;

    for (i = 0; i < fake_remembered->len; i++)
        if (strcmp (g_ptr_array_index (fake_remembered, i), client_id) == 0)
            return TRUE;
    return FALSE;
}

static void
fake_clip_remember (const char *client_id)
{
    g_ptr_array_add (fake_remembered, g_strdup (client_id));
}

static const mcterm_far2l_parent_t clip_parent = {
    .is_bound = fake_is_bound,
    .list = fake_list,
    .read = fake_read,
    .close = fake_close,
    .max_chunk = fake_max_chunk,
    .clip_get = fake_clip_get,
    .clip_set = fake_clip_set,
    .clip_has_text = fake_clip_has_text,
    .clip_remembered = fake_clip_remembered,
    .clip_remember = fake_clip_remember,
};

/* --------------------------------------------------------------------------------------------- */

/* the body of a request APC: what lies between ESC _ and BEL */

static char *
request_body (GByteArray *stack)
{
    char *frame = far2l_frame_request (stack);
    char *body = g_strndup (frame + 2, strlen (frame) - 3);

    g_free (frame);
    g_byte_array_free (stack, TRUE);
    return body;
}

/* the stack of a frame the terminal wrote: ESC _ far2l<base64> BEL or ESC _ f2l<base64> BEL */

static GByteArray *
frame_stack (const char *frame, gsize intro)
{
    GByteArray *st = g_byte_array_new ();
    const gsize len = strlen (frame);

    ck_assert (len > intro + 1);
    ck_assert (frame[len - 1] == '\a');
    ck_assert (far2l_frame_decode (frame + intro, len - intro - 1, st));
    return st;
}

/* --------------------------------------------------------------------------------------------- */

static int
ask (mcterm_far2l_t *f, GByteArray *request, far2l_reply_t *reply, GByteArray **keep)
{
    char *body = request_body (request);
    char *answer = mcterm_far2l_apc (f, body, strlen (body));

    g_free (body);
    ck_assert_ptr_nonnull (answer);
    ck_assert (strncmp (answer, "\033_far2l", 7) == 0);
    *keep = frame_stack (answer, 7);
    g_free (answer);
    ck_assert (far2l_dnd_decode_reply ((*keep)->data, (*keep)->len, reply));
    return reply->status;
}

/* --------------------------------------------------------------------------------------------- */

static mcterm_far2l_t *
bound_terminal (guint8 *binding, far2l_dnd_grant_t *grant)
{
    mcterm_far2l_t *f = mcterm_far2l_new (&fake_parent);
    far2l_reply_t reply;
    GByteArray *keep;
    int i;

    for (i = 0; i < FAR2L_ID_LEN; i++)
        binding[i] = (guint8) (0xA0 + i);

    /* the emulator answers far2l1, the terminal only takes note of it */
    ck_assert_ptr_null (mcterm_far2l_apc (f, "far2l1", 6));

    ck_assert_int_eq (
        ask (f, far2l_dnd_encode_bind (1, TRUE, binding, 65536, 32768, 4, 3), &reply, &keep),
        FAR2L_OK);
    ck_assert (far2l_dnd_decode_bind_reply (&reply, 1, binding, 65536, 32768, 4, 3, grant));
    g_byte_array_free (keep, TRUE);
    return f;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_mcterm_far2l_negotiation)
{
    mcterm_far2l_t *f = mcterm_far2l_new (&fake_parent);

    /* the answer to far2l1 is the emulator's; here it only makes the program one that may bind */
    ck_assert (!mcterm_far2l_on (f));
    ck_assert_ptr_null (mcterm_far2l_apc (f, "far2l1", 6));
    ck_assert (mcterm_far2l_on (f));
    ck_assert (!mcterm_far2l_bound (f));

    /* far2l0 takes them back */
    ck_assert_ptr_null (mcterm_far2l_apc (f, "far2l0", 6));
    ck_assert (!mcterm_far2l_on (f));

    /* other APCs are not ours */
    ck_assert_ptr_null (mcterm_far2l_apc (f, "Gi=1;", 5));
    mcterm_far2l_free (f);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_mcterm_far2l_bind)
{
    guint8 binding[FAR2L_ID_LEN];
    far2l_dnd_grant_t grant;
    far2l_reply_t reply;
    GByteArray *keep;
    mcterm_far2l_t *f = bound_terminal (binding, &grant);

    ck_assert (mcterm_far2l_bound (f));
    ck_assert_int_eq (grant.window, 1);
    ck_assert_int_eq ((int) grant.features, FAR2L_DND_FEATURE_STREAM);
    ck_assert_int_le ((int) grant.max_chunk, 32768);

    /* the same again is safe */
    ck_assert_int_eq (
        ask (f, far2l_dnd_encode_bind (2, TRUE, binding, 65536, 32768, 4, 3), &reply, &keep),
        FAR2L_OK);
    g_byte_array_free (keep, TRUE);
    /* something else for the same binding is not */
    ck_assert_int_eq (
        ask (f, far2l_dnd_encode_bind (3, TRUE, binding, 65536, 1024, 4, 3), &reply, &keep),
        FAR2L_E_BAD_REQUEST);
    far2l_reply_clear (&reply);
    g_byte_array_free (keep, TRUE);
    /* limits below the minimum */
    binding[0] ^= 1;
    ck_assert_int_eq (
        ask (f, far2l_dnd_encode_bind (4, TRUE, binding, 100, 1, 1, 1), &reply, &keep),
        FAR2L_E_BAD_REQUEST);
    far2l_reply_clear (&reply);
    g_byte_array_free (keep, TRUE);
    /* no representation it can serve */
    ck_assert_int_eq (
        ask (f, far2l_dnd_encode_bind (5, TRUE, binding, 65536, 1024, 1, 2), &reply, &keep),
        FAR2L_E_UNSUPPORTED);
    far2l_reply_clear (&reply);
    g_byte_array_free (keep, TRUE);

    /* another interaction family is answered with nothing after the RID */
    {
        const guint8 clip[] = { 'c', 9 };
        GByteArray *st = g_byte_array_new ();
        char *body, *answer;

        g_byte_array_append (st, clip, sizeof (clip));
        body = request_body (st);
        answer = mcterm_far2l_apc (f, body, strlen (body));
        keep = frame_stack (answer, 7);
        ck_assert (far2l_dnd_decode_reply (keep->data, keep->len, &reply));
        ck_assert_int_eq (reply.rid, 9);
        ck_assert_int_eq (reply.status, FAR2L_E_NO_DND);
        g_byte_array_free (keep, TRUE);
        g_free (answer);
        g_free (body);
    }

    mcterm_far2l_free (f);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_mcterm_far2l_drop)
{
    guint8 binding[FAR2L_ID_LEN], outer_offer[FAR2L_ID_LEN];
    far2l_dnd_grant_t grant;
    far2l_reply_t reply;
    GByteArray *keep, *ev;
    far2l_dnd_event_t event;
    far2l_drop_t drop;
    GPtrArray *entries = g_ptr_array_new_with_free_func (far2l_dnd_entry_free);
    mcterm_far2l_t *f = bound_terminal (binding, &grant);
    far2l_dnd_entry_t *e;
    guint64 next = 0;
    guint8 flags;
    guint64 observed;
    const guint8 *data;
    gsize len;
    char *out = NULL;

    memset (outer_offer, 0x5A, sizeof (outer_offer));
    memset (&drop, 0, sizeof (drop));
    memcpy (drop.offer, outer_offer, FAR2L_ID_LEN);
    outer_closed = -1;

    /* the event names an offer of its own, at the cell of the program's terminal */
    ck_assert (mcterm_far2l_drop (f, &drop, 7, 3, &out));
    ck_assert_ptr_nonnull (out);
    ev = frame_stack (out, 5);
    g_free (out);
    ck_assert (far2l_dnd_decode_event (ev->data, ev->len, &event));
    g_byte_array_free (ev, TRUE);
    ck_assert_int_eq (memcmp (event.binding, binding, FAR2L_ID_LEN), 0);
    ck_assert_int_eq (memcmp (event.offer, outer_offer, FAR2L_ID_LEN) != 0, 1);
    ck_assert_int_eq (event.x, 7);
    ck_assert_int_eq (event.y, 3);

    /* LIST: only the files that can be read from the start, under names that are names */
    ck_assert_int_eq (ask (f, far2l_dnd_encode_list (10, event.offer, 0), &reply, &keep), FAR2L_OK);
    ck_assert (far2l_dnd_decode_list_reply (&reply, &next, entries));
    g_byte_array_free (keep, TRUE);
    ck_assert (next == FAR2L_DND_LAST_PAGE);
    ck_assert_int_eq ((int) entries->len, 2);
    e = g_ptr_array_index (entries, 0);
    ck_assert_str_eq (e->name, "a.txt");
    ck_assert_int_eq ((int) e->item_id, 10);
    ck_assert (e->flags & FAR2L_DND_ITEM_STREAM);
    ck_assert (e->flags & FAR2L_DND_ITEM_SIZE_KNOWN);
    e = g_ptr_array_index (entries, 1);
    ck_assert_str_eq (e->name, "b.bin");

    /* a cursor that was never given, a parent that does not exist */
    ck_assert_int_eq (ask (f, far2l_dnd_encode_list (11, event.offer, 99), &reply, &keep),
                      FAR2L_E_UNKNOWN_ITEM);
    far2l_reply_clear (&reply);
    g_byte_array_free (keep, TRUE);

    /* READ is answered with a READ of the outer offer, and the short answer is a valid one */
    ck_assert_int_eq (ask (f, far2l_dnd_encode_read (12, event.offer, 10, 0, 100), &reply, &keep),
                      FAR2L_OK);
    ck_assert (far2l_dnd_decode_read_reply (&reply, 100, &observed, &flags, &data, &len));
    ck_assert_int_eq ((int) len, 3);
    ck_assert_int_eq (memcmp (data, "hel", 3), 0);
    ck_assert_int_eq (flags & FAR2L_DND_READ_EOF, 0);
    ck_assert_int_eq ((int) observed, 5);
    g_byte_array_free (keep, TRUE);
    ck_assert_int_eq (ask (f, far2l_dnd_encode_read (13, event.offer, 10, 3, 100), &reply, &keep),
                      FAR2L_OK);
    ck_assert (far2l_dnd_decode_read_reply (&reply, 100, &observed, &flags, &data, &len));
    ck_assert_int_eq ((int) len, 2);
    ck_assert_int_eq (memcmp (data, "lo", 2), 0);
    ck_assert (flags & FAR2L_DND_READ_EOF);
    g_byte_array_free (keep, TRUE);

    /* zero is not "everything", an item nobody listed is not there, an offer nobody made neither */
    ck_assert_int_eq (ask (f, far2l_dnd_encode_read (14, event.offer, 10, 0, 0), &reply, &keep),
                      FAR2L_E_BAD_REQUEST);
    far2l_reply_clear (&reply);
    g_byte_array_free (keep, TRUE);
    ck_assert_int_eq (ask (f, far2l_dnd_encode_read (15, event.offer, 12, 0, 10), &reply, &keep),
                      FAR2L_E_UNKNOWN_ITEM);
    far2l_reply_clear (&reply);
    g_byte_array_free (keep, TRUE);
    ck_assert_int_eq (ask (f, far2l_dnd_encode_read (16, outer_offer, 10, 0, 10), &reply, &keep),
                      FAR2L_E_OFFER_GONE);
    far2l_reply_clear (&reply);
    g_byte_array_free (keep, TRUE);

    /* CLOSE releases the outer offer with the reason, and repeating it is a success */
    ck_assert_int_lt (outer_closed, 0);
    ck_assert_int_eq (
        ask (f, far2l_dnd_encode_close (17, event.offer, FAR2L_DND_CLOSE_PROCESSED), &reply, &keep),
        FAR2L_OK);
    g_byte_array_free (keep, TRUE);
    ck_assert_int_eq (outer_closed, FAR2L_DND_CLOSE_PROCESSED);
    ck_assert_int_eq (memcmp (outer_closed_offer, outer_offer, FAR2L_ID_LEN), 0);
    ck_assert_int_eq (
        ask (f, far2l_dnd_encode_close (18, event.offer, FAR2L_DND_CLOSE_PROCESSED), &reply, &keep),
        FAR2L_OK);
    g_byte_array_free (keep, TRUE);
    ck_assert_int_eq (ask (f, far2l_dnd_encode_list (19, event.offer, 0), &reply, &keep),
                      FAR2L_E_OFFER_GONE);
    far2l_reply_clear (&reply);
    g_byte_array_free (keep, TRUE);

    /* an offer still open when the terminal goes is given back, cancelled */
    outer_closed = -1;
    ck_assert (mcterm_far2l_drop (f, &drop, -1, -1, &out));
    g_free (out);
    mcterm_far2l_free (f);
    ck_assert_int_eq (outer_closed, FAR2L_DND_CLOSE_CANCELLED);
    g_ptr_array_free (entries, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_mcterm_far2l_no_drop_without_binding)
{
    mcterm_far2l_t *f = mcterm_far2l_new (&fake_parent);
    far2l_drop_t drop;
    char *out = NULL;

    memset (&drop, 0, sizeof (drop));
    ck_assert (!mcterm_far2l_drop (f, &drop, 1, 1, &out));
    ck_assert_ptr_null (mcterm_far2l_apc (f, "far2l1", 6));
    /* the extensions are on but the program has not asked for drops */
    ck_assert (!mcterm_far2l_drop (f, &drop, 1, 1, &out));
    ck_assert_ptr_null (out);
    mcterm_far2l_free (f);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

/* The clipboard: a request is its arguments, the sub-command, 'c' and the RID, pushed in this
   order; the answer is popped from the end */

static void
push_le (GByteArray *a, guint64 v, gsize n)
{
    gsize i;

    for (i = 0; i < n; i++)
    {
        const guint8 b = (guint8) (v >> (8 * i));

        g_byte_array_append (a, &b, 1);
    }
}

static guint64
pop_le (GByteArray *a, gsize n)
{
    guint64 v = 0;
    gsize i;

    ck_assert_uint_ge (a->len, n);
    for (i = n; i > 0; i--)
        v = (v << 8) | a->data[a->len - n + i - 1];
    g_byte_array_set_size (a, a->len - n);
    return v;
}

/* NULL when the terminal does not answer (now) */
static GByteArray *
clip (mcterm_far2l_t *f, GByteArray *args, char sub, guint8 rid)
{
    char *body, *answer;
    GByteArray *st = NULL;

    push_le (args, (guint8) sub, 1);
    push_le (args, 'c', 1);
    push_le (args, rid, 1);
    body = request_body (args);
    answer = mcterm_far2l_apc (f, body, strlen (body));
    g_free (body);
    if (answer != NULL)
    {
        ck_assert (strncmp (answer, "\033_far2l", 7) == 0);
        st = frame_stack (answer, 7);
        g_free (answer);
        ck_assert_uint_eq (pop_le (st, 1), rid);
    }
    return st;
}

static GByteArray *
clip_open_args (const char *client)
{
    GByteArray *a = g_byte_array_new ();

    g_byte_array_append (a, (const guint8 *) client, (guint) strlen (client));
    push_le (a, strlen (client), 4);
    return a;
}

static GByteArray *
clip_fmt_args (guint32 fmt)
{
    GByteArray *a = g_byte_array_new ();

    push_le (a, fmt, 4);
    return a;
}

static GByteArray *
clip_set_args (guint32 fmt, const char *data, gsize len)
{
    GByteArray *a = g_byte_array_new ();

    g_byte_array_append (a, (const guint8 *) data, (guint) len);
    push_le (a, len, 4);
    push_le (a, fmt, 4);
    return a;
}

/* status and features of an OPEN answer */
static gint8
open_status (GByteArray *st)
{
    const gint8 status = (gint8) pop_le (st, 1);

    ck_assert_uint_eq (pop_le (st, 8), 3);  // data IDs and chunks
    g_byte_array_free (st, TRUE);
    return status;
}

/* the text a GETDATA gives, "" for none; -1 in *size when the clipboard is not open */
static char *
get_text (mcterm_far2l_t *f, guint8 rid, gint64 *size)
{
    GByteArray *st = clip (f, clip_fmt_args (1), 'g', rid);
    guint32 n;
    char *text;

    ck_assert_ptr_nonnull (st);
    n = (guint32) pop_le (st, 4);
    *size = n == 0xFFFFFFFFu ? -1 : (gint64) n;
    if (n == 0xFFFFFFFFu)
        n = 0;
    ck_assert_uint_ge (st->len, n);
    text = g_strndup ((const char *) st->data + st->len - n, n);
    g_byte_array_set_size (st, st->len - n);
    if (*size >= 0)
        ck_assert_uint_eq (pop_le (st, 8) != 0, n > 0);  // the data ID
    ck_assert_uint_eq (st->len, 0);
    g_byte_array_free (st, TRUE);
    return text;
}

/* --------------------------------------------------------------------------------------------- */

static const char client_a[] = "mc-test-client-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static const char client_b[] = "mc-test-client-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

START_TEST (test_mcterm_far2l_clipboard_access)
{
    mcterm_far2l_t *f;
    char *answer;
    GByteArray *st;

    fake_clip = g_string_new ("");
    fake_remembered = g_ptr_array_new_with_free_func (g_free);
    f = mcterm_far2l_new (&clip_parent);

    /* nothing before the extensions are on */
    ck_assert_ptr_null (clip (f, clip_open_args (client_a), 'o', 1));
    ck_assert_ptr_null (mcterm_far2l_apc (f, "far2l1", 6));

    /* a client ID the protocol does not allow fails at once */
    ck_assert_int_eq (open_status (clip (f, clip_open_args ("short"), 'o', 1)), 0);
    ck_assert_int_eq (
        open_status (clip (f, clip_open_args ("UPPER-case-is-not-allowed-here-x"), 'o', 1)), 0);

    /* the first OPEN waits for the user; another one meanwhile fails */
    ck_assert_ptr_null (clip (f, clip_open_args (client_a), 'o', 2));
    ck_assert (mcterm_far2l_question_pending (f));
    ck_assert_int_eq (open_status (clip (f, clip_open_args (client_a), 'o', 3)), 0);

    /* "Block" */
    answer = mcterm_far2l_answer (f, MCTERM_F2L_CLIP_BLOCK);
    st = frame_stack (answer, 7);
    g_free (answer);
    ck_assert_uint_eq (pop_le (st, 1), 2);
    ck_assert_int_eq (open_status (st), 0);
    ck_assert (!mcterm_far2l_question_pending (f));
    ck_assert_ptr_null (mcterm_far2l_answer (f, MCTERM_F2L_CLIP_ALLOW));

    /* "Own clipboard" */
    ck_assert_ptr_null (clip (f, clip_open_args (client_a), 'o', 4));
    answer = mcterm_far2l_answer (f, MCTERM_F2L_CLIP_OWN);
    st = frame_stack (answer, 7);
    g_free (answer);
    ck_assert_uint_eq (pop_le (st, 1), 4);
    ck_assert_int_eq (open_status (st), -1);

    /* "Allow": open at once, and the next OPEN is not asked about */
    ck_assert_ptr_null (clip (f, clip_open_args (client_a), 'o', 5));
    answer = mcterm_far2l_answer (f, MCTERM_F2L_CLIP_ALLOW);
    st = frame_stack (answer, 7);
    g_free (answer);
    ck_assert_uint_eq (pop_le (st, 1), 5);
    ck_assert_int_eq (open_status (st), 1);
    st = clip (f, g_byte_array_new (), 'c', 6);
    ck_assert_int_eq ((gint8) pop_le (st, 1), 1);
    g_byte_array_free (st, TRUE);
    ck_assert_int_eq (open_status (clip (f, clip_open_args (client_a), 'o', 7)), 1);
    ck_assert_uint_eq (fake_remembered->len, 0);

    /* far2l0 forgets it; "Allow always" is remembered past it */
    ck_assert_ptr_null (mcterm_far2l_apc (f, "far2l0", 6));
    ck_assert_ptr_null (mcterm_far2l_apc (f, "far2l1", 6));
    ck_assert_ptr_null (clip (f, clip_open_args (client_a), 'o', 8));
    answer = mcterm_far2l_answer (f, MCTERM_F2L_CLIP_ALWAYS);
    g_free (answer);
    ck_assert_uint_eq (fake_remembered->len, 1);
    ck_assert_str_eq (g_ptr_array_index (fake_remembered, 0), client_a);
    ck_assert_ptr_null (mcterm_far2l_apc (f, "far2l0", 6));
    ck_assert_ptr_null (mcterm_far2l_apc (f, "far2l1", 6));
    ck_assert_int_eq (open_status (clip (f, clip_open_args (client_a), 'o', 9)), 1);

    /* another client is asked about on its own */
    st = clip (f, g_byte_array_new (), 'c', 10);
    g_byte_array_free (st, TRUE);
    ck_assert_ptr_null (clip (f, clip_open_args (client_b), 'o', 11));
    ck_assert (mcterm_far2l_question_pending (f));

    /* a program that gives the extensions up has its question dropped */
    ck_assert_ptr_null (mcterm_far2l_apc (f, "far2l0", 6));
    ck_assert (!mcterm_far2l_question_pending (f));
    ck_assert_ptr_null (mcterm_far2l_answer (f, MCTERM_F2L_CLIP_ALLOW));

    mcterm_far2l_free (f);
    g_ptr_array_free (fake_remembered, TRUE);
    g_string_free (fake_clip, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_mcterm_far2l_clipboard_data)
{
    static const char ef[] = { 0x44, 0x04, 0, 0 };  // U+0444 in UTF-32LE
    mcterm_far2l_t *f;
    GByteArray *st, *args;
    gint64 size;
    char *text;
    char chunk[256];

    fake_clip = g_string_new ("");
    fake_remembered = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add (fake_remembered, g_strdup (client_a));
    f = mcterm_far2l_new (&clip_parent);
    ck_assert_ptr_null (mcterm_far2l_apc (f, "far2l1", 6));

    /* nothing works on a clipboard that is not open */
    text = get_text (f, 1, &size);
    ck_assert_int_eq (size, -1);
    g_free (text);
    st = clip (f, clip_set_args (1, "x", 1), 's', 2);
    ck_assert_int_eq ((gint8) pop_le (st, 1), -1);
    g_byte_array_free (st, TRUE);

    ck_assert_int_eq (open_status (clip (f, clip_open_args (client_a), 'o', 3)), 1);
    // opens do not nest
    ck_assert_int_eq (open_status (clip (f, clip_open_args (client_a), 'o', 3)), 0);

    /* text set goes to mc's clipboard, with an ID of the data under the status */
    st = clip (f, clip_set_args (1, "hello", 5), 's', 4);
    ck_assert_int_eq ((gint8) pop_le (st, 1), 1);
    ck_assert_uint_ne (pop_le (st, 8), 0);
    g_byte_array_free (st, TRUE);
    ck_assert_str_eq (fake_clip->str, "hello");

    st = clip (f, clip_fmt_args (1), 'a', 5);
    ck_assert_uint_eq (pop_le (st, 1), 1);
    g_byte_array_free (st, TRUE);

    /* no paste key: the clipboard reads as empty, and its ID as none */
    text = get_text (f, 6, &size);
    ck_assert_int_eq (size, 0);
    g_free (text);
    st = clip (f, clip_fmt_args (1), 'i', 7);
    ck_assert_uint_eq (pop_le (st, 8), 0);
    g_byte_array_free (st, TRUE);

    /* Alt-Ctrl-V is no paste key; Ctrl+V is */
    mcterm_far2l_key (f, KEY_M_ALT | KEY_M_CTRL | 'v');
    text = get_text (f, 8, &size);
    ck_assert_int_eq (size, 0);
    g_free (text);
    mcterm_far2l_key (f, XCTRL ('v'));
    text = get_text (f, 9, &size);
    ck_assert_str_eq (text, "hello");
    g_free (text);
    st = clip (f, clip_fmt_args (1), 'i', 10);
    ck_assert_uint_ne (pop_le (st, 8), 0);
    g_byte_array_free (st, TRUE);

    /* data in chunks: the ones with RID 0 are not answered, the others with the RID alone */
    memset (chunk, 'x', sizeof (chunk));
    args = g_byte_array_new ();
    g_byte_array_append (args, (const guint8 *) chunk, sizeof (chunk));
    push_le (args, sizeof (chunk) >> 8, 2);
    ck_assert_ptr_null (clip (f, args, 'S', 0));
    args = g_byte_array_new ();
    g_byte_array_append (args, (const guint8 *) chunk, sizeof (chunk));
    push_le (args, sizeof (chunk) >> 8, 2);
    st = clip (f, args, 'S', 11);
    ck_assert_uint_eq (st->len, 0);
    g_byte_array_free (st, TRUE);
    st = clip (f, clip_set_args (1, "yz", 2), 's', 12);
    ck_assert_int_eq ((gint8) pop_le (st, 1), 1);
    g_byte_array_free (st, TRUE);
    ck_assert_uint_eq (fake_clip->len, 2 * sizeof (chunk) + 2);
    ck_assert (fake_clip->str[0] == 'x' && strcmp (fake_clip->str + 2 * sizeof (chunk), "yz") == 0);

    /* UTF-32 text both ways */
    st = clip (f, clip_set_args (13, ef, sizeof (ef)), 's', 13);
    ck_assert_int_eq ((gint8) pop_le (st, 1), 1);
    g_byte_array_free (st, TRUE);
    ck_assert_str_eq (fake_clip->str, "\xd1\x84");

    /* a format of its own, by name, keeps its data next to the text */
    args = g_byte_array_new ();
    g_byte_array_append (args, (const guint8 *) "FAR_VerticalBlock_Unicode", 25);
    push_le (args, 25, 4);
    st = clip (f, args, 'r', 14);
    ck_assert_uint_ge (pop_le (st, 4), 0xC000);
    g_byte_array_free (st, TRUE);

    /* CLOSE, and the clipboard is closed */
    st = clip (f, g_byte_array_new (), 'c', 15);
    ck_assert_int_eq ((gint8) pop_le (st, 1), 1);
    g_byte_array_free (st, TRUE);
    text = get_text (f, 16, &size);
    ck_assert_int_eq (size, -1);
    g_free (text);

    /* an unknown sub-command has the empty answer */
    st = clip (f, g_byte_array_new (), 'Z', 17);
    ck_assert_uint_eq (st->len, 0);
    g_byte_array_free (st, TRUE);

    mcterm_far2l_free (f);
    g_ptr_array_free (fake_remembered, TRUE);
    g_string_free (fake_clip, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_mcterm_far2l_clipboard_none)
{
    mcterm_far2l_t *f = mcterm_far2l_new (&fake_parent);
    GByteArray *st;

    /* a terminal with no clipboard to give tells the program to use its own */
    ck_assert_ptr_null (mcterm_far2l_apc (f, "far2l1", 6));
    ck_assert_int_eq (open_status (clip (f, clip_open_args (client_a), 'o', 1)), -1);
    st = clip (f, clip_fmt_args (1), 'g', 2);
    ck_assert_uint_eq (st->len, 0);
    g_byte_array_free (st, TRUE);
    mcterm_far2l_free (f);
}
END_TEST

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_mcterm_far2l_negotiation);
    tcase_add_test (tc_core, test_mcterm_far2l_bind);
    tcase_add_test (tc_core, test_mcterm_far2l_drop);
    tcase_add_test (tc_core, test_mcterm_far2l_no_drop_without_binding);
    tcase_add_test (tc_core, test_mcterm_far2l_clipboard_access);
    tcase_add_test (tc_core, test_mcterm_far2l_clipboard_data);
    tcase_add_test (tc_core, test_mcterm_far2l_clipboard_none);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
