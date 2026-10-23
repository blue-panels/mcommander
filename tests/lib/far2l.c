/*
   lib - far2l terminal extensions and drag and drop: the wire codec

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

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"

#include <string.h>

#include "lib/tty/far2l.h"

/* --------------------------------------------------------------------------------------------- */

static void
make_offer (guint8 *offer)
{
    int i;

    for (i = 0; i < FAR2L_ID_LEN; i++)
        offer[i] = (guint8) i;
}

/* --------------------------------------------------------------------------------------------- */

/* The bytes of the specification (section 10), made with far2l's own StackSerializer. */

START_TEST (test_far2l_read_request_vector)
{
    guint8 offer[FAR2L_ID_LEN];
    GByteArray *stack;
    char *frame;
    static const guint8 expected_stack[] = { 0x00, 0x80, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03,
                                             0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                             0x0c, 0x0d, 0x0e, 0x0f, 0x72, 0x64, 0x2a };

    make_offer (offer);
    stack = far2l_dnd_encode_read (42, offer, 7, 4096, 32768);
    frame = far2l_frame_request (stack);

    ck_assert_int_eq ((int) stack->len, (int) sizeof (expected_stack));
    ck_assert_int_eq (memcmp (stack->data, expected_stack, sizeof (expected_stack)), 0);
    ck_assert_str_eq (frame, "\033_far2l:AIAAAAAQAAAAAAAABwAAAAAAAAAAAQIDBAUGBwgJCgsMDQ4PcmQq\a");

    g_free (frame);
    g_byte_array_free (stack, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_read_reply_vector)
{
    /* LF, CR, ESC, BEL and 0xff are data, they do not end the APC */
    static const char body[] = "AAoNGwf/BgAAAAEAAAAAAAAAAAEq";
    static const guint8 expected[] = { 0x00, 0x0a, 0x0d, 0x1b, 0x07, 0xff };
    GByteArray *stack = g_byte_array_new ();
    far2l_reply_t reply;
    guint64 observed = 99;
    guint8 flags = 0;
    const guint8 *data = NULL;
    gsize len = 0;

    ck_assert (far2l_frame_decode (body, strlen (body), stack));
    ck_assert (far2l_dnd_decode_reply (stack->data, stack->len, &reply));
    ck_assert_int_eq (reply.rid, 42);
    ck_assert_int_eq (reply.status, FAR2L_OK);
    ck_assert (far2l_dnd_decode_read_reply (&reply, 32768, &observed, &flags, &data, &len));
    ck_assert_int_eq ((int) observed, 0);
    ck_assert_int_eq (flags, FAR2L_DND_READ_EOF);
    ck_assert_int_eq ((int) len, (int) sizeof (expected));
    ck_assert_int_eq (memcmp (data, expected, sizeof (expected)), 0);
    /* more than asked is refused, before anything is allocated */
    ck_assert (!far2l_dnd_decode_read_reply (&reply, 5, &observed, &flags, &data, &len));

    far2l_reply_clear (&reply);
    g_byte_array_free (stack, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_frame_decode)
{
    GByteArray *stack = g_byte_array_new ();

    /* base64 without its padding is tolerated, garbage is not */
    ck_assert (far2l_frame_decode ("YQpiAwAAAA", 10, stack));
    ck_assert_int_eq ((int) stack->len, 7);
    ck_assert_int_eq (memcmp (stack->data, "a\nb\3\0\0\0", 7), 0);
    ck_assert (!far2l_frame_decode ("YQ!i", 4, stack));
    ck_assert (!far2l_frame_decode ("", 0, stack));
    ck_assert (!far2l_frame_decode ("Y", 1, stack));

    g_byte_array_free (stack, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_reply_errors)
{
    /* RID 5, status -2, message "gone" */
    static const guint8 err[] = { 'g', 'o', 'n', 'e', 4, 0, 0, 0, 0xfe, 5 };
    /* RID only: a far2l that does not know the command */
    static const guint8 bare[] = { 7 };
    far2l_reply_t reply;

    ck_assert (far2l_dnd_decode_reply (err, sizeof (err), &reply));
    ck_assert_int_eq (reply.rid, 5);
    ck_assert_int_eq (reply.status, FAR2L_E_OFFER_GONE);
    ck_assert_str_eq (reply.message, "gone");
    far2l_reply_clear (&reply);

    ck_assert (far2l_dnd_decode_reply (bare, sizeof (bare), &reply));
    ck_assert_int_eq (reply.rid, 7);
    ck_assert_int_eq (reply.status, FAR2L_E_NO_DND);

    /* a message longer than its stack is refused */
    {
        static const guint8 lie[] = { 'g', 100, 0, 0, 0, 0xfe, 5 };

        ck_assert (!far2l_dnd_decode_reply (lie, sizeof (lie), &reply));
    }
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A stack written the way the specification lists it: every field goes in front. */

static void
put (GByteArray *a, const void *data, guint len)
{
    g_byte_array_prepend (a, data, len);
}

static void
put_int (GByteArray *a, guint64 v, guint bytes)
{
    guint8 b[8];
    guint i;

    for (i = 0; i < bytes; i++)
        b[i] = (guint8) ((v >> (8 * i)) & 0xff);
    put (a, b, bytes);
}

static void
put_str (GByteArray *a, const char *s)
{
    put_int (a, strlen (s), 4);
    put (a, s, (guint) strlen (s));
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
bind_reply (const guint8 *binding, guint32 max_frame, guint32 max_chunk, guint32 features,
            const guint8 *expect, guint32 wanted)
{
    GByteArray *b = g_byte_array_new ();
    far2l_reply_t reply;
    far2l_dnd_grant_t grant;
    gboolean ok;

    put_int (b, 1, 2);
    put (b, binding, FAR2L_ID_LEN);
    put_int (b, max_frame, 4);
    put_int (b, max_chunk, 4);
    put_int (b, 1, 2);
    put_int (b, 600, 4);
    put_int (b, features, 4);
    memset (&reply, 0, sizeof (reply));
    reply.status = FAR2L_OK;
    reply.body = b->data;
    reply.body_len = b->len;
    ok = far2l_dnd_decode_bind_reply (&reply, 1, expect, 65536, 32768, 1, wanted, &grant);
    if (ok)
    {
        ck_assert_int_eq ((int) grant.idle_seconds, 600);
        ck_assert_int_eq ((int) grant.max_chunk, (int) max_chunk);
    }
    g_byte_array_free (b, TRUE);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_bind)
{
    guint8 binding[FAR2L_ID_LEN], other[FAR2L_ID_LEN];
    GByteArray *req;

    make_offer (binding);
    make_offer (other);
    other[3] ^= 0xff;

    req = far2l_dnd_encode_bind (1, TRUE, binding, 65536, 32768, 1, FAR2L_DND_FEATURE_STREAM);
    /* RID, command and sub-command come out of the stack first */
    ck_assert_int_eq (req->data[req->len - 1], 1);
    ck_assert_int_eq (req->data[req->len - 2], 'd');
    ck_assert_int_eq (req->data[req->len - 3], 'b');
    g_byte_array_free (req, TRUE);

    ck_assert (bind_reply (binding, 65536, 32768, 1, binding, 1));
    /* the binding has to be echoed exactly */
    ck_assert (!bind_reply (other, 65536, 32768, 1, binding, 1));
    /* limits are the asked ones or lower */
    ck_assert (!bind_reply (binding, 131072, 32768, 1, binding, 1));
    /* a feature nobody asked for */
    ck_assert (!bind_reply (binding, 65536, 32768, 3, binding, 1));
    /* a chunk whose reply cannot fit the frame */
    ck_assert (!bind_reply (binding, 4096, 32768, 1, binding, 1));
    ck_assert (bind_reply (binding, 4096, 1024, 1, binding, 1));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static void
put_entry (GByteArray *page, guint64 id, const char *name)
{
    GByteArray *e = g_byte_array_new ();

    put_int (e, id, 8);
    put_int (e, FAR2L_DND_KIND_FILE, 1);
    put_int (e, FAR2L_DND_ITEM_STREAM | FAR2L_DND_ITEM_SIZE_KNOWN, 2);
    put_int (e, 5, 8);
    put_str (e, name);
    put_int (e, 0, 1);
    put_str (e, "");
    put_str (e, "");
    put_str (e, "");
    put_int (page, e->len, 4);
    put (page, e->data, e->len);
    g_byte_array_free (e, TRUE);
}

START_TEST (test_far2l_list)
{
    GByteArray *page = g_byte_array_new ();
    GPtrArray *entries = g_ptr_array_new_with_free_func (far2l_dnd_entry_free);
    far2l_reply_t reply;
    guint64 next = 0;
    far2l_dnd_entry_t *e;

    put_int (page, FAR2L_DND_LAST_PAGE, 8);
    put_int (page, 3, 4);
    put_entry (page, 1, "a.txt");
    put_entry (page, 0, "no item id");  // one unusable entry does not cost the page
    put_entry (page, 9, "\xd1\x84");
    memset (&reply, 0, sizeof (reply));
    reply.status = FAR2L_OK;
    reply.body = page->data;
    reply.body_len = page->len;

    ck_assert (far2l_dnd_decode_list_reply (&reply, &next, entries));
    ck_assert (next == FAR2L_DND_LAST_PAGE);
    ck_assert_int_eq ((int) entries->len, 2);
    e = g_ptr_array_index (entries, 0);
    ck_assert_int_eq ((int) e->item_id, 1);
    ck_assert_str_eq (e->name, "a.txt");
    ck_assert_int_eq ((int) e->size, 5);
    ck_assert (e->flags & FAR2L_DND_ITEM_STREAM);

    /* a page that is not the last one has an entry */
    g_byte_array_set_size (page, 0);
    put_int (page, 7, 8);
    put_int (page, 0, 4);
    reply.body = page->data;
    reply.body_len = page->len;
    ck_assert (!far2l_dnd_decode_list_reply (&reply, &next, entries));

    g_ptr_array_free (entries, TRUE);
    g_byte_array_free (page, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_event)
{
    guint8 binding[FAR2L_ID_LEN], offer[FAR2L_ID_LEN];
    guint8 stack[2 + 4 + 2 + 2 + FAR2L_ID_LEN + FAR2L_ID_LEN + 1];
    guint8 *p = stack;
    far2l_dnd_event_t ev;
    int i;

    make_offer (binding);
    make_offer (offer);
    offer[0] = 0x77;

    /* physically: flags, modifiers, y, x, offer, binding, 'D'; each little endian */
    *p++ = 1;
    *p++ = 0;  // flags: modifiers known
    *p++ = 0x10;
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;  // modifiers 0x10
    *p++ = 5;
    *p++ = 0;  // y
    *p++ = 40;
    *p++ = 0;  // x
    for (i = 0; i < FAR2L_ID_LEN; i++)
        *p++ = offer[i];
    for (i = 0; i < FAR2L_ID_LEN; i++)
        *p++ = binding[i];
    *p++ = 'D';

    ck_assert (far2l_dnd_decode_event (stack, sizeof (stack), &ev));
    ck_assert_int_eq (ev.x, 40);
    ck_assert_int_eq (ev.y, 5);
    ck_assert_int_eq ((int) ev.modifiers, 0x10);
    ck_assert_int_eq (ev.offer[0], 0x77);
    ck_assert_int_eq (memcmp (ev.binding, binding, FAR2L_ID_LEN), 0);

    /* a tail nobody knows what to do with is refused */
    {
        guint8 longer[sizeof (stack) + 1];

        longer[0] = 0;
        memcpy (longer + 1, stack, sizeof (stack));
        ck_assert (!far2l_dnd_decode_event (longer, sizeof (longer), &ev));
    }
    /* another event is not this one */
    stack[sizeof (stack) - 1] = 'K';
    ck_assert (!far2l_dnd_decode_event (stack, sizeof (stack), &ev));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_name_is_safe)
{
    ck_assert (far2l_dnd_name_is_safe ("file.txt"));
    ck_assert (far2l_dnd_name_is_safe ("\xd1\x84\xd0\xb0\xd0\xb9\xd0\xbb"));
    ck_assert (!far2l_dnd_name_is_safe (""));
    ck_assert (!far2l_dnd_name_is_safe ("."));
    ck_assert (!far2l_dnd_name_is_safe (".."));
    ck_assert (!far2l_dnd_name_is_safe ("a/b"));
    ck_assert (!far2l_dnd_name_is_safe ("..\\evil"));
    ck_assert (!far2l_dnd_name_is_safe ("a\nb"));
    ck_assert (!far2l_dnd_name_is_safe ("\xff"));
    ck_assert (!far2l_dnd_name_is_safe (NULL));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_far2l_read_request_vector);
    tcase_add_test (tc_core, test_far2l_read_reply_vector);
    tcase_add_test (tc_core, test_far2l_frame_decode);
    tcase_add_test (tc_core, test_far2l_reply_errors);
    tcase_add_test (tc_core, test_far2l_bind);
    tcase_add_test (tc_core, test_far2l_list);
    tcase_add_test (tc_core, test_far2l_event);
    tcase_add_test (tc_core, test_far2l_name_is_safe);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
