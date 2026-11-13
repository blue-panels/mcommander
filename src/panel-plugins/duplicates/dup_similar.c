/*
   Panel plugin duplicates for the M-Commander
   Fingerprints of the content for a search of similar files

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

/** \file dup_similar.c
 *  \brief Source: fingerprints of the content for a search of similar files
 *
 *  The perceptual hash: the picture in grey, reduced to 32x32, the DCT of it, and of the 8x8
 *  lowest frequencies a bit each: set when the value is above their median.  Two pictures that
 *  look alike differ in few bits, whatever their size, format and compression.
 */

#include <config.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef DUP_HAVE_JPEG
#include <setjmp.h>
#include <jpeglib.h>
#endif
#ifdef DUP_HAVE_PNG
#include <png.h>
#endif

#include "lib/global.h"
#include "lib/util.h"  // MC_PTR_FREE

#include "dup_similar.h"

/*** file scope macro definitions ****************************************************************/

#define LOW        8  // the side of the block of the lowest frequencies

#define TEXT_CHUNK (64 * 1024)

/*** file scope type declarations ****************************************************************/

/* A grey picture of any size */
typedef struct
{
    unsigned char *pixels;
    int width;
    int height;
    guint64 full_pixels;  // of the image before any scaling
} grey_image_t;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
is_jpeg (const unsigned char *head, size_t len)
{
    return len >= 3 && head[0] == 0xFF && head[1] == 0xD8 && head[2] == 0xFF;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
is_png (const unsigned char *head, size_t len)
{
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };

    return len >= sizeof (sig) && memcmp (head, sig, sizeof (sig)) == 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Each output value is the mean of the source pixels it covers */
static void
reduce (const grey_image_t *img, double *out)
{
    int ox, oy;

    for (oy = 0; oy < DUP_PHASH_SIDE; oy++)
    {
        const int y0 = oy * img->height / DUP_PHASH_SIDE;
        const int y1 = MAX (y0 + 1, (oy + 1) * img->height / DUP_PHASH_SIDE);

        for (ox = 0; ox < DUP_PHASH_SIDE; ox++)
        {
            const int x0 = ox * img->width / DUP_PHASH_SIDE;
            const int x1 = MAX (x0 + 1, (ox + 1) * img->width / DUP_PHASH_SIDE);
            double sum = 0;
            int x, y;

            for (y = y0; y < y1 && y < img->height; y++)
                for (x = x0; x < x1 && x < img->width; x++)
                    sum += img->pixels[(size_t) y * (size_t) img->width + (size_t) x];
            out[oy * DUP_PHASH_SIDE + ox] = sum / ((y1 - y0) * (x1 - x0));
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

#ifdef DUP_HAVE_JPEG
typedef struct
{
    struct jpeg_error_mgr pub;
    jmp_buf jump;
} jpeg_error_t;

static void
jpeg_error_exit (j_common_ptr cinfo)
{
    longjmp (((jpeg_error_t *) cinfo->err)->jump, 1);
}

static void
jpeg_silent (j_common_ptr cinfo)
{
    (void) cinfo;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
read_jpeg (const char *path, grey_image_t *img)
{
    struct jpeg_decompress_struct cinfo;
    jpeg_error_t err;
    FILE *f;
    volatile gboolean ok = FALSE;

    f = fopen (path, "rb");
    if (f == NULL)
        return FALSE;

    img->pixels = NULL;
    cinfo.err = jpeg_std_error (&err.pub);
    err.pub.error_exit = jpeg_error_exit;
    err.pub.output_message = jpeg_silent;

    if (setjmp (err.jump) == 0)
    {
        unsigned int denom = 8;
        JSAMPROW row;

        jpeg_create_decompress (&cinfo);
        jpeg_stdio_src (&cinfo, f);
        jpeg_read_header (&cinfo, TRUE);
        img->full_pixels = (guint64) cinfo.image_width * cinfo.image_height;

        /* the decoder scales by 1/2, 1/4 or 1/8 at almost no cost; at least 64 pixels stay */
        while (denom > 1
               && MIN (cinfo.image_width, cinfo.image_height) / denom < 2 * DUP_PHASH_SIDE)
            denom /= 2;
        cinfo.scale_num = 1;
        cinfo.scale_denom = denom;
        cinfo.out_color_space = JCS_GRAYSCALE;

        jpeg_start_decompress (&cinfo);
        img->width = (int) cinfo.output_width;
        img->height = (int) cinfo.output_height;
        img->pixels = g_malloc ((size_t) img->width * (size_t) img->height);
        while (cinfo.output_scanline < cinfo.output_height)
        {
            row = img->pixels + (size_t) cinfo.output_scanline * (size_t) img->width;
            jpeg_read_scanlines (&cinfo, &row, 1);
        }
        jpeg_finish_decompress (&cinfo);
        ok = TRUE;
    }
    else
        MC_PTR_FREE (img->pixels);

    jpeg_destroy_decompress (&cinfo);
    fclose (f);
    return ok;
}
#endif

/* --------------------------------------------------------------------------------------------- */

#ifdef DUP_HAVE_PNG
/* The grey of a JPEG is its luma Y; the same formula here keeps a PNG copy close to it */
static gboolean
read_png (const char *path, grey_image_t *img)
{
    png_image image;
    unsigned char *rgb;
    size_t i, n;

    memset (&image, 0, sizeof (image));
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file (&image, path))
        return FALSE;

    image.format = PNG_FORMAT_RGB;
    rgb = g_malloc (PNG_IMAGE_SIZE (image));
    if (!png_image_finish_read (&image, NULL, rgb, 0, NULL))
    {
        png_image_free (&image);
        g_free (rgb);
        return FALSE;
    }

    img->width = (int) image.width;
    img->height = (int) image.height;
    img->full_pixels = (guint64) image.width * image.height;
    n = (size_t) img->width * (size_t) img->height;
    img->pixels = g_malloc (n);
    for (i = 0; i < n; i++)
        img->pixels[i] =
            (unsigned char) ((299 * rgb[3 * i] + 587 * rgb[3 * i + 1] + 114 * rgb[3 * i + 2] + 500)
                             / 1000);
    g_free (rgb);
    return TRUE;
}
#endif

/* --------------------------------------------------------------------------------------------- */

static int
cmp_double (const void *a, const void *b)
{
    const double da = *(const double *) a;
    const double db = *(const double *) b;

    return da < db ? -1 : (da > db ? 1 : 0);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

dup_file_kind_t
dup_file_kind (const unsigned char *head, size_t len)
{
    if (is_jpeg (head, len) || is_png (head, len))
        return DUP_KIND_IMAGE;
    if (memchr (head, '\0', len) == NULL)
        return DUP_KIND_TEXT;
    return DUP_KIND_OTHER;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dup_images_readable (void)
{
#if defined(DUP_HAVE_JPEG) || defined(DUP_HAVE_PNG)
    return TRUE;
#else
    return FALSE;
#endif
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dup_image_supported (const unsigned char *head, size_t len)
{
#ifdef DUP_HAVE_JPEG
    if (is_jpeg (head, len))
        return TRUE;
#endif
#ifdef DUP_HAVE_PNG
    if (is_png (head, len))
        return TRUE;
#endif
    (void) head;
    (void) len;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

char *
dup_text_hash (const char *path, guint64 *bytes, gint *cancel)
{
    GChecksum *sum;
    FILE *f;
    unsigned char *buf;
    GString *spaces;
    guint newlines = 0;  // line ends not written yet: those at the end of the file are dropped
    gboolean cr = FALSE;
    size_t n;
    char *digest;

    *bytes = 0;
    f = fopen (path, "rb");
    if (f == NULL)
        return NULL;

    sum = g_checksum_new (G_CHECKSUM_SHA256);
    buf = g_malloc (TEXT_CHUNK);
    spaces = g_string_new (NULL);

    while ((cancel == NULL || g_atomic_int_get (cancel) == 0)
           && (n = fread (buf, 1, TEXT_CHUNK, f)) > 0)
    {
        size_t i;

        *bytes += n;
        for (i = 0; i < n; i++)
        {
            const unsigned char c = buf[i];

            if (cr)
            {
                cr = FALSE;
                newlines++;
                if (c == '\n')
                    continue;
            }

            if (c == '\r' || c == '\n')
            {
                g_string_truncate (spaces, 0);
                if (c == '\r')
                    cr = TRUE;
                else
                    newlines++;
            }
            else if (c == ' ' || c == '\t')
                g_string_append_c (spaces, (char) c);
            else
            {
                for (; newlines > 0; newlines--)
                    g_checksum_update (sum, (const guchar *) "\n", 1);
                if (spaces->len != 0)
                {
                    g_checksum_update (sum, (const guchar *) spaces->str, (gssize) spaces->len);
                    g_string_truncate (spaces, 0);
                }
                g_checksum_update (sum, &c, 1);
            }
        }
    }

    digest = ferror (f) || (cancel != NULL && g_atomic_int_get (cancel) != 0)
        ? NULL
        : g_strdup (g_checksum_get_string (sum));
    fclose (f);
    g_string_free (spaces, TRUE);
    g_free (buf);
    g_checksum_free (sum);
    return digest;
}

/* --------------------------------------------------------------------------------------------- */

guint64
dup_phash_of_grey (const double *grey)
{
    static double cosines[LOW][DUP_PHASH_SIDE];
    static gboolean ready = FALSE;
    double coef[LOW * LOW], sorted[LOW * LOW - 1];
    double median;
    guint64 hash = 0;
    int u, v, x, y;

    if (!ready)
    {
        for (u = 0; u < LOW; u++)
            for (x = 0; x < DUP_PHASH_SIDE; x++)
                cosines[u][x] = cos ((2 * x + 1) * u * G_PI / (2 * DUP_PHASH_SIDE));
        ready = TRUE;
    }

    for (v = 0; v < LOW; v++)
        for (u = 0; u < LOW; u++)
        {
            double s = 0;

            for (y = 0; y < DUP_PHASH_SIDE; y++)
            {
                double row = 0;

                for (x = 0; x < DUP_PHASH_SIDE; x++)
                    row += grey[y * DUP_PHASH_SIDE + x] * cosines[u][x];
                s += row * cosines[v][y];
            }
            coef[v * LOW + u] = s;
        }

    /* the mean brightness (0, 0) says nothing of the shape: out of the median */
    memcpy (sorted, coef + 1, sizeof (sorted));
    qsort (sorted, G_N_ELEMENTS (sorted), sizeof (sorted[0]), cmp_double);
    median = sorted[G_N_ELEMENTS (sorted) / 2];

    for (u = 0; u < LOW * LOW; u++)
        if (coef[u] > median)
            hash |= G_GUINT64_CONSTANT (1) << u;

    return hash;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dup_image_phash (const char *path, guint64 *hash, guint64 *pixels, guint64 *bytes)
{
    unsigned char head[8];
    size_t len = 0;
    grey_image_t img = { NULL, 0, 0, 0 };
    gboolean ok = FALSE;
    struct stat st;
    FILE *f;

    *bytes = 0;
    f = fopen (path, "rb");
    if (f == NULL)
        return FALSE;
    len = fread (head, 1, sizeof (head), f);
    fclose (f);
    (void) len;  // unused in a build with neither libjpeg nor libpng

#ifdef DUP_HAVE_JPEG
    if (is_jpeg (head, len))
        ok = read_jpeg (path, &img);
#endif
#ifdef DUP_HAVE_PNG
    if (is_png (head, len))
        ok = read_png (path, &img);
#endif

    if (ok && img.width > 0 && img.height > 0)
    {
        double grey[DUP_PHASH_SIDE * DUP_PHASH_SIDE];

        reduce (&img, grey);
        *hash = dup_phash_of_grey (grey);
        *pixels = img.full_pixels;
    }
    else
        ok = FALSE;

    if (stat (path, &st) == 0)
        *bytes = (guint64) st.st_size;

    g_free (img.pixels);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

int
dup_phash_distance (guint64 a, guint64 b)
{
    guint64 x = a ^ b;
    int n = 0;

    for (; x != 0; x &= x - 1)
        n++;
    return n;
}

/* --------------------------------------------------------------------------------------------- */

int
dup_phash_similarity (guint64 a, guint64 b)
{
    return 100 - (dup_phash_distance (a, b) * 100 + 32) / 64;
}

/* --------------------------------------------------------------------------------------------- */

int
dup_phash_max_distance (int percent)
{
    return (100 - CLAMP (percent, 0, 100)) * 64 / 100;
}

/* --------------------------------------------------------------------------------------------- */
