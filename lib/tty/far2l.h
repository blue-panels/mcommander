/** \file far2l.h
 *  \brief Header: far2l terminal extensions and drag and drop over them
 *
 *  The terminal drag and drop protocol is specified in docs/FAR2L_DND.md of
 *  https://github.com/unxed/f4 (unxed/f4#1628, elfmz/far2l#3647).  This is its
 *  application side: mc asks the terminal to switch drop reception on, the
 *  terminal announces a drop with a short event naming an offer, and mc pulls
 *  the list of the dropped files and their bytes, one bounded range per reply.
 *
 *  Turning the far2l extensions on and reading the keys and the mouse they
 *  bring is done in key.c (enable_far2l_input); this file only needs them to be
 *  on.  Its requests read the terminal's replies themselves, and put everything
 *  else that arrives back into the keyboard's input.
 */

#ifndef MC__TTY_FAR2L_H
#define MC__TTY_FAR2L_H

#include <glib.h>

/*** typedefs(not structures) and defined constants **********************************************/

#define FAR2L_ID_LEN 16

/* status of a reply, as the protocol names it (1 is success) */
#define FAR2L_OK             1
#define FAR2L_E_IO           0
#define FAR2L_E_DENIED       (-1)
#define FAR2L_E_OFFER_GONE   (-2)
#define FAR2L_E_UNKNOWN_ITEM (-3)
#define FAR2L_E_BAD_REQUEST  (-4)
#define FAR2L_E_UNSUPPORTED  (-5)
#define FAR2L_E_CHANGED      (-6)
#define FAR2L_E_LIMIT        (-7)
#define FAR2L_E_CANCELLED    (-8)
/* what never comes from the wire */
#define FAR2L_E_TIMEOUT  (-100)
#define FAR2L_E_PROTOCOL (-101)
#define FAR2L_E_NO_DND   (-102)

/* features of BIND */
#define FAR2L_DND_FEATURE_STREAM    1u
#define FAR2L_DND_FEATURE_REFERENCE 2u

/* flags of a LIST entry */
#define FAR2L_DND_ITEM_REFERENCE     1u
#define FAR2L_DND_ITEM_STREAM        2u
#define FAR2L_DND_ITEM_RANDOM_ACCESS 4u
#define FAR2L_DND_ITEM_FROZEN        8u
#define FAR2L_DND_ITEM_SIZE_KNOWN    16u

#define FAR2L_DND_KIND_FILE          1

/* flags of a READ reply */
#define FAR2L_DND_READ_EOF        1u
#define FAR2L_DND_READ_SIZE_KNOWN 2u

/* reasons of CLOSE */
#define FAR2L_DND_CLOSE_REJECTED  0
#define FAR2L_DND_CLOSE_PROCESSED 1
#define FAR2L_DND_CLOSE_CANCELLED 2
#define FAR2L_DND_CLOSE_FAILED    3

#define FAR2L_DND_LAST_PAGE       G_MAXUINT64

/* the profile mc asks for */
#define FAR2L_DND_MAX_FRAME 65536u
#define FAR2L_DND_MAX_CHUNK 32768u
#define FAR2L_DND_WINDOW    1u

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    guint8 rid;
    gint8 status;
    const guint8 *body; /* what follows the status, still a stack; points into the decoded stack */
    gsize body_len;
    char *message; /* of an error reply; g_free() */
} far2l_reply_t;

typedef struct
{
    guint16 version;
    guint8 binding[FAR2L_ID_LEN];
    guint32 max_frame;
    guint32 max_chunk;
    guint16 window;
    guint32 idle_seconds;
    guint32 features;
} far2l_dnd_grant_t;

typedef struct
{
    guint64 item_id;
    guint8 kind;
    guint16 flags;
    guint64 size;
    char *name; /* display name, UTF-8; never a destination path */
} far2l_dnd_entry_t;

typedef struct
{
    guint8 binding[FAR2L_ID_LEN];
    guint8 offer[FAR2L_ID_LEN];
    gint16 x, y; /* zero-based cell, -1 when unknown */
    guint32 modifiers;
    guint16 flags;
} far2l_dnd_event_t;

/* a drop as the rest of mc sees it */
typedef far2l_dnd_event_t far2l_drop_t;

typedef void (*far2l_drop_handler_fn) (const far2l_drop_t *drop);

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* ----- codec (no I/O) ----- */

/* Requests: the stack of a request, RID and command included; g_byte_array_free() it. */
GByteArray *far2l_dnd_encode_bind (guint8 rid, gboolean enable, const guint8 *binding,
                                   guint32 max_frame, guint32 max_chunk, guint16 window,
                                   guint32 wanted_features);
GByteArray *far2l_dnd_encode_list (guint8 rid, const guint8 *offer, guint64 cursor);
GByteArray *far2l_dnd_encode_read (guint8 rid, const guint8 *offer, guint64 item_id, guint64 offset,
                                   guint32 length);
GByteArray *far2l_dnd_encode_close (guint8 rid, const guint8 *offer, guint8 reason);

/* A complete request APC around a stack: ESC _ far2l: base64 BEL; g_free() it. */
char *far2l_frame_request (const GByteArray *stack);
/* base64 of the body of an APC into a stack; missing padding is tolerated. */
gboolean far2l_frame_decode (const char *b64, gsize len, GByteArray *out);

/* Reply: RID, status and the rest.  FALSE when the stack is not a reply at all;
   status FAR2L_E_NO_DND when nothing follows the RID (a far2l without DnD). */
gboolean far2l_dnd_decode_reply (const guint8 *stack, gsize len, far2l_reply_t *reply);
void far2l_reply_clear (far2l_reply_t *reply);

gboolean far2l_dnd_decode_bind_reply (const far2l_reply_t *reply, guint16 version,
                                      const guint8 *binding, guint32 max_frame, guint32 max_chunk,
                                      guint16 window, guint32 wanted, far2l_dnd_grant_t *grant);
/* Entries are far2l_dnd_entry_t *, freed with far2l_dnd_entry_free. */
gboolean far2l_dnd_decode_list_reply (const far2l_reply_t *reply, guint64 *next_cursor,
                                      GPtrArray *entries);
gboolean far2l_dnd_decode_read_reply (const far2l_reply_t *reply, guint32 asked,
                                      guint64 *observed_size, guint8 *flags, const guint8 **data,
                                      gsize *data_len);
/* FALSE for any event that is not INPUT_DND. */
gboolean far2l_dnd_decode_event (const guint8 *stack, gsize len, far2l_dnd_event_t *event);
void far2l_dnd_entry_free (gpointer entry);

/* Can name be a file name in the directory a drop lands in, and nothing else? */
gboolean far2l_dnd_name_is_safe (const char *name);

/* ----- the terminal ----- */

/* Switch drop reception on once the far2l extensions are on (enable_far2l_input), and off
   before they go off.  The terminal that does not know drag and drop leaves it unbound. */
void far2l_dnd_bind (void);
void far2l_dnd_unbind (void);
gboolean far2l_dnd_is_bound (void);

/* Called for a drop, from the event loop, never from inside another request. */
void far2l_dnd_set_handler (far2l_drop_handler_fn handler);

/* A far2l packet with the stack of an INPUT_DND event (the command is the last byte).  TRUE
   when it is a drop for the current binding: it is queued, and the key code that says so is
   MCKEY_FAR2L_DND. */
gboolean far2l_dnd_event (const guint8 *stack, gsize len);
/* Run the handler for the queued drop, if there is one. */
void far2l_dnd_dispatch (void);

/* Requests; these read the terminal's replies themselves and put everything else back into
   the keyboard's input.  The result is FAR2L_OK or one of the errors above. */
int far2l_dnd_list (const guint8 *offer, GPtrArray *entries /* far2l_dnd_entry_t * */);
int far2l_dnd_read (const guint8 *offer, guint64 item_id, guint64 offset, guint32 length,
                    guint8 **data, gsize *data_len, gboolean *eof);
void far2l_dnd_close (const guint8 *offer, guint8 reason);
guint32 far2l_dnd_max_chunk (void);

#endif
