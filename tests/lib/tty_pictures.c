/*
   lib - sixel decoder and kitty graphics commands

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

#define TEST_SUITE_NAME "/lib/tty"

#include "tests/mctest.h"

#include <string.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "lib/tty/kitty_graphics.h"
#include "lib/tty/sixel.h"

/* --------------------------------------------------------------------------------------------- */

#define DECODE(str, w, h) mc_sixel_decode (str, sizeof (str) - 1, w, h)

static guint32
pixel (const guint8 *rgba, int width, int x, int y)
{
    const guint8 *p = rgba + ((gsize) y * (gsize) width + (gsize) x) * 4;

    return ((guint32) p[0] << 24) | ((guint32) p[1] << 16) | ((guint32) p[2] << 8) | p[3];
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_decode_rgb_and_raster)
{
    // 4x12, transparent ground; red 2 columns in the first band, then green in the second
    static const char six[] = "\033P0;1;0q\"1;1;4;12#1;2;100;0;0#1!2~-#2;2;0;100;0#2~\033\\";
    int w, h;
    guint8 *rgba;

    rgba = DECODE (six, &w, &h);
    ck_assert_ptr_nonnull (rgba);
    ck_assert_int_eq (w, 4);
    ck_assert_int_eq (h, 12);
    ck_assert_uint_eq (pixel (rgba, w, 0, 0), 0xff0000ff);
    ck_assert_uint_eq (pixel (rgba, w, 1, 5), 0xff0000ff);
    ck_assert_uint_eq (pixel (rgba, w, 2, 0), 0);
    ck_assert_uint_eq (pixel (rgba, w, 0, 6), 0x00ff00ff);
    ck_assert_uint_eq (pixel (rgba, w, 1, 6), 0);
    ck_assert_uint_eq (pixel (rgba, w, 3, 11), 0);
    g_free (rgba);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_decode_bits_and_carriage_return)
{
    // '@' is bit 0 only, 'A' is bit 1 only; '$' goes back to the left over the same band
    static const char six[] = "\033P0;1q#0;2;0;0;100#0@$A\033\\";
    int w, h;
    guint8 *rgba;

    rgba = DECODE (six, &w, &h);
    ck_assert_ptr_nonnull (rgba);
    ck_assert_int_eq (w, 1);
    ck_assert_int_eq (h, 6);
    ck_assert_uint_eq (pixel (rgba, w, 0, 0), 0x0000ffff);
    ck_assert_uint_eq (pixel (rgba, w, 0, 1), 0x0000ffff);
    ck_assert_uint_eq (pixel (rgba, w, 0, 2), 0);
    g_free (rgba);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_decode_hls)
{
    // hue 120 is red with the DEC hues, 240 green, 0 blue
    static const char six[] = "\033P0;1q#1;1;120;50;100#1~#2;1;240;50;100#2~#3;1;0;50;100#3~\033\\";
    int w, h;
    guint8 *rgba;

    rgba = DECODE (six, &w, &h);
    ck_assert_ptr_nonnull (rgba);
    ck_assert_int_eq (w, 3);
    ck_assert_uint_eq (pixel (rgba, w, 0, 0), 0xff0000ff);
    ck_assert_uint_eq (pixel (rgba, w, 1, 0), 0x00ff00ff);
    ck_assert_uint_eq (pixel (rgba, w, 2, 0), 0x0000ffff);
    g_free (rgba);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_decode_ground_of_color_0)
{
    // P2 is 0: what is not drawn has color 0, black by default
    static const char six[] = "\033P0;0q\"1;1;2;6#1;2;100;100;100#1@\033\\";
    int w, h;
    guint8 *rgba;

    rgba = DECODE (six, &w, &h);
    ck_assert_ptr_nonnull (rgba);
    ck_assert_uint_eq (pixel (rgba, w, 0, 0), 0xffffffff);
    ck_assert_uint_eq (pixel (rgba, w, 1, 0), 0x000000ff);
    ck_assert_uint_eq (pixel (rgba, w, 0, 5), 0x000000ff);
    g_free (rgba);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_decode_clips_to_raster)
{
    // the data runs past the declared 2x6: the rest is dropped
    static const char six[] = "\033P0;1q\"1;1;2;6#1;2;100;0;0#1!10~-!10~\033\\";
    int w, h;
    guint8 *rgba;

    rgba = DECODE (six, &w, &h);
    ck_assert_ptr_nonnull (rgba);
    ck_assert_int_eq (w, 2);
    ck_assert_int_eq (h, 6);
    ck_assert_uint_eq (pixel (rgba, w, 1, 5), 0xff0000ff);
    g_free (rgba);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static const struct bad_ds
{
    const char *sixel;
} bad_ds[] = {
    { "" },
    { "\033Pq\033\\" },                      // nothing drawn
    { "\033P0;1;0" },                        // no 'q'
    { "P0;1q~~" },                           // no DCS
    { "\033Pq\"1;1;100000;100000~\033\\" },  // over the pixel cap
    { "\033Pq\"1;1;4096;1025~\033\\" },      // just over the cap
};

START_PARAMETRIZED_TEST (test_decode_bad, bad_ds)
{
    int w = -1, h = -1;

    ck_assert_ptr_null (mc_sixel_decode (data->sixel, strlen (data->sixel), &w, &h));
    ck_assert_int_eq (w, 0);
    ck_assert_int_eq (h, 0);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_decode_odd_input_does_not_break)
{
    // a color past the palette, a repeat with no character, garbage in the data
    static const char six[] = "\033P0;1q#999;2;1;2;3#999~!5$\x01zz#~\033\\";
    int w, h;
    guint8 *rgba;

    rgba = DECODE (six, &w, &h);
    ck_assert_ptr_nonnull (rgba);
    ck_assert_int_gt (w, 0);
    g_free (rgba);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_measure)
{
    int w, h;

    mc_sixel_measure ("\"1;1;16;32~", 11, &w, &h);
    ck_assert_int_eq (w, 16);
    ck_assert_int_eq (h, 32);

    mc_sixel_measure ("!7~$~~-~", 8, &w, &h);
    ck_assert_int_eq (w, 7);
    ck_assert_int_eq (h, 12);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_put_delete_free)
{
    GString *out = g_string_new (NULL);

    tty_kitty_put (out, 1073741825, 3, 2, 4);
    ck_assert_str_eq (out->str, "\033_Ga=p,i=1073741825,p=3,r=2,c=4,C=1,q=2\033\\");

    g_string_truncate (out, 0);
    tty_kitty_delete_placement (out, 7, 3);
    ck_assert_str_eq (out->str, "\033_Ga=d,d=i,i=7,p=3,q=2\033\\");

    g_string_truncate (out, 0);
    tty_kitty_free (out, 7);
    ck_assert_str_eq (out->str, "\033_Ga=d,d=I,i=7,q=2\033\\");

    g_string_free (out, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Join the chunks of a transmission back into the payload, checking the framing */
static GByteArray *
unchunk (const char *s, gboolean *compressed, int *chunks)
{
    GString *b64 = g_string_new (NULL);
    GByteArray *payload;
    guchar *raw;
    gsize raw_len;
    int more = 1;

    *chunks = 0;
    *compressed = FALSE;
    while (more)
    {
        const char *semi, *end;

        ck_assert (strncmp (s, "\033_G", 3) == 0);
        semi = strchr (s, ';');
        end = strstr (s, "\033\\");
        ck_assert_ptr_nonnull (semi);
        ck_assert_ptr_nonnull (end);
        if (*chunks == 0)
        {
            ck_assert (strncmp (s + 3, "a=t,i=9,f=32,s=", 15) == 0);
            *compressed = g_strstr_len (s, semi - s, "o=z") != NULL;
        }
        more = semi[-1] == '1';
        ck_assert_int_le (end - semi - 1, TTY_KITTY_CHUNK);
        g_string_append_len (b64, semi + 1, end - semi - 1);
        (*chunks)++;
        s = end + 2;
    }
    ck_assert_str_eq (s, "");

    raw = g_base64_decode (b64->str, &raw_len);
    payload = g_byte_array_new_take (raw, raw_len);
    g_string_free (b64, TRUE);
    return payload;
}

START_TEST (test_kitty_transmit_round_trip)
{
    // noise does not compress: the payload is many chunks
    const int w = 64, h = 64;
    const gsize size = (gsize) w * (gsize) h * 4;
    guint8 *rgba;
    GString *out = g_string_new (NULL);
    GByteArray *payload;
    gboolean compressed;
    int chunks;
    gsize i;

    rgba = g_malloc (size);
    for (i = 0; i < size; i++)
        rgba[i] = (guint8) g_random_int ();

    tty_kitty_transmit (out, 9, rgba, w, h);
    payload = unchunk (out->str, &compressed, &chunks);
    ck_assert_int_gt (chunks, 1);

#ifdef HAVE_ZLIB
    ck_assert (compressed);
    {
        uLongf len = (uLongf) size;
        guint8 *plain = g_malloc (size);

        ck_assert_int_eq (uncompress (plain, &len, payload->data, payload->len), Z_OK);
        ck_assert_uint_eq (len, size);
        ck_assert_int_eq (memcmp (plain, rgba, size), 0);
        g_free (plain);
    }
#else
    ck_assert (!compressed);
    ck_assert_uint_eq (payload->len, size);
    ck_assert_int_eq (memcmp (payload->data, rgba, size), 0);
#endif

    g_byte_array_unref (payload);
    g_string_free (out, TRUE);
    g_free (rgba);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_kitty_transmit_one_chunk)
{
    static const guint8 rgba[4] = { 1, 2, 3, 4 };
    GString *out = g_string_new (NULL);

    tty_kitty_transmit (out, 9, rgba, 1, 1);
    ck_assert (g_str_has_prefix (out->str, "\033_Ga=t,i=9,f=32,s=1,v=1,"));
    ck_assert (strstr (out->str, "q=2,m=0;") != NULL);
    ck_assert (g_str_has_suffix (out->str, "\033\\"));
    ck_assert (strstr (out->str + 1, "\033_G") == NULL);
    g_string_free (out, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_decode_rgb_and_raster);
    tcase_add_test (tc_core, test_decode_bits_and_carriage_return);
    tcase_add_test (tc_core, test_decode_hls);
    tcase_add_test (tc_core, test_decode_ground_of_color_0);
    tcase_add_test (tc_core, test_decode_clips_to_raster);
    mctest_add_parameterized_test (tc_core, test_decode_bad, bad_ds);
    tcase_add_test (tc_core, test_decode_odd_input_does_not_break);
    tcase_add_test (tc_core, test_measure);
    tcase_add_test (tc_core, test_kitty_put_delete_free);
    tcase_add_test (tc_core, test_kitty_transmit_round_trip);
    tcase_add_test (tc_core, test_kitty_transmit_one_chunk);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
