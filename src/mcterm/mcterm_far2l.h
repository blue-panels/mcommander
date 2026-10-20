/** \file mcterm_far2l.h
 *  \brief Header: mcterm as a far2l terminal, for the programs that run in it
 *
 *  The clipboard of the extensions is mc's own clipboard (the clipfile, clipboard_store and
 *  clipboard_paste, OSC 52 or the far2l terminal outside, whatever mc has), served here as a
 *  far2l terminal serves it: the user is asked whether a client may use it, the answer is kept
 *  for the client ID, and the clipboard is read only shortly after the user pressed Ctrl+V or
 *  Shift+Insert in this terminal.  None of it needs a far2l terminal outside.
 *
 *  A program in mc's terminal that speaks the far2l extensions (f4, far2l, another mc) can be
 *  given the drag and drop of the terminal mc itself runs in: a file dropped on the window of
 *  the outer far2l terminal reaches mc, and mc passes it on to the program as a drop on its
 *  own terminal.  Each hop is an application for the offer of the one outside it and the
 *  terminal for the one inside: the program's LIST and READ are answered with LIST and READ of
 *  the outer offer, chunk by chunk, with nothing kept in between.
 *
 *  That the program has the extensions on (far2l1 answered with far2lok, and its keys given as
 *  far2l events) is the emulator's business, see mcview_vterm_far2l_active; what is here is
 *  what rides on them.  Drop reception can be bound only while the terminal mc itself runs in
 *  has drag and drop bound.
 */

#ifndef MC__MCTERM_FAR2L_H
#define MC__MCTERM_FAR2L_H

#include "lib/global.h"
#include "lib/tty/far2l.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* the outer terminal's side, replaceable so that the terminal can be tested without one */
typedef struct
{
    gboolean (*is_bound) (void);
    int (*list) (const guint8 *offer, GPtrArray *entries);
    int (*read) (const guint8 *offer, guint64 item_id, guint64 offset, guint32 length,
                 guint8 **data, gsize *data_len, gboolean *eof);
    void (*close) (const guint8 *offer, guint8 reason);
    guint32 (*max_chunk) (void);
    /* mc's clipboard: its text (NULL for none; g_free() it), a new text, and whether it has any */
    char *(*clip_get) (gsize *len);
    void (*clip_set) (const char *text, gsize len);
    gboolean (*clip_has_text) (void);
    /* the clients the user allowed for good */
    gboolean (*clip_remembered) (const char *client_id);
    void (*clip_remember) (const char *client_id);
} mcterm_far2l_parent_t;

/* The answers of the user to a program that wants the clipboard */
enum
{
    MCTERM_F2L_CLIP_BLOCK = 0,  // this time no
    MCTERM_F2L_CLIP_OWN,        // the program is to use its own clipboard
    MCTERM_F2L_CLIP_ALLOW,      // yes, while the program has the extensions on
    MCTERM_F2L_CLIP_ALWAYS      // yes, and never ask again for this client
};

typedef struct mcterm_far2l_struct mcterm_far2l_t;

/*** declarations of public functions ************************************************************/

/* Whether a program in mc's terminal may have the extensions: always, unless MC_FAR2L=0.  What
   they give it needs no far2l terminal outside, but for drag and drop. */
gboolean mcterm_far2l_allowed (void);

/* parent NULL: the terminal mc runs in */
mcterm_far2l_t *mcterm_far2l_new (const mcterm_far2l_parent_t *parent);
/* Gives every offer back to the outer terminal, cancelled. */
void mcterm_far2l_free (mcterm_far2l_t *f);

/* The program has asked for the extensions, and the emulator has answered. */
gboolean mcterm_far2l_on (const mcterm_far2l_t *f);
/* The program has bound drop reception: a drop is worth passing on. */
gboolean mcterm_far2l_bound (const mcterm_far2l_t *f);

/* A key went to the program: Ctrl+V and Shift+Insert let it read the clipboard for a while. */
void mcterm_far2l_key (mcterm_far2l_t *f, int key);

/* A program asked to open the clipboard, and the user is to be asked first: not where the
   terminal reads the program's output, but from the idle loop.  mcterm_far2l_answer () takes
   the user's answer (MCTERM_F2L_CLIP_*) and gives what to write to the program (g_free() it;
   NULL for nothing, also when the program gave the extensions up meanwhile). */
gboolean mcterm_far2l_question_pending (const mcterm_far2l_t *f);
char *mcterm_far2l_answer (mcterm_far2l_t *f, int answer);

/* One APC of the program, what lies between ESC _ and its terminator (far2l1 and far2l0 too: the
   emulator has dealt with them and the terminal follows).  What is to be written to the program
   comes back as text (g_free() it), NULL for nothing. */
char *mcterm_far2l_apc (mcterm_far2l_t *f, const char *body, gsize len);

/* A drop on the outer terminal landed on this terminal at the cell x, y of it (-1, -1 when
   unknown).  TRUE: the terminal has taken the offer, which it will close, and the event to
   write to the program is in out; FALSE: not taken, the offer is the caller's. */
gboolean mcterm_far2l_drop (mcterm_far2l_t *f, const far2l_drop_t *drop, int x, int y, char **out);

#endif
