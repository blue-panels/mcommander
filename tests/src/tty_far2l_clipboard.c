/*
   src - unit tests for the far2l clipboard

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

#define TEST_SUITE_NAME "/src/tty_far2l_clipboard"

#include "tests/mctest.h"

#include <fcntl.h>

/* The real code talks to a terminal the test plays. */
#define tty_lowlevel_getch test_tty_lowlevel_getch
#define tty_nodelay        test_tty_nodelay
#define tty_unget_input    test_tty_unget_input
#define tty_raw_write      test_tty_raw_write
#include "lib/tty/key.c"
#undef tty_lowlevel_getch
#undef tty_nodelay
#undef tty_unget_input
#undef tty_raw_write

static int test_input[4096];
static size_t test_input_len;
static size_t test_input_pos;

// what the played terminal does
static gboolean term_refuses;
static gboolean term_silent;
static const char *term_clip_text;
static GString *term_got_text;
static int term_requests;
static int term_closes;

int
test_tty_lowlevel_getch (void)
{
    return test_input_pos < test_input_len ? test_input[test_input_pos++] : -1;
}

void
test_tty_nodelay (gboolean set)
{
    (void) set;
}

/* The bytes go back in front of what is still to be read. */
void
test_tty_unget_input (const unsigned char *data, size_t len)
{
    size_t i;

    ck_assert_uint_lt (test_input_len + len, G_N_ELEMENTS (test_input));
    memmove (test_input + test_input_pos + len, test_input + test_input_pos,
             (test_input_len - test_input_pos) * sizeof (test_input[0]));
    for (i = 0; i < len; i++)
        test_input[test_input_pos + i] = data[i];
    test_input_len += len;
}

static void
append_str (const char *s)
{
    while (*s != '\0')
        test_input[test_input_len++] = (unsigned char) *s++;
}

/* The answer: the values in the order they are pushed, the id last */
static void
answer (const GByteArray *stk)
{
    char *b64 = g_base64_encode (stk->data, stk->len);

    append_str ("\033_far2l");
    append_str (b64);
    append_str ("\a");
    g_free (b64);
}

static guint64
pop (GByteArray *a, unsigned int size)
{
    guint64 v = 0;

    ck_assert (far2l_clip_pop (a, size, &v));
    return v;
}

void
test_tty_raw_write (const char *data, size_t len)
{
    GByteArray *stk;
    gsize n = 0;
    guchar *raw;
    char *b64;
    guint8 id, cls, cmd;

    ck_assert_uint_gt (len, 11);
    ck_assert_int_eq (strncmp (data, "\033_far2l:", 8), 0);
    ck_assert_int_eq (data[len - 1], '\a');
    b64 = g_strndup (data + 8, len - 9);
    raw = g_base64_decode (b64, &n);
    g_free (b64);
    stk = g_byte_array_new_take (raw, n);

    id = (guint8) pop (stk, 1);
    cls = (guint8) pop (stk, 1);
    cmd = (guint8) pop (stk, 1);
    ck_assert_int_eq (cls, 'c');
    term_requests++;

    if (cmd == 'c')
        term_closes++;
    else if (!term_silent)
    {
        GByteArray *r = g_byte_array_new ();

        if (cmd == 'o')
        {
            const guint32 idlen = (guint32) pop (stk, 4);

            ck_assert_uint_ge (idlen, 32);  // the terminal wants a client id of 32 to 256 bytes
            ck_assert_uint_le (idlen, 256);
            far2l_push_u32 (r, 0);
            far2l_push_u32 (r, 0);
            far2l_push_u8 (r, term_refuses ? 0 : 1);
        }
        else if (cmd == 's')
        {
            const guint32 fmt = (guint32) pop (stk, 4);
            const guint32 tlen = (guint32) pop (stk, 4);

            ck_assert_uint_eq (fmt, 1);
            ck_assert_uint_eq (stk->len, tlen);
            if (term_got_text != NULL)
                g_string_free (term_got_text, TRUE);
            term_got_text = g_string_new_len ((const char *) stk->data, tlen);
            far2l_push_u32 (r, 0);
            far2l_push_u32 (r, 0);
            far2l_push_u8 (r, 1);
        }
        else if (cmd == 'g')
        {
            ck_assert_uint_eq ((guint32) pop (stk, 4), 1);
            // what the terminal pushes: a data id, the text, its length
            far2l_push_u32 (r, 0);
            far2l_push_u32 (r, 0);
            if (term_clip_text == NULL)
                far2l_push_u32 (r, 0xFFFFFFFF);
            else
            {
                g_byte_array_append (r, (const guint8 *) term_clip_text,
                                     strlen (term_clip_text) + 1);
                far2l_push_u32 (r, (guint32) strlen (term_clip_text) + 1);
            }
        }
        far2l_push_u8 (r, id);
        answer (r);
        g_byte_array_free (r, TRUE);
    }
    g_byte_array_free (stk, TRUE);
}

static void
setup (void)
{
    input_fd = open ("/dev/null", O_RDONLY);
    ck_assert_int_ge (input_fd, 0);
    test_input_len = test_input_pos = 0;
    term_refuses = term_silent = FALSE;
    term_clip_text = NULL;
    term_got_text = NULL;
    term_requests = term_closes = 0;
    far2l_input_active = TRUE;
    far2l_clip_timeout = 200 * 1000;
}

static void
teardown (void)
{
    far2l_input_active = FALSE;
    if (term_got_text != NULL)
        g_string_free (term_got_text, TRUE);
    close (input_fd);
    input_fd = -1;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_set_text)
{
    ck_assert (tty_far2l_clipboard_set ("hello\nworld", 11));
    ck_assert_ptr_ne (term_got_text, NULL);
    ck_assert_str_eq (term_got_text->str, "hello\nworld");
    ck_assert_int_eq (term_closes, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_get_text)
{
    char *text = NULL;
    size_t len = 0;

    term_clip_text = "from the terminal";
    ck_assert (tty_far2l_clipboard_get (&text, &len));
    ck_assert_str_eq (text, "from the terminal");
    ck_assert_uint_eq (len, 17);  // the NUL the terminal counted is not part of it
    g_free (text);
    ck_assert_int_eq (term_closes, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_get_nothing)
{
    char *text = NULL;
    size_t len = 0;

    term_clip_text = NULL;
    ck_assert (!tty_far2l_clipboard_get (&text, &len));
    ck_assert_ptr_eq (text, NULL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The terminal's user said no: nothing is set and nothing is read */
START_TEST (test_refused)
{
    char *text = NULL;
    size_t len = 0;

    term_refuses = TRUE;
    term_clip_text = "secret";
    ck_assert (!tty_far2l_clipboard_set ("x", 1));
    ck_assert (!tty_far2l_clipboard_get (&text, &len));
    ck_assert_ptr_eq (text, NULL);
    ck_assert_ptr_eq (term_got_text, NULL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A terminal that never answers is given up on after the timeout */
START_TEST (test_no_answer)
{
    char *text = NULL;
    size_t len = 0;

    term_silent = TRUE;
    ck_assert (!tty_far2l_clipboard_set ("x", 1));
    ck_assert (!tty_far2l_clipboard_get (&text, &len));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Without the extensions nothing is written to the terminal at all */
START_TEST (test_not_negotiated)
{
    char *text = NULL;
    size_t len = 0;

    far2l_input_active = FALSE;
    ck_assert (!tty_far2l_clipboard_available ());
    ck_assert (!tty_far2l_clipboard_set ("x", 1));
    ck_assert (!tty_far2l_clipboard_get (&text, &len));
    ck_assert_int_eq (term_requests, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Keys typed while the terminal thinks are not lost: they are read again after it */
START_TEST (test_typed_keys_survive)
{
    append_str ("ab");
    ck_assert (tty_far2l_clipboard_set ("x", 1));
    ck_assert_int_eq (test_tty_lowlevel_getch (), 'a');
    ck_assert_int_eq (test_tty_lowlevel_getch (), 'b');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_set_text);
    tcase_add_test (tc_core, test_get_text);
    tcase_add_test (tc_core, test_get_nothing);
    tcase_add_test (tc_core, test_refused);
    tcase_add_test (tc_core, test_no_answer);
    tcase_add_test (tc_core, test_not_negotiated);
    tcase_add_test (tc_core, test_typed_keys_survive);

    return mctest_run_all (tc_core);
}
