/** \file  clipboard.h
 *  \brief Header: Util for external clipboard
 */

#ifndef MC__CLIPBOARD_H
#define MC__CLIPBOARD_H

/*** typedefs(not structures) and defined constants **********************************************/

/* The info file next to the clipfile, written by the editor: the digest of the clipfile, the kind
   of the block and the codeset of the text. It is valid only for the content it was written for. */
#define CLIP_INFO_SUFFIX ".info"
#define CLIP_DIGEST_TYPE G_CHECKSUM_MD5
#define CLIP_DIGEST_LEN  32
#define CLIP_CODESET_MAX 32

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

extern char *clipboard_store_path;
extern char *clipboard_paste_path;
extern gboolean clipboard_osc52;

/*** declarations of public functions ************************************************************/

char *clipboard_osc52_sequence (const char *text, size_t len);
char *clipboard_osc52_from_file (const char *clip_path);

gboolean clipboard_file_to_ext_clip (const gchar *event_group_name, const gchar *event_name,
                                     gpointer init_data, gpointer data);
gboolean clipboard_file_from_ext_clip (const gchar *event_group_name, const gchar *event_name,
                                       gpointer init_data, gpointer data);

gboolean clipboard_text_to_file (const gchar *event_group_name, const gchar *event_name,
                                 gpointer init_data, gpointer data);
gboolean clipboard_text_from_file (const gchar *event_group_name, const gchar *event_name,
                                   gpointer init_data, gpointer data);

void clipboard_info_write (const char *clip_path, const char *digest, gboolean vertical,
                           const char *codeset);
gboolean clipboard_info_read (const char *clip_path, char *digest, gboolean *vertical,
                              char *codeset);
void clipboard_info_drop (const char *clip_path);

/*** inline functions ****************************************************************************/

#endif
