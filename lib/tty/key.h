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
#define KEY_M_MASK  0x7000

#define XCTRL(x)    (KEY_M_CTRL | ((x) & 0x1F))
#define ALT(x)      (KEY_M_ALT | (unsigned int) (x))

/* To define sequences and return codes */
#define MCKEY_NOACTION 0
#define MCKEY_ESCAPE   1

/* Return code for the mouse sequence */
#define MCKEY_MOUSE -2

/* Return code for the extended mouse sequence */
#define MCKEY_EXTENDED_MOUSE -3

/* A bracketed paste taken as one block: tty_paste_take() gives its text */
#define MCKEY_PASTE -7

/* Return code for brackets of bracketed paste mode */
#define MCKEY_BRACKETED_PASTING_START -4
#define MCKEY_BRACKETED_PASTING_END   -5

/* Return code for a file drop announced by a far2l terminal (see far2l.h) */
#define MCKEY_FAR2L_DND -6

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

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
extern gboolean tty_far_ctrl_lbracket;  // Far mode: Ctrl-[ is a key of its own, not Esc
extern int mou_auto_repeat;

extern gboolean bracketed_pasting_in_progress;
/* Set by a loop that can take a paste as one block (MCKEY_PASTE) */
extern gboolean tty_paste_as_block;

/*** declarations of public functions ************************************************************/

gboolean define_sequence (int code, const char *seq, int action);

void init_key (void);
void init_key_input_fd (void);
void done_key (void);

int tty_keyname_to_keycode (const char *name, char **label);
char *tty_keycode_to_keyname (const int keycode);
/* What to do when the terminal is gone (src/resurrect.c) */
void tty_set_hangup_hook (void (*hook) (void));
/* The descriptor mc reads its terminal from: 0 if the standard input is a terminal */
int tty_input_fd (void);
/* mouse support */
int tty_get_event (struct Gpm_Event *event, gboolean redo_event, gboolean block);
gboolean is_idle (void);
/* The clipboard of a terminal that speaks the far2l extensions */
gboolean tty_far2l_clipboard_available (void);
gboolean tty_far2l_clipboard_set (const char *text, size_t len);
gboolean tty_far2l_clipboard_get (char **text, size_t *len);
int tty_getch (void);
GString *tty_paste_take (void);
void tty_paste_sanitize (GString *text);

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
/* Win32 input mode, if the terminal knows it: every key comes as CSI Vk;Sc;Uc;Kd;Cs;Rc _ */
void enable_win32_input (void);
void disable_win32_input (void);
/* far2l extensions, if the terminal knows them: every key comes as APC f2l <base64> ST.
   Kitty and Win32 input mode stay off while they are on. */
void enable_far2l_input (void);
void disable_far2l_input (void);
/* Which of them the keys come in now: "far2l", "kitty", "win32" or "legacy" */
const char *tty_input_protocol (void);

/*** inline functions ****************************************************************************/

static inline gboolean
is_abort_char (int c)
{
    return ((c == (int) ESC_CHAR) || (c == (int) KEY_F (10)));
}

#endif