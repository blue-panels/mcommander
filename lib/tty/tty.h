
/** \file tty.h
 *  \brief Header: %interface to the terminal controlling library
 *
 *  This file is the %interface to the terminal controlling library:
 *  slang or ncurses. It provides an additional layer of abstraction
 *  above the "real" libraries to keep the number of ifdefs in the other
 *  files small.
 */

#ifndef MC__TTY_H
#define MC__TTY_H

#include "lib/global.h"  // include <glib.h>

#ifdef HAVE_SLANG
#include "tty-slang.h"
#else
#include "tty-ncurses.h"
#endif

/*** typedefs(not structures) and defined constants **********************************************/

#define KEY_KP_ADD      4001
#define KEY_KP_SUBTRACT 4002
#define KEY_KP_MULTIPLY 4003

// In UTF-8 locales it's always the gunichar.
// In 8-bit locales it's either one of the MC_ACS_* special values for single frame characters,
// or the 8-bit character code.
typedef unsigned int mc_tty_char_t;

/*** enums ***************************************************************************************/

// These values always represent the given line drawing characters, in a common way regardless of
// the underlying screen library. Only used in 8-bit mode, in UTF-8 the actual codepoint is used.
// These values really have the type 'mc_tty_char_t', but enum cannot denote it.
enum
{
    // Even though these are only used in 8-bit mode, let's start the numbers above the highest
    // Unicode value to avoid any chance of confusion.
    MC_ACS_HLINE = 0x110000,  // U+2500 light horizontal
    MC_ACS_VLINE,             // U+2502 light vertical
    MC_ACS_ULCORNER,          // U+250C light down and right
    MC_ACS_URCORNER,          // U+2510 light down and left
    MC_ACS_LLCORNER,          // U+2514 light up and right
    MC_ACS_LRCORNER,          // U+2518 light up and left
    MC_ACS_TTEE,              // U+252C light down and horizontal
    MC_ACS_BTEE,              // U+2534 light up and horizontal
    MC_ACS_LTEE,              // U+251C light vertical and right
    MC_ACS_RTEE,              // U+2524 light vertical and left
    MC_ACS_PLUS,              // U+253C light vertical and horizontal
};

// These refer to the roles, the given positions of more prominent and less prominent boxes.
// The actual characters, taken from the skin, may or may not match the role name.
typedef enum
{
    // single lines
    MC_TTY_FRM_HORIZ,
    MC_TTY_FRM_VERT,
    MC_TTY_FRM_LEFTTOP,
    MC_TTY_FRM_RIGHTTOP,
    MC_TTY_FRM_LEFTBOTTOM,
    MC_TTY_FRM_RIGHTBOTTOM,
    MC_TTY_FRM_TOPMIDDLE,
    MC_TTY_FRM_BOTTOMMIDDLE,
    MC_TTY_FRM_LEFTMIDDLE,
    MC_TTY_FRM_RIGHTMIDDLE,
    MC_TTY_FRM_CROSS,

    // double lines
    MC_TTY_FRM_DHORIZ,
    MC_TTY_FRM_DVERT,
    MC_TTY_FRM_DLEFTTOP,
    MC_TTY_FRM_DRIGHTTOP,
    MC_TTY_FRM_DLEFTBOTTOM,
    MC_TTY_FRM_DRIGHTBOTTOM,
    MC_TTY_FRM_DTOPMIDDLE,
    MC_TTY_FRM_DBOTTOMMIDDLE,
    MC_TTY_FRM_DLEFTMIDDLE,
    MC_TTY_FRM_DRIGHTMIDDLE,

    MC_TTY_FRM_MAX
} mc_tty_frm_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

// The actual characters used for frame drawing, as taken from the skin. Indexed by MC_TTY_FRM_*,
// the values are either one of MCS_ACS_* or a character in the current locale.
extern mc_tty_char_t mc_tty_frm[];

extern int tty_tigetflag (const char *terminfo_cap, const char *termcap_cap);
extern int tty_tigetnum (const char *terminfo_cap, const char *termcap_cap);
extern char *tty_tigetstr (const char *terminfo_cap, const char *termcap_cap);

/*** declarations of public functions ************************************************************/

gboolean tty_background_rgb (int *r, int *g, int *b);

extern void tty_beep (void);

/* {{{ Input }}} */

extern gboolean tty_check_xterm_compat (gboolean force_xterm);
extern void tty_init (gboolean mouse_enable, gboolean is_xterm);
extern void tty_shutdown (void);

extern void tty_start_interrupt_key (void);
extern void tty_enable_interrupt_key (void);
extern void tty_disable_interrupt_key (void);
extern gboolean tty_got_interrupt (void);

extern gboolean tty_got_winch (void);
extern gboolean tty_flush_winch (void);

extern void tty_reset_prog_mode (void);
extern void tty_reset_shell_mode (void);

extern void tty_raw_mode (void);
extern void tty_noraw_mode (void);

extern void tty_noecho (void);
extern int tty_flush_input (void);

extern void tty_keypad (gboolean set);
extern void tty_nodelay (gboolean set);
extern int tty_baudrate (void);

/* {{{ Output }}} */

/*
   The output functions do not check themselves for screen overflows,
   so make sure that you never write more than what fits on the screen.
   While SLang provides such a feature, ncurses does not.
 */

extern int tty_reset_screen (void);
extern void tty_touch_screen (void);

extern void tty_gotoyx (int y, int x);
extern void tty_getyx (int *py, int *px);

extern void tty_display_8bit (gboolean what);
extern void tty_print_char (mc_tty_char_t c);
extern void tty_print_anychar (mc_tty_char_t c);
extern void tty_print_string (const char *s);
extern void tty_printf (const char *s, ...) G_GNUC_PRINTF (1, 2);

extern void tty_print_one_vline (gboolean single);
extern void tty_print_one_hline (gboolean single);
extern void tty_draw_hline (int y, int x, mc_tty_char_t ch, int len);
extern void tty_draw_vline (int y, int x, mc_tty_char_t ch, int len);
extern void tty_draw_box (int y, int x, int rows, int cols, gboolean single);
extern void tty_draw_box_shadow (int y, int x, int rows, int cols, int shadow_color);
extern void tty_fill_region (int y, int x, int rows, int cols, unsigned char ch);

extern int tty_resize (int fd);
extern void tty_refresh (void);
extern void tty_change_screen_size (void);

/* Output the screen library has no cell for - sixel pictures - goes to the
   terminal behind its back. Only after tty_refresh(), when the library's own
   output is out; the cursor comes back to where the library left it. */
extern void tty_raw_write (const char *data, size_t len);
/* Make the library write these rows again on the next refresh, changed or
   not: a picture drawn over them has to be erased or drawn again. */
extern void tty_touch_area (int y, int x, int rows, int cols);
/* Painters run after every tty_refresh(), in the order they were added. */
typedef void (*tty_painter_fn) (void *data);
extern void tty_painter_add (tty_painter_fn fn, void *data);
extern void tty_painter_remove (tty_painter_fn fn, void *data);
/* Ask the terminal whether it draws sixel and how big its cells are:
   Primary Device Attributes and XTWINOPS 16, and whether it knows the kitty
   keyboard protocol. Once, after tty_init(), while
   nothing else reads the keyboard; a terminal that does not answer within a
   moment is taken not to know. MC_SIXEL=0 or 1 overrides the answer. */
extern void tty_probe_graphics (void);
/* Bytes read past the keyboard's back go back to it, in order. */
extern void tty_unget_input (const unsigned char *data, size_t len);
extern gboolean tty_has_sixel (void);
/* The terminal answered CSI ? u. MC_KITTY_KEYBOARD=0 turns the question off. */
extern gboolean tty_has_kitty_keyboard (void);
/* Pixels per cell, 0 when the terminal did not say. */
extern void tty_cell_size (int *width, int *height);

/* Clear screen */
extern void tty_clear_screen (void);

extern void tty_enter_ca_mode (void);
extern void tty_exit_ca_mode (void);

/*** inline functions ****************************************************************************/

#endif
