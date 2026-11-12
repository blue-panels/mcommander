/** \file dup_similar.h
 *  \brief Header: fingerprints of the content for a search of similar files
 */

#ifndef MC__PANEL_PLUGIN_DUP_SIMILAR_H
#define MC__PANEL_PLUGIN_DUP_SIMILAR_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* The side of the grey picture the perceptual hash is made of */
#define DUP_PHASH_SIDE 32

typedef enum
{
    DUP_SIMILAR_BY_TYPE = 0,  // images by their picture, text by its text, the rest by bytes
    DUP_SIMILAR_IMAGES,
    DUP_SIMILAR_TEXT
} dup_similar_method_t;

typedef enum
{
    DUP_KIND_OTHER = 0,
    DUP_KIND_IMAGE,
    DUP_KIND_TEXT
} dup_file_kind_t;

/*** declarations of public functions ************************************************************/

/* The bytes read to tell the kind of a file: a NUL among them is a binary, as git takes it */
#define DUP_KIND_PROBE 8000

/* What @head (the first bytes of a file) looks like: a JPEG or PNG image, text, or else */
dup_file_kind_t dup_file_kind (const unsigned char *head, size_t len);

/* Can this build read the images of @head */
gboolean dup_image_supported (const unsigned char *head, size_t len);

/* Can this build read any image at all */
gboolean dup_images_readable (void);

/* Text: the hex SHA-256 of the text with CRLF as LF and no spaces or tabs at the end of the
   lines.  NULL when the file cannot be read, or when @cancel (may be NULL) is set while it is
   read.  @bytes gets what was read. */
char *dup_text_hash (const char *path, guint64 *bytes, gint *cancel);

/* The 64-bit perceptual hash of a grey picture of DUP_PHASH_SIDE x DUP_PHASH_SIDE */
guint64 dup_phash_of_grey (const double *grey);

/* An image file: its picture reduced to DUP_PHASH_SIDE x DUP_PHASH_SIDE grey values, and the
   perceptual hash of it.  @pixels gets the width times the height of the image.  FALSE when the
   file cannot be read or decoded. */
gboolean dup_image_phash (const char *path, guint64 *hash, guint64 *pixels, guint64 *bytes);

/* 0..64 bits that differ */
int dup_phash_distance (guint64 a, guint64 b);

/* The similarity in percent of two hashes */
int dup_phash_similarity (guint64 a, guint64 b);

/* The most bits two hashes may differ in to be similar at @percent */
int dup_phash_max_distance (int percent);

/*** inline functions ****************************************************************************/

#endif
