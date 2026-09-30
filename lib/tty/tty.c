/*
   Interface to the terminal controlling library.

   Copyright (C) 2005-2026
   Free Software Foundation, Inc.
   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

   Written by:
   Roland Illig <roland.illig@gmx.de>, 2005.
   Andrew Borodin <aborodin@vmail.ru>, 2009.
   Ilia Maslakov <il.smind@gmail.com>, 2011, 2026.

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

/** \file tty.c
 *  \brief Source: %interface to the terminal controlling library
 */

#include <config.h>

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>  // memset()

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#endif
#include <unistd.h>  // exit()

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

/* In some systems (like Solaris 11.4 SPARC), TIOCSWINSZ is defined in termios.h */
#include <termios.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/util.h"

#include "tty.h"
#include "tty-internal.h"
#include "color.h"  // tty_set_normal_attrs()
#include "mouse.h"  // use_mouse_p
#include "win.h"

/*** global variables ****************************************************************************/

mc_tty_char_t mc_tty_frm[MC_TTY_FRM_MAX];

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static SIG_ATOMIC_VOLATILE_T got_interrupt = 0;

typedef struct
{
    tty_painter_fn fn;
    void *data;
} tty_painter_t;

static GSList *painters = NULL;
static gboolean painting = FALSE;

/* The terminal's own background, as OSC 11 reported it; -1 while unknown. */
static int background_rgb = -1;

static gboolean has_sixel = FALSE;
static gboolean has_kitty_keyboard = FALSE;
static gboolean has_win32_input = FALSE;
static gboolean has_far2l_input = FALSE;
static int cell_width = 0;
static int cell_height = 0;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
sigintr_handler (int signo)
{
    (void) &signo;
    got_interrupt = 1;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/**
 * The terminal's own background color, as it answered at startup.
 *
 * Worth asking where a skin leaves the background to the terminal: mc then knows
 * nothing about the ground its text sits on, and cannot pick a color to sit near
 * it.  Not every terminal answers, hence the return value.
 */

gboolean
tty_background_rgb (int *r, int *g, int *b)
{
    if (background_rgb < 0)
        return FALSE;

    *r = (background_rgb >> 16) & 0xFF;
    *g = (background_rgb >> 8) & 0xFF;
    *b = background_rgb & 0xFF;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_run_painters (void)
{
    GSList *l;

    /* A painter that refreshes would run the painters again. */
    if (painting)
        return;

    painting = TRUE;
    for (l = painters; l != NULL; l = g_slist_next (l))
    {
        const tty_painter_t *p = l->data;

        p->fn (p->data);
    }
    painting = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_painter_add (tty_painter_fn fn, void *data)
{
    tty_painter_t *p = g_new (tty_painter_t, 1);

    p->fn = fn;
    p->data = data;
    painters = g_slist_append (painters, p);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_painter_remove (tty_painter_fn fn, void *data)
{
    GSList *l;

    for (l = painters; l != NULL; l = g_slist_next (l))
    {
        tty_painter_t *p = l->data;

        if (p->fn == fn && p->data == data)
        {
            painters = g_slist_delete_link (painters, l);
            g_free (p);
            return;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

void
tty_raw_write (const char *data, size_t len)
{
    while (len > 0)
    {
        ssize_t n = write (STDOUT_FILENO, data, len);

        if (n < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return;
        }
        data += n;
        len -= (size_t) n;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The answers: CSI ? <attributes> c, where 4 among the attributes is sixel,
   and CSI 6 ; <height> ; <width> t. The sequences are taken out of the
   buffer as they are found, and what is left is the user's. */
static void
tty_parse_graphics_reply (char *buf, size_t *len, gboolean *sixel_seen)
{
    size_t i = 0;

    while (i + 2 < *len)
    {
        const char *p = buf + i + 2;
        const char *end = buf + *len;
        size_t taken = 0;

        if (buf[i] == ESC_CHAR && buf[i + 1] == ']')
        {
            /* OSC 11 ; rgb:RRRR/GGGG/BBBB, ended by BEL or ST */
            const char *q = p;
            int comp[3] = { 0, 0, 0 };
            int n = 0;

            if (strncmp (q, "11;rgb:", 7) == 0)
            {
                q += 7;
                for (n = 0; n < 3 && q < end; n++)
                {
                    int v = 0, digits = 0;

                    while (q < end && g_ascii_isxdigit (*q) && digits < 4)
                    {
                        v = v * 16 + g_ascii_xdigit_value (*q);
                        q++;
                        digits++;
                    }
                    if (digits == 0)
                        break;
                    // components may be 1 to 4 hex digits wide; bring them to 8 bits
                    while (digits < 4)
                    {
                        v = v * 16 + (v & 0xF);
                        digits++;
                    }
                    comp[n] = v >> 8;
                    if (n < 2)
                    {
                        if (q >= end || *q != '/')
                            break;
                        q++;
                    }
                }
            }

            /* Find the terminator so the reply is not left for the keyboard. The tty can
               take a BEL out (see the query), so an ESC that starts no ST ends it too. */
            while (q < end && *q != '\a' && *q != ESC_CHAR)
                q++;
            if (q >= end || (*q == ESC_CHAR && q + 1 >= end))
                break;  // still arriving

            if (n == 3)
                background_rgb = (comp[0] << 16) | (comp[1] << 8) | comp[2];
            if (*q == '\a')
                q++;
            else if (q[1] == '\\')
                q += 2;
            taken = (size_t) (q - (buf + i));
            memmove (buf + i, buf + i + taken, *len - i - taken);
            *len -= taken;
            continue;
        }

        if (buf[i] == ESC_CHAR && buf[i + 1] == '_' && end - p >= 7
            && strncmp (p, "far2lok", 7) == 0)
        {
            /* APC far2lok, ended by BEL or ST: the terminal speaks the far2l extensions */
            if (p + 7 < end && p[7] == '\a')
                taken = 10;
            else if (p + 8 < end && p[7] == ESC_CHAR && p[8] == '\\')
                taken = 11;

            if (taken != 0)
            {
                has_far2l_input = TRUE;
                memmove (buf + i, buf + i + taken, *len - i - taken);
                *len -= taken;
                continue;
            }
        }

        if (buf[i] != ESC_CHAR || buf[i + 1] != '[')
        {
            i++;
            continue;
        }

        if (*p == '?')
        {
            int n = 0, first = 0;
            gboolean have = FALSE, sixel = FALSE, have_first = FALSE;

            for (p++; p < end; p++)
            {
                if (*p >= '0' && *p <= '9')
                {
                    n = n * 10 + (*p - '0');
                    have = TRUE;
                }
                else if (*p == '$' && have_first && first == 9001 && have && p + 1 < end
                         && p[1] == 'y')
                {
                    /* CSI ? 9001 ; <state> $ y: Win32 input mode is known (1 set, 2 reset) */
                    if (n == 1 || n == 2)
                        has_win32_input = TRUE;
                    taken = (size_t) (p + 2 - (buf + i));
                    break;
                }
                else if (*p == 'u' && have)
                {
                    /* CSI ? <flags> u: the kitty keyboard protocol is known */
                    has_kitty_keyboard = TRUE;
                    taken = (size_t) (p + 1 - (buf + i));
                    break;
                }
                else if (*p == ';' || *p == 'c')
                {
                    if (have && !have_first)
                    {
                        first = n;
                        have_first = TRUE;
                    }
                    if (have && n == 4)
                        sixel = TRUE;
                    n = 0;
                    have = FALSE;
                    if (*p == 'c')
                    {
                        has_sixel = sixel;
                        *sixel_seen = TRUE;
                        taken = (size_t) (p + 1 - (buf + i));
                        break;
                    }
                }
                else
                    break;
            }
        }
        else if (p[0] == '6' && p[1] == ';')
        {
            int h = 0, w = 0;

            for (p += 2; p < end && *p >= '0' && *p <= '9'; p++)
                h = h * 10 + (*p - '0');
            if (p < end && *p == ';')
                for (p++; p < end && *p >= '0' && *p <= '9'; p++)
                    w = w * 10 + (*p - '0');
            if (p < end && *p == 't')
            {
                if (h > 0 && w > 0)
                {
                    cell_width = w;
                    cell_height = h;
                }
                taken = (size_t) (p + 1 - (buf + i));
            }
        }

        if (taken == 0)
        {
            i++;
            continue;
        }
        memmove (buf + i, buf + i + taken, *len - i - taken);
        *len -= taken;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
tty_probe_graphics (void)
{
    const char *env = getenv ("MC_SIXEL");
    const char *kitty_env = getenv ("MC_KITTY_KEYBOARD");
    const char *term = getenv ("TERM");
    char buf[512];
    size_t len = 0;
    gboolean sixel_seen = FALSE;
    gboolean forced_off = env != NULL && env[0] == '0';
    gboolean forced_on = env != NULL && env[0] == '1';
    gboolean multiplexer =
        term != NULL && (strncmp (term, "screen", 6) == 0 || strncmp (term, "tmux", 4) == 0);
    gboolean no_sixel = forced_off || (!forced_on && multiplexer);
    gboolean ask_kitty = !multiplexer && (kitty_env == NULL || kitty_env[0] != '0');
    const char *win32_env = getenv ("MC_WIN32_INPUT");
    gboolean ask_win32 = !multiplexer && (win32_env == NULL || win32_env[0] != '0');
    const char *far2l_env = getenv ("MC_FAR2L");
    gboolean ask_far2l = !multiplexer && (far2l_env == NULL || far2l_env[0] != '0');
    int waited_ms = 0;

    has_sixel = FALSE;
    has_kitty_keyboard = FALSE;
    has_win32_input = FALSE;
    has_far2l_input = FALSE;
    cell_width = 0;
    cell_height = 0;

#ifdef TIOCGWINSZ
    {
        struct winsize ws;

        memset (&ws, 0, sizeof (ws));
        if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0
            && ws.ws_xpixel > 0 && ws.ws_ypixel > 0)
        {
            cell_width = ws.ws_xpixel / ws.ws_col;
            cell_height = ws.ws_ypixel / ws.ws_row;
        }
    }
#endif

    /* A multiplexer answers for itself and keeps the DCS: no sixel through it
       unless the user says so. */
    if (no_sixel && !ask_kitty && !ask_win32 && !ask_far2l)
        return;

    if (isatty (STDIN_FILENO) && isatty (STDOUT_FILENO))
    {
        /* Every terminal answers DA1, and answers in the order it is asked: DA1 goes
           last, so its answer comes after all the others, the ones about the kitty keyboard
           protocol, the Win32 input mode (DECRQM 9001) and the far2l extensions too. OSC 11
           ends with ST, not BEL: the answer ends the same way, and with S-Lang BEL (Ctrl-G)
           is the interrupt character, which the tty takes out of the input. */
        static const char kitty_query[] = ESC_STR "[?u";
        static const char win32_query[] = ESC_STR "[?9001$p";
        /* APC far2l1 turns the far2l extensions on; a terminal that has them answers
           APC far2lok, and every other one ignores the sequence */
        static const char far2l_query[] = ESC_STR "_far2l1" ESC_STR "\\";
        static const char query[] = ESC_STR "[16t" ESC_STR "]11;?" ESC_STR "\\" ESC_STR "[c";

        if (ask_kitty)
            tty_raw_write (kitty_query, sizeof (kitty_query) - 1);
        if (ask_win32)
            tty_raw_write (win32_query, sizeof (win32_query) - 1);
        if (ask_far2l)
            tty_raw_write (far2l_query, sizeof (far2l_query) - 1);
        tty_raw_write (query, sizeof (query) - 1);

        /* A terminal that answers nothing costs the whole wait once, at startup. */
        while (len < sizeof (buf) - 1 && !sixel_seen && waited_ms < 1000)
        {
            fd_set fds;
            struct timeval tv = { 0, 50000 };
            ssize_t n;

            FD_ZERO (&fds);
            FD_SET (STDIN_FILENO, &fds);
            if (select (STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0)
            {
                waited_ms += 50;
                continue;
            }
            n = read (STDIN_FILENO, buf + len, sizeof (buf) - 1 - len);
            if (n <= 0)
                break;
            len += (size_t) n;
            tty_parse_graphics_reply (buf, &len, &sixel_seen);
        }

        /* Whatever else came in was typed: it goes back to the keyboard. */
        if (len > 0)
            tty_unget_input ((const unsigned char *) buf, len);
    }

    if (no_sixel)
        has_sixel = FALSE;
    if (forced_on)
        has_sixel = TRUE;

    /* Sixel with no word on the cell: take the usual 8x16, so that the
       pictures are placed and sized at all, rather than not drawn. */
    if (has_sixel && (cell_width <= 0 || cell_height <= 0))
    {
        cell_width = 8;
        cell_height = 16;
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_has_sixel (void)
{
    return has_sixel;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_has_kitty_keyboard (void)
{
    return has_kitty_keyboard;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_cell_size (int *width, int *height)
{
    *width = cell_width;
    *height = cell_height;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_has_win32_input (void)
{
    return has_win32_input;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_has_far2l_input (void)
{
    return has_far2l_input;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Check terminal type. If $TERM is not set or value is empty, mc finishes with EXIT_FAILURE.
 *
 * @param force_xterm Set forced the XTerm type
 *
 * @return true if @param force_xterm is true or value of $TERM is one of following:
 *         alacritty*
 *         contour*
 *         dtterm
 *         Eterm
 *         foot*
 *         konsole*
 *         rxvt*
 *         screen*
 *         term*
 *         tmux*
 */
gboolean
tty_check_xterm_compat (const gboolean force_xterm)
{
    static const char *xterm_compatible_terminals[] = {
        "alacritty",  //
        "contour",    //
        "dtterm",     //
        "Eterm",      //
        "foot",       //
        "konsole",    //
        "rxvt",       //
        "screen",     //
        "tmux",       //
        "xterm",      //
        NULL,
    };

    const char *termvalue = getenv ("TERM");
    if (termvalue == NULL || *termvalue == '\0')
    {
        fputs (_ ("The TERM environment variable is unset!\n"), stderr);
        my_exit (EXIT_FAILURE);
    }

    if (force_xterm)
        return TRUE;

    for (const char **p = xterm_compatible_terminals; *p != NULL; p++)
        if (strncmp (termvalue, *p, strlen (*p)) == 0)
            return TRUE;

    return FALSE;
}
/* --------------------------------------------------------------------------------------------- */

extern void
tty_start_interrupt_key (void)
{
    struct sigaction act;

    memset (&act, 0, sizeof (act));
    act.sa_handler = sigintr_handler;
    sigemptyset (&act.sa_mask);
#ifdef SA_RESTART
    act.sa_flags = SA_RESTART;
#endif
    my_sigaction (SIGINT, &act, NULL);
}

/* --------------------------------------------------------------------------------------------- */

extern void
tty_enable_interrupt_key (void)
{
    struct sigaction act;

    memset (&act, 0, sizeof (act));
    act.sa_handler = sigintr_handler;
    sigemptyset (&act.sa_mask);
    my_sigaction (SIGINT, &act, NULL);
    got_interrupt = 0;
}

/* --------------------------------------------------------------------------------------------- */

extern void
tty_disable_interrupt_key (void)
{
    struct sigaction act;

    memset (&act, 0, sizeof (act));
    act.sa_handler = SIG_IGN;
    sigemptyset (&act.sa_mask);
    my_sigaction (SIGINT, &act, NULL);
}

/* --------------------------------------------------------------------------------------------- */

extern gboolean
tty_got_interrupt (void)
{
    gboolean rv;

    rv = (got_interrupt != 0);
    got_interrupt = 0;
    return rv;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_got_winch (void)
{
    fd_set fdset;
    // instant timeout
    struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };
    int ok;

    FD_ZERO (&fdset);
    FD_SET (sigwinch_pipe[0], &fdset);

    while ((ok = select (sigwinch_pipe[0] + 1, &fdset, NULL, NULL, &timeout)) < 0)
        if (errno != EINTR)
        {
            perror (_ ("Cannot check SIGWINCH pipe"));
            exit (EXIT_FAILURE);
        }

    return (ok != 0 && FD_ISSET (sigwinch_pipe[0], &fdset));
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_flush_winch (void)
{
    ssize_t n;
    gboolean ret = FALSE;

    // merge all SIGWINCH events raised to this moment
    do
    {
        char x[16];

        // read multiple events at a time
        n = read (sigwinch_pipe[0], &x, sizeof (x));

        // at least one SIGWINCH came
        if (n > 0)
            ret = TRUE;
    }
    while (n > 0 || (n == -1 && errno == EINTR));

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_print_one_hline (gboolean single)
{
    tty_print_char (mc_tty_frm[single ? MC_TTY_FRM_HORIZ : MC_TTY_FRM_DHORIZ]);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_print_one_vline (gboolean single)
{
    tty_print_char (mc_tty_frm[single ? MC_TTY_FRM_VERT : MC_TTY_FRM_DVERT]);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_draw_box (int y, int x, int ys, int xs, gboolean single)
{
    int y2, x2;

    if (ys <= 0 || xs <= 0)
        return;

    ys--;
    xs--;

    y2 = y + ys;
    x2 = x + xs;

    tty_draw_vline (y, x, mc_tty_frm[single ? MC_TTY_FRM_VERT : MC_TTY_FRM_DVERT], ys);
    tty_draw_vline (y, x2, mc_tty_frm[single ? MC_TTY_FRM_VERT : MC_TTY_FRM_DVERT], ys);
    tty_draw_hline (y, x, mc_tty_frm[single ? MC_TTY_FRM_HORIZ : MC_TTY_FRM_DHORIZ], xs);
    tty_draw_hline (y2, x, mc_tty_frm[single ? MC_TTY_FRM_HORIZ : MC_TTY_FRM_DHORIZ], xs);
    tty_gotoyx (y, x);
    tty_print_char (mc_tty_frm[single ? MC_TTY_FRM_LEFTTOP : MC_TTY_FRM_DLEFTTOP]);
    tty_gotoyx (y2, x);
    tty_print_char (mc_tty_frm[single ? MC_TTY_FRM_LEFTBOTTOM : MC_TTY_FRM_DLEFTBOTTOM]);
    tty_gotoyx (y, x2);
    tty_print_char (mc_tty_frm[single ? MC_TTY_FRM_RIGHTTOP : MC_TTY_FRM_DRIGHTTOP]);
    tty_gotoyx (y2, x2);
    tty_print_char (mc_tty_frm[single ? MC_TTY_FRM_RIGHTBOTTOM : MC_TTY_FRM_DRIGHTBOTTOM]);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_draw_box_shadow (int y, int x, int rows, int cols, int shadow_color)
{
    // draw right shadow
    tty_colorize_area (y + 1, x + cols, rows - 1, 2, shadow_color);
    // draw bottom shadow
    tty_colorize_area (y + rows, x + 2, 1, cols, shadow_color);
}

/* --------------------------------------------------------------------------------------------- */

char *
mc_tty_normalize_from_utf8 (const char *str)
{
    GIConv conv;
    GString *buffer;
    const char *_system_codepage = str_detect_termencoding ();

    if (str_isutf8 (_system_codepage))
        return g_strdup (str);

    conv = g_iconv_open (_system_codepage, "UTF-8");
    if (conv == INVALID_CONV)
        return g_strdup (str);

    buffer = g_string_new ("");

    if (str_convert (conv, str, buffer) == ESTR_FAILURE)
    {
        g_string_free (buffer, TRUE);
        str_close_conv (conv);
        return g_strdup (str);
    }
    str_close_conv (conv);

    return g_string_free (buffer, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/** Resize given terminal using TIOCSWINSZ, return ioctl() result */
int
tty_resize (int fd)
{
#if defined TIOCSWINSZ
    struct winsize tty_size;

    /* Make sure to copy to the inner terminal all the fields, even the ones we don't care about,
     * including ws_xpixel and ws_ypixel. */
    if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &tty_size) == -1)
        return -1;
    return ioctl (fd, TIOCSWINSZ, &tty_size);
#else
    return 0;
#endif
}

/* --------------------------------------------------------------------------------------------- */

/** Clear screen */
void
tty_clear_screen (void)
{
    tty_set_normal_attrs ();
    tty_fill_region (0, 0, LINES, COLS, ' ');
    tty_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

void
tty_init_xterm_support (gboolean is_xterm)
{
    const char *termvalue;

    termvalue = getenv ("TERM");

    // Check mouse and ca capabilities
    /* terminfo/termcap structures have been already initialized,
       in slang_init() or/and init_curses()  */
    xmouse_seq = tty_tigetstr ("kmous", "Km");
    smcup = tty_tigetstr ("smcup", "ti");
    rmcup = tty_tigetstr ("rmcup", "te");

    if (strcmp (termvalue, "cygwin") == 0)
    {
        is_xterm = TRUE;
        use_mouse_p = MOUSE_DISABLED;
    }

    if (is_xterm)
    {
        // Default to the standard xterm sequence
        if (xmouse_seq == NULL)
            xmouse_seq = ESC_STR "[M";

        // Enable mouse unless explicitly disabled by --nomouse
        if (use_mouse_p != MOUSE_DISABLED)
        {
            if (mc_global.tty.old_mouse)
                use_mouse_p = MOUSE_XTERM_NORMAL_TRACKING;
            else
            {
                // FIXME: this dirty hack to set supported type of tracking the mouse
                const char *color_term = getenv ("COLORTERM");
                if (strncmp (termvalue, "rxvt", 4) == 0
                    || (color_term != NULL && strncmp (color_term, "rxvt", 4) == 0)
                    || strcmp (termvalue, "Eterm") == 0)
                    use_mouse_p = MOUSE_XTERM_NORMAL_TRACKING;
                else
                    use_mouse_p = MOUSE_XTERM_BUTTON_EVENT_TRACKING;
            }
        }
    }

    /* There's only one termcap entry "kmous", typically containing "\E[M" or "\E[<".
     * We need the former in xmouse_seq, the latter in xmouse_extended_seq.
     * See tickets 2956, 3954, and 4063 for details. */
    if (xmouse_seq != NULL)
    {
        if (strcmp (xmouse_seq, ESC_STR "[<") == 0)
        {
            xmouse_seq = ESC_STR "[M";
            ncurses_key_mouse_means_extended = TRUE;
        }

        xmouse_extended_seq = ESC_STR "[<";
    }
}

/* --------------------------------------------------------------------------------------------- */
