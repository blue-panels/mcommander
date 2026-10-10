/*
   Keyboard support routines.

   Copyright (C) 1994-2026
   Free Software Foundation, Inc.

   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

   Written by:
   Miguel de Icaza, 1994, 1995
   Janne Kukonlehto, 1994, 1995
   Jakub Jelinek, 1995
   Norbert Warmuth, 1997
   Denys Vlasenko <vda.linux@googlemail.com>, 2013
   Slava Zanko <slavazanko@gmail.com>, 2013
   Egmont Koblinger <egmont@gmail.com>, 2013
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the M-Commander
   a fork of GNU Midnight Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file key.c
 *  \brief Source: keyboard support routines
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "lib/global.h"

#include "lib/vfs/vfs.h"

#include "tty.h"
#include "tty-internal.h"  // mouse_enabled
#include "mouse.h"
#include "key.h"

#include "lib/widget.h"  // mc_refresh()

#ifdef HAVE_TEXTMODE_X11_SUPPORT
#include "x11conn.h"
#endif

#ifdef __linux__
#if defined(__GLIBC__) && (__GLIBC__ < 2)
#include <linux/termios.h>  // TIOCLINUX
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#endif

#ifdef __CYGWIN__
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#endif

#ifdef __QNXNTO__
#include <dlfcn.h>
#include <Ph.h>
#include <sys/dcmd_chr.h>
#endif

/*** global variables ****************************************************************************/

int mou_auto_repeat = 100;     // ms
int double_click_speed = 250;  // ms
gboolean old_esc_mode = TRUE;
/* Every key as a kitty event with its release, so that lone modifiers are seen */
gboolean keybar_modifiers = FALSE;
/* timeout for old_esc_mode in usec */
int old_esc_mode_timeout = G_USEC_PER_SEC;  // us, settable via env

gboolean bracketed_pasting_in_progress = FALSE;
gboolean tty_paste_as_block = FALSE;
gboolean tty_paste_keys_pending = FALSE;

/* This table is a mapping between names and the constants we use
 * We use this to allow users to define alternate definitions for
 * certain keys that may be missing from the terminal database
 */
const key_code_name_t key_name_conv_tab[] = {
    { ESC_CHAR, "escape", N_ ("Escape"), "Esc" },
    /* KEY_F(0) is not here, since we are mapping it to f10, so there is no reason
       to define f0 as well. Also, it makes Learn keys a bunch of problems :( */
    { KEY_F (1), "f1", N_ ("Function key 1"), "F1" },
    { KEY_F (2), "f2", N_ ("Function key 2"), "F2" },
    { KEY_F (3), "f3", N_ ("Function key 3"), "F3" },
    { KEY_F (4), "f4", N_ ("Function key 4"), "F4" },
    { KEY_F (5), "f5", N_ ("Function key 5"), "F5" },
    { KEY_F (6), "f6", N_ ("Function key 6"), "F6" },
    { KEY_F (7), "f7", N_ ("Function key 7"), "F7" },
    { KEY_F (8), "f8", N_ ("Function key 8"), "F8" },
    { KEY_F (9), "f9", N_ ("Function key 9"), "F9" },
    { KEY_F (10), "f10", N_ ("Function key 10"), "F10" },
    { KEY_F (11), "f11", N_ ("Function key 11"), "F11" },
    { KEY_F (12), "f12", N_ ("Function key 12"), "F12" },
    { KEY_F (13), "f13", N_ ("Function key 13"), "F13" },
    { KEY_F (14), "f14", N_ ("Function key 14"), "F14" },
    { KEY_F (15), "f15", N_ ("Function key 15"), "F15" },
    { KEY_F (16), "f16", N_ ("Function key 16"), "F16" },
    { KEY_F (17), "f17", N_ ("Function key 17"), "F17" },
    { KEY_F (18), "f18", N_ ("Function key 18"), "F18" },
    { KEY_F (19), "f19", N_ ("Function key 19"), "F19" },
    { KEY_F (20), "f20", N_ ("Function key 20"), "F20" },
    { ALT ('\t'), "complete", N_ ("Completion/M-tab"), "Meta-Tab" },
    { KEY_BTAB, "backtab", N_ ("BackTab/S-tab"), "Shift-Tab" },
    { KEY_BACKSPACE, "backspace", N_ ("Backspace"), "Backspace" },
    { KEY_UP, "up", N_ ("Up arrow"), "Up" },
    { KEY_DOWN, "down", N_ ("Down arrow"), "Down" },
    { KEY_LEFT, "left", N_ ("Left arrow"), "Left" },
    { KEY_RIGHT, "right", N_ ("Right arrow"), "Right" },
    { KEY_IC, "insert", N_ ("Insert"), "Ins" },
    { KEY_DC, "delete", N_ ("Delete"), "Del" },
    { KEY_HOME, "home", N_ ("Home"), "Home" },
    { KEY_END, "end", N_ ("End key"), "End" },
    { KEY_PPAGE, "pgup", N_ ("Page Up"), "PgUp" },
    { KEY_NPAGE, "pgdn", N_ ("Page Down"), "PgDn" },
    { (int) '/', "kpslash", N_ ("/ on keypad"), "/" },
    { KEY_KP_MULTIPLY, "kpasterisk", N_ ("* on keypad"), "*" },
    { KEY_KP_SUBTRACT, "kpminus", N_ ("- on keypad"), "-" },
    { KEY_KP_ADD, "kpplus", N_ ("+ on keypad"), "+" },

    // From here on, these won't be shown in Learn keys (no space)
    { KEY_LEFT, "kpleft", N_ ("Left arrow keypad"), "Left" },
    { KEY_RIGHT, "kpright", N_ ("Right arrow keypad"), "Right" },
    { KEY_UP, "kpup", N_ ("Up arrow keypad"), "Up" },
    { KEY_DOWN, "kpdown", N_ ("Down arrow keypad"), "Down" },
    { KEY_HOME, "kphome", N_ ("Home on keypad"), "Home" },
    { KEY_END, "kpend", N_ ("End on keypad"), "End" },
    { KEY_NPAGE, "kpnpage", N_ ("Page Down keypad"), "PgDn" },
    { KEY_PPAGE, "kpppage", N_ ("Page Up keypad"), "PgUp" },
    { KEY_IC, "kpinsert", N_ ("Insert on keypad"), "Ins" },
    { KEY_DC, "kpdelete", N_ ("Delete on keypad"), "Del" },
    { (int) '\n', "kpenter", N_ ("Enter on keypad"), "Enter" },
    { KEY_F (21), "f21", N_ ("Function key 21"), "F21" },
    { KEY_F (22), "f22", N_ ("Function key 22"), "F22" },
    { KEY_F (23), "f23", N_ ("Function key 23"), "F23" },
    { KEY_F (24), "f24", N_ ("Function key 24"), "F24" },
    { KEY_A1, "a1", N_ ("A1 key"), "A1" },
    { KEY_C1, "c1", N_ ("C1 key"), "C1" },

    // Alternative label
    { ESC_CHAR, "esc", N_ ("Escape"), "Esc" },
    { KEY_BACKSPACE, "bs", N_ ("Backspace"), "Bakspace" },
    { KEY_IC, "ins", N_ ("Insert"), "Ins" },
    { KEY_DC, "del", N_ ("Delete"), "Del" },
    { (int) '*', "asterisk", N_ ("Asterisk"), "*" },
    { (int) '-', "minus", N_ ("Minus"), "-" },
    { (int) '+', "plus", N_ ("Plus"), "+" },
    { (int) '.', "dot", N_ ("Dot"), "." },
    { (int) '<', "lt", N_ ("Less than"), "<" },
    { (int) '>', "gt", N_ ("Great than"), ">" },
    { (int) '=', "equal", N_ ("Equal"), "=" },
    { (int) ',', "comma", N_ ("Comma"), "," },
    { (int) '\'', "apostrophe", N_ ("Apostrophe"), "\'" },
    { (int) ':', "colon", N_ ("Colon"), ":" },
    { (int) ';', "semicolon", N_ ("Semicolon"), ";" },
    { (int) '!', "exclamation", N_ ("Exclamation mark"), "!" },
    { (int) '?', "question", N_ ("Question mark"), "?" },
    { (int) '&', "ampersand", N_ ("Ampersand"), "&" },
    { (int) '$', "dollar", N_ ("Dollar sign"), "$" },
    { (int) '"', "quota", N_ ("Quotation mark"), "\"" },
    { (int) '%', "percent", N_ ("Percent sign"), "%" },
    { (int) '^', "caret", N_ ("Caret"), "^" },
    { (int) '~', "tilda", N_ ("Tilda"), "~" },
    { (int) '`', "prime", N_ ("Prime"), "`" },
    { (int) '_', "underline", N_ ("Underline"), "_" },
    { (int) '_', "understrike", N_ ("Understrike"), "_" },
    { (int) '|', "pipe", N_ ("Pipe"), "|" },
    { (int) '(', "lparenthesis", N_ ("Left parenthesis"), "(" },
    { (int) ')', "rparenthesis", N_ ("Right parenthesis"), ")" },
    { (int) '[', "lbracket", N_ ("Left bracket"), "[" },
    { (int) ']', "rbracket", N_ ("Right bracket"), "]" },
    { (int) '{', "lbrace", N_ ("Left brace"), "{" },
    { (int) '}', "rbrace", N_ ("Right brace"), "}" },
    { (int) '\n', "enter", N_ ("Enter"), "Enter" },
    { (int) '\t', "tab", N_ ("Tab key"), "Tab" },
    { (int) ' ', "space", N_ ("Space key"), "Space" },
    { (int) '/', "slash", N_ ("Slash key"), "/" },
    { (int) '\\', "backslash", N_ ("Backslash key"), "\\" },
    { (int) '#', "number", N_ ("Number sign #"), "#" },
    { (int) '#', "hash", N_ ("Number sign #"), "#" },
    // TRANSLATORS: Please translate as in "at sign" (@).
    { (int) '@', "at", N_ ("At sign"), "@" },

    // meta keys
    { KEY_M_CTRL, "control", N_ ("Ctrl"), "Ctrl" },
    { KEY_M_CTRL, "ctrl", N_ ("Ctrl"), "Ctrl" },
    { KEY_M_ALT, "meta", N_ ("Alt"), "Alt" },
    { KEY_M_ALT, "alt", N_ ("Alt"), "Alt" },
    { KEY_M_ALT, "ralt", N_ ("RAlt"), "RAlt" },
    { KEY_M_SHIFT, "shift", N_ ("Shift"), "Shift" },
    { KEY_M_SUPER, "super", N_ ("Super"), "Super" },

    { 0, NULL, NULL, NULL },
};

/*** file scope macro definitions ****************************************************************/

#define MC_USEC_PER_MSEC 1000

/* Shortest gap between screen redraws while input is still coming in, in
   microseconds. Ten a second is enough to show that something is happening. */
#define MC_BURST_REFRESH_INTERVAL (100 * MC_USEC_PER_MSEC)

/* The maximum sequence length (32 + null terminator) */
#define SEQ_BUFFER_LEN 33

/* Kitty keyboard protocol: disambiguate escape codes (1) and report alternate keys (4) */
#define KITTY_KEYBOARD_BASE 5
/* Event types (2), every key as an escape code (8) and the text (16), for a program in mcterm */
#define KITTY_KEYBOARD_EXTRA    (2 | 8 | 16)
#define KITTY_KEYBOARD_EVENTS   2
#define KITTY_KEYBOARD_ALL_KEYS 8
/* The flags that report a lone modifier key with its release */
#define KITTY_KEYBOARD_MODIFIERS (2 | 8)
/* The longest CSI sequence kept: a key with TTY_KITTY_TEXT_MAX code points of text fits */
#define KITTY_CSI_MAX (TTY_KITTY_TEXT_MAX * 8 + 64)
/* The rest of a CSI sequence comes in the same write; this only guards a stuck read */
#define KITTY_CSI_TIMEOUT   (100 * MC_USEC_PER_MSEC)

#define KITTY_MOD_SHIFT     TTY_KITTY_MOD_SHIFT
#define KITTY_MOD_ALT       TTY_KITTY_MOD_ALT
#define KITTY_MOD_CTRL      TTY_KITTY_MOD_CTRL
#define KITTY_MOD_SUPER     TTY_KITTY_MOD_SUPER
#define KITTY_MOD_HYPER     TTY_KITTY_MOD_HYPER
#define KITTY_MOD_META      TTY_KITTY_MOD_META
#define KITTY_MOD_CAPS_LOCK TTY_KITTY_MOD_CAPS_LOCK
#define KITTY_MOD_NUM_LOCK  TTY_KITTY_MOD_NUM_LOCK

/* Kitty key numbers in the private use area */
#define KITTY_KEY_KP_0     57399
#define KITTY_KEY_KP_BEGIN 57427
/* Left Shift (57441) up to ISO Level 5 Shift (57454) */
#define KITTY_KEY_MOD_FIRST 57441
#define KITTY_KEY_MOD_LAST  57454
/* Left Shift, Ctrl, Alt, Super, Hyper, Meta, then the same six on the right */
#define KITTY_KEY_MOD_SIDES 6

/*** file scope type declarations ****************************************************************/

/* Linux console keyboard modifiers */
typedef enum
{
    SHIFT_PRESSED = (1 << 0),
    ALTR_PRESSED = (1 << 1),
    CONTROL_PRESSED = (1 << 2),
    ALTL_PRESSED = (1 << 3)
} mod_pressed_t;

typedef struct key_def
{
    char ch;   // Holds the matching char code
    int code;  // The code returned, valid if child == NULL
    struct key_def *next;
    struct key_def *child;  // sequence continuation
    int action;             /* optional action to be done. Now used only
                               to mark that we are just after the first
                               Escape */
} key_def;

typedef struct
{
    int code;
    const char *seq;
    int action;
} key_define_t;

/* File descriptor monitoring add/remove routines */
typedef struct
{
    int fd;
    select_fn callback;
    void *info;
} select_t;

typedef enum KeySortType
{
    KEY_NOSORT = 0,
    KEY_SORTBYNAME,
    KEY_SORTBYCODE
} KeySortType;

#ifdef __QNXNTO__
typedef int (*ph_dv_f) (void *, void *);
typedef int (*ph_ov_f) (void *);
typedef int (*ph_pqc_f) (unsigned short, PhCursorInfo_t *);
#endif

/*** forward declarations (file scope functions) *************************************************/

static inline gboolean kitty_text_pending (void);

/*** file scope variables ************************************************************************/

/* A paste read as one block: limit the kept text, the gap between bytes, and the total wait. */
#define TTY_PASTE_MAX_BYTES (16 * 1024 * 1024)
#define TTY_PASTE_GAP_USEC  (2 * G_USEC_PER_SEC)
#define TTY_PASTE_MAX_USEC  (30 * G_USEC_PER_SEC)

static GString *tty_paste_block = NULL;

static key_define_t mc_default_keys[] = {
    { ESC_CHAR, ESC_STR, MCKEY_ESCAPE },
    { ESC_CHAR, ESC_STR ESC_STR, MCKEY_NOACTION },
    { MCKEY_BRACKETED_PASTING_START, ESC_STR "[200~", MCKEY_NOACTION },
    { MCKEY_BRACKETED_PASTING_END, ESC_STR "[201~", MCKEY_NOACTION },
    { 0, NULL, MCKEY_NOACTION },
};

/* Broken terminfo and termcap databases on xterminals */
static key_define_t xterm_key_defines[] = {
    { KEY_F (1), ESC_STR "OP", MCKEY_NOACTION },
    { KEY_F (2), ESC_STR "OQ", MCKEY_NOACTION },
    { KEY_F (3), ESC_STR "OR", MCKEY_NOACTION },
    { KEY_F (4), ESC_STR "OS", MCKEY_NOACTION },
    { KEY_F (1), ESC_STR "[11~", MCKEY_NOACTION },
    { KEY_F (2), ESC_STR "[12~", MCKEY_NOACTION },
    { KEY_F (3), ESC_STR "[13~", MCKEY_NOACTION },
    { KEY_F (4), ESC_STR "[14~", MCKEY_NOACTION },
    { KEY_F (5), ESC_STR "[15~", MCKEY_NOACTION },
    { KEY_F (6), ESC_STR "[17~", MCKEY_NOACTION },
    { KEY_F (7), ESC_STR "[18~", MCKEY_NOACTION },
    { KEY_F (8), ESC_STR "[19~", MCKEY_NOACTION },
    { KEY_F (9), ESC_STR "[20~", MCKEY_NOACTION },
    { KEY_F (10), ESC_STR "[21~", MCKEY_NOACTION },

    // old xterm Shift-arrows
    { KEY_M_SHIFT | KEY_UP, ESC_STR "O2A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN, ESC_STR "O2B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "O2C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT, ESC_STR "O2D", MCKEY_NOACTION },

    // new xterm Shift-arrows
    { KEY_M_SHIFT | KEY_UP, ESC_STR "[1;2A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN, ESC_STR "[1;2B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[1;2C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT, ESC_STR "[1;2D", MCKEY_NOACTION },

    // more xterm keys with modifiers
    { KEY_M_CTRL | KEY_PPAGE, ESC_STR "[5;5~", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_NPAGE, ESC_STR "[6;5~", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_IC, ESC_STR "[2;5~", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_DC, ESC_STR "[3;5~", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_HOME, ESC_STR "[1;5H", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_END, ESC_STR "[1;5F", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_HOME, ESC_STR "[1;2H", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_END, ESC_STR "[1;2F", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_UP, ESC_STR "[1;5A", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_DOWN, ESC_STR "[1;5B", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_RIGHT, ESC_STR "[1;5C", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_LEFT, ESC_STR "[1;5D", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_IC, ESC_STR "[2;2~", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DC, ESC_STR "[3;2~", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP, ESC_STR "[1;6A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN, ESC_STR "[1;6B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "[1;6C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT, ESC_STR "[1;6D", MCKEY_NOACTION },
    { KEY_M_SHIFT | '\t', ESC_STR "[Z", MCKEY_NOACTION },

    // putty
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP, ESC_STR "[[1;6A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN, ESC_STR "[[1;6B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "[[1;6C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT, ESC_STR "[[1;6D", MCKEY_NOACTION },

    // putty alt-arrow keys
    // removed as source esc esc esc trouble
    /*
       { KEY_M_ALT | KEY_UP,    ESC_STR ESC_STR "OA", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_DOWN,  ESC_STR ESC_STR "OB", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_RIGHT, ESC_STR ESC_STR "OC", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_LEFT,  ESC_STR ESC_STR "OD", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_PPAGE, ESC_STR ESC_STR "[5~", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_NPAGE, ESC_STR ESC_STR "[6~", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_HOME,  ESC_STR ESC_STR "[1~", MCKEY_NOACTION },
       { KEY_M_ALT | KEY_END,   ESC_STR ESC_STR "[4~", MCKEY_NOACTION },

       { KEY_M_CTRL | KEY_M_ALT | KEY_UP,    ESC_STR ESC_STR "[1;2A", MCKEY_NOACTION },
       { KEY_M_CTRL | KEY_M_ALT | KEY_DOWN,  ESC_STR ESC_STR "[1;2B", MCKEY_NOACTION },
       { KEY_M_CTRL | KEY_M_ALT | KEY_RIGHT, ESC_STR ESC_STR "[1;2C", MCKEY_NOACTION },
       { KEY_M_CTRL | KEY_M_ALT | KEY_LEFT,  ESC_STR ESC_STR "[1;2D", MCKEY_NOACTION },

       { KEY_M_CTRL | KEY_M_ALT | KEY_PPAGE, ESC_STR ESC_STR "[[5;5~", MCKEY_NOACTION },
       { KEY_M_CTRL | KEY_M_ALT | KEY_NPAGE, ESC_STR ESC_STR "[[6;5~", MCKEY_NOACTION },
       { KEY_M_CTRL | KEY_M_ALT | KEY_HOME,  ESC_STR ESC_STR "[1;5H", MCKEY_NOACTION },
       { KEY_M_CTRL | KEY_M_ALT | KEY_END,   ESC_STR ESC_STR "[1;5F", MCKEY_NOACTION },
     */
    // xterm alt-arrow keys
    { KEY_M_ALT | KEY_UP, ESC_STR "[1;3A", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_DOWN, ESC_STR "[1;3B", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_RIGHT, ESC_STR "[1;3C", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_LEFT, ESC_STR "[1;3D", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_PPAGE, ESC_STR "[5;3~", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_NPAGE, ESC_STR "[6;3~", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_HOME, ESC_STR "[1~", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_END, ESC_STR "[4~", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_UP, ESC_STR "[1;7A", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_DOWN, ESC_STR "[1;7B", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_RIGHT, ESC_STR "[1;7C", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_LEFT, ESC_STR "[1;7D", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_PPAGE, ESC_STR "[5;7~", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_NPAGE, ESC_STR "[6;7~", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_HOME, ESC_STR "OH", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_END, ESC_STR "OF", MCKEY_NOACTION },

    { KEY_M_SHIFT | KEY_M_ALT | KEY_UP, ESC_STR "[1;4A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_ALT | KEY_DOWN, ESC_STR "[1;4B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_ALT | KEY_RIGHT, ESC_STR "[1;4C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_ALT | KEY_LEFT, ESC_STR "[1;4D", MCKEY_NOACTION },

    // rxvt keys with modifiers
    { KEY_M_SHIFT | KEY_UP, ESC_STR "[a", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN, ESC_STR "[b", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[c", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT, ESC_STR "[d", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_UP, ESC_STR "Oa", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_DOWN, ESC_STR "Ob", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_RIGHT, ESC_STR "Oc", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_LEFT, ESC_STR "Od", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_PPAGE, ESC_STR "[5^", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_NPAGE, ESC_STR "[6^", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_HOME, ESC_STR "[7^", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_END, ESC_STR "[8^", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_HOME, ESC_STR "[7$", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_END, ESC_STR "[8$", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_IC, ESC_STR "[2^", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_DC, ESC_STR "[3^", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DC, ESC_STR "[3$", MCKEY_NOACTION },

    // konsole keys with modifiers
    { KEY_M_SHIFT | KEY_HOME, ESC_STR "O2H", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_END, ESC_STR "O2F", MCKEY_NOACTION },

    // gnome-terminal
    { KEY_M_SHIFT | KEY_UP, ESC_STR "[2A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN, ESC_STR "[2B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[2C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT, ESC_STR "[2D", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_UP, ESC_STR "[5A", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_DOWN, ESC_STR "[5B", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_RIGHT, ESC_STR "[5C", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_LEFT, ESC_STR "[5D", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP, ESC_STR "[6A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN, ESC_STR "[6B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "[6C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT, ESC_STR "[6D", MCKEY_NOACTION },

    // gnome-terminal - application mode
    { KEY_M_CTRL | KEY_UP, ESC_STR "O5A", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_DOWN, ESC_STR "O5B", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_RIGHT, ESC_STR "O5C", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_LEFT, ESC_STR "O5D", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP, ESC_STR "O6A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN, ESC_STR "O6B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "O6C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT, ESC_STR "O6D", MCKEY_NOACTION },

    // iTerm
    { KEY_M_SHIFT | KEY_PPAGE, ESC_STR "[5;2~", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_NPAGE, ESC_STR "[6;2~", MCKEY_NOACTION },

    // putty
    { KEY_M_SHIFT | KEY_PPAGE, ESC_STR "[[5;53~", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_NPAGE, ESC_STR "[[6;53~", MCKEY_NOACTION },

    // keypad keys
    { KEY_IC, ESC_STR "Op", MCKEY_NOACTION },
    { KEY_DC, ESC_STR "On", MCKEY_NOACTION },
    { '/', ESC_STR "Oo", MCKEY_NOACTION },
    { '\n', ESC_STR "OM", MCKEY_NOACTION },

    { 0, NULL, MCKEY_NOACTION },
};

/* qansi-m terminals have a much more key combinations,
   which are undefined in termcap/terminfo */
static key_define_t qansi_key_defines[] = {
    // qansi-m terminal
    { KEY_M_CTRL | KEY_NPAGE, ESC_STR "[u", MCKEY_NOACTION },        // Ctrl-PgDown
    { KEY_M_CTRL | KEY_PPAGE, ESC_STR "[v", MCKEY_NOACTION },        // Ctrl-PgUp
    { KEY_M_CTRL | KEY_HOME, ESC_STR "[h", MCKEY_NOACTION },         // Ctrl-Home
    { KEY_M_CTRL | KEY_END, ESC_STR "[y", MCKEY_NOACTION },          // Ctrl-End
    { KEY_M_CTRL | KEY_IC, ESC_STR "[`", MCKEY_NOACTION },           // Ctrl-Insert
    { KEY_M_CTRL | KEY_DC, ESC_STR "[p", MCKEY_NOACTION },           // Ctrl-Delete
    { KEY_M_CTRL | KEY_LEFT, ESC_STR "[d", MCKEY_NOACTION },         // Ctrl-Left
    { KEY_M_CTRL | KEY_RIGHT, ESC_STR "[c", MCKEY_NOACTION },        // Ctrl-Right
    { KEY_M_CTRL | KEY_DOWN, ESC_STR "[b", MCKEY_NOACTION },         // Ctrl-Down
    { KEY_M_CTRL | KEY_UP, ESC_STR "[a", MCKEY_NOACTION },           // Ctrl-Up
    { KEY_M_CTRL | KEY_KP_ADD, ESC_STR "[s", MCKEY_NOACTION },       // Ctrl-Gr-Plus
    { KEY_M_CTRL | KEY_KP_SUBTRACT, ESC_STR "[t", MCKEY_NOACTION },  // Ctrl-Gr-Minus
    { KEY_M_CTRL | '\t', ESC_STR "[z", MCKEY_NOACTION },             // Ctrl-Tab
    { KEY_M_SHIFT | '\t', ESC_STR "[Z", MCKEY_NOACTION },            // Shift-Tab
    { KEY_M_CTRL | KEY_F (1), ESC_STR "[1~", MCKEY_NOACTION },       // Ctrl-F1
    { KEY_M_CTRL | KEY_F (2), ESC_STR "[2~", MCKEY_NOACTION },       // Ctrl-F2
    { KEY_M_CTRL | KEY_F (3), ESC_STR "[3~", MCKEY_NOACTION },       // Ctrl-F3
    { KEY_M_CTRL | KEY_F (4), ESC_STR "[4~", MCKEY_NOACTION },       // Ctrl-F4
    { KEY_M_CTRL | KEY_F (5), ESC_STR "[5~", MCKEY_NOACTION },       // Ctrl-F5
    { KEY_M_CTRL | KEY_F (6), ESC_STR "[6~", MCKEY_NOACTION },       // Ctrl-F6
    { KEY_M_CTRL | KEY_F (7), ESC_STR "[7~", MCKEY_NOACTION },       // Ctrl-F7
    { KEY_M_CTRL | KEY_F (8), ESC_STR "[8~", MCKEY_NOACTION },       // Ctrl-F8
    { KEY_M_CTRL | KEY_F (9), ESC_STR "[9~", MCKEY_NOACTION },       // Ctrl-F9
    { KEY_M_CTRL | KEY_F (10), ESC_STR "[10~", MCKEY_NOACTION },     // Ctrl-F10
    { KEY_M_CTRL | KEY_F (11), ESC_STR "[11~", MCKEY_NOACTION },     // Ctrl-F11
    { KEY_M_CTRL | KEY_F (12), ESC_STR "[12~", MCKEY_NOACTION },     // Ctrl-F12
    { KEY_M_ALT | KEY_F (1), ESC_STR "[17~", MCKEY_NOACTION },       // Alt-F1
    { KEY_M_ALT | KEY_F (2), ESC_STR "[18~", MCKEY_NOACTION },       // Alt-F2
    { KEY_M_ALT | KEY_F (3), ESC_STR "[19~", MCKEY_NOACTION },       // Alt-F3
    { KEY_M_ALT | KEY_F (4), ESC_STR "[20~", MCKEY_NOACTION },       // Alt-F4
    { KEY_M_ALT | KEY_F (5), ESC_STR "[21~", MCKEY_NOACTION },       // Alt-F5
    { KEY_M_ALT | KEY_F (6), ESC_STR "[22~", MCKEY_NOACTION },       // Alt-F6
    { KEY_M_ALT | KEY_F (7), ESC_STR "[23~", MCKEY_NOACTION },       // Alt-F7
    { KEY_M_ALT | KEY_F (8), ESC_STR "[24~", MCKEY_NOACTION },       // Alt-F8
    { KEY_M_ALT | KEY_F (9), ESC_STR "[25~", MCKEY_NOACTION },       // Alt-F9
    { KEY_M_ALT | KEY_F (10), ESC_STR "[26~", MCKEY_NOACTION },      // Alt-F10
    { KEY_M_ALT | KEY_F (11), ESC_STR "[27~", MCKEY_NOACTION },      // Alt-F11
    { KEY_M_ALT | KEY_F (12), ESC_STR "[28~", MCKEY_NOACTION },      // Alt-F12
    { KEY_M_ALT | 'a', ESC_STR "Na", MCKEY_NOACTION },               // Alt-a
    { KEY_M_ALT | 'b', ESC_STR "Nb", MCKEY_NOACTION },               // Alt-b
    { KEY_M_ALT | 'c', ESC_STR "Nc", MCKEY_NOACTION },               // Alt-c
    { KEY_M_ALT | 'd', ESC_STR "Nd", MCKEY_NOACTION },               // Alt-d
    { KEY_M_ALT | 'e', ESC_STR "Ne", MCKEY_NOACTION },               // Alt-e
    { KEY_M_ALT | 'f', ESC_STR "Nf", MCKEY_NOACTION },               // Alt-f
    { KEY_M_ALT | 'g', ESC_STR "Ng", MCKEY_NOACTION },               // Alt-g
    { KEY_M_ALT | 'h', ESC_STR "Nh", MCKEY_NOACTION },               // Alt-h
    { KEY_M_ALT | 'i', ESC_STR "Ni", MCKEY_NOACTION },               // Alt-i
    { KEY_M_ALT | 'j', ESC_STR "Nj", MCKEY_NOACTION },               // Alt-j
    { KEY_M_ALT | 'k', ESC_STR "Nk", MCKEY_NOACTION },               // Alt-k
    { KEY_M_ALT | 'l', ESC_STR "Nl", MCKEY_NOACTION },               // Alt-l
    { KEY_M_ALT | 'm', ESC_STR "Nm", MCKEY_NOACTION },               // Alt-m
    { KEY_M_ALT | 'n', ESC_STR "Nn", MCKEY_NOACTION },               // Alt-n
    { KEY_M_ALT | 'o', ESC_STR "No", MCKEY_NOACTION },               // Alt-o
    { KEY_M_ALT | 'p', ESC_STR "Np", MCKEY_NOACTION },               // Alt-p
    { KEY_M_ALT | 'q', ESC_STR "Nq", MCKEY_NOACTION },               // Alt-q
    { KEY_M_ALT | 'r', ESC_STR "Nr", MCKEY_NOACTION },               // Alt-r
    { KEY_M_ALT | 's', ESC_STR "Ns", MCKEY_NOACTION },               // Alt-s
    { KEY_M_ALT | 't', ESC_STR "Nt", MCKEY_NOACTION },               // Alt-t
    { KEY_M_ALT | 'u', ESC_STR "Nu", MCKEY_NOACTION },               // Alt-u
    { KEY_M_ALT | 'v', ESC_STR "Nv", MCKEY_NOACTION },               // Alt-v
    { KEY_M_ALT | 'w', ESC_STR "Nw", MCKEY_NOACTION },               // Alt-w
    { KEY_M_ALT | 'x', ESC_STR "Nx", MCKEY_NOACTION },               // Alt-x
    { KEY_M_ALT | 'y', ESC_STR "Ny", MCKEY_NOACTION },               // Alt-y
    { KEY_M_ALT | 'z', ESC_STR "Nz", MCKEY_NOACTION },               // Alt-z
    { KEY_KP_SUBTRACT, ESC_STR "[S", MCKEY_NOACTION },               // Gr-Minus
    { KEY_KP_ADD, ESC_STR "[T", MCKEY_NOACTION },                    // Gr-Plus
    { 0, NULL, MCKEY_NOACTION },
};

/* This holds all the key definitions */
static key_def *keys = NULL;
/* Last learned sequence per normalized keycode. */
static GHashTable *key_sequences = NULL;

static int input_fd;
static int disabled_channels = 0;  // Disable channels checking

static GSList *select_list = NULL;

static int seq_buffer[SEQ_BUFFER_LEN];
static int *seq_append = NULL;

static int *pending_keys = NULL;

static gboolean kitty_keyboard_active = FALSE;
/* Win32 input mode of Windows Terminal (DECSET 9001), used before the kitty protocol */
static gboolean win32_input_active = FALSE;
/* The high half of a UTF-16 pair a Win32 input record gave, waiting for the low one */
static gunichar win32_high_surrogate = 0;
/* The kitty event of the key get_key_code () gave last */
static tty_key_event_t kitty_event;
static gboolean kitty_event_valid = FALSE;
static int kitty_event_code;
/* The flags asked of the terminal, and the ones a program in mcterm wants on top of them */
static guint kitty_flags_wanted = 0;
static guint kitty_flags_sent = 0;
/* The rest of the text of the last kitty key, given as single bytes */
static unsigned char kitty_text[TTY_KITTY_TEXT_MAX * 6];
static size_t kitty_text_len = 0;
static size_t kitty_text_pos = 0;
/* How many more times the last key comes: a Win32 input record with a repeat count */
static unsigned int key_repeat_left = 0;
/* The repeat count of the Win32 input record parsed last, 1 for any other key */
static unsigned int win32_record_repeat = 1;
/* The modifier keys held now, a bit for each from Left Shift to Right Meta */
static guint kitty_mod_keys = 0;
/* The kitty modifier bit of each of the six modifier keys of a side */
static const guint kitty_mod_kinds[KITTY_KEY_MOD_SIDES] = {
    KITTY_MOD_SHIFT, KITTY_MOD_CTRL,  KITTY_MOD_ALT,
    KITTY_MOD_SUPER, KITTY_MOD_HYPER, KITTY_MOD_META,
};

/* Keypad keys from KP_0 (57399) to KP_BEGIN (57427). -1: no mc key */
static const int kitty_keypad_keys[] = {
    '0',
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    '.',
    '/',
    KEY_KP_MULTIPLY,
    KEY_KP_SUBTRACT,
    KEY_KP_ADD,
    '\r',
    '=',
    ',',
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_PPAGE,
    KEY_NPAGE,
    KEY_HOME,
    KEY_END,
    KEY_IC,
    KEY_DC,
    -1,
};

#ifdef __QNXNTO__
ph_dv_f ph_attach;
ph_ov_f ph_input_group;
ph_pqc_f ph_query_cursor;
#endif

#ifdef HAVE_TEXTMODE_X11_SUPPORT
static Display *x11_display;
static Window x11_window;
#endif

static KeySortType has_been_sorted = KEY_NOSORT;

static const size_t key_conv_tab_size = G_N_ELEMENTS (key_name_conv_tab) - 1;

static const key_code_name_t *key_conv_tab_sorted[G_N_ELEMENTS (key_name_conv_tab) - 1];

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
select_cmp_by_fd_set (gconstpointer a, gconstpointer b)
{
    const select_t *s = (const select_t *) a;
    const fd_set *f = (const fd_set *) b;

    return (FD_ISSET (s->fd, f) ? 0 : 1);
}

/* --------------------------------------------------------------------------------------------- */

static int
select_cmp_by_fd (gconstpointer a, gconstpointer b)
{
    const select_t *s = (const select_t *) a;
    const int fd = GPOINTER_TO_INT (b);

    return (s->fd == fd ? 0 : 1);
}

/* --------------------------------------------------------------------------------------------- */

static int
add_selects (fd_set *select_set)
{
    int top_fd = 0;

    if (disabled_channels == 0)
    {
        GSList *s;

        for (s = select_list; s != NULL; s = g_slist_next (s))
        {
            select_t *p = (select_t *) s->data;

            FD_SET (p->fd, select_set);
            if (p->fd > top_fd)
                top_fd = p->fd;
        }
    }

    return top_fd;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
check_selects (fd_set *select_set)
{
    gboolean ui_update_requested = FALSE;

    while (disabled_channels == 0)
    {
        GSList *s;
        select_t *p;

        s = g_slist_find_custom (select_list, select_set, select_cmp_by_fd_set);
        if (s == NULL)
            break;

        p = (select_t *) s->data;
        FD_CLR (p->fd, select_set);

        if (p->callback (p->fd, p->info) != 0)
            ui_update_requested = TRUE;
    }

    return ui_update_requested;
}

/* --------------------------------------------------------------------------------------------- */
/* If set timeout is set, then we wait 0.1 seconds, else, we block */

static void
try_channels (gboolean set_timeout)
{
    struct timeval time_out;
    static fd_set select_set;

    while (TRUE)
    {
        struct timeval *timeptr = NULL;
        int maxfdp, v;

        FD_ZERO (&select_set);
        FD_SET (input_fd, &select_set);  // Add stdin
        maxfdp = MAX (add_selects (&select_set), input_fd);

        if (set_timeout)
        {
            time_out.tv_sec = 0;
            time_out.tv_usec = 100 * MC_USEC_PER_MSEC;
            timeptr = &time_out;
        }

        v = select (maxfdp + 1, &select_set, NULL, NULL, timeptr);
        if (v > 0)
        {
            check_selects (&select_set);
            if (FD_ISSET (input_fd, &select_set))
                break;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Build a chain of nodes for @seq. Only the last node carries the key: a node
 * in the middle of the chain is a step on the way to it, and a code there would
 * name a key the caller never asked for.
 */
static key_def *
create_sequence (const char *seq, int code, int action)
{
    key_def *base, *attach;

    for (base = attach = NULL; *seq != '\0'; seq++)
    {
        key_def *p;
        const gboolean last = seq[1] == '\0';

        p = g_new (key_def, 1);
        if (base == NULL)
            base = p;
        if (attach != NULL)
            attach->child = p;

        p->ch = *seq;
        p->code = last ? code : 0;
        p->child = NULL;
        p->next = NULL;
        p->action = last ? action : MCKEY_NOACTION;
        attach = p;
    }
    return base;
}

/* --------------------------------------------------------------------------------------------- */

static void
define_sequences (const key_define_t *kd)
{
    int i;

    for (i = 0; kd[i].code != 0; i++)
        define_sequence (kd[i].code, kd[i].seq, kd[i].action);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
lookup_sequence_recursive (const key_def *node, int code, GString *buf)
{
    const key_def *p;

    for (p = node; p != NULL; p = p->next)
    {
        g_string_append_c (buf, p->ch);

        if (p->child == NULL && p->code == code)
            return TRUE;

        if (p->child != NULL && lookup_sequence_recursive (p->child, code, buf))
            return TRUE;

        g_string_truncate (buf, buf->len - 1);
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Match raw bytes against the key trie. Walk the trie character by character.
 * Returns keycode if found, or 0 if not found.
 */
static int
match_seq_in_trie (const key_def *node, const char *seq, int len, int pos)
{
    const key_def *p;

    if (pos >= len)
        return 0;

    for (p = node; p != NULL; p = p->next)
    {
        if (p->ch == seq[pos])
        {
            if (pos + 1 == len && p->code != 0)
                return p->code;
            if (p->child != NULL)
            {
                int result = match_seq_in_trie (p->child, seq, len, pos + 1);

                if (result != 0)
                    return result;
            }
        }
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_TEXTMODE_X11_SUPPORT
static void
init_key_x11 (void)
{
    if (getenv ("DISPLAY") != NULL && !mc_global.tty.disable_x11)
    {
        x11_display = mc_XOpenDisplay (0);

        if (x11_display != NULL)
            x11_window = DefaultRootWindow (x11_display);
    }
}
#endif

/* --------------------------------------------------------------------------------------------- */
/* Workaround for System V Curses vt100 bug */

static int
getch_with_delay (void)
{
    int c;

    /* This routine could be used on systems without mouse support,
       so we need to do the select check :-( */
    while (TRUE)
    {
        // select() does not see input already buffered by the screen library
        if (pending_keys == NULL && !kitty_text_pending () && !tty_lowlevel_input_pending ())
            try_channels (FALSE);

        // Try to get a character
        c = get_key_code (0);
        if (c != -1)
            break;

        // Failed -> wait 0.1 secs and try again
        try_channels (TRUE);
    }

    // Success -> return the character
    return c;
}

/* --------------------------------------------------------------------------------------------- */

static void
xmouse_get_event (Gpm_Event *ev, gboolean extended)
{
    static gint64 tv1 = 0;  // Force first click as single
    static int clicks = 0;
    static int last_btn = 0;
    int btn;

    // Decode Xterm mouse information to a GPM style event

    if (!extended)
    {
        // Variable btn has following meaning:
        // 0 = btn1 dn, 1 = btn2 dn, 2 = btn3 dn, 3 = btn up
        btn = tty_lowlevel_getch () - 32;
        // Coordinates are 33-based
        // Transform them to 1-based
        ev->x = tty_lowlevel_getch () - 32;
        ev->y = tty_lowlevel_getch () - 32;
    }
    else
    {
        /* SGR 1006 extension (e.g. "\e[<0;12;300M"):
           - Numbers are encoded in decimal to make it ASCII-safe
           and to overcome the limit of 223 columns/rows.
           - Mouse release is encoded by trailing 'm' rather than 'M'
           so that the released button can be reported.
           - Numbers are no longer offset by 32. */
        char c;

        btn = ev->x = ev->y = 0;
        ev->type = 0;  // In case we return on an invalid sequence

        while ((c = tty_lowlevel_getch ()) != ';')
        {
            if (c < '0' || c > '9')
                return;
            btn = 10 * btn + (c - '0');
        }
        while ((c = tty_lowlevel_getch ()) != ';')
        {
            if (c < '0' || c > '9')
                return;
            ev->x = 10 * ev->x + (c - '0');
        }
        while ((c = tty_lowlevel_getch ()) != 'M' && c != 'm')
        {
            if (c < '0' || c > '9')
                return;
            ev->y = 10 * ev->y + (c - '0');
        }
        /* Legacy mouse protocol doesn't tell which button was released,
           conveniently all of mc's widgets are written not to rely on this
           information. With the SGR extension the released button becomes
           known, but for the sake of simplicity we just ignore it. */
        if (c == 'm')
            btn = 3;
    }

    // There seems to be no way of knowing which button was released
    // So we assume all the buttons were released

    if (btn == 3)
    {
        if (last_btn != 0)
        {
            if ((last_btn & (GPM_B_UP | GPM_B_DOWN)) != 0)
            {
                // FIXME: DIRTY HACK
                // don't generate GPM_UP after mouse wheel
                // need for menu event handling
                ev->type = 0;
                tv1 = 0;
            }
            else
            {
                ev->type = GPM_UP | (GPM_SINGLE << clicks);
            }
            ev->buttons = 0;
            last_btn = 0;
        }
        else
        {
            // Bogus event, maybe mouse wheel
            ev->type = 0;
        }
    }
    else
    {
        gint64 tv2;

        tv2 = g_get_monotonic_time ();
        if (tv1 != 0 && tv2 - tv1 < (gint64) double_click_speed * MC_USEC_PER_MSEC)
        {
            clicks++;
            clicks %= 3;
        }
        else
            clicks = 0;

        if (btn >= 32 && btn <= 34)
        {
            btn -= 32;
            ev->type = GPM_DRAG;
        }
        else
        {
            ev->type = GPM_DOWN;
            tv1 = g_get_monotonic_time ();
        }

        switch (btn)
        {
        case 0:
            ev->buttons = GPM_B_LEFT;
            break;
        case 1:
            ev->buttons = GPM_B_MIDDLE;
            break;
        case 2:
            ev->buttons = GPM_B_RIGHT;
            break;
        case 64:
            ev->buttons = GPM_B_UP;
            clicks = 0;
            break;
        case 65:
            ev->buttons = GPM_B_DOWN;
            clicks = 0;
            break;
        default:
            // Nothing
            ev->type = 0;
            ev->buttons = 0;
            break;
        }
        ev->type |= (GPM_SINGLE << clicks);
        last_btn = ev->buttons;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get modifier state (shift, alt, ctrl) for the last key pressed.
 * We are assuming that the state didn't change since the key press.
 * This is only correct if get_modifier() is called very fast after
 * the input was received, so that the user didn't release the
 * modifier keys yet.
 */

static int
get_modifier (void)
{
    int result = 0;
#ifdef __QNXNTO__
    static int in_photon = 0;
    static int ph_ig = 0;
#endif

#ifdef HAVE_TEXTMODE_X11_SUPPORT
    if (x11_window != 0)
    {
        Window root, child;
        int root_x, root_y;
        int win_x, win_y;
        unsigned int mask;

        mc_XQueryPointer (x11_display, x11_window, &root, &child, &root_x, &root_y, &win_x, &win_y,
                          &mask);

        if ((mask & ShiftMask) != 0)
            result |= KEY_M_SHIFT;
        if ((mask & ControlMask) != 0)
            result |= KEY_M_CTRL;
        return result;
    }
#endif

#ifdef __QNXNTO__
    if (in_photon == 0)
    {
        // First time here, let's load Photon library and attach to Photon
        in_photon = -1;

        if (getenv ("PHOTON2_PATH") != NULL)
        {
            // QNX 6.x has no support for RTLD_LAZY
            void *ph_handle;

            ph_handle = dlopen ("/usr/lib/libph.so", RTLD_NOW);
            if (ph_handle != NULL)
            {
                ph_attach = (ph_dv_f) dlsym (ph_handle, "PhAttach");
                ph_input_group = (ph_ov_f) dlsym (ph_handle, "PhInputGroup");
                ph_query_cursor = (ph_pqc_f) dlsym (ph_handle, "PhQueryCursor");
                if ((ph_attach != NULL) && (ph_input_group != NULL) && (ph_query_cursor != NULL)
                    && (*ph_attach) (0, 0) != NULL)
                {
                    // Attached
                    ph_ig = (*ph_input_group) (0);
                    in_photon = 1;
                }
            }
        }
    }
    // We do not have Photon running. Assume we are in text console or xterm
    if (in_photon == -1)
    {
        int mod_status;
        int shift_ext_status;

        if (devctl (fileno (stdin), DCMD_CHR_LINESTATUS, &mod_status, sizeof (mod_status), NULL)
            == -1)
            return 0;

        shift_ext_status = mod_status & 0xffffff00UL;
        mod_status &= 0x7f;
        if ((mod_status & _LINESTATUS_CON_ALT) != 0)
            result |= KEY_M_ALT;
        if ((mod_status & _LINESTATUS_CON_CTRL) != 0)
            result |= KEY_M_CTRL;
        if ((mod_status & _LINESTATUS_CON_SHIFT) != 0 || (shift_ext_status & 0x00000800UL) != 0)
            result |= KEY_M_SHIFT;
    }
    else
    {
        PhCursorInfo_t cursor_info;

        (*ph_query_cursor) (ph_ig, &cursor_info);
        if ((cursor_info.key_mods & 0x04) != 0)
            result |= KEY_M_ALT;
        if ((cursor_info.key_mods & 0x02) != 0)
            result |= KEY_M_CTRL;
        if ((cursor_info.key_mods & 0x01) != 0)
            result |= KEY_M_SHIFT;
    }
#endif

#if defined __linux__ || (defined __CYGWIN__ && defined TIOCLINUX)
    {
        unsigned char modifiers = 6;

        if (ioctl (0, TIOCLINUX, &modifiers) < 0)
            return 0;

        // Translate Linux modifiers into mc modifiers
        if ((modifiers & SHIFT_PRESSED) != 0)
            result |= KEY_M_SHIFT;
        if ((modifiers & (ALTL_PRESSED | ALTR_PRESSED)) != 0)
            result |= KEY_M_ALT;
        if ((modifiers & CONTROL_PRESSED) != 0)
            result |= KEY_M_CTRL;
    }
#endif

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
push_char (int c)
{
    gboolean ret = FALSE;

    if (seq_append == NULL)
        seq_append = seq_buffer;

    if (seq_append != &(seq_buffer[SEQ_BUFFER_LEN - 2]))
    {
        *(seq_append++) = c;
        *seq_append = '\0';
        ret = TRUE;
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tty_icrnl_enabled (void)
{
    struct termios mode;

    return tcgetattr (input_fd, &mode) == 0 && (mode.c_iflag & ICRNL) != 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Apply corrections for the keycode generated in get_key_code() */

static int
correct_key_code (int code, gboolean from_sequence)
{
    unsigned int c = code & ~KEY_M_MASK;   // code without modifier
    unsigned int mod = code & KEY_M_MASK;  // modifier
#ifdef __QNXNTO__
    unsigned int qmod; /* bunch of the QNX console
                          modifiers needs unchanged */
#endif

    /*
     * Add key modifiers directly from X11 or OS.
     * Ordinary characters only get modifiers from sequences.
     */
    if (c < 32 || c >= 256)
        mod |= get_modifier ();

    /* Only a raw LF needs the legacy Ctrl-Enter inference. A registered
       sequence already names its key, including any explicit modifiers.
       A LF in pasted text is a line break, not a key. */
    if (c == '\r')
        c = '\n';
    else if (c == '\n' && !from_sequence && !bracketed_pasting_in_progress && !tty_icrnl_enabled ())
        mod |= KEY_M_CTRL;
    else if (c == KEY_ENTER)
        c = '\n';

    // This is reported to be useful on AIX
    if (c == KEY_SCANCEL)
        c = '\t';

    // Convert Back Tab to Shift+Tab
    if (c == KEY_BTAB)
    {
        c = '\t';
        mod = KEY_M_SHIFT;
    }

    // F0 is the same as F10 for out purposes
    if (c == KEY_F (0))
        c = KEY_F (10);

    /*
     * We are not interested if Ctrl was pressed when entering control
     * characters, so assume that it was.  When checking for such keys,
     * XCTRL macro should be used.  In some cases, we are interested,
     * e.g. to distinguish Ctrl-Enter from Enter.
     */
    if (c == '\b')
    {
        // Special case for backspase ('\b' < 32)
        c = KEY_BACKSPACE;
        mod &= ~KEY_M_CTRL;
    }
    else if (c < 32 && c != ESC_CHAR && c != '\t' && c != '\n')
        mod |= KEY_M_CTRL;

#ifdef __QNXNTO__
    qmod = get_modifier ();

    if (c == 127 && mod == 0)
    {
        // Add Ctrl/Alt/Shift-BackSpace
        mod |= get_modifier ();
        c = KEY_BACKSPACE;
    }

    if (c == '0' && mod == 0 && (qmod & KEY_M_SHIFT) == KEY_M_SHIFT)
    {
        // Add Shift-Insert on key pad
        mod = KEY_M_SHIFT;
        c = KEY_IC;
    }

    if (c == '.' && mod == 0 && (qmod & KEY_M_SHIFT) == KEY_M_SHIFT)
    {
        // Add Shift-Del on key pad
        mod = KEY_M_SHIFT;
        c = KEY_DC;
    }
#endif

    // Unrecognized 0177 is delete (preserve Ctrl)
    if (c == 0177)
        c = KEY_BACKSPACE;

#if 0
    // Unrecognized Ctrl-d is delete 
    if (c == 'd' & 31)
    {
        c = KEY_DC;
        mod &= ~KEY_M_CTRL;
    }

    // Unrecognized Ctrl-h is backspace 
    if (c == 'h' & 31)
    {
        c = KEY_BACKSPACE;
        mod &= ~KEY_M_CTRL;
    }
#endif

    // Shift+BackSpace is backspace
    if (c == KEY_BACKSPACE && (mod & KEY_M_SHIFT) != 0)
        mod &= ~KEY_M_SHIFT;

#ifdef HAVE_NCURSES
    /* ncurses reports modified function keys as KEY_F offsets. */
    if (c >= (unsigned) KEY_F (13) && c <= (unsigned) KEY_F (24))
    {
        c -= 12; /* kf(n+12) -> KEY_F(n) */
        mod |= KEY_M_SHIFT;
    }
    else if (c >= (unsigned) KEY_F (25) && c <= (unsigned) KEY_F (36))
    {
        c -= 24; /* kf(n+24) -> KEY_F(n) */
        mod |= KEY_M_CTRL;
    }
    else if (c >= (unsigned) KEY_F (37) && c <= (unsigned) KEY_F (48))
    {
        c -= 36; /* kf(n+36) -> KEY_F(n) */
        mod |= KEY_M_SHIFT | KEY_M_CTRL;
    }
    else if (c >= (unsigned) KEY_F (49) && c <= (unsigned) KEY_F (60))
    {
        c -= 48; /* kf(n+48) -> KEY_F(n) */
        mod |= KEY_M_ALT;
    }
#endif

    // Convert Shift+Fn to F(n+10)
    if (c >= KEY_F (1) && c <= KEY_F (10) && (mod & KEY_M_SHIFT) != 0)
        c += 10;

    // Remove Shift information from function keys
    if (c >= KEY_F (1) && c <= KEY_F (20))
        mod &= ~KEY_M_SHIFT;

    if (!mc_global.tty.alternate_plus_minus)
        switch (c)
        {
        case KEY_KP_ADD:
            c = '+';
            break;
        case KEY_KP_SUBTRACT:
            c = '-';
            break;
        case KEY_KP_MULTIPLY:
            c = '*';
            break;
        default:
            break;
        }

    return (mod | c);
}

/* --------------------------------------------------------------------------------------------- */

static int
getch_with_timeout (unsigned int delay_us)
{
    fd_set Read_FD_Set;
    int c;
    struct timeval time_out;

    time_out.tv_sec = delay_us / G_USEC_PER_SEC;
    time_out.tv_usec = delay_us % G_USEC_PER_SEC;
    tty_nodelay (TRUE);
    FD_ZERO (&Read_FD_Set);
    FD_SET (input_fd, &Read_FD_Set);
    select (input_fd + 1, &Read_FD_Set, NULL, NULL, &time_out);
    c = tty_lowlevel_getch ();
    tty_nodelay (FALSE);
    return c;
}

/* --------------------------------------------------------------------------------------------- */
/* Called after ESC ]. A digit right behind it is no key: it is a late answer of the terminal to
   an OSC query. It is read up to its BEL or ST and dropped. The digit gets a short wait, for an
   answer that comes in two pieces. */

static gboolean
skip_osc_reply (void)
{
    int c;
    int n;

    c = getch_with_timeout (20 * 1000);
    if (c == -1)
        return FALSE;
    if (!g_ascii_isdigit (c))
    {
        const unsigned char b = (unsigned char) c;

        tty_unget_input (&b, 1);
        return FALSE;
    }

    for (n = 0; n < 1024; n++)
    {
        c = getch_with_timeout (old_esc_mode_timeout);
        if (c == -1 || c == '\a')
            break;
        if (c == ESC_CHAR)
        {
            (void) getch_with_timeout (old_esc_mode_timeout);
            break;
        }
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* Is the pending sequence plus @c the start of a CSI sequence with numeric parameters? */

static gboolean
kitty_csi_started (int c)
{
    const int *p;

    if (seq_append == NULL || seq_append - seq_buffer < 2 || seq_buffer[0] != ESC_CHAR
        || seq_buffer[1] != '[')
        return FALSE;

    for (p = seq_buffer + 2; p < seq_append; p++)
        if (!g_ascii_isdigit (*p) && *p != ';' && *p != ':')
            return FALSE;

    return g_ascii_isdigit (c) || c == ';' || c == ':' || (c >= 0x40 && c <= 0x7E);
}

/* --------------------------------------------------------------------------------------------- */
/* The key of CSI number ~ in the kitty protocol, -1 for none */

static int
kitty_tilde_key (unsigned int number)
{
    switch (number)
    {
    case 2:
        return KEY_IC;
    case 3:
        return KEY_DC;
    case 5:
        return KEY_PPAGE;
    case 6:
        return KEY_NPAGE;
    case 7:
        return KEY_HOME;
    case 8:
        return KEY_END;
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        return KEY_F ((int) number - 10);
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
        return KEY_F ((int) number - 11);
    case 23:
    case 24:
        return KEY_F ((int) number - 12);
    default:
        return -1;
    }
}

/* --------------------------------------------------------------------------------------------- */
/* Turn a kitty key event into the code a legacy terminal would give for the same key. */

static int
kitty_key_code (int final, unsigned int key, unsigned int shifted, unsigned int base,
                unsigned int mods)
{
    int mod = 0;
    gboolean ctrl_digit;

    mods = mods > 0 ? mods - 1 : 0;
    if ((mods & KITTY_MOD_HYPER) != 0)
        return -1;
    if ((mods & KITTY_MOD_SUPER) != 0)
        mod |= KEY_M_SUPER;
    if ((mods & KITTY_MOD_SHIFT) != 0)
        mod |= KEY_M_SHIFT;
    if ((mods & (KITTY_MOD_ALT | KITTY_MOD_META)) != 0)
        mod |= KEY_M_ALT;
    if ((mods & KITTY_MOD_CTRL) != 0)
        mod |= KEY_M_CTRL;

    switch (final)
    {
    case 'A':
        return mod | KEY_UP;
    case 'B':
        return mod | KEY_DOWN;
    case 'C':
        return mod | KEY_RIGHT;
    case 'D':
        return mod | KEY_LEFT;
    case 'H':
        return mod | KEY_HOME;
    case 'F':
        return mod | KEY_END;
    case 'P':
        return mod | KEY_F (1);
    case 'Q':
        return mod | KEY_F (2);
    case 'S':
        return mod | KEY_F (4);
    case '~':
    {
        const int code = kitty_tilde_key (key);

        return code == -1 ? -1 : mod | code;
    }
    case 'u':
        break;
    default:
        return -1;
    }

    if (key >= KITTY_KEY_KP_0 && key <= KITTY_KEY_KP_BEGIN)
    {
        const int code = kitty_keypad_keys[key - KITTY_KEY_KP_0];

        return code == -1 ? -1 : mod | code;
    }

    switch (key)
    {
    case 13:
        return mod | '\r';
    case 9:
        return mod | '\t';
    case 27:
        return mod | ESC_CHAR;
    case 127:
        return mod | KEY_BACKSPACE;
    default:
        break;
    }

    // a key of another layout counts as the key at its place in the base layout
    if (key > 126)
        key = base;
    if (key < 32 || key > 126)
        return -1;

    ctrl_digit = (mod & KEY_M_CTRL) != 0 && g_ascii_isdigit ((gchar) key);
    if ((mod & KEY_M_SHIFT) != 0)
    {
        // Ctrl with a digit uses the base key even when kitty reports its shifted symbol
        if (!ctrl_digit)
        {
            if (shifted > 31 && shifted < 127)
                key = shifted;
            else
                key = (unsigned int) g_ascii_toupper ((gchar) key);
        }
        mod &= ~KEY_M_SHIFT;
    }

    if ((mod & KEY_M_CTRL) != 0)
    {
        if (key == ' ' || g_ascii_islower ((gchar) key) || (key >= '@' && key <= '_'))
        {
            // the control character itself, as a legacy terminal sends it
            key &= 0x1F;
            mod &= ~KEY_M_CTRL;
        }
        else if (!g_ascii_isdigit ((gchar) key))
            key = (unsigned int) XCTRL (key);
    }

    return mod | (int) key;
}

/* --------------------------------------------------------------------------------------------- */
/* Parse the parameters and the final byte of a CSI sequence (what follows ESC [) into @ev. */

static gboolean
kitty_parse_csi (const char *params, size_t len, tty_key_event_t *ev)
{
    unsigned int field[3][TTY_KITTY_TEXT_MAX];
    unsigned int nsub[3] = { 1, 1, 1 };
    gboolean text_cut = FALSE;
    unsigned int f = 0, sub = 0;
    size_t i;

    if (len == 0)
        return FALSE;

    memset (field, 0, sizeof (field));
    for (i = 0; i + 1 < len; i++)
    {
        const unsigned char ch = (unsigned char) params[i];

        if (g_ascii_isdigit (ch))
        {
            if (f < 3 && sub < TTY_KITTY_TEXT_MAX && field[f][sub] < 0x10FFFF)
                field[f][sub] = field[f][sub] * 10 + (unsigned int) (ch - '0');
        }
        else if (ch == ':')
        {
            sub++;
            if (f < 3 && sub < TTY_KITTY_TEXT_MAX)
                nsub[f] = sub + 1;
            else if (f == 2)
                text_cut = TRUE;
        }
        else if (ch == ';')
        {
            f++;
            sub = 0;
        }
        else
            return FALSE;
    }
    if ((unsigned char) params[len - 1] < 0x40 || (unsigned char) params[len - 1] > 0x7E)
        return FALSE;

    memset (ev, 0, sizeof (*ev));
    ev->final = params[len - 1];
    ev->key = field[0][0];
    ev->shifted = field[0][1];
    ev->base = field[0][2];
    ev->mods = field[1][0] > 0 ? field[1][0] - 1 : 0;
    ev->event = field[1][1] == 0 ? TTY_KITTY_PRESS : (int) field[1][1];
    // a part of the text would be another text: none is kept
    if (f >= 2 && !text_cut)
        for (sub = 0; sub < nsub[2] && field[2][sub] != 0; sub++)
            ev->text[ev->text_len++] = (gunichar) field[2][sub];
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* Win32 input mode: CSI Vk ; Sc ; Uc ; Kd ; Cs ; Rc _ is one key record of the Windows console.
   The bits of the control key state Cs: */

#define WIN32_CS_RIGHT_ALT  0x0001
#define WIN32_CS_LEFT_ALT   0x0002
#define WIN32_CS_RIGHT_CTRL 0x0004
#define WIN32_CS_LEFT_CTRL  0x0008
#define WIN32_CS_SHIFT      0x0010
#define WIN32_CS_NUM_LOCK   0x0020
#define WIN32_CS_CAPS_LOCK  0x0080
#define WIN32_CS_ENHANCED   0x0100

/* The kitty form of a key that has a final letter or a number with '~' in the kitty protocol */
static gboolean
win32_vk_kitty_form (unsigned int vk, char *final, unsigned int *key)
{
    static const struct
    {
        unsigned int vk;
        char final;
        unsigned int key;
    } forms[] = {
        { 0x25, 'D', 1 },  { 0x26, 'A', 1 },  { 0x27, 'C', 1 },  { 0x28, 'B', 1 },
        { 0x24, 'H', 1 },  { 0x23, 'F', 1 },  { 0x2D, '~', 2 },  { 0x2E, '~', 3 },
        { 0x21, '~', 5 },  { 0x22, '~', 6 },  { 0x70, 'P', 1 },  { 0x71, 'Q', 1 },
        { 0x72, '~', 13 }, { 0x73, 'S', 1 },  { 0x74, '~', 15 }, { 0x75, '~', 17 },
        { 0x76, '~', 18 }, { 0x77, '~', 19 }, { 0x78, '~', 20 }, { 0x79, '~', 21 },
        { 0x7A, '~', 23 }, { 0x7B, '~', 24 },
    };
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (forms); i++)
        if (forms[i].vk == vk)
        {
            *final = forms[i].final;
            *key = forms[i].key;
            return TRUE;
        }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/* The character of a punctuation key (VK_OEM_*) in the US layout, 0 for none */

static unsigned int
win32_vk_base_key (unsigned int vk)
{
    switch (vk)
    {
    case 0xBA:
        return ';';
    case 0xBB:
        return '=';
    case 0xBC:
        return ',';
    case 0xBD:
        return '-';
    case 0xBE:
        return '.';
    case 0xBF:
        return '/';
    case 0xC0:
        return '`';
    case 0xDB:
        return '[';
    case 0xDC:
        return '\\';
    case 0xDD:
        return ']';
    case 0xDE:
        return '\'';
    default:
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */
/* The kitty key number of a key of the CSI u form, 0 for none */

static unsigned int
win32_vk_kitty_key (unsigned int vk, unsigned int sc, unsigned int cs, unsigned int uc)
{
    const gboolean right = (cs & WIN32_CS_ENHANCED) != 0;

    switch (vk)
    {
    case 0x08:
        return 127;
    case 0x09:
        return 9;
    case 0x0D:
        return right ? 57414 : 13;  // the keypad Enter is an enhanced key
    case 0x1B:
        return 27;
    case 0x20:
        return 32;
    case 0x10:
        return sc == 0x36 ? 57447 : 57441;  // Shift: the scan code tells the right one
    case 0xA0:
        return 57441;
    case 0xA1:
        return 57447;
    case 0x11:
        return right ? 57448 : 57442;
    case 0xA2:
        return 57442;
    case 0xA3:
        return 57448;
    case 0x12:
        return right ? 57449 : 57443;
    case 0xA4:
        return 57443;
    case 0xA5:
        return 57449;
    case 0x5B:
        return 57444;
    case 0x5C:
        return 57450;
    case 0x14:
        return 57358;
    case 0x91:
        return 57359;
    case 0x90:
        return 57360;
    case 0x2C:
        return 57361;  // Print Screen
    case 0x13:
        return 57362;  // Pause
    case 0x5D:
        return 57363;  // Menu
    case 0x6A:
        return 57411;
    case 0x6B:
        return 57413;
    case 0x6C:
        return 57415;
    case 0x6D:
        return 57412;
    case 0x6E:
        return 57409;
    case 0x6F:
        return 57410;
    default:
        break;
    }

    if (vk >= 'A' && vk <= 'Z')
        return vk - 'A' + 'a';  // the key in the base layout, whatever the layout gives
    if (vk >= '0' && vk <= '9')
        return vk;
    if (vk >= 0x60 && vk <= 0x69)
        return KITTY_KEY_KP_0 + vk - 0x60;
    if (vk >= 0x7C && vk <= 0x87)
        return 57376 + vk - 0x7C;  // F13 to F24

    // a punctuation key: the character of the layout, else the one of the key in the US layout
    if (uc >= 0x20 && uc < 0x7F)
        return uc;
    return win32_vk_base_key (vk);
}

/* --------------------------------------------------------------------------------------------- */
/* Parse the parameters of a Win32 input record (what follows ESC [, with the '_') into @ev.
   A half of a UTF-16 pair leaves @ev->final 0 and the half in @ev->key. */

static gboolean
win32_parse_record (const char *params, size_t len, tty_key_event_t *ev)
{
    unsigned int field[6] = { 0, 0, 0, 0, 0, 1 };
    unsigned int f = 0;
    unsigned int vk, sc, uc, cs;
    gboolean ctrl, alt;
    size_t i;

    if (len == 0 || params[len - 1] != '_')
        return FALSE;

    for (i = 0; i + 1 < len; i++)
    {
        const unsigned char ch = (unsigned char) params[i];

        if (g_ascii_isdigit (ch))
        {
            if (f < G_N_ELEMENTS (field) && field[f] < 0x10FFFF)
                field[f] = field[f] * 10 + (unsigned int) (ch - '0');
        }
        else if (ch == ';')
        {
            f++;
            if (f < G_N_ELEMENTS (field))
                field[f] = 0;
        }
        else
            return FALSE;
    }

    // a held key may come as one record with a count, a WORD in the Windows console
    win32_record_repeat = field[5] == 0 ? 1 : MIN (field[5], 0xFFFF);

    vk = field[0];
    sc = field[1];
    uc = field[2];
    cs = field[4];

    memset (ev, 0, sizeof (*ev));
    ev->event = field[3] != 0 ? TTY_KITTY_PRESS : TTY_KITTY_RELEASE;

    ctrl = (cs & (WIN32_CS_LEFT_CTRL | WIN32_CS_RIGHT_CTRL)) != 0;
    alt = (cs & (WIN32_CS_LEFT_ALT | WIN32_CS_RIGHT_ALT)) != 0;
    // AltGr is Ctrl and Alt on Windows: with a character it is only that character
    if (ctrl && alt && uc >= 0x20 && uc != 0x7F)
        ctrl = alt = FALSE;

    if ((cs & WIN32_CS_SHIFT) != 0)
        ev->mods |= KITTY_MOD_SHIFT;
    if (alt)
        ev->mods |= KITTY_MOD_ALT;
    if (ctrl)
        ev->mods |= KITTY_MOD_CTRL;
    if ((cs & WIN32_CS_CAPS_LOCK) != 0)
        ev->mods |= KITTY_MOD_CAPS_LOCK;
    if ((cs & WIN32_CS_NUM_LOCK) != 0)
        ev->mods |= KITTY_MOD_NUM_LOCK;

    // a character outside the BMP comes as two records, one for each half of its UTF-16 pair:
    // the half is kept in key, and win32_join_pair () puts the two together
    if (uc >= 0xD800 && uc <= 0xDFFF)
    {
        ev->key = uc;
        return TRUE;
    }

    // Alt and the digits of the keypad give their character on the release of Alt
    if (ev->event == TTY_KITTY_RELEASE && (vk == 0x12 || vk == 0xA4 || vk == 0xA5) && uc >= 0x20
        && uc != 0x7F)
    {
        ev->event = TTY_KITTY_PRESS;
        ev->mods &= ~KITTY_MOD_ALT;
        vk = 0;
    }

    if (!win32_vk_kitty_form (vk, &ev->final, &ev->key))
    {
        ev->key = win32_vk_kitty_key (vk, sc, cs, uc);
        // an input method or a paste gives a character with no key
        if (ev->key == 0 && uc >= 0x20 && uc != 0x7F)
            ev->key = uc;
        if (ev->key == 0)
            return TRUE;
        ev->final = 'u';
        if (vk >= 'A' && vk <= 'Z')
            ev->base = ev->key;
        else if (win32_vk_base_key (vk) != 0)
            ev->base = win32_vk_base_key (vk);
        // the shifted character, as the kitty protocol gives it with its alternate keys
        if ((ev->mods & KITTY_MOD_SHIFT) != 0 && uc > 0x20 && uc < 0x7F)
            ev->shifted = uc;
    }

    if (ev->event == TTY_KITTY_PRESS && uc >= 0x20 && uc != 0x7F)
    {
        ev->text[0] = (gunichar) uc;
        ev->text_len = 1;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* Put the two halves of a UTF-16 pair of the live input together. TRUE when @ev is a whole key:
   any key but a half, or the second half that makes the character with the first one. */

static gboolean
win32_join_pair (tty_key_event_t *ev)
{
    gunichar cp;

    if (ev->final != '\0')
        return TRUE;
    if (ev->event != TTY_KITTY_PRESS)
        return FALSE;
    if (ev->key >= 0xD800 && ev->key <= 0xDBFF)
    {
        win32_high_surrogate = ev->key;
        return FALSE;
    }
    // anything but a second half after a first one: no character, and the first half is gone
    if (win32_high_surrogate == 0 || ev->key < 0xDC00 || ev->key > 0xDFFF)
    {
        win32_high_surrogate = 0;
        return FALSE;
    }

    cp = 0x10000 + ((win32_high_surrogate - 0xD800) << 10) + (ev->key - 0xDC00);
    win32_high_surrogate = 0;
    ev->final = 'u';
    ev->key = cp;
    ev->text[0] = cp;
    ev->text_len = 1;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* Parse a key sequence (what follows ESC [) of the protocol that is on into @ev */

static gboolean
key_parse_csi (const char *params, size_t len, tty_key_event_t *ev)
{
    win32_record_repeat = 1;
    if (win32_input_active && len > 0 && params[len - 1] == '_')
        return win32_parse_record (params, len, ev);
    return kitty_parse_csi (params, len, ev);
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
key_events_active (void)
{
    return kitty_keyboard_active || win32_input_active;
}

/* --------------------------------------------------------------------------------------------- */
/* The mc key of a kitty event: -1 for none, KEY_KITTY_EVENT for a key mc has no code for. */

static int
kitty_event_to_code (const tty_key_event_t *ev)
{
    int code;

    if (ev->event == TTY_KITTY_RELEASE)
        return KEY_KITTY_EVENT;

    code = kitty_key_code (ev->final, ev->key, ev->shifted, ev->base, ev->mods + 1);
    if (code == -1 && ev->final != 'u')
    {
        /* a legacy form that came with an event type or a lock: the same sequence without them */
        const unsigned int mods = ev->mods & ~(KITTY_MOD_CAPS_LOCK | KITTY_MOD_NUM_LOCK);
        GString *seq = g_string_new (ESC_STR "[");

        if (mods != 0)
            g_string_append_printf (seq, "%u;%u", ev->key, mods + 1);
        else if (ev->final == '~')
            g_string_append_printf (seq, "%u", ev->key);
        g_string_append_c (seq, ev->final);
        code = tty_match_seq_to_keycode (seq->str, (int) seq->len);
        g_string_free (seq, TRUE);
        if (code == 0)
            code = -1;
    }

    return code == -1 ? KEY_KITTY_EVENT : code;
}

/* --------------------------------------------------------------------------------------------- */

static guint
kitty_flags_base (void)
{
    return KITTY_KEYBOARD_BASE | (keybar_modifiers ? KITTY_KEYBOARD_EXTRA : 0);
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
kitty_text_pending (void)
{
    return kitty_text_pos < kitty_text_len || key_repeat_left > 0;
}

/* --------------------------------------------------------------------------------------------- */
/* The text of a key with no Ctrl, Alt or Super, in the bytes a legacy terminal would send.
   Returns the first byte and keeps the rest for get_key_code (), or -1 when there is no text. */

static int
kitty_text_start (const tty_key_event_t *ev)
{
    GString *utf8;
    char *bytes;
    gsize len = 0;
    int i;

    if (ev->final != 'u' || ev->text_len == 0
        || (ev->mods
            & (KITTY_MOD_ALT | KITTY_MOD_CTRL | KITTY_MOD_SUPER | KITTY_MOD_HYPER | KITTY_MOD_META))
            != 0)
        return -1;

    utf8 = g_string_sized_new (16);
    for (i = 0; i < ev->text_len; i++)
    {
        if (ev->text[i] < 0x20 || ev->text[i] == 0x7F || !g_unichar_validate (ev->text[i]))
        {
            g_string_free (utf8, TRUE);
            return -1;
        }
        g_string_append_unichar (utf8, ev->text[i]);
    }

    if (mc_global.utf8_display)
    {
        len = utf8->len;
        bytes = g_string_free (utf8, FALSE);
    }
    else
    {
        bytes = g_locale_from_utf8 (utf8->str, (gssize) utf8->len, NULL, &len, NULL);
        g_string_free (utf8, TRUE);
    }

    if (bytes == NULL || len == 0 || len > sizeof (kitty_text))
    {
        g_free (bytes);
        return -1;
    }

    memcpy (kitty_text, bytes, len);
    g_free (bytes);
    kitty_text_len = len;
    kitty_text_pos = 1;
    return kitty_text[0];
}

/* --------------------------------------------------------------------------------------------- */
/* Follow the held modifier keys. A modifier key sets or clears its own bit; the modifier field
   of any event fixes the other ones, so a release the terminal never sent is not kept. */

static void
kitty_track_modifiers (const tty_key_event_t *ev)
{
    int own = -1;
    int i;

    if (ev->final == 'u' && ev->key >= KITTY_KEY_MOD_FIRST
        && ev->key < KITTY_KEY_MOD_FIRST + 2 * KITTY_KEY_MOD_SIDES)
    {
        const guint bit = 1U << (ev->key - KITTY_KEY_MOD_FIRST);

        if (ev->event == TTY_KITTY_RELEASE)
            kitty_mod_keys &= ~bit;
        else
            kitty_mod_keys |= bit;
        own = (int) ((ev->key - KITTY_KEY_MOD_FIRST) % KITTY_KEY_MOD_SIDES);
    }

    for (i = 0; i < KITTY_KEY_MOD_SIDES; i++)
    {
        const guint both = (1U << i) | (1U << (i + KITTY_KEY_MOD_SIDES));

        if (i == own)
            continue;
        if ((ev->mods & kitty_mod_kinds[i]) == 0)
            kitty_mod_keys &= ~both;
        else if ((kitty_mod_keys & both) == 0)
            kitty_mod_keys |= 1U << i;
    }
}

/* --------------------------------------------------------------------------------------------- */
/* The mc key of a kitty event read from the terminal: the text of a printable key, -1 for a
   release no one asked for, MCKEY_MODIFIERS for a lone modifier, else what kitty_event_to_code ()
   gives. */

static int
kitty_live_code (const tty_key_event_t *ev)
{
    int code;

    // a lone modifier is a key only for a program in mcterm that asked for every key
    if (ev->final == 'u' && ev->key >= KITTY_KEY_MOD_FIRST && ev->key <= KITTY_KEY_MOD_LAST)
        return (kitty_flags_wanted & KITTY_KEYBOARD_ALL_KEYS) != 0 ? KEY_KITTY_EVENT
                                                                   : MCKEY_MODIFIERS;

    if (ev->event == TTY_KITTY_RELEASE)
        return (kitty_flags_wanted & KITTY_KEYBOARD_EVENTS) != 0 ? KEY_KITTY_EVENT : -1;

    code = kitty_text_start (ev);
    return code != -1 ? code : kitty_event_to_code (ev);
}

/* --------------------------------------------------------------------------------------------- */
/* Read the rest of a CSI sequence after the pending bytes and @c, and decode it.
   Returns -1 for a sequence that is no key at all, KEY_KITTY_EVENT for a key mc has no code for. */

static int
kitty_read_csi (int c)
{
    const size_t pending = (size_t) (seq_append - seq_buffer);
    GString *params = g_string_sized_new (32);
    size_t i;
    int ch, code = -1;

    for (i = 2;; i++)
    {
        if (i < pending)
            ch = seq_buffer[i];
        else if (i == pending)
            ch = c;
        else
        {
            ch = getch_with_timeout (KITTY_CSI_TIMEOUT);
            if (ch == -1)
                break;
        }

        if (ch < 0x20 || ch > 0x7E)
            break;
        if (params->len >= KITTY_CSI_MAX)
        {
            // too long to keep: read it to its end, so that no tail of it is taken as keys
            if (ch >= 0x40)
                break;
            continue;
        }
        g_string_append_c (params, (char) ch);
        if (ch >= 0x40)
        {
            if (params->len == 1 && (ch == 'I' || ch == 'O'))
            {
                // the terminal got or lost the focus: a modifier released elsewhere is not seen
                code = kitty_mod_keys != 0 ? MCKEY_MODIFIERS : -1;
                kitty_mod_keys = 0;
            }
            else if (key_parse_csi (params->str, params->len, &kitty_event))
            {
                // a half of a UTF-16 pair is a key only with the other half
                if (win32_join_pair (&kitty_event))
                {
                    kitty_event_valid = TRUE;
                    kitty_track_modifiers (&kitty_event);
                    code = kitty_live_code (&kitty_event);
                    if (win32_record_repeat > 1 && kitty_event.event != TTY_KITTY_RELEASE
                        && code >= 0)
                        key_repeat_left = win32_record_repeat - 1;
                }
            }
            break;
        }
    }

    g_string_free (params, TRUE);
    return code;
}

/* --------------------------------------------------------------------------------------------- */
/* Remove the kitty release events at the start of the bytes @raw */

static void
kitty_drop_leading_releases (GString *raw)
{
    while (key_events_active () && raw->len > 3 && raw->str[0] == ESC_CHAR && raw->str[1] == '[')
    {
        tty_key_event_t ev;
        size_t end;

        for (end = 2; end < raw->len; end++)
            if ((unsigned char) raw->str[end] >= 0x40 && (unsigned char) raw->str[end] <= 0x7E)
                break;
        if (end == raw->len || !key_parse_csi (raw->str + 2, end - 1, &ev)
            || ev.event != TTY_KITTY_RELEASE)
            return;
        g_string_erase (raw, 0, (gssize) end + 1);
    }
}

/* --------------------------------------------------------------------------------------------- */
/* Read the bytes of one key into @raw: wait for the first one, then take what comes in a short
   time after it. */

static void
learn_read_burst (GString *raw)
{
    // LEARN_TIMEOUT in ms
#define LEARN_TIMEOUT 200

    fd_set Read_FD_Set;
    gint64 end_time;
    int c;

    c = tty_lowlevel_getch ();
    while (c == -1)
        c = tty_lowlevel_getch ();  // Sanity check, should be unnecessary
    g_string_append_c (raw, (char) c);

    end_time = g_get_monotonic_time () + LEARN_TIMEOUT * MC_USEC_PER_MSEC;

    tty_nodelay (TRUE);
    while (TRUE)
    {
        while ((c = tty_lowlevel_getch ()) == -1)
        {
            gint64 time_out;
            struct timeval tv;

            time_out = end_time - g_get_monotonic_time ();
            if (time_out <= 0)
                break;

            tv.tv_sec = time_out / G_USEC_PER_SEC;
            tv.tv_usec = time_out % G_USEC_PER_SEC;
            FD_ZERO (&Read_FD_Set);
            FD_SET (input_fd, &Read_FD_Set);
            select (input_fd + 1, &Read_FD_Set, NULL, NULL, &tv);
        }
        if (c == -1)
            break;
        g_string_append_c (raw, (char) c);
    }
    tty_nodelay (FALSE);
#undef LEARN_TIMEOUT
}

/* --------------------------------------------------------------------------------------------- */
/* Where the captured key starts in @raw, -1 to wait for more. A kitty terminal reports the
   modifiers of Ctrl-X before the X: they are skipped. A modifier alone is the key when all the
   modifiers are released with no other key. */

static gssize
kitty_capture_start (const GString *raw)
{
    gssize first_mod = -1;
    guint held = 0;
    size_t pos = 0;

    if (!key_events_active ())
        return 0;

    while (pos < raw->len)
    {
        tty_key_event_t ev;
        size_t end;

        if (raw->str[pos] != ESC_CHAR || pos + 1 >= raw->len || raw->str[pos + 1] != '[')
            return (gssize) pos;
        for (end = pos + 2; end < raw->len; end++)
            if ((unsigned char) raw->str[end] >= 0x40 && (unsigned char) raw->str[end] <= 0x7E)
                break;
        if (end == raw->len)
            return -1;
        if (!key_parse_csi (raw->str + pos + 2, end - pos - 1, &ev))
            return (gssize) pos;

        if (ev.final != 'u' || ev.key < KITTY_KEY_MOD_FIRST || ev.key > KITTY_KEY_MOD_LAST)
        {
            if (ev.event != TTY_KITTY_RELEASE)
                return (gssize) pos;
        }
        else if (ev.event == TTY_KITTY_RELEASE)
        {
            held &= ~(1U << (ev.key - KITTY_KEY_MOD_FIRST));
            if (held == 0 && first_mod >= 0)
                return first_mod;
        }
        else
        {
            if (first_mod < 0)
                first_mod = (gssize) pos;
            held |= 1U << (ev.key - KITTY_KEY_MOD_FIRST);
        }
        pos = end + 1;
    }

    return -1;
}

/* --------------------------------------------------------------------------------------------- */
/* The length of the first key in the bytes @raw. A kitty terminal sends the release (and the
   repeats) of a key right after it, as another CSI sequence. */

static size_t
kitty_first_key_len (const GString *raw)
{
    size_t end;

    if (!key_events_active () || raw->len < 3 || raw->str[0] != ESC_CHAR || raw->str[1] != '[')
        return raw->len;

    for (end = 2; end < raw->len; end++)
        if ((unsigned char) raw->str[end] >= 0x40 && (unsigned char) raw->str[end] <= 0x7E)
        {
            tty_key_event_t ev;
            size_t next;

            if (end + 1 >= raw->len || raw->str[end + 1] != ESC_CHAR)
                return raw->len;

            // the first half of a UTF-16 pair of Win32 records: the key is both of them
            if (key_parse_csi (raw->str + 2, end - 1, &ev) && ev.final == '\0'
                && ev.event == TTY_KITTY_PRESS && ev.key >= 0xD800 && ev.key <= 0xDBFF)
                for (next = end + 3; next < raw->len; next++)
                    if ((unsigned char) raw->str[next] >= 0x40
                        && (unsigned char) raw->str[next] <= 0x7E)
                        return next + 1 < raw->len && raw->str[next + 1] == ESC_CHAR ? next + 1
                                                                                     : raw->len;
            return end + 1;
        }

    return raw->len;
}

/* --------------------------------------------------------------------------------------------- */

static void
learn_store_key (GString *buffer, int c)
{
    if (c == ESC_CHAR)
        g_string_append (buffer, "\\e");
    else if (c < ' ')
    {
        g_string_append_c (buffer, '^');
        g_string_append_c (buffer, c + 'a' - 1);
    }
    else if (c == '^')
        g_string_append (buffer, "^^");
    else
        g_string_append_c (buffer, (char) c);
}

/* --------------------------------------------------------------------------------------------- */

static void
k_dispose (key_def *k)
{
    if (k != NULL)
    {
        k_dispose (k->child);
        k_dispose (k->next);
        g_free (k);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
key_code_comparator_by_name (const void *p1, const void *p2)
{
    const key_code_name_t *n1 = *(const key_code_name_t *const *) p1;
    const key_code_name_t *n2 = *(const key_code_name_t *const *) p2;

    return g_ascii_strcasecmp (n1->name, n2->name);
}

/* --------------------------------------------------------------------------------------------- */

static int
key_code_comparator_by_code (const void *p1, const void *p2)
{
    const key_code_name_t *n1 = *(const key_code_name_t *const *) p1;
    const key_code_name_t *n2 = *(const key_code_name_t *const *) p2;

    return n1->code - n2->code;
}

/* --------------------------------------------------------------------------------------------- */

static inline void
sort_key_conv_tab (enum KeySortType type_sort)
{
    if (has_been_sorted != type_sort)
    {
        size_t i;

        for (i = 0; i < key_conv_tab_size; i++)
            key_conv_tab_sorted[i] = &key_name_conv_tab[i];

        if (type_sort == KEY_SORTBYNAME)
            qsort (key_conv_tab_sorted, key_conv_tab_size, sizeof (key_conv_tab_sorted[0]),
                   &key_code_comparator_by_name);
        else if (type_sort == KEY_SORTBYCODE)
            qsort (key_conv_tab_sorted, key_conv_tab_size, sizeof (key_conv_tab_sorted[0]),
                   &key_code_comparator_by_code);

        has_been_sorted = type_sort;
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
lookup_keyname (const char *name, int *idx)
{
    if (name[0] != '\0')
    {
        const key_code_name_t key = { 0, name, NULL, NULL };
        const key_code_name_t *keyp = &key;
        const key_code_name_t **res;

        if (name[1] == '\0')
        {
            *idx = -1;
            return (int) name[0];
        }

        sort_key_conv_tab (KEY_SORTBYNAME);

        res = bsearch (&keyp, key_conv_tab_sorted, key_conv_tab_size,
                       sizeof (key_conv_tab_sorted[0]), key_code_comparator_by_name);

        if (res != NULL)
        {
            *idx = (int) (res - key_conv_tab_sorted);
            return (*res)->code;
        }
    }

    *idx = -1;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
lookup_keycode (const int code, int *idx)
{
    if (code != 0)
    {
        const key_code_name_t key = { code, NULL, NULL, NULL };
        const key_code_name_t *keyp = &key;
        const key_code_name_t **res;

        sort_key_conv_tab (KEY_SORTBYCODE);

        res = bsearch (&keyp, key_conv_tab_sorted, key_conv_tab_size,
                       sizeof (key_conv_tab_sorted[0]), key_code_comparator_by_code);

        if (res != NULL)
        {
            *idx = (int) (res - key_conv_tab_sorted);
            return TRUE;
        }
    }

    *idx = -1;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

int
tty_normalize_keycode (int code)
{
    unsigned int c = code & ~KEY_M_MASK;   // code without modifier
    unsigned int mod = code & KEY_M_MASK;  // modifier

    if (c == KEY_ENTER)
        c = '\n';

    if (c == KEY_F (0))
        c = KEY_F (10);

    if (c >= KEY_F (1) && c <= KEY_F (10) && (mod & KEY_M_SHIFT) != 0)
        c += 10;

    if (c >= KEY_F (1) && c <= KEY_F (20))
        mod &= ~KEY_M_SHIFT;

    return (int) (mod | c);
}

/* --------------------------------------------------------------------------------------------- */

static void
remember_key_sequence (int code, const char *seq)
{
    int normalized_code;

    if (code <= 0 || seq == NULL)
        return;

    normalized_code = tty_normalize_keycode (code);
    if (key_sequences == NULL)
        key_sequences = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

    g_hash_table_replace (key_sequences, GINT_TO_POINTER (normalized_code), g_strdup (seq));
}

/* --------------------------------------------------------------------------------------------- */

static char *
key_sequence_to_printable (const char *seq, size_t len)
{
    GString *out;
    size_t i;

    out = g_string_sized_new (len * 2);
    for (i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char) seq[i];

        if (c == 27)
            g_string_append (out, "\\e");
        else if (c < 32)
            g_string_append_printf (out, "^%c", c + 64);
        else
            g_string_append_c (out, (char) c);
    }

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/* This has to be called before init_slang or whatever routine
   calls any define_sequence */

void
init_key (void)
{
    const char *term;

    term = getenv ("TERM");

    // This has to be the first define_sequence
    // So, we can assume that the first keys member has ESC
    define_sequences (mc_default_keys);

    // Terminfo on irix does not have some keys
    if (mc_global.tty.xterm_flag
        || (term != NULL
            && (strncmp (term, "iris-ansi", 9) == 0 || strncmp (term, "xterm", 5) == 0
                || strncmp (term, "rxvt", 4) == 0 || strncmp (term, "screen", 6) == 0)))
        define_sequences (xterm_key_defines);

    // load some additional keys (e.g. direct Alt-? support)
    load_xtra_key_defines ();

#ifdef HAVE_TEXTMODE_X11_SUPPORT
    init_key_x11 ();
#endif

    /* Load the qansi-m key definitions
       if we are running under the qansi-m terminal */
    if (term != NULL && (strncmp (term, "qansi-m", 7) == 0))
        define_sequences (qansi_key_defines);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * This has to be called after SLang_init_tty/slint_init
 */

void
init_key_input_fd (void)
{
#ifdef HAVE_SLANG
    input_fd = SLang_TT_Read_FD;
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
done_key (void)
{
    k_dispose (keys);
    keys = NULL;
    g_clear_pointer (&key_sequences, g_hash_table_destroy);
    g_slist_free_full (select_list, g_free);
    select_list = NULL;

#ifdef HAVE_TEXTMODE_X11_SUPPORT
    if (x11_display)
        mc_XCloseDisplay (x11_display);
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
add_select_channel (int fd, select_fn callback, void *info)
{
    select_t *new;

    new = g_new (select_t, 1);
    new->fd = fd;
    new->callback = callback;
    new->info = info;

    select_list = g_slist_prepend (select_list, new);
}

/* --------------------------------------------------------------------------------------------- */

void
delete_select_channel (int fd)
{
    GSList *p;

    p = g_slist_find_custom (select_list, GINT_TO_POINTER (fd), select_cmp_by_fd);
    if (p != NULL)
    {
        g_free (p->data);
        select_list = g_slist_delete_link (select_list, p);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
channels_up (void)
{
    if (disabled_channels == 0)
        fputs ("Error: channels_up called with disabled_channels = 0\n", stderr);
    disabled_channels--;
}

/* --------------------------------------------------------------------------------------------- */

void
channels_down (void)
{
    disabled_channels++;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Return the code associated with the symbolic name keyname
 */

int
tty_keyname_to_keycode (const char *name, char **label)
{
    char **lc_keys, **p;
    char *cname;
    int k = -1;
    int key = 0;
    int lc_index = -1;

    int use_meta = -1;
    int use_ctrl = -1;
    int use_shift = -1;
    int use_super = -1;

    if (name == NULL)
        return 0;

    cname = g_strstrip (g_strdup (name));
    lc_keys = g_strsplit_set (cname, "-+ ", -1);
    g_free (cname);

    for (p = lc_keys; p != NULL && *p != NULL; p++)
    {
        if ((*p)[0] != '\0')
        {
            int idx;

            key = lookup_keyname (g_strstrip (*p), &idx);

            if (key == KEY_M_ALT)
                use_meta = idx;
            else if (key == KEY_M_CTRL)
                use_ctrl = idx;
            else if (key == KEY_M_SHIFT)
                use_shift = idx;
            else if (key == KEY_M_SUPER)
                use_super = idx;
            else
            {
                k = key;
                lc_index = idx;
                break;
            }
        }
    }

    g_strfreev (lc_keys);

    // output
    if (k <= 0)
        return 0;

    if (label != NULL)
    {
        GString *s;

        s = g_string_new ("");

        if (use_super != -1)
        {
            g_string_append (s, key_conv_tab_sorted[use_super]->shortcut);
            g_string_append_c (s, '-');
        }
        if (use_meta != -1)
        {
            g_string_append (s, key_conv_tab_sorted[use_meta]->shortcut);
            g_string_append_c (s, '-');
        }
        if (use_ctrl != -1)
        {
            g_string_append (s, key_conv_tab_sorted[use_ctrl]->shortcut);
            g_string_append_c (s, '-');
        }
        if (use_shift != -1)
        {
            if (k < 127)
                g_string_append_c (s, (gchar) g_ascii_toupper ((gchar) k));
            else
            {
                g_string_append (s, key_conv_tab_sorted[use_shift]->shortcut);
                g_string_append_c (s, '-');
                g_string_append (s, key_conv_tab_sorted[lc_index]->shortcut);
            }
        }
        else if (k < 128)
        {
            if ((k >= 'A') || (lc_index < 0) || (key_conv_tab_sorted[lc_index]->shortcut == NULL))
                g_string_append_c (s, (gchar) g_ascii_tolower ((gchar) k));
            else
                g_string_append (s, key_conv_tab_sorted[lc_index]->shortcut);
        }
        else if ((lc_index != -1) && (key_conv_tab_sorted[lc_index]->shortcut != NULL))
            g_string_append (s, key_conv_tab_sorted[lc_index]->shortcut);
        else
            g_string_append_c (s, (gchar) g_ascii_tolower ((gchar) key));

        *label = g_string_free (s, FALSE);
    }

    if (use_shift != -1)
    {
        if (k < 127 && k > 31)
            k = g_ascii_toupper ((gchar) k);
        else
            k |= KEY_M_SHIFT;
    }

    if (use_ctrl != -1)
    {
        // XCTRL ('1') is Ctrl-Q: a digit keeps its code and gets the modifier
        if (k < 256 && !g_ascii_isdigit ((gchar) k))
            k = XCTRL (k);
        else
            k |= KEY_M_CTRL;
    }

    if (use_meta != -1)
        k = ALT (k);

    if (use_super != -1)
        k |= KEY_M_SUPER;

    return k;
}

/* --------------------------------------------------------------------------------------------- */

char *
tty_keycode_to_keyname (const int keycode)
{
    // code without modifier
    unsigned int k = keycode & ~KEY_M_MASK;
    // modifier
    unsigned int mod = keycode & KEY_M_MASK;

    int key_idx = -1;

    GString *s;
    int idx;

    s = g_string_sized_new (8);

    if (lookup_keycode (k, &key_idx) || (k > 0 && k < 256))
    {
        if ((mod & KEY_M_SUPER) != 0 && lookup_keycode (KEY_M_SUPER, &idx))
        {
            g_string_append (s, key_conv_tab_sorted[idx]->shortcut);
            g_string_append_c (s, '-');
        }

        if ((mod & KEY_M_CTRL) != 0)
        {
            // non printeble chars like a CTRL-[A..Z]
            if (k < 32)
                k += 64;

            if (lookup_keycode (KEY_M_CTRL, &idx))
            {
                g_string_append (s, key_conv_tab_sorted[idx]->shortcut);
                g_string_append_c (s, '-');
            }
        }

        if ((mod & KEY_M_ALT) != 0 && lookup_keycode (KEY_M_ALT, &idx))
        {
            g_string_append (s, key_conv_tab_sorted[idx]->shortcut);
            g_string_append_c (s, '-');
        }

        if ((mod & KEY_M_SHIFT) != 0)
        {
            if (lookup_keycode (KEY_M_SHIFT, &idx))
            {
                if (k >= ' ' && k < 127)
                    g_string_append_c (s, (gchar) g_ascii_toupper ((gchar) k));
                else
                {
                    g_string_append (s, key_conv_tab_sorted[idx]->shortcut);
                    g_string_append_c (s, '-');
                    if (key_idx >= 0 && key_conv_tab_sorted[key_idx]->shortcut != NULL)
                        g_string_append (s, key_conv_tab_sorted[key_idx]->shortcut);
                    else
                        g_string_append_c (s, (gchar) k);
                }
            }
        }
        else if (k < 128)
        {
            if ((k >= 'A') || (key_idx < 0) || (key_conv_tab_sorted[key_idx]->name == NULL))
                g_string_append_c (s, (gchar) k);
            else
                g_string_append (s, key_conv_tab_sorted[key_idx]->name);
        }
        else if ((key_idx != -1) && (key_conv_tab_sorted[key_idx]->name != NULL))
            g_string_append (s, key_conv_tab_sorted[key_idx]->name);
        else
            g_string_append_c (s, (gchar) keycode);
    }

    return g_string_free (s, s->len == 0);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Return TRUE on success, FALSE on error.
 * An error happens if SEQ is a beginning of an existing longer sequence.
 */

gboolean
define_sequence (int code, const char *seq, int action)
{
    key_def *base;
    const char *original_seq = seq;

    if (strlen (seq) > SEQ_BUFFER_LEN - 1)
        return FALSE;

    for (base = keys; (base != NULL) && (*seq != '\0');)
        if (*seq == base->ch)
        {
            if (base->child == NULL)
            {
                if (*(seq + 1) != '\0')
                    base->child = create_sequence (seq + 1, code, action);
                else
                {
                    // The sequence matches an existing one.
                    base->code = code;
                    base->action = action;
                }
                remember_key_sequence (code, original_seq);
                return TRUE;
            }

            base = base->child;
            seq++;
        }
        else
        {
            if (base->next != NULL)
                base = base->next;
            else
            {
                base->next = create_sequence (seq, code, action);
                remember_key_sequence (code, original_seq);
                return TRUE;
            }
        }

    if (*seq == '\0')
    {
        // Attempt to redefine a sequence with a shorter sequence.
        return FALSE;
    }

    keys = create_sequence (seq, code, action);
    remember_key_sequence (code, original_seq);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Check if we are idle, i.e. there are no pending keyboard or mouse
 * events.  Return 1 is idle, 0 is there are pending events.
 */
gboolean
is_idle (void)
{
    int nfd;
    fd_set select_set;
    struct timeval time_out;

    /* A read from the terminal empties the file descriptor into the screen
       library's buffer in one go, and mc decodes escape sequences into a queue
       of its own. Either can hold a paste worth of keys while select() below
       reports nothing left to read. */
    if (tty_paste_keys_pending || pending_keys != NULL || kitty_text_pending ()
        || tty_lowlevel_input_pending ())
        return FALSE;

    FD_ZERO (&select_set);
    FD_SET (input_fd, &select_set);
    nfd = MAX (0, input_fd) + 1;
    time_out.tv_sec = 0;
    time_out.tv_usec = 0;
#ifdef HAVE_LIBGPM
    if (mouse_enabled && use_mouse_p == MOUSE_GPM)
    {
        if (gpm_fd >= 0)
        {
            FD_SET (gpm_fd, &select_set);
            nfd = MAX (nfd, gpm_fd + 1);
        }
        else
        {
            if (mouse_fd >= 0)  // error indicative
            {
                if (FD_ISSET (mouse_fd, &select_set))
                    FD_CLR (mouse_fd, &select_set);
                mouse_fd = gpm_fd;
            }
            // gpm_fd == -2 means under some X terminal
            if (gpm_fd == -1)
            {
                mouse_enabled = FALSE;
                use_mouse_p = MOUSE_NONE;
            }
        }
    }
#endif
    return (select (nfd, &select_set, 0, 0, &time_out) <= 0);
}

/* --------------------------------------------------------------------------------------------- */

int
get_key_code (int no_delay)
{
    int c;
    gboolean from_sequence = FALSE;
    static key_def *this = NULL, *parent;
    static gint64 esc_time = -1;
    static int lastnodelay = -1;

    kitty_event_valid = FALSE;

    if (kitty_text_pos < kitty_text_len)
    {
        c = correct_key_code (kitty_text[kitty_text_pos++], FALSE);
        kitty_event_code = c;
        return c;
    }

    // the same key again, with its text, for a record with a repeat count
    if (key_repeat_left > 0)
    {
        key_repeat_left--;
        kitty_event.event = TTY_KITTY_REPEAT;
        kitty_event_valid = TRUE;
        c = correct_key_code (kitty_live_code (&kitty_event), FALSE);
        kitty_event_code = c;
        return c;
    }

    if (no_delay != lastnodelay)
    {
        this = NULL;
        lastnodelay = no_delay;
    }

pend_send:
    if (pending_keys != NULL)
    {
        gboolean bad_seq;

        c = *pending_keys++;
        while (c == ESC_CHAR)
            c = ALT (*pending_keys++);

        bad_seq = (*pending_keys != ESC_CHAR && *pending_keys != '\0');
        if (*pending_keys == '\0' || bad_seq)
            pending_keys = seq_append = NULL;

        if (bad_seq)
        {
            /* This is an unknown ESC sequence.
             * To prevent interpreting its tail as a random garbage,
             * eat and discard all buffered and quickly following chars.
             * Small, but non-zero timeout is needed to reconnect
             * escape sequence split up by e.g. a serial line.
             */
            int last = 0;
            const gboolean csi = seq_buffer[0] == ESC_CHAR && seq_buffer[1] == '[';
            /* A CSI answer of the terminal can be longer than 20 bytes: DA1 of xterm is 33. */
            int paranoia = csi ? 256 : 20;
            const int *p;

            for (p = seq_buffer; *p != '\0'; p++)
                last = *p;

            /* A CSI sequence ends on its final byte: what comes after it is not its tail. */
            if (!csi || last < 0x40 || last > 0x7e || p - seq_buffer <= 2)
            {
                int ch;

                while ((ch = getch_with_timeout (old_esc_mode_timeout)) >= 0 && --paranoia != 0)
                    if (csi && ch >= 0x40 && ch <= 0x7e)
                        break;
            }
        }
        else
            goto done;
    }

nodelay_try_again:
    if (no_delay != 0)
        tty_nodelay (TRUE);

    c = tty_lowlevel_getch ();
#if (defined(USE_NCURSES) || defined(USE_NCURSESW)) && defined(KEY_RESIZE)
    if (c == KEY_RESIZE)
        goto nodelay_try_again;
#endif

    if (no_delay != 0)
    {
        tty_nodelay (FALSE);
        if (c == -1)
        {
            if (this == NULL || parent == NULL || parent->action != MCKEY_ESCAPE || !old_esc_mode
                || esc_time == -1 || g_get_monotonic_time () < esc_time + old_esc_mode_timeout)
                return -1;

            this = NULL;
            pending_keys = seq_append = NULL;
            return ESC_CHAR;
        }
    }
    else if (c == -1)
    {
        /* Maybe we got an incomplete match.
           This we do only in delay mode, since otherwise
           tty_lowlevel_getch can return -1 at any time. */
        if (seq_append == NULL)
        {
            this = NULL;
            return -1;
        }

        pending_keys = seq_buffer;
        goto pend_send;
    }

    // Search the key on the root
    if (no_delay == 0 || this == NULL)
    {
        this = keys;
        parent = NULL;
    }

    while (this != NULL)
    {
        if (c == this->ch)
        {
            if (this->child == NULL || (this->code != 0 && this->action != MCKEY_ESCAPE))
            {
                if (this->child == NULL)
                {
                    // We got a complete match, return and reset search
                    pending_keys = seq_append = NULL;
                    c = this->code;
                    from_sequence = TRUE;
                    goto done;
                }

                /* Node has both code and children (prefix of longer seq).
                 * Peek ahead: if nothing follows quickly, return this code. */
                {
                    int next_c;

                    if (!push_char (c))
                    {
                        pending_keys = seq_buffer;
                        goto pend_send;
                    }

                    tty_nodelay (TRUE);
                    next_c = tty_lowlevel_getch ();
                    tty_nodelay (FALSE);

                    if (next_c == -1)
                    {
                        /* no more bytes -- return this node's code */
                        pending_keys = seq_append = NULL;
                        c = this->code;
                        from_sequence = TRUE;
                        goto done;
                    }

                    /* got another byte -- continue matching in children */
                    parent = this;
                    this = this->child;
                    c = next_c;
                    continue;
                }
            }

            // No match yet, but it may be a prefix for a valid seq
            if (!push_char (c))
            {
                pending_keys = seq_buffer;
                goto pend_send;
            }

            parent = this;
            this = this->child;
            if (parent->action == MCKEY_ESCAPE && old_esc_mode)
            {
                if (no_delay != 0)
                {
                    esc_time = g_get_monotonic_time ();
                    goto nodelay_try_again;
                }

                esc_time = -1;
                c = getch_with_timeout (old_esc_mode_timeout);
                if (c != -1)
                    continue;

                pending_keys = seq_append = NULL;
                this = NULL;
                return ESC_CHAR;
            }

            if (no_delay != 0)
                goto nodelay_try_again;
            c = tty_lowlevel_getch ();
            continue;
        }

        // c != this->ch. Try other keys with this prefix
        if (this->next != NULL)
        {
            this = this->next;
            continue;
        }

        // No match found. Is it one of our ESC <key> specials?
        if ((parent != NULL) && (parent->action == MCKEY_ESCAPE))
        {
            if (c == ']' && skip_osc_reply ())
            {
                pending_keys = seq_append = NULL;
                this = NULL;
                return -1;
            }

            // Convert escape-digits to F-keys
            if (g_ascii_isdigit (c))
                c = KEY_F (c - '0');
            else if (c == ' ')
                c = ESC_CHAR;
            else
                c = ALT (c);

            pending_keys = seq_append = NULL;
            goto done;
        }

        if (key_events_active () && kitty_csi_started (c))
        {
            c = kitty_read_csi (c);
            pending_keys = seq_append = NULL;
            if (c == -1 || c == KEY_KITTY_EVENT || c == MCKEY_MODIFIERS)
            {
                this = NULL;
                kitty_event_code = c;
                // a dropped release must not leave the next key waiting for the select timeout
                if (c == -1 && tty_lowlevel_input_pending ())
                {
                    // nor give its event to that key
                    kitty_event_valid = FALSE;
                    goto nodelay_try_again;
                }
                return c;
            }
            goto done;
        }

        // Unknown sequence. Maybe a prefix of a longer one. Save it.
        push_char (c);
        pending_keys = seq_buffer;
        goto pend_send;
    }  // while (this != NULL)

done:
    this = NULL;
    c = correct_key_code (c, from_sequence);
    kitty_event_code = c;
    return c;
}

/* --------------------------------------------------------------------------------------------- */
/* A line break becomes LF; ESC and the other control bytes but the tab are dropped, so nothing in
   the text can act as a key or end a bracketed paste. */

static void
tty_paste_clean (GString *text)
{
    size_t i, o = 0;

    for (i = 0; i < text->len; i++)
    {
        unsigned char c = (unsigned char) text->str[i];

        if (c == '\r')
        {
            if (i + 1 < text->len && text->str[i + 1] == '\n')
                continue;
            c = '\n';
        }
        else if (c != '\n' && c != '\t' && (c < 0x20 || c == 0x7f))
            continue;

        text->str[o++] = (char) c;
    }
    g_string_truncate (text, o);
}

/* --------------------------------------------------------------------------------------------- */
/* Read the paste up to ESC [ 2 0 1 ~. Silence or the overall time limit ends an
   unterminated paste, so the next key can be processed normally. */

static GString *
tty_paste_collect (void)
{
    static const char end_mark[] = ESC_STR "[201~";
    const size_t end_len = sizeof (end_mark) - 1;
    size_t end_matched = 0;
    GString *text = g_string_new (NULL);
    gint64 deadline = g_get_monotonic_time () + TTY_PASTE_GAP_USEC;
    const gint64 max_deadline = g_get_monotonic_time () + TTY_PASTE_MAX_USEC;
    gboolean ended = FALSE;

    tty_nodelay (TRUE);
    while (!ended)
    {
        int c;

        if (g_get_monotonic_time () >= max_deadline)
            break;

        c = tty_lowlevel_getch ();

        if (c == -1)
        {
            fd_set rs;
            struct timeval tv;

            if (g_get_monotonic_time () >= deadline)
                break;

            FD_ZERO (&rs);
            FD_SET (input_fd, &rs);
            tv.tv_sec = 0;
            tv.tv_usec = 20000;
            (void) select (input_fd + 1, &rs, NULL, NULL, &tv);
            continue;
        }

        // a key code of the screen library (ncurses: KEY_RESIZE, a translated sequence)
        if (c > 0xff)
            continue;

        deadline = g_get_monotonic_time () + TTY_PASTE_GAP_USEC;
        if (text->len < TTY_PASTE_MAX_BYTES)
            g_string_append_c (text, (char) c);
        if (c == end_mark[end_matched])
            end_matched++;
        else
            end_matched = (c == ESC_CHAR) ? 1 : 0;
        if (end_matched == end_len)
        {
            ended = TRUE;
            end_matched = 0;
            if (text->len >= end_len
                && memcmp (text->str + text->len - end_len, end_mark, end_len) == 0)
                g_string_truncate (text, text->len - end_len);
        }
    }
    tty_nodelay (FALSE);

    // Discard a partial end mark when the terminal stopped sending the paste.
    if (!ended && end_matched != 0 && text->len >= end_matched
        && memcmp (text->str + text->len - end_matched, end_mark, end_matched) == 0)
        g_string_truncate (text, text->len - end_matched);

    bracketed_pasting_in_progress = FALSE;

    tty_paste_clean (text);
    if (text->len == 0)
    {
        g_string_free (text, TRUE);
        return NULL;
    }
    return text;
}

/* --------------------------------------------------------------------------------------------- */

static int
tty_paste_next (void)
{
    if (tty_paste_block != NULL)
        g_string_free (tty_paste_block, TRUE);
    tty_paste_block = tty_paste_collect ();
    return (tty_paste_block != NULL) ? MCKEY_PASTE : EV_NONE;
}

/* --------------------------------------------------------------------------------------------- */
/* Returns a character read from stdin with appropriate interpretation */
/* Also takes care of generated mouse events */
/* Returns EV_MOUSE if it is a mouse event */
/* Returns EV_NONE  if non-blocking or interrupt set and nothing was done */

int
tty_get_event (struct Gpm_Event *event, gboolean redo_event, gboolean block)
{
    int c;
    int flag = 0;  // Return value from select
#ifdef HAVE_LIBGPM
    static struct Gpm_Event ev;  // Mouse event
#endif
    struct timeval time_out;
    struct timeval *time_addr = NULL;
    static gint64 last_refresh = 0;

    /* With nothing waiting, draw and be done. While input is still arriving,
       draw on a time budget instead: a paste hands us thousands of keys at
       once, and redrawing every few of them buries the terminal in escape
       sequences faster than it can consume them. mc then blocks in write()
       until the consumer catches up, which on a slow one is forever. */
    if (is_idle ())
    {
        mc_refresh ();
        last_refresh = 0;
    }
    else
    {
        const gint64 now = g_get_monotonic_time ();

        if (last_refresh == 0 || now - last_refresh >= MC_BURST_REFRESH_INTERVAL)
        {
            mc_refresh ();
            last_refresh = now;
        }
    }

    vfs_timeout_handler ();

    /* Ok, we use (event->x < 0) to signal that the event does not contain
       a suitable position for the mouse, so we can't use show_mouse_pointer
       on it.
     */
    if (event->x > 0)
    {
        show_mouse_pointer (event->x, event->y);
        if (!redo_event)
            event->x = -1;
    }

    // Repeat if using mouse
    while (pending_keys == NULL)
    {
        int nfd;
        fd_set select_set;

        FD_ZERO (&select_set);
        FD_SET (input_fd, &select_set);
        nfd = MAX (add_selects (&select_set), MAX (0, input_fd)) + 1;

#ifdef HAVE_LIBGPM
        if (mouse_enabled && (use_mouse_p == MOUSE_GPM))
        {
            if (gpm_fd >= 0)
            {
                FD_SET (gpm_fd, &select_set);
                nfd = MAX (nfd, gpm_fd + 1);
            }
            else
            {
                if (mouse_fd >= 0)  // error indicative
                {
                    if (FD_ISSET (mouse_fd, &select_set))
                        FD_CLR (mouse_fd, &select_set);
                    mouse_fd = gpm_fd;
                }
                // gpm_fd == -2 means under some X terminal
                if (gpm_fd == -1)
                {
                    mouse_enabled = FALSE;
                    use_mouse_p = MOUSE_NONE;
                }
                break;
            }
        }
#endif

        if (redo_event)
        {
            time_out.tv_usec = mou_auto_repeat * MC_USEC_PER_MSEC;
            time_out.tv_sec = 0;

            time_addr = &time_out;
        }
        else
        {
            int seconds;

            seconds = vfs_timeouts ();
            time_addr = NULL;

            if (seconds != 0)
            {
                /* the timeout could be improved and actually be
                 * the number of seconds until the next vfs entry
                 * timeouts in the stamp list.
                 */

                time_out.tv_sec = seconds;
                time_out.tv_usec = 0;
                time_addr = &time_out;
            }
        }

        if (!block || tty_got_winch ())
        {
            time_addr = &time_out;
            time_out.tv_sec = 0;
            time_out.tv_usec = 0;
        }

        // select() does not see input already buffered by the screen library
        if (kitty_text_pending () || tty_lowlevel_input_pending ())
            break;

        tty_enable_interrupt_key ();
        flag = select (nfd, &select_set, NULL, NULL, time_addr);
        tty_disable_interrupt_key ();

        /* select timed out: it could be for any of the following reasons:
         * redo_event -> it was because of the MOU_REPEAT handler
         * !block     -> we did not block in the select call
         * else       -> 10 second timeout to check the vfs status.
         */
        if (flag == 0)
        {
            if (redo_event)
                return EV_MOUSE;
            if (!block || tty_got_winch ())
                return EV_NONE;
            vfs_timeout_handler ();
        }
        if (flag == -1 && errno == EINTR)
            return EV_NONE;

        {
            gboolean ui_update = check_selects (&select_set);

            if (FD_ISSET (input_fd, &select_set))
                break; /* keyboard input takes priority */

            if (ui_update)
                return EV_NONE; /* no keyboard -- return so idle hooks can run */
        }

#ifdef HAVE_LIBGPM
        if (mouse_enabled && use_mouse_p == MOUSE_GPM)
        {
            if (gpm_fd >= 0)
            {
                if (FD_ISSET (gpm_fd, &select_set))
                {
                    int status;

                    status = Gpm_GetEvent (&ev);
                    if (status == 1)  // success
                    {
                        Gpm_FitEvent (&ev);
                        *event = ev;
                        return EV_MOUSE;
                    }
                    if (status <= 0)  // connection closed; -1 == error
                    {
                        if (mouse_fd >= 0 && FD_ISSET (mouse_fd, &select_set))
                            FD_CLR (mouse_fd, &select_set);

                        disable_mouse ();
                        return EV_NONE;
                    }
                }
            }
            else
            {
                if (mouse_fd >= 0)  // error indicative
                {
                    if (FD_ISSET (mouse_fd, &select_set))
                        FD_CLR (mouse_fd, &select_set);
                    mouse_fd = gpm_fd;
                }
                // gpm_fd == -2 means under some X terminal
                if (gpm_fd == -1)
                {
                    mouse_enabled = FALSE;
                    use_mouse_p = MOUSE_NONE;
                }
                break;
            }
        }
#endif
    }

#ifndef HAVE_SLANG
    flag = is_wintouched (stdscr);
    untouchwin (stdscr);
#endif
    c = block ? getch_with_delay () : get_key_code (1);

#ifndef HAVE_SLANG
    if (flag > 0)
        tty_touch_screen ();
#endif

    if (mouse_enabled
        && (c == MCKEY_MOUSE
#ifdef KEY_MOUSE
            || c == KEY_MOUSE
#endif
            || c == MCKEY_EXTENDED_MOUSE))
    {
        // Mouse event. See tickets 2956, 3954 and 4144 for extended mode detection.
        gboolean extended = c == MCKEY_EXTENDED_MOUSE;

#ifdef KEY_MOUSE
        extended = extended || (c == KEY_MOUSE && ncurses_key_mouse_means_extended);
#endif

        xmouse_get_event (event, extended);
        c = (event->type != 0) ? EV_MOUSE : EV_NONE;
    }
    else if (c == MCKEY_BRACKETED_PASTING_START && tty_paste_as_block)
        c = tty_paste_next ();
    else if (c == MCKEY_BRACKETED_PASTING_START)
    {
        bracketed_pasting_in_progress = TRUE;
        c = EV_NONE;
    }
    else if (c == MCKEY_BRACKETED_PASTING_END)
    {
        bracketed_pasting_in_progress = FALSE;
        c = EV_NONE;
    }

    return c;
}

/* --------------------------------------------------------------------------------------------- */
/* Returns a key press, mouse events are discarded */

int
tty_getch (void)
{
    Gpm_Event ev;
    int key;

    ev.x = -1;
    do
        key = tty_get_event (&ev, FALSE, TRUE);
    while (key == EV_NONE || key == MCKEY_MODIFIERS);
    return key;
}

/* --------------------------------------------------------------------------------------------- */

char *
learn_key (void)
{
    return learn_key_ex (NULL);
}

/* --------------------------------------------------------------------------------------------- */

char *
learn_key_ex (char **after)
{
    GString *raw;
    GString *buffer;
    size_t i, first;

    raw = g_string_sized_new (16);

    tty_keypad (FALSE);  // disable interpreting keys by ncurses
    while (TRUE)
    {
        gssize start;

        learn_read_burst (raw);
        // the release of the key that started the capture (Enter on a button) is no key
        kitty_drop_leading_releases (raw);
        if (raw->len == 0)
            continue;
        start = kitty_capture_start (raw);
        if (start >= 0)
        {
            g_string_erase (raw, 0, start);
            break;
        }
    }
    tty_keypad (TRUE);

    first = kitty_first_key_len (raw);
    buffer = g_string_sized_new (16);
    for (i = 0; i < first; i++)
        learn_store_key (buffer, (unsigned char) raw->str[i]);
    if (after != NULL)
    {
        GString *rest = g_string_sized_new (16);

        for (; i < raw->len; i++)
            learn_store_key (rest, (unsigned char) raw->str[i]);
        *after = g_string_free (rest, FALSE);
    }
    g_string_free (raw, TRUE);

    return g_string_free (buffer, buffer->len == 0);
}

/* --------------------------------------------------------------------------------------------- */

char *
tty_key_lookup_sequence (int code)
{
    GString *buf;
    const char *seq;

    if (keys == NULL || code <= 0)
        return NULL;

    seq = key_sequences != NULL
        ? g_hash_table_lookup (key_sequences, GINT_TO_POINTER (tty_normalize_keycode (code)))
        : NULL;
    if (seq != NULL)
        return key_sequence_to_printable (seq, strlen (seq));

    buf = g_string_sized_new (16);
    if (lookup_sequence_recursive (keys, code, buf))
    {
        /* convert raw bytes to printable escape notation */
        char *result;

        result = key_sequence_to_printable (buf->str, buf->len);
        g_string_free (buf, TRUE);
        return result;
    }

    g_string_free (buf, TRUE);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

int
tty_match_seq_to_keycode (const char *seq, int len)
{
    if (keys == NULL || seq == NULL || len <= 0)
        return 0;

    return match_seq_in_trie (keys, seq, len, 0);
}

/* --------------------------------------------------------------------------------------------- */
gboolean
tty_key_event (int key, tty_key_event_t *ev)
{
    if (!kitty_event_valid || key != kitty_event_code)
        return FALSE;
    if (ev != NULL)
        *ev = kitty_event;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

guint
tty_kitty_modifiers (void)
{
    guint mods = 0;
    int i;

    // Win32 input mode always reports the modifier keys
    if (!win32_input_active
        && (!kitty_keyboard_active
            || (kitty_flags_sent & KITTY_KEYBOARD_MODIFIERS) != KITTY_KEYBOARD_MODIFIERS))
        return 0;

    for (i = 0; i < KITTY_KEY_MOD_SIDES; i++)
        if ((kitty_mod_keys & ((1U << i) | (1U << (i + KITTY_KEY_MOD_SIDES)))) != 0)
            mods |= kitty_mod_kinds[i];
    return mods;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_key_event_take (int key, tty_key_event_t *ev)
{
    if (!tty_key_event (key, ev))
        return FALSE;
    kitty_text_len = kitty_text_pos = 0;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* Decode the bytes from learn_key () without reading or changing the live keyboard input.
   Returns 0 when they are not one whole key. */

int
tty_decode_key_seq (const char *seq, int len)
{
    int code;
    tty_key_event_t ev;

    if (seq == NULL || len <= 0 || memchr (seq, '\0', (size_t) len) != NULL)
        return 0;

    code = tty_match_seq_to_keycode (seq, len);
    if (code > 0)
        return correct_key_code (code, TRUE);

    if (len == 1)
        return correct_key_code ((unsigned char) seq[0], FALSE);

    if (len == 2 && seq[0] == ESC_CHAR)
    {
        const unsigned char c = (unsigned char) seq[1];

        if (g_ascii_isdigit (c))
            code = KEY_F (c - '0');
        else if (c == ' ')
            code = ESC_CHAR;
        else
            code = ALT (c);
        return correct_key_code (code, FALSE);
    }

    if (!tty_kitty_seq_event (seq, len, &ev))
        return 0;

    code = kitty_event_to_code (&ev);
    return code > 0 && code != KEY_KITTY_EVENT ? correct_key_code (code, FALSE) : 0;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_kitty_seq_event (const char *seq, int len, tty_key_event_t *ev)
{
    if (!key_events_active () || seq == NULL || len < 3 || seq[0] != ESC_CHAR || seq[1] != '[')
        return FALSE;

    return key_parse_csi (seq + 2, (size_t) len - 2, ev);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Build a key name string: "ctrl-alt-shift-<base>".
 * Returns newly allocated string. Caller must g_free().
 */
char *
tty_build_key_name (const char *base, int modifiers)
{
    GString *s;

    s = g_string_sized_new (32);
    if ((modifiers & KEY_M_CTRL) != 0)
        g_string_append (s, "ctrl-");
    if ((modifiers & KEY_M_ALT) != 0)
        g_string_append (s, "alt-");
    if ((modifiers & KEY_M_SHIFT) != 0)
        g_string_append (s, "shift-");
    g_string_append (s, base);
    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/* xterm and linux console only: set keypad to numeric or application
   mode. Only in application keypad mode it's possible to distinguish
   the '+' key and the '+' on the keypad ('*' and '-' ditto) */

void
numeric_keypad_mode (void)
{
    if (mc_global.tty.console_flag != '\0' || mc_global.tty.xterm_flag)
    {
        fputs (ESC_STR ">", stdout);
        fflush (stdout);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
application_keypad_mode (void)
{
    if (mc_global.tty.console_flag != '\0' || mc_global.tty.xterm_flag)
    {
        fputs (ESC_STR "=", stdout);
        fflush (stdout);
    }
}

/* --------------------------------------------------------------------------------------------- */
/* The text of the last MCKEY_PASTE; the caller frees it. */

GString *
tty_paste_take (void)
{
    GString *text = tty_paste_block;

    tty_paste_block = NULL;
    return text;
}

/* --------------------------------------------------------------------------------------------- */

void
enable_bracketed_paste (void)
{
    printf (ESC_STR "[?2004h");
    fflush (stdout);
}

/* --------------------------------------------------------------------------------------------- */

void
disable_bracketed_paste (void)
{
    printf (ESC_STR "[?2004l");
    fflush (stdout);
    bracketed_pasting_in_progress = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
enable_kitty_keyboard (void)
{
    if (kitty_keyboard_active || win32_input_active)
        return;

    /* Windows Terminal knows both. Its kitty protocol (1.25) sends the text of a key that is not
       in the US layout once more on the release, so the Win32 input mode goes first. */
    if (tty_has_win32_input ())
    {
        printf (ESC_STR "[?9001h");
        if (keybar_modifiers)
            printf (ESC_STR "[?1004h");
        fflush (stdout);
        win32_input_active = TRUE;
        win32_high_surrogate = 0;
        key_repeat_left = 0;
        kitty_mod_keys = 0;
        return;
    }

    if (!tty_has_kitty_keyboard ())
        return;

    kitty_flags_sent = kitty_flags_base () | kitty_flags_wanted;
    printf (ESC_STR "[>%uu", kitty_flags_sent);
    if (keybar_modifiers)
        printf (ESC_STR "[?1004h");
    fflush (stdout);
    kitty_keyboard_active = TRUE;
    kitty_mod_keys = 0;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_kitty_keyboard_want (guint flags)
{
    kitty_flags_wanted = flags & KITTY_KEYBOARD_EXTRA;
    if (!kitty_keyboard_active || (kitty_flags_base () | kitty_flags_wanted) == kitty_flags_sent)
        return;

    kitty_flags_sent = kitty_flags_base () | kitty_flags_wanted;
    printf (ESC_STR "[=%u;1u", kitty_flags_sent);
    fflush (stdout);
}

/* --------------------------------------------------------------------------------------------- */

void
disable_kitty_keyboard (void)
{
    if (win32_input_active)
    {
        if (keybar_modifiers)
            printf (ESC_STR "[?1004l");
        printf (ESC_STR "[?9001l");
        fflush (stdout);
        win32_input_active = FALSE;
        key_repeat_left = 0;
        kitty_mod_keys = 0;
        return;
    }

    if (!kitty_keyboard_active)
        return;

    if (keybar_modifiers)
        printf (ESC_STR "[?1004l");
    printf (ESC_STR "[<u");
    fflush (stdout);
    kitty_keyboard_active = FALSE;
    kitty_mod_keys = 0;
}

/* --------------------------------------------------------------------------------------------- */
