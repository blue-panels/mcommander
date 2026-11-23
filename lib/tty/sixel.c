/*
   Interface to the terminal controlling library for the M-Commander
   Sixel data: its size, and its pixels

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

#include <string.h>

#include "lib/global.h"

#include "sixel.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define PALETTE_SIZE 256
#define NUM_MAX      100000

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* VT340, in percent */
static const guint8 vt340_palette[16][3] = {
    { 0, 0, 0 },    { 20, 20, 80 }, { 80, 13, 13 }, { 20, 80, 20 }, { 80, 20, 80 }, { 20, 80, 80 },
    { 80, 80, 20 }, { 53, 53, 53 }, { 26, 26, 26 }, { 33, 33, 60 }, { 60, 26, 26 }, { 33, 60, 33 },
    { 60, 33, 60 }, { 33, 60, 60 }, { 60, 60, 33 }, { 80, 80, 80 },
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static guint8
percent (int v)
{
    v = CLAMP (v, 0, 100);
    return (guint8) ((v * 255 + 50) / 100);
}

/* --------------------------------------------------------------------------------------------- */

static double
hue_to_channel (double m1, double m2, double h)
{
    if (h < 0)
        h += 360;
    if (h >= 360)
        h -= 360;
    if (h < 60)
        return m1 + (m2 - m1) * h / 60;
    if (h < 180)
        return m2;
    if (h < 240)
        return m1 + (m2 - m1) * (240 - h) / 60;
    return m1;
}

/* --------------------------------------------------------------------------------------------- */

/* Hue in degrees with blue at 0, red at 120 and green at 240; lightness and saturation in
   percent */
static guint32
hls_to_rgb (int hue, int lightness, int saturation)
{
    const double l = CLAMP (lightness, 0, 100) / 100.0;
    const double s = CLAMP (saturation, 0, 100) / 100.0;
    const double h = (double) ((hue % 360 + 360 + 240) % 360);
    double m1, m2, r, g, b;

    if (saturation <= 0)
        r = g = b = l;
    else
    {
        m2 = l <= 0.5 ? l * (1 + s) : l + s - l * s;
        m1 = 2 * l - m2;
        r = hue_to_channel (m1, m2, h + 120);
        g = hue_to_channel (m1, m2, h);
        b = hue_to_channel (m1, m2, h - 120);
    }

    return ((guint32) (r * 255 + 0.5) << 16) | ((guint32) (g * 255 + 0.5) << 8)
        | (guint32) (b * 255 + 0.5);
}

/* --------------------------------------------------------------------------------------------- */

/* Up to @max numbers separated by ';', from @i on; returns how many were given */
static int
read_numbers (const char *data, gsize len, gsize *i, int *out, int max)
{
    int n = 0;
    gboolean have = FALSE;

    memset (out, 0, sizeof (int) * (size_t) max);

    while (*i < len)
    {
        const char c = data[*i];

        if (c >= '0' && c <= '9')
        {
            if (n < max && out[n] < NUM_MAX)
                out[n] = out[n] * 10 + (c - '0');
            have = TRUE;
        }
        else if (c == ';')
        {
            n++;
            have = TRUE;
        }
        else
            break;
        (*i)++;
    }

    return have ? MIN (n + 1, max) : 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_sixel_measure (const char *data, gsize len, int *width, int *height)
{
    gsize i = 0;
    int x = 0, w = 0, bands = 0;
    gboolean drew = FALSE;

    *width = 0;
    *height = 0;

    if (i < len && data[i] == '"')
    {
        int p[4];

        i++;
        if (read_numbers (data, len, &i, p, 4) >= 4 && p[2] > 0 && p[3] > 0)
        {
            *width = p[2];
            *height = p[3];
            return;
        }
    }

    for (; i < len; i++)
    {
        const char c = data[i];

        if (c >= '?' && c <= '~')
        {
            x++;
            drew = TRUE;
        }
        else if (c == '!')
        {
            int n = 0;

            for (i++; i < len && data[i] >= '0' && data[i] <= '9'; i++)
                if (n < NUM_MAX)
                    n = n * 10 + (data[i] - '0');
            if (i < len && data[i] >= '?' && data[i] <= '~')
            {
                x += (n > 0) ? n : 1;
                drew = TRUE;
            }
        }
        else if (c == '$' || c == '-')
        {
            w = MAX (w, x);
            x = 0;
            if (c == '-')
            {
                bands++;
                drew = FALSE;
            }
        }
        // '#' and its parameters, and anything else, take no space
    }

    w = MAX (w, x);
    if (drew)
        bands++;

    *width = w;
    *height = bands * 6;
}

/* --------------------------------------------------------------------------------------------- */

guint8 *
mc_sixel_decode (const char *dcs, gsize len, int *width, int *height)
{
    guint32 palette[PALETTE_SIZE];
    guint8 *rgba;
    const char *q, *data;
    gsize data_len, i;
    int params[3], w, h, x = 0, y = 0, color = 0, k;
    gboolean transparent;

    *width = 0;
    *height = 0;

    if (len < 3 || dcs[0] != '\033' || dcs[1] != 'P')
        return NULL;
    q = memchr (dcs + 2, 'q', len - 2);
    if (q == NULL)
        return NULL;

    i = 2;
    (void) read_numbers (dcs, (gsize) (q - dcs), &i, params, 3);
    transparent = params[1] == 1;

    data = q + 1;
    data_len = len - (gsize) (data - dcs);
    if (data_len >= 2 && data[data_len - 2] == '\033' && data[data_len - 1] == '\\')
        data_len -= 2;

    mc_sixel_measure (data, data_len, &w, &h);
    if (w <= 0 || h <= 0 || (gint64) w * h > MC_SIXEL_MAX_PIXELS)
        return NULL;

    for (k = 0; k < PALETTE_SIZE; k++)
        palette[k] = 0;
    for (k = 0; k < 16; k++)
        palette[k] = ((guint32) percent (vt340_palette[k][0]) << 16)
            | ((guint32) percent (vt340_palette[k][1]) << 8) | percent (vt340_palette[k][2]);

    rgba = g_malloc ((gsize) w * (gsize) h * 4);
    if (transparent)
        memset (rgba, 0, (gsize) w * (gsize) h * 4);
    else
        for (k = 0; k < w * h; k++)
        {
            rgba[k * 4] = (guint8) (palette[0] >> 16);
            rgba[k * 4 + 1] = (guint8) (palette[0] >> 8);
            rgba[k * 4 + 2] = (guint8) palette[0];
            rgba[k * 4 + 3] = 255;
        }

    for (i = 0; i < data_len;)
    {
        const char c = data[i];
        int repeat = 1;

        if (c == '#')
        {
            int p[5];
            int n;

            i++;
            n = read_numbers (data, data_len, &i, p, 5);
            if (n == 0)
                continue;
            color = p[0] % PALETTE_SIZE;
            if (n >= 5 && p[1] == 2)
                palette[color] = ((guint32) percent (p[2]) << 16) | ((guint32) percent (p[3]) << 8)
                    | percent (p[4]);
            else if (n >= 5 && p[1] == 1)
                palette[color] = hls_to_rgb (p[2], p[3], p[4]);
            continue;
        }
        if (c == '"')
        {
            int p[4];

            i++;
            (void) read_numbers (data, data_len, &i, p, 4);
            continue;
        }
        if (c == '$')
        {
            x = 0;
            i++;
            continue;
        }
        if (c == '-')
        {
            x = 0;
            y += 6;
            i++;
            continue;
        }
        if (c == '!')
        {
            int p[1];

            i++;
            (void) read_numbers (data, data_len, &i, p, 1);
            repeat = MAX (p[0], 1);
            if (i >= data_len)
                break;
        }

        if (data[i] >= '?' && data[i] <= '~')
        {
            const int bits = data[i] - '?';
            const guint32 rgb = palette[color];
            int bit;

            for (bit = 0; bit < 6 && bits != 0; bit++)
            {
                const int py = y + bit;
                int px;

                if ((bits & (1 << bit)) == 0 || py >= h)
                    continue;
                for (px = x; px < x + repeat && px < w; px++)
                {
                    guint8 *p = rgba + ((gsize) py * (gsize) w + (gsize) px) * 4;

                    p[0] = (guint8) (rgb >> 16);
                    p[1] = (guint8) (rgb >> 8);
                    p[2] = (guint8) rgb;
                    p[3] = 255;
                }
            }
            x = MIN (x + repeat, w);
        }
        i++;
    }

    *width = w;
    *height = h;
    return rgba;
}

/* --------------------------------------------------------------------------------------------- */
