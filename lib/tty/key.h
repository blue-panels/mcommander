/** \file key.h
 *  \brief Header: keyboard support routines
 */

#ifndef MC__KEY_H
#define MC__KEY_H

#include "lib/global.h"  // <glib.h>
#include "tty.h"         // KEY_F macro

/*** typedefs(not structures) and defined constants **********************************************/

/* Possible return values from tty_get_event: */
#define EV_MOUSE -2
#define EV_NONE  -1

/*
 * Internal representation of the key modifiers.  It is used in the
 * sequence tables and the keycodes in the mc sources.
 */
#define KEY_M_SHIFT 0x1000
#define KEY_M_ALT   0x2000
#define KEY_M_CTRL  0x4000
#define KEY_M_SUPER 0x8000
#define KEY_M_MASK  0xF000

#define XCTRL(x)    (KEY_M_CTRL | ((x) & 0x1F))
#define ALT(x)      (KEY_M_ALT | (unsigned int) (x))

/* To define sequences and return codes */
#define MCKEY_NOACTION 0
#define MCKEY_ESCAPE   1

/* Return code for the mouse sequence */
#define MCKEY_MOUSE -2

/* Return code for the extended mouse sequence */
#define MCKEY_EXTENDED_MOUSE -3

/* Return code for brackets of bracketed paste mode */
#define MCKEY_BRACKETED_PASTING_START -4
#define MCKEY_BRACKETED_PASTING_END   -5

/* A bracketed paste read as one block: tty_paste_take() gives its text */
#define MCKEY_PASTE -6

/* A kitty key event mc has no key code for: a release, a media key, a key with Super. The
   widget that wants it reads it with tty_key_event (). */
#define KEY_KITTY_EVENT 0xFFF

/* Kitty modifier bits (the modifier field minus one) */
#define TTY_KITTY_MOD_SHIFT     0x01
#define TTY_KITTY_MOD_ALT       0x02
#define TTY_KITTY_MOD_CTRL      0x04
#define TTY_KITTY_MOD_SUPER     0x08
#define TTY_KITTY_MOD_HYPER     0x10
#define TTY_KITTY_MOD_META      0x20
#define TTY_KITTY_MOD_CAPS_LOCK 0x40
#define TTY_KITTY_MOD_NUM_LOCK  0x80

#define TTY_KITTY_PRESS         1
#define TTY_KITTY_REPEAT        2
#define TTY_KITTY_RELEASE       3

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* A key as the terminal sent it by the kitty keyboard protocol */
typedef struct
{
    char final;            // 'u', '~' or the letter of a legacy form (A, P, ...)
    unsigned int key;      // the key number, or the first parameter of a legacy form
    unsigned int shifted;  // 0 when not sent
    unsigned int base;     // the key in the base layout, 0 when not sent
    unsigned int mods;     // TTY_KITTY_MOD_* bits
    int event;             // TTY_KITTY_PRESS, TTY_KITTY_REPEAT or TTY_KITTY_RELEASE
    gunichar text[8];
    int text_len;
} tty_key_event_t;

typedef struct
{
    int code;
    const char *name;
    const char *longname;
    const char *shortcut;
} key_code_name_t;

struct Gpm_Event;

/*** global variables defined in .c file *********************************************************/

extern const key_code_name_t key_name_conv_tab[];

extern int old_esc_mode_timeout;

extern int double_click_speed;
extern gboolean old_esc_mode;
extern int mou_auto_repeat;

extern gboolean bracketed_pasting_in_progress;
/* Set by a loop that takes a paste as one block (MCKEY_PASTE) */
extern gboolean tty_paste_as_block;
/* Set while more keys of a paste are still to be given: is_idle() is FALSE then */
extern gboolean tty_paste_keys_pending;

/*** declarations of public functions ************************************************************/

gboolean define_sequence (int code, const char *seq, int action);

void init_key (void);
void init_key_input_fd (void);
void done_key (void);

int tty_keyname_to_keycode (const char *name, char **label);
char *tty_keycode_to_keyname (const int keycode);
/* mouse support */
int tty_get_event (struct Gpm_Event *event, gboolean redo_event, gboolean block);
gboolean is_idle (void);
int tty_getch (void);
GString *tty_paste_take (void);

/* While waiting for input, the program can select on more than one file */
typedef int (*select_fn) (int fd, void *info);

/* Channel manipulation */
void add_select_channel (int fd, select_fn callback, void *info);
void delete_select_channel (int fd);

/* Activate/deactivate the channel checking */
void channels_up (void);
void channels_down (void);

/* internally used in key.c, defined in keyxtra.c */
void load_xtra_key_defines (void);

/* Learn a single key */
char *learn_key (void);
int tty_normalize_keycode (int code);
char *tty_key_lookup_sequence (int code);
int tty_match_seq_to_keycode (const char *seq, int len);
/* The kitty event of @key, the key get_key_code () gave last; FALSE when it came otherwise */
gboolean tty_key_event (int key, tty_key_event_t *ev);
/* The kitty event in the bytes of a key that learn_key () gave; FALSE when they are none */
gboolean tty_kitty_seq_event (const char *seq, int len, tty_key_event_t *ev);
int tty_decode_key_seq (const char *seq, int len);
char *tty_build_key_name (const char *base, int modifiers);

/* Returns a key code (interpreted) */
int get_key_code (int nodelay);

/* Set keypad mode (xterm and linux console only) */
void numeric_keypad_mode (void);
void application_keypad_mode (void);

/* Bracketed paste mode */
void enable_bracketed_paste (void);
void disable_bracketed_paste (void);

/* Kitty keyboard protocol, if the terminal knows it */
void enable_kitty_keyboard (void);
void disable_kitty_keyboard (void);
/* Ask the terminal for the kitty flags 2, 8 and 16 in @flags on top of mc's own, or for no more */
void tty_kitty_keyboard_want (guint flags);

/*** inline functions ****************************************************************************/

static inline gboolean
is_abort_char (int c)
{
    return ((c == (int) ESC_CHAR) || (c == (int) KEY_F (10)));
}

#endif
