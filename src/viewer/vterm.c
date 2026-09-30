/*
   Internal file viewer for the Midnight Commander
   VT100 terminal sequence parser for ANSI terminal replay mode.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <string.h>

#include "lib/global.h"

#include "ansi.h"
#include "terminal_buffer.h"
#include "vterm.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define VTERM_ESC_CHAR               0x1Bu

#define MCVIEW_VTERM_MAX_CANVAS_ROWS 2000

/* Ceilings on the history: rows, and cells so long lines cannot eat memory. */
#define MCVIEW_VTERM_MAX_HISTORY_ROWS  5000
#define MCVIEW_VTERM_MAX_HISTORY_CELLS (1024 * 1024)

#define MCVIEW_VTERM_MAX_CANVAS_COLS   4096

/* A sixel picture bigger than this is dropped: a screen of it in 256 colors
   is a few hundred kilobytes. */
#define MCVIEW_VTERM_MAX_SIXEL_BYTES (4 * 1024 * 1024)

/* Ceilings on the pictures kept: a program can draw at the same spot forever. */
#define MCVIEW_VTERM_MAX_IMAGES          64
#define MCVIEW_VTERM_MAX_IMAGES_BYTES    (32 * 1024 * 1024)

#define MCVIEW_VTERM_DEFAULT_CELL_WIDTH  8
#define MCVIEW_VTERM_DEFAULT_CELL_HEIGHT 16

/*** file scope type declarations ****************************************************************/

struct mcview_vterm_struct
{
    mcview_ansi_state_t ansi;

    gboolean saw_esc;
    gboolean in_csi;
    gboolean csi_private;
    gboolean in_osc;
    gboolean in_osc_esc;
    gboolean osc_overflow;
    gboolean in_dcs;
    gboolean in_dcs_esc;
    /* APC, the string far2l's extensions are asked for with: ESC _ far2l1 ST */
    gboolean in_apc;
    gboolean in_apc_esc;
    gboolean apc_overflow;
    char apc_buf[16];
    int apc_len;
    gboolean
        far2l_allowed;      // the terminal mc runs in has the extensions: the program may have them
    gboolean far2l_active;  // ... and asked for them
    gboolean csi_gt;
    gboolean in_esc_char;
    /* Which of G0..G3 the designation being read names, -1 when it names none. */
    int esc_charset_slot;
    /* What each of G0..G3 was designated as; '0' is the DEC line-drawing set. */
    unsigned char charset[4];
    /* The one of them the shifts (SO, SI) print from. */
    int charset_gl;

    int params[MCVIEW_VTERM_MAX_PARAMS];
    int param_count;
    int current_param;
    gboolean has_current;

    unsigned char utf8_buf[4];
    int utf8_len;
    int utf8_expected;

    char osc_buf[2048];
    int osc_len;
    char dcs_buf[64];
    int dcs_len;
    /* Inside DCS ... q: the sixel data is collected whole, prefix and all. */
    gboolean in_sixel;
    gboolean sixel_overflow;
    GString *sixel;
    GPtrArray *images;
    gsize images_bytes;
    int cell_width;
    int cell_height;
    gboolean sixel_capable;
    char reply_buf[48];  // VTERM_REPLY built on the spot: the window sizes
    guint images_generation;
    char *osc7_raw;
    guint osc7_generation;
    /* The semantic prompt marks (OSC 133), kept raw: what they mean is the host's business. */
    char *osc133_raw;
    guint osc133_generation;

    int cursor_row;
    int cursor_col;

    int term_rows;
    int term_cols;
    int scroll_top;
    int scroll_bottom;

    off_t replay_offset;

    mcview_terminal_buffer_t *buf;
    mcview_terminal_buffer_t *snapshot_buf;
    int snapshot_cursor_row;
    int snapshot_cursor_col;
    mcview_terminal_buffer_t *alt_frame_buf;
    GPtrArray *history;       // rows that left the top of the main screen, oldest first
    GArray *history_wrapped;  // one flag per history row: it goes on in the next one
    gboolean keep_history;
    gsize history_cells;
    // Rows that ever left the top; unlike the history, this count does not move.
    gint64 scrolled_rows;

    gboolean new_chars_since_snapshot;

    gboolean in_alt_screen;

    gboolean app_cursor_keys;

    gboolean insert_mode;  // IRM: a printed character pushes the rest of the line right

    gboolean autowrap;      // DECAWM: a character past the last column starts a new row
    gboolean pending_wrap;  // the last column is taken; the next character wraps

    /* Where the rows went in the last reflow: for each row of the old
       history and screen, the first new row of its line and the offset of
       its cells in that line. */
    GArray *reflow_line_start;
    GArray *reflow_offset;
    GHashTable *reflow_pads;  // old row -> GArray of int: its columns a blank was put before
    gint64 reflow_oldest_abs;
    int reflow_old_cols;
    int reflow_new_cols;

    int dpy_top_row;
};

/*** file scope variables ************************************************************************/

/*** forward declarations (file scope functions) *************************************************/

static vterm_event_t vterm_dispatch_csi (mcview_vterm_t *vt, unsigned char final_byte);
static vterm_event_t vterm_handle_utf8 (mcview_vterm_t *vt, unsigned char byte);
static void vterm_finalize_param (mcview_vterm_t *vt);
static vterm_event_t vterm_make (mcview_vterm_t *vt, vterm_result_t type);
static void vterm_handle_osc (mcview_vterm_t *vt);
static vterm_event_t vterm_finish_apc (mcview_vterm_t *vt);
static void vterm_finish_osc (mcview_vterm_t *vt);
static void vterm_finish_sixel (mcview_vterm_t *vt);
static void vterm_images_clear (mcview_vterm_t *vt);
static void vterm_history_trim (mcview_vterm_t *vt);
static void vterm_images_shift (mcview_vterm_t *vt, int top, int bottom, int delta);
static void mcview_vterm_scroll_up (mcview_vterm_t *vt, int top, int bottom,
                                    const mcview_ansi_state_t *ansi);

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

static vterm_event_t
vterm_make (mcview_vterm_t *vt, vterm_result_t type)
{
    vterm_event_t ev;

    memset (&ev, 0, sizeof (ev));
    ev.type = type;
    ev.ansi = vt->ansi;
    return ev;
}

/* --------------------------------------------------------------------------------------------- */

static void
vterm_finalize_param (mcview_vterm_t *vt)
{
    if (vt->param_count < MCVIEW_VTERM_MAX_PARAMS)
    {
        vt->params[vt->param_count] = vt->current_param;
        vt->param_count++;
    }
    vt->current_param = 0;
    vt->has_current = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static vterm_event_t
vterm_dispatch_csi (mcview_vterm_t *vt, unsigned char final_byte)
{
    int p0, p1;

    /* Device Attributes: CSI c and CSI 0 c ask what the terminal is, CSI > c
       asks for its version. Answer VT100 with an advanced video option. */
    if (final_byte == 'c' && !vt->csi_private)
    {
        vterm_event_t ev = vterm_make (vt, VTERM_REPLY);

        /* With sixel it is a VT240 that draws them: level 2 with sixel graphics. */
        ev.reply = vt->csi_gt   ? ESC_STR "[>0;0;0c"
            : vt->sixel_capable ? ESC_STR "[?62;4;22c"
                                : ESC_STR "[?1;2c";
        vt->csi_gt = FALSE;
        return ev;
    }

    /* XTWINOPS: the sizes a program draws pictures by. Only when there are
       pictures to draw; otherwise the question goes unanswered, as before. */
    if (final_byte == 't' && !vt->csi_private && vt->sixel_capable && vt->param_count > 0)
    {
        vterm_event_t ev = vterm_make (vt, VTERM_REPLY);

        switch (vt->params[0])
        {
        case 14: /* text area in pixels */
            g_snprintf (vt->reply_buf, sizeof (vt->reply_buf), ESC_STR "[4;%d;%dt",
                        vt->term_rows * vt->cell_height, vt->term_cols * vt->cell_width);
            break;
        case 16: /* a cell in pixels */
            g_snprintf (vt->reply_buf, sizeof (vt->reply_buf), ESC_STR "[6;%d;%dt", vt->cell_height,
                        vt->cell_width);
            break;
        case 18: /* text area in cells */
            g_snprintf (vt->reply_buf, sizeof (vt->reply_buf), ESC_STR "[8;%d;%dt", vt->term_rows,
                        vt->term_cols);
            break;
        default:
            return vterm_make (vt, VTERM_CONSUMED);
        }
        ev.reply = vt->reply_buf;
        return ev;
    }

    /* DSR: CSI 5 n asks whether the terminal is well, CSI 6 n where the cursor is.
       Programs that ask the second one wait for the answer and give up without it. */
    if (final_byte == 'n' && !vt->csi_private && vt->param_count > 0
        && (vt->params[0] == 5 || vt->params[0] == 6))
    {
        vterm_event_t ev = vterm_make (vt, VTERM_REPLY);

        if (vt->params[0] == 5)
            ev.reply = ESC_STR "[0n";
        else
        {
            g_snprintf (vt->reply_buf, sizeof (vt->reply_buf), ESC_STR "[%d;%dR",
                        CLAMP (vt->cursor_row, 0, MAX (vt->term_rows - 1, 0)) + 1,
                        CLAMP (vt->cursor_col, 0, MAX (vt->term_cols - 1, 0)) + 1);
            ev.reply = vt->reply_buf;
        }
        return ev;
    }

    if (vt->csi_private)
    {
        if (vt->param_count > 0)
        {
            switch (vt->params[0])
            {
            case 1:
                vt->app_cursor_keys = (final_byte == 'h');
                break;
            case 7:
                mcview_vterm_set_autowrap (vt, final_byte == 'h');
                break;
            case 1049:
                if (final_byte == 'h')
                    return vterm_make (vt, VTERM_ALT_SCREEN_ENTER);
                if (final_byte == 'l')
                    return vterm_make (vt, VTERM_ALT_SCREEN_EXIT);
                break;
            default:
                break;
            }
        }
        return vterm_make (vt, VTERM_CONSUMED);
    }

    p0 = (vt->param_count > 0) ? vt->params[0] : 0;
    p1 = (vt->param_count > 1) ? vt->params[1] : 0;

    switch (final_byte)
    {
    case 'm':
        /* SGR: ansi.c already applied it. */
        return vterm_make (vt, VTERM_SGR);

    case 'h': /* SM -- set mode */
    case 'l': /* RM -- reset mode */
        if (p0 == 4)
            vt->insert_mode = (final_byte == 'h');
        return vterm_make (vt, VTERM_CONSUMED);

    case 'H': /* cursor position (1-based row;col) */
    case 'f': /* same */
    {
        vterm_event_t ev = vterm_make (vt, VTERM_CURSOR_ABS);
        ev.param1 = (p0 > 0 ? p0 : 1) - 1; /* 1-based -> 0-based */
        ev.param2 = (p1 > 0 ? p1 : 1) - 1;
        return ev;
    }

    case 'A': /* cursor up */
    {
        vterm_event_t ev = vterm_make (vt, VTERM_CURSOR_UP);
        ev.param1 = (p0 > 0) ? p0 : 1;
        return ev;
    }

    case 'B': /* cursor down */
    {
        vterm_event_t ev = vterm_make (vt, VTERM_CURSOR_DOWN);
        ev.param1 = (p0 > 0) ? p0 : 1;
        return ev;
    }

    case 'C': /* cursor forward */
    {
        vterm_event_t ev = vterm_make (vt, VTERM_CURSOR_FWD);
        ev.param1 = (p0 > 0) ? p0 : 1;
        return ev;
    }

    case 'D': /* cursor back N */
    {
        vterm_event_t ev = vterm_make (vt, VTERM_CURSOR_BACK);
        ev.param1 = (p0 > 0) ? p0 : 1;
        return ev;
    }

    case 'E': /* cursor next line */
    {
        vterm_event_t ev = vterm_make (vt, VTERM_CURSOR_ABS);
        ev.param1 = vt->cursor_row + ((p0 > 0) ? p0 : 1);
        ev.param2 = 0;
        return ev;
    }

    case 'F': /* cursor previous line */
    {
        int n = (p0 > 0) ? p0 : 1;
        vterm_event_t ev = vterm_make (vt, VTERM_CURSOR_ABS);
        ev.param1 = (vt->cursor_row >= n) ? vt->cursor_row - n : 0;
        ev.param2 = 0;
        return ev;
    }

    case 'G': /* cursor horizontal absolute (1-based col) */
    {
        vterm_event_t ev = vterm_make (vt, VTERM_CURSOR_ABS);
        ev.param1 = vt->cursor_row;
        ev.param2 = (p0 > 0 ? p0 : 1) - 1;
        return ev;
    }

    case 'K': /* erase in line */
        switch (p0)
        {
        case 0:
            return vterm_make (vt, VTERM_ERASE_EOL);
        case 1:
            return vterm_make (vt, VTERM_ERASE_BOL);
        case 2:
            return vterm_make (vt, VTERM_ERASE_LINE);
        default:
            return vterm_make (vt, VTERM_CONSUMED);
        }

    case 'J': /* erase in display */
        if (p0 == 2)
            return vterm_make (vt, VTERM_ERASE_SCREEN);
        if (p0 == 1)
            return vterm_make (vt, VTERM_ERASE_TO_BOS);
        return vterm_make (vt, VTERM_ERASE_TO_EOS);

    case 'r': /* DECSTBM -- set scroll region (1-based top;bottom) */
    {
        int top = (p0 > 0 ? p0 : 1) - 1;
        int bot = (p1 > 0 ? p1 : vt->term_rows) - 1;
        vterm_event_t ev;

        if (top < 0)
            top = 0;
        if (bot >= vt->term_rows)
            bot = vt->term_rows - 1;
        if (top >= bot) /* invalid region: consume without effect */
            return vterm_make (vt, VTERM_CONSUMED);
        ev = vterm_make (vt, VTERM_SET_SCROLL_REGION);
        ev.param1 = top;
        ev.param2 = bot;
        return ev;
    }

    case 'd': /* VPA -- vertical position absolute (1-based row) */
    {
        vterm_event_t ev = vterm_make (vt, VTERM_CURSOR_ROW_ABS);
        ev.param1 = (p0 > 0 ? p0 : 1) - 1;
        return ev;
    }

    case 'X': /* ECH -- erase characters */
    {
        vterm_event_t ev = vterm_make (vt, VTERM_ERASE_CHARS);
        ev.param1 = (p0 > 0) ? p0 : 1;
        return ev;
    }

    case 'P': /* DCH -- delete characters, shift left */
    {
        vterm_event_t ev = vterm_make (vt, VTERM_DCH);
        ev.param1 = (p0 > 0) ? p0 : 1;
        return ev;
    }

    case '@': /* ICH -- insert blank characters, shift right */
    {
        vterm_event_t ev = vterm_make (vt, VTERM_ICH);
        ev.param1 = (p0 > 0) ? p0 : 1;
        return ev;
    }

    default:
        return vterm_make (vt, VTERM_CONSUMED);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The DEC Special Graphics set that ESC ( 0 designates: the letters stand for
   the line-drawing and other characters an application draws frames with.
   Outside 0x5F..0x7E the set is plain ASCII. */
static gunichar
vterm_dec_graphic (unsigned char byte)
{
    static const gunichar map[] = {
        0x00A0, /* _ */ 0x25C6, /* ` */ 0x2592, /* a */ 0x2409, /* b */
        0x240C, /* c */ 0x240D, /* d */ 0x240A, /* e */ 0x00B0, /* f */
        0x00B1, /* g */ 0x2424, /* h */ 0x240B, /* i */ 0x2518, /* j */
        0x2510, /* k */ 0x250C, /* l */ 0x2514, /* m */ 0x253C, /* n */
        0x23BA, /* o */ 0x23BB, /* p */ 0x2500, /* q */ 0x23BC, /* r */
        0x23BD, /* s */ 0x251C, /* t */ 0x2524, /* u */ 0x2534, /* v */
        0x252C, /* w */ 0x2502, /* x */ 0x2264, /* y */ 0x2265, /* z */
        0x03C0, /* { */ 0x2260, /* | */ 0x00A3, /* } */ 0x00B7, /* ~ */
    };

    if (byte < 0x5Fu || byte > 0x7Eu)
        return (gunichar) byte;
    return map[byte - 0x5Fu];
}

/* --------------------------------------------------------------------------------------------- */

/* What a printed byte stands for in the character set now in use. */
static gunichar
vterm_charset_char (const mcview_vterm_t *vt, unsigned char byte)
{
    if (vt->charset[vt->charset_gl] != '0')
        return (gunichar) byte;
    return vterm_dec_graphic (byte);
}

/* --------------------------------------------------------------------------------------------- */

static int
utf8_seq_len (unsigned char b)
{
    if (b < 0x80)
        return 1;
    if ((b & 0xE0) == 0xC0)
        return 2;
    if ((b & 0xF0) == 0xE0)
        return 3;
    if ((b & 0xF8) == 0xF0)
        return 4;
    return -1; /* continuation byte or invalid */
}

/* --------------------------------------------------------------------------------------------- */

static vterm_event_t
vterm_handle_utf8 (mcview_vterm_t *vt, unsigned char byte)
{
    if (vt->utf8_expected > 0)
    {
        if ((byte & 0xC0) != 0x80)
        {
            vt->utf8_len = 0;
            vt->utf8_expected = 0;
        }
        else
        {
            vt->utf8_buf[vt->utf8_len++] = byte;
            if (vt->utf8_len == vt->utf8_expected)
            {
                gunichar ch;
                vterm_event_t ev = vterm_make (vt, VTERM_CHAR);

                switch (vt->utf8_expected)
                {
                case 2:
                    ch = ((gunichar) (vt->utf8_buf[0] & 0x1F) << 6) | (vt->utf8_buf[1] & 0x3F);
                    break;
                case 3:
                    ch = ((gunichar) (vt->utf8_buf[0] & 0x0F) << 12)
                        | ((gunichar) (vt->utf8_buf[1] & 0x3F) << 6) | (vt->utf8_buf[2] & 0x3F);
                    break;
                case 4:
                    ch = ((gunichar) (vt->utf8_buf[0] & 0x07) << 18)
                        | ((gunichar) (vt->utf8_buf[1] & 0x3F) << 12)
                        | ((gunichar) (vt->utf8_buf[2] & 0x3F) << 6) | (vt->utf8_buf[3] & 0x3F);
                    break;
                default:
                    ch = '?';
                    break;
                }

                vt->utf8_len = 0;
                vt->utf8_expected = 0;
                ev.ch = ch;
                return ev;
            }
            return vterm_make (vt, VTERM_CONSUMED);
        }
    }

    {
        int expected = utf8_seq_len (byte);

        if (expected == 1)
        {
            vterm_event_t ev = vterm_make (vt, VTERM_CHAR);
            ev.ch = vterm_charset_char (vt, byte);
            return ev;
        }

        if (expected > 1)
        {
            vt->utf8_buf[0] = byte;
            vt->utf8_len = 1;
            vt->utf8_expected = expected;
            return vterm_make (vt, VTERM_CONSUMED);
        }

        return vterm_make (vt, VTERM_CONSUMED);
    }
}

/* --------------------------------------------------------------------------------------------- */
/* ESC _ ... ST ended. The far2l extensions are asked for and given up with far2l1 and far2l0; the
   answer to the first is far2lok, and only a terminal that has them itself gives it. Every other
   APC is swallowed whole, so that its text is not drawn. */

static vterm_event_t
vterm_finish_apc (mcview_vterm_t *vt)
{
    const gboolean overflow = vt->apc_overflow;

    vt->in_apc = FALSE;
    vt->in_apc_esc = FALSE;
    vt->apc_overflow = FALSE;
    vt->apc_buf[vt->apc_len] = '\0';

    if (!overflow && vt->far2l_allowed && strcmp (vt->apc_buf, "far2l1") == 0)
    {
        vterm_event_t ev = vterm_make (vt, VTERM_REPLY);

        vt->far2l_active = TRUE;
        ev.reply = ESC_STR "_far2lok" ESC_STR "\\";
        return ev;
    }
    if (!overflow && strcmp (vt->apc_buf, "far2l0") == 0)
        vt->far2l_active = FALSE;

    return vterm_make (vt, VTERM_CONSUMED);
}

/* --------------------------------------------------------------------------------------------- */

static void
vterm_handle_osc (mcview_vterm_t *vt)
{
    if (strncmp (vt->osc_buf, "7;", 2) == 0)
    {
        g_free (vt->osc7_raw);
        vt->osc7_raw = g_strdup (vt->osc_buf);
        vt->osc7_generation++;
    }
    else if (strncmp (vt->osc_buf, "133;", 4) == 0)
    {
        g_free (vt->osc133_raw);
        vt->osc133_raw = g_strdup (vt->osc_buf);
        vt->osc133_generation++;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * End an OSC sequence and act on it.
 *
 * A sequence longer than the buffer arrives truncated, and a path cut short is worse than no
 * path at all: drop the whole of it.
 */

static void
vterm_finish_osc (mcview_vterm_t *vt)
{
    vt->in_osc = FALSE;
    vt->osc_buf[vt->osc_len] = '\0';

    if (!vt->osc_overflow)
        vterm_handle_osc (vt);

    vt->osc_overflow = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
vterm_image_free (gpointer data)
{
    mcview_vterm_image_t *image = data;

    if (image->data != NULL)
        g_bytes_unref (image->data);
    g_free (image);
}

/* --------------------------------------------------------------------------------------------- */

static void
vterm_images_clear (mcview_vterm_t *vt)
{
    if (vt->images == NULL || vt->images->len == 0)
        return;

    g_ptr_array_set_size (vt->images, 0);
    vt->images_bytes = 0;
    vt->images_generation++;
}

/* --------------------------------------------------------------------------------------------- */

/* Rows @top..@bottom moved by @delta (negative: up). Where the rows leaving
   the top become the history, the pictures on them stay with it: their row
   counts from the top of the live screen and so goes negative, and the
   viewer draws them again when the user scrolls back. A picture that leaves
   the region any other way is gone: sixel cannot be drawn in part. */
static void
vterm_images_shift (mcview_vterm_t *vt, int top, int bottom, int delta)
{
    const gboolean to_history = top == 0 && vt->keep_history && !vt->in_alt_screen;
    const int oldest = to_history ? -mcview_vterm_history_len (vt) : 0;
    guint i;

    if (vt->images == NULL || delta == 0)
        return;

    for (i = 0; i < vt->images->len;)
    {
        mcview_vterm_image_t *image = g_ptr_array_index (vt->images, i);
        const gboolean above = image->row + image->rows - 1 < top;

        if ((above && !to_history) || image->row > bottom)
        {
            i++;
            continue;
        }

        image->row += delta;
        vt->images_generation++;
        if (image->row + image->rows - 1 > bottom
            || (to_history ? image->row + image->rows - 1 < oldest : image->row < top))
        {
            vt->images_bytes -= image->data != NULL ? g_bytes_get_size (image->data) : 0;
            g_ptr_array_remove_index (vt->images, i);
        }
        else
            i++;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Pictures touching any of the rows @from..@to are gone. */
static void
vterm_images_erase_rows (mcview_vterm_t *vt, int from, int to)
{
    guint i;

    if (vt->images == NULL)
        return;

    for (i = 0; i < vt->images->len;)
    {
        const mcview_vterm_image_t *image = g_ptr_array_index (vt->images, i);

        if (image->row + image->rows - 1 < from || image->row > to)
            i++;
        else
        {
            vt->images_bytes -= image->data != NULL ? g_bytes_get_size (image->data) : 0;
            g_ptr_array_remove_index (vt->images, i);
            vt->images_generation++;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The size of the picture in pixels. The raster attributes name it outright;
   without them it is what the data draws: six rows per band, a column per
   sixel character, repeats counted. */
static void
vterm_sixel_measure (const char *data, gsize len, int *width, int *height)
{
    gsize i = 0;
    int x = 0, w = 0, bands = 0;
    gboolean drew = FALSE;

    *width = 0;
    *height = 0;

    if (i < len && data[i] == '"')
    {
        int p[4] = { 0, 0, 0, 0 };
        int n = 0;

        i++;
        while (i < len && n < 4 && ((data[i] >= '0' && data[i] <= '9') || data[i] == ';'))
        {
            if (data[i] == ';')
                n++;
            else if (p[n] < 100000)
                p[n] = p[n] * 10 + (data[i] - '0');
            i++;
        }
        if (n >= 3 && p[2] > 0 && p[3] > 0)
        {
            *width = p[2];
            *height = p[3];
            return;
        }
    }

    for (; i < len; i++)
    {
        const char c = data[i];

        if (c >= '?' && c <= '~')
        {
            x++;
            drew = TRUE;
        }
        else if (c == '!')
        {
            int n = 0;

            for (i++; i < len && data[i] >= '0' && data[i] <= '9'; i++)
                if (n < 100000)
                    n = n * 10 + (data[i] - '0');
            if (i < len && data[i] >= '?' && data[i] <= '~')
            {
                x += (n > 0) ? n : 1;
                drew = TRUE;
            }
        }
        else if (c == '$' || c == '-')
        {
            w = MAX (w, x);
            x = 0;
            if (c == '-')
            {
                bands++;
                drew = FALSE;
            }
        }
        /* '#' and its parameters, and anything else, take no space. */
    }

    w = MAX (w, x);
    if (drew)
        bands++;

    *width = w;
    *height = bands * 6;
}

/* --------------------------------------------------------------------------------------------- */

/* ESC \ closed the sixel data: it becomes a picture at the cursor, and the
   cursor goes below it, the way a terminal with sixel scrolling does. */
static void
vterm_finish_sixel (mcview_vterm_t *vt)
{
    mcview_vterm_image_t *image;
    const char *body;
    gsize body_len;
    int width, height, rows, cols;

    vt->in_dcs = FALSE;
    vt->in_sixel = FALSE;

    if (vt->sixel_overflow)
    {
        vt->sixel_overflow = FALSE;
        g_string_truncate (vt->sixel, 0);
        return;
    }

    /* The data starts after the 'q' of the prefix. */
    body = strchr (vt->sixel->str + 2, 'q') + 1;
    body_len = vt->sixel->len - (body - vt->sixel->str);
    vterm_sixel_measure (body, body_len, &width, &height);

    g_string_append (vt->sixel, ESC_STR "\\");

    if (width <= 0 || height <= 0 || vt->cursor_col >= vt->term_cols)
    {
        g_string_truncate (vt->sixel, 0);
        return;
    }

    /* The full width: a picture is drawn whole or not at all, and one that
       runs past the widget must be seen to run past it. */
    cols = (width + vt->cell_width - 1) / vt->cell_width;
    rows = (height + vt->cell_height - 1) / vt->cell_height;

    /* Taller than what is left of the scroll region: the region scrolls up,
       and the picture with it. A picture taller than the region is gone the
       moment it is drawn, the way it is on a terminal. */
    if (vt->cursor_row >= vt->scroll_top && vt->cursor_row <= vt->scroll_bottom)
    {
        int overflow = vt->cursor_row + rows - 1 - vt->scroll_bottom;

        while (overflow > 0)
        {
            mcview_vterm_scroll_up (vt, vt->scroll_top, vt->scroll_bottom, &vt->ansi);
            vt->cursor_row--;
            overflow--;
        }
    }

    if (vt->cursor_row < 0 || vt->cursor_row + rows - 1 >= vt->term_rows)
    {
        vt->cursor_row = MAX (vt->cursor_row, 0);
        g_string_truncate (vt->sixel, 0);
        return;
    }

    image = g_new0 (mcview_vterm_image_t, 1);
    image->row = vt->cursor_row;
    image->col = vt->cursor_col;
    image->rows = rows;
    image->cols = cols;
    image->width = width;
    image->height = height;
    /* Where the terminal draws no sixel the bytes have nowhere to go; the
       place of the picture still matters for the layout. */
    if (vt->sixel_capable)
    {
        image->data = g_bytes_new (vt->sixel->str, vt->sixel->len);
        vt->images_bytes += vt->sixel->len;
    }
    g_string_truncate (vt->sixel, 0);

    if (vt->images == NULL)
        vt->images = g_ptr_array_new_with_free_func (vterm_image_free);
    g_ptr_array_add (vt->images, image);
    while (vt->images->len > MCVIEW_VTERM_MAX_IMAGES
           || (vt->images_bytes > MCVIEW_VTERM_MAX_IMAGES_BYTES && vt->images->len > 1))
    {
        const mcview_vterm_image_t *oldest = g_ptr_array_index (vt->images, 0);

        vt->images_bytes -= oldest->data != NULL ? g_bytes_get_size (oldest->data) : 0;
        g_ptr_array_remove_index (vt->images, 0);
    }
    vt->images_generation++;

    vt->new_chars_since_snapshot = TRUE;
    vt->cursor_row += rows;
    if (vt->cursor_row > vt->scroll_bottom && image->row <= vt->scroll_bottom)
        vt->cursor_row = vt->scroll_bottom;
    if (vt->cursor_row >= vt->term_rows)
        vt->cursor_row = vt->term_rows - 1;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mcview_vterm_history_push (mcview_vterm_t *vt, int row)
{
    GArray *cells;
    guint8 wrapped;

    if (vt->history == NULL)
    {
        vt->history = g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);
        vt->history_wrapped = g_array_new (FALSE, FALSE, sizeof (guint8));
    }

    // Only the width that can be drawn again: a program may write far past it.
    cells = mcview_terminal_buffer_row_copy_n (vt->buf, row, vt->term_cols);
    if (cells == NULL)
    {
        // An empty line is still a line of the output.
        cells = g_array_new (FALSE, TRUE, sizeof (mcview_vterm_cell_t));
    }

    wrapped = mcview_terminal_buffer_is_wrapped (vt->buf, row) ? 1 : 0;
    g_ptr_array_add (vt->history, cells);
    g_array_append_val (vt->history_wrapped, wrapped);
    vt->history_cells += cells->len;

    vterm_history_trim (vt);
}

/* --------------------------------------------------------------------------------------------- */

static void
vterm_history_trim (mcview_vterm_t *vt)
{
    while (vt->history->len > MCVIEW_VTERM_MAX_HISTORY_ROWS
           || (vt->history_cells > MCVIEW_VTERM_MAX_HISTORY_CELLS && vt->history->len > 1))
    {
        const GArray *first = (const GArray *) g_ptr_array_index (vt->history, 0);

        vt->history_cells -= first->len;
        g_ptr_array_remove_index (vt->history, 0);
        g_array_remove_index (vt->history_wrapped, 0);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
vterm_cell_is_blank (const mcview_vterm_cell_t *cell)
{
    return cell->ch == 0
        || (cell->ch == ' ' && cell->attr.bg == MCVIEW_ANSI_COLOR_DEFAULT && !cell->attr.reverse
            && !cell->attr.underline);
}

/* --------------------------------------------------------------------------------------------- */

/* Where cell @col of the old row @old went in its line in the last reflow. */
static gint64
vterm_reflow_off (const mcview_vterm_t *vt, gint64 old, int col)
{
    gint64 off = g_array_index (vt->reflow_offset, gint64, old) + col;
    const GArray *pads = g_hash_table_lookup (vt->reflow_pads, GINT_TO_POINTER ((int) old));
    guint u;

    for (u = 0; pads != NULL && u < pads->len; u++)
        if (g_array_index (pads, int, u) <= col)
            off++;

    return off;
}

/* --------------------------------------------------------------------------------------------- */

/* The rows are laid out again for a screen @cols wide: the lines the
   terminal broke are put together and broken where the new width wants
   them. What the screen showed stays on it, the cursor with it; the rest
   is the history. */
static void
vterm_reflow (mcview_vterm_t *vt, int cols)
{
    const int old_cols = vt->term_cols;
    const int hist_len = mcview_vterm_history_len (vt);
    const int used = MAX (MAX (mcview_terminal_buffer_max_row (vt->buf), vt->cursor_row) + 1, 1);
    const int total = hist_len + used;
    const gint64 oldest_abs = vt->scrolled_rows - hist_len;
    GPtrArray *lines;     // GArray of cells per logical line
    GArray *row_line;     // int per old row: its line
    GArray *line_first;   // gint64 per line: its first new row
    GPtrArray *new_rows;  // GArray of cells per new row
    GArray *new_wrapped;  // guint8 per new row
    GArray *line = NULL;
    mcview_vterm_cell_t empty;
    mcview_ansi_state_t ansi;
    gint64 cursor, cursor_off, screen_start, new_total;
    guint u;
    int i;

    if (vt->history == NULL)
    {
        vt->history = g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);
        vt->history_wrapped = g_array_new (FALSE, FALSE, sizeof (guint8));
    }

    memset (&empty, 0, sizeof (empty));
    mcview_ansi_state_init (&ansi);
    lines = g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);
    row_line = g_array_sized_new (FALSE, FALSE, sizeof (int), total);
    line_first = g_array_new (FALSE, FALSE, sizeof (gint64));
    new_rows = g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);
    new_wrapped = g_array_new (FALSE, FALSE, sizeof (guint8));

    if (vt->reflow_line_start == NULL)
    {
        vt->reflow_line_start = g_array_new (FALSE, FALSE, sizeof (gint64));
        vt->reflow_offset = g_array_new (FALSE, FALSE, sizeof (gint64));
        vt->reflow_pads = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
                                                 (GDestroyNotify) g_array_unref);
    }
    g_array_set_size (vt->reflow_line_start, 0);
    g_array_set_size (vt->reflow_offset, 0);
    g_hash_table_remove_all (vt->reflow_pads);

    // The old rows, put together into lines.
    for (i = 0; i < total; i++)
    {
        GArray *cells;
        gboolean wrapped;
        const int line_idx = (int) lines->len;
        gint64 off;
        int pads = 0;

        if (i < hist_len)
        {
            cells = (GArray *) mcview_vterm_history_row (vt, i);
            wrapped = mcview_vterm_history_row_wrapped (vt, i);
        }
        else
        {
            cells = mcview_terminal_buffer_row_copy (vt->buf, i - hist_len);
            wrapped = mcview_terminal_buffer_is_wrapped (vt->buf, i - hist_len);
        }

        if (line == NULL)
            line = g_array_new (FALSE, TRUE, sizeof (mcview_vterm_cell_t));
        // The blank that sent a wide character to this row is not text.
        else if (line->len > 0 && g_array_index (line, mcview_vterm_cell_t, line->len - 1).ch == 0
                 && cells != NULL && cells->len > 1
                 && g_array_index (cells, mcview_vterm_cell_t, 1).ch == MCVIEW_VTERM_WIDE_TAIL)
            g_array_set_size (line, line->len - 1);
        off = line->len;
        g_array_append_val (row_line, line_idx);
        g_array_append_val (vt->reflow_offset, off);

        if (cells != NULL)
        {
            const int n = MIN ((int) cells->len, old_cols);
            int j;

            for (j = 0; j < n; j++)
            {
                const mcview_vterm_cell_t *cell = &g_array_index (cells, mcview_vterm_cell_t, j);

                // A wide character the new width would cut in two starts the next row.
                if (cols > 1 && j + 1 < n && (gint64) line->len % cols == cols - 1
                    && cell->ch != MCVIEW_VTERM_WIDE_TAIL
                    && g_array_index (cells, mcview_vterm_cell_t, j + 1).ch
                        == MCVIEW_VTERM_WIDE_TAIL)
                {
                    GArray *at = g_hash_table_lookup (vt->reflow_pads, GINT_TO_POINTER (i));

                    if (at == NULL)
                    {
                        at = g_array_new (FALSE, FALSE, sizeof (int));
                        g_hash_table_insert (vt->reflow_pads, GINT_TO_POINTER (i), at);
                    }
                    g_array_append_val (at, j);
                    g_array_append_val (line, empty);
                    pads++;
                }
                g_array_append_val (line, *cell);
            }
            if (i >= hist_len)
                g_array_unref (cells);
        }

        if (wrapped && i + 1 < total)
        {
            // A broken row is full to the edge, whatever the buffer kept of it.
            while ((gint64) line->len < off + old_cols + pads)
                g_array_append_val (line, empty);
            continue;
        }

        while (line->len > 0
               && vterm_cell_is_blank (&g_array_index (line, mcview_vterm_cell_t, line->len - 1)))
            g_array_set_size (line, line->len - 1);
        g_ptr_array_add (lines, line);
        line = NULL;
    }

    // The lines, cut for the new width.
    for (u = 0; u < lines->len; u++)
    {
        const GArray *l = (const GArray *) g_ptr_array_index (lines, u);
        const gint64 first = new_rows->len;
        gint64 pos = 0;

        g_array_append_val (line_first, first);
        do
        {
            const guint8 more = (pos + cols < (gint64) l->len) ? 1 : 0;
            GArray *row = g_array_new (FALSE, TRUE, sizeof (mcview_vterm_cell_t));

            if (pos < (gint64) l->len)
                g_array_append_vals (row, &g_array_index (l, mcview_vterm_cell_t, pos),
                                     MIN (cols, (gint64) l->len - pos));
            g_ptr_array_add (new_rows, row);
            g_array_append_val (new_wrapped, more);
            pos += cols;
        }
        while (pos < (gint64) l->len);
    }
    new_total = new_rows->len;

    for (i = 0; i < total; i++)
    {
        const gint64 first = g_array_index (line_first, gint64, g_array_index (row_line, int, i));

        g_array_append_val (vt->reflow_line_start, first);
    }
    vt->reflow_oldest_abs = oldest_abs;
    vt->reflow_old_cols = old_cols;
    vt->reflow_new_cols = cols;

    // The screen starts where its first row's line starts, and keeps the cursor.
    cursor_off = vterm_reflow_off (vt, hist_len + vt->cursor_row, vt->cursor_col);
    cursor = g_array_index (vt->reflow_line_start, gint64, hist_len + vt->cursor_row)
        + cursor_off / cols;
    screen_start = g_array_index (vt->reflow_line_start, gint64, hist_len);
    if (cursor - screen_start >= vt->term_rows)
        screen_start = cursor - vt->term_rows + 1;

    // The pictures follow their rows; one that leaves the screen is gone.
    if (vt->images != NULL)
    {
        for (u = 0; u < vt->images->len;)
        {
            mcview_vterm_image_t *image = g_ptr_array_index (vt->images, u);
            const int old = hist_len + image->row;
            gint64 row = -1;

            if (image->row >= 0 && old < total)
                row = g_array_index (vt->reflow_line_start, gint64, old)
                    + vterm_reflow_off (vt, old, 0) / cols - screen_start;
            if (row >= 0 && row < vt->term_rows)
            {
                image->row = (int) row;
                u++;
            }
            else
            {
                vt->images_bytes -= image->data != NULL ? g_bytes_get_size (image->data) : 0;
                g_ptr_array_remove_index (vt->images, u);
            }
        }
        vt->images_generation++;
    }

    // Everything above the screen is the history.
    g_ptr_array_set_size (vt->history, 0);
    g_array_set_size (vt->history_wrapped, 0);
    vt->history_cells = 0;
    for (i = 0; i < screen_start; i++)
    {
        GArray *row = (GArray *) g_ptr_array_steal_index (new_rows, 0);
        guint8 wrapped = g_array_index (new_wrapped, guint8, i);

        g_ptr_array_add (vt->history, row);
        g_array_append_val (vt->history_wrapped, wrapped);
        vt->history_cells += row->len;
    }
    vt->scrolled_rows = oldest_abs + screen_start;
    vterm_history_trim (vt);

    mcview_terminal_buffer_clear (vt->buf);
    for (i = 0; i < vt->term_rows && (gint64) i < new_total - screen_start; i++)
    {
        const GArray *row = (const GArray *) g_ptr_array_index (new_rows, i);

        if (row->len > 0)
            mcview_terminal_buffer_set_row (vt->buf, i, row);
        else
            mcview_terminal_buffer_erase_line (vt->buf, i, cols, &ansi);
        mcview_terminal_buffer_set_wrapped (
            vt->buf, i, g_array_index (new_wrapped, guint8, screen_start + i) != 0);
    }
    if (i > 0)
        mcview_terminal_buffer_set_max_row (vt->buf, i - 1);

    vt->cursor_row = (int) (cursor - screen_start);
    vt->cursor_col = (int) (cursor_off % cols);
    vt->pending_wrap = FALSE;

    g_ptr_array_unref (lines);
    g_array_unref (row_line);
    g_array_unref (line_first);
    g_ptr_array_unref (new_rows);
    g_array_unref (new_wrapped);
}

/* --------------------------------------------------------------------------------------------- */

/* Move the region one row up. A row that leaves the top of the screen goes to
   the history. */
static void
mcview_vterm_scroll_up (mcview_vterm_t *vt, int top, int bottom, const mcview_ansi_state_t *ansi)
{
    if (top == 0 && !vt->in_alt_screen)
    {
        /* Counted even when no history is kept: what the count names is the
           row, not the copy of it. */
        vt->scrolled_rows++;
        if (vt->keep_history)
            mcview_vterm_history_push (vt, top);
    }

    mcview_terminal_buffer_scroll_up (vt->buf, top, bottom, vt->term_cols, ansi);
    vterm_images_shift (vt, top, bottom, -1);
}

/* --------------------------------------------------------------------------------------------- */

/* A character over one half of a wide one leaves the other half blank. */
static void
vterm_split_wide (mcview_vterm_t *vt, int row, int col, int width, const mcview_ansi_state_t *ansi)
{
    const mcview_vterm_cell_t *cell;

    cell = mcview_terminal_buffer_get (vt->buf, row, col);
    if (cell != NULL && cell->ch == MCVIEW_VTERM_WIDE_TAIL && col > 0)
        mcview_terminal_buffer_put_char (vt->buf, row, col - 1, ' ', ansi);

    cell = mcview_terminal_buffer_get (vt->buf, row, col + width);
    if (cell != NULL && cell->ch == MCVIEW_VTERM_WIDE_TAIL)
        mcview_terminal_buffer_put_char (vt->buf, row, col + width, ' ', ansi);
}

/* --------------------------------------------------------------------------------------------- */

/* Down one row, from the bottom of the region by scrolling it. */
static void
vterm_linefeed (mcview_vterm_t *vt, const mcview_ansi_state_t *ansi)
{
    vt->cursor_col = 0;
    if (vt->cursor_row >= vt->scroll_top && vt->cursor_row < vt->scroll_bottom)
        vt->cursor_row++;
    else if (vt->cursor_row == vt->scroll_bottom)
        mcview_vterm_scroll_up (vt, vt->scroll_top, vt->scroll_bottom, ansi);
    else if (vt->cursor_row < vt->term_rows - 1)
        vt->cursor_row++;
}

/* --------------------------------------------------------------------------------------------- */

/* The newest row of the history, handed over to the caller, or NULL if there is
   none left. */
static GArray *
mcview_vterm_history_pop (mcview_vterm_t *vt, gboolean *wrapped)
{
    GArray *cells;
    guint last;

    *wrapped = FALSE;
    if (vt->history == NULL || vt->history->len == 0)
        return NULL;

    last = vt->history->len - 1;
    cells = (GArray *) g_ptr_array_steal_index (vt->history, last);
    *wrapped = g_array_index (vt->history_wrapped, guint8, last) != 0;
    g_array_remove_index (vt->history_wrapped, last);
    vt->history_cells -= cells->len;

    return cells;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_set_keep_history (mcview_vterm_t *vt, gboolean keep)
{
    vt->keep_history = keep;
    if (!keep && vt->history != NULL)
    {
        g_ptr_array_unref (vt->history);
        vt->history = NULL;
        g_array_unref (vt->history_wrapped);
        vt->history_wrapped = NULL;
        vt->history_cells = 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_vterm_history_len (const mcview_vterm_t *vt)
{
    return (vt == NULL || vt->history == NULL) ? 0 : (int) vt->history->len;
}

/* --------------------------------------------------------------------------------------------- */

gint64
mcview_vterm_scrolled_rows (const mcview_vterm_t *vt)
{
    return (vt == NULL) ? 0 : vt->scrolled_rows;
}

/* --------------------------------------------------------------------------------------------- */

const GArray *
mcview_vterm_history_row (const mcview_vterm_t *vt, int index)
{
    if (vt == NULL || vt->history == NULL || index < 0 || index >= (int) vt->history->len)
        return NULL;

    return (const GArray *) g_ptr_array_index (vt->history, index);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_vterm_history_row_wrapped (const mcview_vterm_t *vt, int index)
{
    if (vt == NULL || vt->history == NULL || index < 0 || index >= (int) vt->history->len)
        return FALSE;

    return g_array_index (vt->history_wrapped, guint8, index) != 0;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_set_autowrap (mcview_vterm_t *vt, gboolean autowrap)
{
    vt->autowrap = autowrap;
    if (!autowrap)
        vt->pending_wrap = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_vterm_reflow_map (const mcview_vterm_t *vt, gint64 abs_row, int col, gint64 *new_abs_row,
                         int *new_col)
{
    gint64 old, off;

    if (vt == NULL || vt->reflow_line_start == NULL)
        return FALSE;

    old = abs_row - vt->reflow_oldest_abs;
    if (old < 0 || old >= (gint64) vt->reflow_line_start->len)
        return FALSE;

    off = vterm_reflow_off (vt, old, CLAMP (col, 0, vt->reflow_old_cols - 1));
    *new_abs_row = vt->reflow_oldest_abs + g_array_index (vt->reflow_line_start, gint64, old)
        + off / vt->reflow_new_cols;
    *new_col = (int) (off % vt->reflow_new_cols);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

mcview_vterm_t *
mcview_vterm_new (void)
{
    mcview_vterm_t *vt;

    vt = g_new0 (mcview_vterm_t, 1);
    mcview_ansi_state_init (&vt->ansi);
    vt->buf = mcview_terminal_buffer_new ();
    vt->dpy_top_row = MCVIEW_VTERM_FOLLOW_END;
    vt->term_rows = 24;
    vt->term_cols = 80;
    vt->scroll_top = 0;
    vt->scroll_bottom = 23;
    vt->esc_charset_slot = -1;
    memset (vt->charset, 'B', sizeof (vt->charset));
    vt->cell_width = MCVIEW_VTERM_DEFAULT_CELL_WIDTH;
    vt->cell_height = MCVIEW_VTERM_DEFAULT_CELL_HEIGHT;
    vt->sixel = g_string_new (NULL);
    return vt;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_free (mcview_vterm_t *vt)
{
    if (vt == NULL)
        return;
    mcview_terminal_buffer_free (vt->buf);
    mcview_terminal_buffer_free (vt->snapshot_buf);
    if (vt->history != NULL)
    {
        g_ptr_array_unref (vt->history);
        g_array_unref (vt->history_wrapped);
    }
    if (vt->reflow_line_start != NULL)
    {
        g_array_unref (vt->reflow_line_start);
        g_array_unref (vt->reflow_offset);
        g_hash_table_destroy (vt->reflow_pads);
    }
    mcview_terminal_buffer_free (vt->alt_frame_buf);
    g_free (vt->osc7_raw);
    g_free (vt->osc133_raw);
    g_string_free (vt->sixel, TRUE);
    if (vt->images != NULL)
        g_ptr_array_unref (vt->images);
    g_free (vt);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_reset (mcview_vterm_t *vt)
{
    mcview_ansi_state_init (&vt->ansi);
    vt->saw_esc = FALSE;
    vt->in_csi = FALSE;
    vt->csi_private = FALSE;
    vt->in_osc = FALSE;
    vt->in_osc_esc = FALSE;
    vt->osc_overflow = FALSE;
    vt->in_dcs = FALSE;
    vt->in_dcs_esc = FALSE;
    vt->in_apc = FALSE;
    vt->in_apc_esc = FALSE;
    vt->apc_len = 0;
    vt->far2l_active = FALSE;
    vt->dcs_len = 0;
    vt->in_sixel = FALSE;
    vt->sixel_overflow = FALSE;
    g_string_truncate (vt->sixel, 0);
    vterm_images_clear (vt);
    vt->csi_gt = FALSE;
    vt->in_esc_char = FALSE;
    vt->esc_charset_slot = -1;
    memset (vt->charset, 'B', sizeof (vt->charset));
    vt->charset_gl = 0;
    vt->param_count = 0;
    vt->current_param = 0;
    vt->has_current = FALSE;
    vt->utf8_len = 0;
    vt->utf8_expected = 0;
    vt->app_cursor_keys = FALSE;
    vt->cursor_row = 0;
    vt->cursor_col = 0;
    vt->scroll_top = 0;
    vt->scroll_bottom = vt->term_rows - 1;
    vt->replay_offset = 0;
    vt->dpy_top_row = MCVIEW_VTERM_FOLLOW_END;
    mcview_terminal_buffer_free (vt->snapshot_buf);
    vt->snapshot_buf = NULL;
    mcview_terminal_buffer_free (vt->alt_frame_buf);
    vt->alt_frame_buf = NULL;
    vt->new_chars_since_snapshot = FALSE;
    vt->in_alt_screen = FALSE;
    vt->insert_mode = FALSE;
    g_free (vt->osc7_raw);
    vt->osc7_raw = NULL;
    g_free (vt->osc133_raw);
    vt->osc133_raw = NULL;
    vt->osc_len = 0;
    if (vt->history != NULL)
    {
        g_ptr_array_set_size (vt->history, 0);
        g_array_set_size (vt->history_wrapped, 0);
    }
    vt->pending_wrap = FALSE;
    vt->history_cells = 0;
    vt->scrolled_rows = 0;
    mcview_terminal_buffer_clear (vt->buf);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_vterm_set_size (mcview_vterm_t *vt, int rows, int cols)
{
    if (rows < 1)
        rows = 1;
    if (cols < 1)
        cols = 1;

    if (rows == vt->term_rows && cols == vt->term_cols)
        return FALSE;

    if (cols != vt->term_cols && vt->keep_history && !vt->in_alt_screen)
        vterm_reflow (vt, cols);

    // A screen made taller takes back what it lost when it was made shorter.
    if (rows > vt->term_rows && vt->keep_history && !vt->in_alt_screen
        && mcview_vterm_history_len (vt) > 0)
    {
        mcview_ansi_state_t ansi;
        int used = MAX (mcview_terminal_buffer_max_row (vt->buf), vt->cursor_row) + 1;
        int take = MIN (rows - used, mcview_vterm_history_len (vt));
        int i;

        mcview_ansi_state_init (&ansi);

        for (i = 0; i < take; i++)
        {
            gboolean wrapped;
            GArray *cells = mcview_vterm_history_pop (vt, &wrapped);

            mcview_terminal_buffer_scroll_down (vt->buf, 0, rows - 1, cols, &ansi);
            mcview_terminal_buffer_set_row (vt->buf, 0, cells);
            mcview_terminal_buffer_set_wrapped (vt->buf, 0, wrapped);
            g_array_unref (cells);
        }

        if (take > 0)
        {
            vt->cursor_row += take;
            /* Those rows are on the screen again, and every row below them
               moved down: what the count names has to move with them. */
            vt->scrolled_rows -= take;
            vterm_images_shift (vt, 0, rows - 1, take);
            mcview_terminal_buffer_set_max_row (vt->buf, used + take - 1);
        }
    }

    // A screen made shorter loses rows off the top; they go to the history.
    if (rows < vt->term_rows && vt->keep_history && !vt->in_alt_screen)
    {
        mcview_ansi_state_t ansi;
        int used = MAX (mcview_terminal_buffer_max_row (vt->buf), vt->cursor_row) + 1;
        int drop = used - rows;
        int i;

        if (drop < 0)
            drop = 0;

        mcview_ansi_state_init (&ansi);

        for (i = 0; i < drop; i++)
            mcview_vterm_scroll_up (vt, 0, vt->term_rows - 1, &ansi);

        vt->cursor_row -= drop;
        if (vt->cursor_row < 0)
            vt->cursor_row = 0;

        mcview_terminal_buffer_set_max_row (vt->buf,
                                            mcview_terminal_buffer_max_row (vt->buf) - drop);
    }

    if (vt->scroll_bottom == vt->term_rows - 1 || vt->scroll_bottom >= rows)
        vt->scroll_bottom = rows - 1;

    vt->term_rows = rows;
    vt->term_cols = cols;
    vterm_images_erase_rows (vt, rows, G_MAXINT);
    if (vt->scroll_top >= vt->scroll_bottom)
        vt->scroll_top = 0;
    if (vt->cursor_row >= vt->term_rows)
        vt->cursor_row = vt->term_rows - 1;
    if (vt->cursor_col >= vt->term_cols)
        vt->cursor_col = vt->term_cols - 1;
    vt->pending_wrap = FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

vterm_event_t
mcview_vterm_feed (mcview_vterm_t *vt, unsigned char byte)
{
    if (vt->in_dcs)
    {
        if (vt->in_dcs_esc)
        {
            vt->in_dcs_esc = FALSE;
            if (byte == '\\')
            {
                if (vt->in_sixel)
                {
                    vterm_finish_sixel (vt);
                    return vterm_make (vt, VTERM_CONSUMED);
                }

                vt->in_dcs = FALSE;
                vt->dcs_buf[vt->dcs_len] = '\0';

                /* XTGETTCAP asks which terminfo capabilities exist:
                   DCS + q <names> ST. Answer that none are known. */
                if (vt->dcs_len >= 2 && vt->dcs_buf[0] == '+' && vt->dcs_buf[1] == 'q')
                {
                    vterm_event_t ev = vterm_make (vt, VTERM_REPLY);

                    ev.reply = ESC_STR "P0+r" ESC_STR "\\";
                    return ev;
                }
            }
        }
        else if (byte == ESC_CHAR)
            vt->in_dcs_esc = TRUE;
        else if (vt->in_sixel)
        {
            /* Only what sixel is made of goes back out to the real terminal:
               a control code smuggled into the data must not end the DCS
               there and go on as a command. CAN and SUB abort, the way they
               abort any control string; the rest is dropped. */
            if (byte >= 0x20u && byte < 0x7Fu)
            {
                if (vt->sixel->len < MCVIEW_VTERM_MAX_SIXEL_BYTES)
                    g_string_append_c (vt->sixel, (char) byte);
                else
                    vt->sixel_overflow = TRUE;
            }
            else if (byte == 0x18u || byte == 0x1Au)
            {
                vt->in_dcs = FALSE;
                vt->in_sixel = FALSE;
                vt->sixel_overflow = FALSE;
                g_string_truncate (vt->sixel, 0);
            }
        }
        else if (byte == 'q' && vt->dcs_len < (int) sizeof (vt->dcs_buf) - 1)
        {
            /* DCS <numbers> q: sixel. XTGETTCAP is DCS + q and stays where it was. */
            int i;

            for (i = 0; i < vt->dcs_len; i++)
                if ((vt->dcs_buf[i] < '0' || vt->dcs_buf[i] > '9') && vt->dcs_buf[i] != ';')
                    break;
            if (i == vt->dcs_len)
            {
                vt->in_sixel = TRUE;
                vt->sixel_overflow = FALSE;
                g_string_truncate (vt->sixel, 0);
                g_string_append (vt->sixel, ESC_STR "P");
                g_string_append_len (vt->sixel, vt->dcs_buf, vt->dcs_len);
                g_string_append_c (vt->sixel, 'q');
            }
            else
                vt->dcs_buf[vt->dcs_len++] = 'q';
        }
        else if (vt->dcs_len < (int) sizeof (vt->dcs_buf) - 1)
            vt->dcs_buf[vt->dcs_len++] = (char) byte;

        return vterm_make (vt, VTERM_CONSUMED);
    }

    if (vt->in_apc)
    {
        if (vt->in_apc_esc)
        {
            vt->in_apc_esc = FALSE;
            if (byte == '\\')
                return vterm_finish_apc (vt);
        }
        else if (byte == 0x07u)
            return vterm_finish_apc (vt);
        else if (byte == ESC_CHAR)
            vt->in_apc_esc = TRUE;
        else if (vt->apc_len < (int) sizeof (vt->apc_buf) - 1)
            vt->apc_buf[vt->apc_len++] = (char) byte;
        else
            vt->apc_overflow = TRUE;
        return vterm_make (vt, VTERM_CONSUMED);
    }

    if (vt->in_osc)
    {
        if (vt->in_osc_esc)
        {
            vt->in_osc_esc = FALSE;
            if (byte == '\\')
                vterm_finish_osc (vt);
        }
        else if (byte == 0x07u)
        {
            vterm_finish_osc (vt);
        }
        else if (byte == ESC_CHAR)
        {
            vt->in_osc_esc = TRUE;
        }
        else if (vt->osc_len < (int) sizeof (vt->osc_buf) - 1)
        {
            vt->osc_buf[vt->osc_len++] = (char) byte;
        }
        else
        {
            vt->osc_overflow = TRUE;
        }
        return vterm_make (vt, VTERM_CONSUMED);
    }

    /* ESC ( B or similar: consume one more byte. */
    if (vt->in_esc_char)
    {
        vt->in_esc_char = FALSE;
        if (vt->esc_charset_slot >= 0)
            vt->charset[vt->esc_charset_slot] = byte;
        mcview_ansi_parse_char (&vt->ansi, (int) byte);
        return vterm_make (vt, VTERM_CONSUMED);
    }

    mcview_ansi_parse_char (&vt->ansi, (int) byte);

    if (vt->saw_esc && byte != ESC_CHAR)
    {
        vt->saw_esc = FALSE;

        if (byte == 'P')
        {
            vt->in_dcs = TRUE;
            vt->in_dcs_esc = FALSE;
            vt->in_sixel = FALSE;
            vt->dcs_len = 0;
        }
        else if (byte == '[')
        {
            vt->in_csi = TRUE;
            vt->csi_private = FALSE;
            vt->csi_gt = FALSE;
            vt->param_count = 0;
            vt->current_param = 0;
            vt->has_current = FALSE;
        }
        else if (byte == '_')
        {
            vt->in_apc = TRUE;
            vt->in_apc_esc = FALSE;
            vt->apc_overflow = FALSE;
            vt->apc_len = 0;
        }
        else if (byte == ']')
        {
            vt->in_osc = TRUE;
            vt->in_osc_esc = FALSE;
            vt->osc_overflow = FALSE;
            vt->osc_len = 0;
        }
        else if (byte == '(' || byte == ')' || byte == '*' || byte == '+')
        {
            /* Charset designation: the next byte says which set the slot holds. */
            vt->in_esc_char = TRUE;
            vt->esc_charset_slot = (byte == '(') ? 0 : (byte == ')') ? 1 : (byte == '*') ? 2 : 3;
        }
        else if (byte == 'M')
        {
            /* RI -- Reverse Index: scroll region down, move cursor up */
            return vterm_make (vt, VTERM_RI);
        }
        return vterm_make (vt, VTERM_CONSUMED);
    }

    if (byte == ESC_CHAR)
    {
        vt->saw_esc = TRUE;
        vt->in_csi = FALSE; /* ESC resets any in-progress CSI */
        vt->utf8_len = 0;
        vt->utf8_expected = 0;
        return vterm_make (vt, VTERM_CONSUMED);
    }

    if (vt->in_csi)
    {
        if (byte >= '0' && byte <= '9')
        {
            if (vt->current_param <= 65535)
                vt->current_param = vt->current_param * 10 + (byte - '0');
            vt->has_current = TRUE;
            return vterm_make (vt, VTERM_CONSUMED);
        }

        if (byte == ';' || byte == ':')
        {
            vterm_finalize_param (vt);
            return vterm_make (vt, VTERM_CONSUMED);
        }

        if (byte == '?')
        {
            vt->csi_private = TRUE;
            return vterm_make (vt, VTERM_CONSUMED);
        }

        if (byte == '>')
        {
            vt->csi_gt = TRUE;
            return vterm_make (vt, VTERM_CONSUMED);
        }

        if (byte >= 0x40u && byte <= 0x7Eu)
        {
            vterm_finalize_param (vt);
            vt->in_csi = FALSE;
            return vterm_dispatch_csi (vt, byte);
        }

        return vterm_make (vt, VTERM_CONSUMED);
    }

    if (byte == 0x0Eu) /* SO: print from G1 */
    {
        vt->charset_gl = 1;
        return vterm_make (vt, VTERM_CONSUMED);
    }
    if (byte == 0x0Fu) /* SI: print from G0 again */
    {
        vt->charset_gl = 0;
        return vterm_make (vt, VTERM_CONSUMED);
    }
    if (byte == '\r')
        return vterm_make (vt, VTERM_CR);
    if (byte == '\n')
        return vterm_make (vt, VTERM_LF);
    if (byte == '\b') /* BS (0x08): move cursor left */
        return vterm_make (vt, VTERM_CURSOR_BACK);
    if (byte == '\t')
    {
        vterm_event_t ev = vterm_make (vt, VTERM_CURSOR_FWD);
        ev.param1 = 8 - (vt->cursor_col % 8);
        return ev;
    }
    if (byte < 0x20)
        return vterm_make (vt, VTERM_CONSUMED);
    return vterm_handle_utf8 (vt, byte);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_apply_event (mcview_vterm_t *vt, const vterm_event_t *ev)
{
    /* Whatever moves or clears takes the pending wrap with it; only a printed
       character makes it happen. */
    if (ev->type != VTERM_CHAR && ev->type != VTERM_SGR && ev->type != VTERM_CONSUMED
        && ev->type != VTERM_REPLY)
        vt->pending_wrap = FALSE;

    switch (ev->type)
    {
    case VTERM_CHAR:
    {
        const int width = g_unichar_iswide (ev->ch) ? 2 : 1;

        if (vt->pending_wrap)
        {
            mcview_terminal_buffer_set_wrapped (vt->buf, vt->cursor_row, TRUE);
            vt->pending_wrap = FALSE;
            vterm_linefeed (vt, &ev->ansi);
        }
        // Both halves of a wide character on one row, as xterm does it.
        if (width == 2 && vt->autowrap && vt->cursor_col > 0 && vt->cursor_col + 1 >= vt->term_cols)
        {
            mcview_terminal_buffer_set_wrapped (vt->buf, vt->cursor_row, TRUE);
            vterm_linefeed (vt, &ev->ansi);
            vt->cursor_col = 0;
        }
        if (vt->insert_mode)
            mcview_terminal_buffer_insert_chars (vt->buf, vt->cursor_row, vt->cursor_col, width,
                                                 vt->term_cols, &ev->ansi);
        if (vt->cursor_col < MCVIEW_VTERM_MAX_CANVAS_COLS)
        {
            vterm_split_wide (vt, vt->cursor_row, vt->cursor_col, width, &ev->ansi);
            mcview_terminal_buffer_put_char (vt->buf, vt->cursor_row, vt->cursor_col, ev->ch,
                                             &ev->ansi);
            if (width == 2 && vt->cursor_col + 1 < MCVIEW_VTERM_MAX_CANVAS_COLS)
                mcview_terminal_buffer_put_char (vt->buf, vt->cursor_row, vt->cursor_col + 1,
                                                 MCVIEW_VTERM_WIDE_TAIL, &ev->ansi);
        }
        vt->cursor_col += width;
        if (vt->autowrap && vt->cursor_col >= vt->term_cols)
        {
            vt->cursor_col = vt->term_cols - 1;
            vt->pending_wrap = TRUE;
        }
        vt->new_chars_since_snapshot = TRUE;
    }
    break;

    case VTERM_CR:
        vt->cursor_col = 0;
        break;

    case VTERM_LF:
        vterm_linefeed (vt, &ev->ansi);
        break;

    case VTERM_CURSOR_ABS:
    {
        int new_row = (ev->param1 >= 0) ? ev->param1 : 0;
        int new_col = (ev->param2 >= 0) ? ev->param2 : 0;

        if (new_row == 0 && vt->new_chars_since_snapshot
            && mcview_terminal_buffer_max_row (vt->buf) >= 0
            && mcview_terminal_buffer_max_row (vt->buf) < MCVIEW_VTERM_MAX_CANVAS_ROWS)
        {
            if (vt->in_alt_screen)
            {
                mcview_terminal_buffer_free (vt->alt_frame_buf);
                vt->alt_frame_buf = mcview_terminal_buffer_copy (vt->buf);
            }
            else
            {
                mcview_terminal_buffer_free (vt->snapshot_buf);
                vt->snapshot_buf = mcview_terminal_buffer_copy (vt->buf);
                vt->snapshot_cursor_row = vt->cursor_row;
                vt->snapshot_cursor_col = vt->cursor_col;
            }
            vt->new_chars_since_snapshot = FALSE;
        }

        vt->cursor_row = MIN (new_row, vt->term_rows - 1);
        vt->cursor_col = MIN (new_col, vt->term_cols - 1);
    }
    break;

    case VTERM_CURSOR_FWD:
        vt->cursor_col += (ev->param1 > 0) ? ev->param1 : 1;
        if (vt->cursor_col >= vt->term_cols)
            vt->cursor_col = vt->term_cols - 1;
        break;

    case VTERM_CURSOR_BACK:
    {
        int n = (ev->param1 > 0) ? ev->param1 : 1;
        vt->cursor_col -= n;
        if (vt->cursor_col < 0)
            vt->cursor_col = 0;
    }
    break;

    case VTERM_CURSOR_UP:
        vt->cursor_row -= ev->param1;
        if (vt->cursor_row < 0)
            vt->cursor_row = 0;
        break;

    case VTERM_CURSOR_DOWN:
        vt->cursor_row += ev->param1;
        if (vt->cursor_row >= vt->term_rows)
            vt->cursor_row = vt->term_rows - 1;
        break;

    case VTERM_ERASE_EOL:
        mcview_terminal_buffer_erase_eol (vt->buf, vt->cursor_row, vt->cursor_col, vt->term_cols,
                                          &ev->ansi);
        break;

    case VTERM_ERASE_BOL:
        mcview_terminal_buffer_erase_bol (vt->buf, vt->cursor_row, vt->cursor_col, vt->term_cols,
                                          &ev->ansi);
        break;

    case VTERM_ERASE_LINE:
        mcview_terminal_buffer_erase_line (vt->buf, vt->cursor_row, vt->term_cols, &ev->ansi);
        break;

    case VTERM_ERASE_SCREEN:
        if (vt->new_chars_since_snapshot && mcview_terminal_buffer_max_row (vt->buf) >= 0
            && mcview_terminal_buffer_max_row (vt->buf) < MCVIEW_VTERM_MAX_CANVAS_ROWS)
        {
            if (vt->in_alt_screen)
            {
                mcview_terminal_buffer_free (vt->alt_frame_buf);
                vt->alt_frame_buf = mcview_terminal_buffer_copy (vt->buf);
            }
            else
            {
                mcview_terminal_buffer_free (vt->snapshot_buf);
                vt->snapshot_buf = mcview_terminal_buffer_copy (vt->buf);
                vt->snapshot_cursor_row = vt->cursor_row;
                vt->snapshot_cursor_col = vt->cursor_col;
            }
        }
        vt->new_chars_since_snapshot = FALSE;
        mcview_terminal_buffer_clear (vt->buf);
        vterm_images_clear (vt);
        vt->cursor_row = 0;
        vt->cursor_col = 0;
        break;

    case VTERM_ERASE_TO_EOS:
    {
        int r;
        /* Erase from cursor to end of current line, then clear all rows below. */
        mcview_terminal_buffer_erase_eol (vt->buf, vt->cursor_row, vt->cursor_col, vt->term_cols,
                                          &ev->ansi);
        for (r = vt->cursor_row + 1; r <= mcview_terminal_buffer_max_row (vt->buf); r++)
            mcview_terminal_buffer_erase_line (vt->buf, r, vt->term_cols, &ev->ansi);
        vterm_images_erase_rows (vt, vt->cursor_row, G_MAXINT);
        break;
    }

    case VTERM_ERASE_TO_BOS:
    {
        int r;
        /* Clear all rows above cursor, then erase from start of current line to cursor. */
        for (r = 0; r < vt->cursor_row; r++)
            mcview_terminal_buffer_erase_line (vt->buf, r, vt->term_cols, &ev->ansi);
        mcview_terminal_buffer_erase_bol (vt->buf, vt->cursor_row, vt->cursor_col, vt->term_cols,
                                          &ev->ansi);
        vterm_images_erase_rows (vt, 0, vt->cursor_row);
        break;
    }

    case VTERM_ALT_SCREEN_ENTER:
        /* Snapshot the main screen so it can be restored on exit (mcterm use case).
         * alt_frame_buf will track the last complete TUI frame inside this bracket. */
        if (mcview_terminal_buffer_max_row (vt->buf) < MCVIEW_VTERM_MAX_CANVAS_ROWS)
        {
            mcview_terminal_buffer_free (vt->snapshot_buf);
            vt->snapshot_buf = mcview_terminal_buffer_copy (vt->buf);
            vt->snapshot_cursor_row = vt->cursor_row;
            vt->snapshot_cursor_col = vt->cursor_col;
        }
        mcview_terminal_buffer_free (vt->alt_frame_buf);
        vt->alt_frame_buf = NULL;
        vt->in_alt_screen = TRUE;
        vt->new_chars_since_snapshot = FALSE;
        /* The pictures belong to the main screen, and there is no bringing
           them back: the terminal has forgotten them too. */
        vterm_images_clear (vt);
        break;

    case VTERM_ALT_SCREEN_EXIT:
        if (vt->snapshot_buf != NULL)
        {
            if (mcview_terminal_buffer_max_row (vt->snapshot_buf) >= 0)
            {
                /* Main screen had content spanning multiple rows: restore it
                 * (shell->app->shell flow).  Single-row noise (debug output,
                 * init sequences) does not qualify -- use alt_frame_buf instead. */
                mcview_terminal_buffer_free (vt->buf);
                mcview_terminal_buffer_free (vt->alt_frame_buf);
                vt->alt_frame_buf = NULL;
                vt->buf = vt->snapshot_buf;
                vt->snapshot_buf = NULL;
                vt->cursor_row = vt->snapshot_cursor_row;
                vt->cursor_col = vt->snapshot_cursor_col;
            }
            else if (vt->alt_frame_buf != NULL)
            {
                /* Main screen was empty but we captured a frame inside alt-screen
                 * (viewer mode: log starts with app).  Show that last frame. */
                mcview_terminal_buffer_free (vt->buf);
                mcview_terminal_buffer_free (vt->snapshot_buf);
                vt->snapshot_buf = NULL;
                vt->buf = vt->alt_frame_buf;
                vt->alt_frame_buf = NULL;
            }
            else if (vt->new_chars_since_snapshot)
            {
                /* No frame snapshot yet (app never did cursor-home) but chars were
                 * written: keep current buf (htop.log-style: exit erases only last row). */
                mcview_terminal_buffer_free (vt->snapshot_buf);
                vt->snapshot_buf = NULL;
            }
            else
            {
                /* Empty alt-screen bracket (init noise): clear to avoid artifacts. */
                mcview_terminal_buffer_free (vt->snapshot_buf);
                vt->snapshot_buf = NULL;
                mcview_terminal_buffer_clear (vt->buf);
                vt->cursor_row = 0;
                vt->cursor_col = 0;
            }
        }
        else
        {
            vt->cursor_row = 0;
            vt->cursor_col = 0;
        }
        vt->in_alt_screen = FALSE;
        vt->new_chars_since_snapshot = FALSE;
        vterm_images_clear (vt);
        break;

    case VTERM_SET_SCROLL_REGION:
        vt->scroll_top = ev->param1;
        vt->scroll_bottom = ev->param2;
        vt->cursor_row = 0;
        vt->cursor_col = 0;
        break;

    case VTERM_CURSOR_ROW_ABS:
    {
        int row = ev->param1;
        if (row < 0)
            row = 0;
        if (row >= vt->term_rows)
            row = vt->term_rows - 1;
        vt->cursor_row = row;
    }
    break;

    case VTERM_ERASE_CHARS:
    {
        int count = (ev->param1 > 0) ? ev->param1 : 1;
        int col_to = vt->cursor_col + count - 1;
        if (col_to >= vt->term_cols)
            col_to = vt->term_cols - 1;
        mcview_terminal_buffer_fill_range (vt->buf, vt->cursor_row, vt->cursor_col, col_to, ' ',
                                           &ev->ansi);
    }
    break;

    case VTERM_DCH:
        mcview_terminal_buffer_delete_chars (vt->buf, vt->cursor_row, vt->cursor_col, ev->param1,
                                             vt->term_cols, &ev->ansi);
        break;

    case VTERM_ICH:
        mcview_terminal_buffer_insert_chars (vt->buf, vt->cursor_row, vt->cursor_col, ev->param1,
                                             vt->term_cols, &ev->ansi);
        break;

    case VTERM_RI:
        if (vt->cursor_row > vt->scroll_top)
            vt->cursor_row--;
        else
        {
            mcview_terminal_buffer_scroll_down (vt->buf, vt->scroll_top, vt->scroll_bottom,
                                                vt->term_cols, &ev->ansi);
            vterm_images_shift (vt, vt->scroll_top, vt->scroll_bottom, 1);
        }
        break;

    case VTERM_SGR:
    case VTERM_CONSUMED:
    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_set_cell_size (mcview_vterm_t *vt, int width, int height)
{
    vt->cell_width = (width > 0) ? width : MCVIEW_VTERM_DEFAULT_CELL_WIDTH;
    vt->cell_height = (height > 0) ? height : MCVIEW_VTERM_DEFAULT_CELL_HEIGHT;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_set_sixel (mcview_vterm_t *vt, gboolean sixel)
{
    vt->sixel_capable = sixel;
}

/* --------------------------------------------------------------------------------------------- */

guint
mcview_vterm_images_len (const mcview_vterm_t *vt)
{
    return (vt->images != NULL) ? vt->images->len : 0;
}

/* --------------------------------------------------------------------------------------------- */

const mcview_vterm_image_t *
mcview_vterm_image (const mcview_vterm_t *vt, guint index)
{
    if (vt->images == NULL || index >= vt->images->len)
        return NULL;

    return (const mcview_vterm_image_t *) g_ptr_array_index (vt->images, index);
}

/* --------------------------------------------------------------------------------------------- */

guint
mcview_vterm_images_generation (const mcview_vterm_t *vt)
{
    return vt->images_generation;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_vterm_cursor_row (const mcview_vterm_t *vt)
{
    return vt->cursor_row;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_vterm_cursor_col (const mcview_vterm_t *vt)
{
    return vt->cursor_col;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_vterm_in_alt_screen (const mcview_vterm_t *vt)
{
    return vt->in_alt_screen;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_vterm_app_cursor_keys (const mcview_vterm_t *vt)
{
    return vt->app_cursor_keys;
}

/* --------------------------------------------------------------------------------------------- */

mcview_terminal_buffer_t *
mcview_vterm_buf (mcview_vterm_t *vt)
{
    return vt->buf;
}

/* --------------------------------------------------------------------------------------------- */

off_t
mcview_vterm_replay_offset (const mcview_vterm_t *vt)
{
    return vt->replay_offset;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_set_replay_offset (mcview_vterm_t *vt, off_t offset)
{
    vt->replay_offset = offset;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_vterm_dpy_top_row (const mcview_vterm_t *vt)
{
    return vt->dpy_top_row;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_set_dpy_top_row (mcview_vterm_t *vt, int row)
{
    /* MCVIEW_VTERM_FOLLOW_END (-1) is a valid sentinel; clamp anything below it. */
    vt->dpy_top_row = (row < MCVIEW_VTERM_FOLLOW_END) ? MCVIEW_VTERM_FOLLOW_END : row;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_vterm_resolve_top_row (const mcview_vterm_t *vt, int data_lines)
{
    int top = vt->dpy_top_row;
    int max;

    if (top >= 0)
        return top;

    /* FOLLOW_END: pin the viewport to the bottom of the canvas. */
    max = mcview_terminal_buffer_max_row (vt->buf);
    if (max < 0)
        return 0;
    top = max - data_lines + 1;
    return (top > 0) ? top : 0;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_vterm_resolve_scrollback_top_row (const mcview_vterm_t *vt, int data_lines)
{
    int top = vt->dpy_top_row;
    int live_rows;
    int max_top;

    live_rows = MAX (mcview_terminal_buffer_max_row (vt->buf), vt->cursor_row) + 1;
    max_top = MAX (mcview_vterm_history_len (vt) + live_rows - data_lines, 0);

    if (top < 0)
        return max_top;

    return MIN (top, max_top);
}

/* --------------------------------------------------------------------------------------------- */

mcview_terminal_buffer_t *
mcview_vterm_compose_scrollback (const mcview_vterm_t *vt, int top_row, int rows)
{
    mcview_terminal_buffer_t *canvas;
    int history_rows;
    int row;

    canvas = mcview_terminal_buffer_new ();
    history_rows = mcview_vterm_history_len (vt);

    for (row = 0; row < rows; row++)
    {
        int source_row = top_row + row;

        if (source_row < history_rows)
        {
            const GArray *history_row = mcview_vterm_history_row (vt, source_row);

            if (history_row != NULL)
                mcview_terminal_buffer_set_row (canvas, row, history_row);
        }
        else
        {
            GArray *cells = mcview_terminal_buffer_row_copy (vt->buf, source_row - history_rows);

            if (cells != NULL)
            {
                mcview_terminal_buffer_set_row (canvas, row, cells);
                g_array_unref (cells);
            }
        }
    }

    return canvas;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_page_up (mcview_vterm_t *vt, int keep)
{
    mcview_ansi_state_t ansi;
    GPtrArray *kept;
    int first, i;

    if (vt == NULL || vt->in_alt_screen)
        return;

    keep = CLAMP (keep, 1, vt->cursor_row + 1);
    first = vt->cursor_row - keep + 1;
    mcview_ansi_state_init (&ansi);

    // The kept rows leave the screen before it goes: the history is not to have them.
    kept = g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);
    for (i = first; i <= vt->cursor_row; i++)
    {
        GArray *cells = mcview_terminal_buffer_row_copy (vt->buf, i);

        if (cells == NULL)
            cells = g_array_new (FALSE, TRUE, sizeof (mcview_vterm_cell_t));
        g_ptr_array_add (kept, cells);
        mcview_terminal_buffer_erase_line (vt->buf, i, vt->term_cols, &ansi);
    }

    for (i = 0; i < vt->term_rows; i++)
        mcview_vterm_scroll_up (vt, 0, vt->term_rows - 1, &ansi);

    mcview_terminal_buffer_clear (vt->buf);
    for (i = 0; i < keep; i++)
    {
        const GArray *cells = g_ptr_array_index (kept, i);

        if (cells->len > 0)
            mcview_terminal_buffer_set_row (vt->buf, vt->term_rows - keep + i, cells);
    }
    g_ptr_array_free (kept, TRUE);

    vt->cursor_row = vt->term_rows - 1;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_clear_history (mcview_vterm_t *vt)
{
    if (vt == NULL || vt->history == NULL)
        return;

    g_ptr_array_set_size (vt->history, 0);
    g_array_set_size (vt->history_wrapped, 0);
    vt->history_cells = 0;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mcview_vterm_osc7_raw (const mcview_vterm_t *vt)
{
    return (vt != NULL) ? vt->osc7_raw : NULL;
}

/* --------------------------------------------------------------------------------------------- */

const char *
mcview_vterm_osc133_raw (const mcview_vterm_t *vt)
{
    return (vt != NULL) ? vt->osc133_raw : NULL;
}

/* --------------------------------------------------------------------------------------------- */

guint
mcview_vterm_osc133_generation (const mcview_vterm_t *vt)
{
    return (vt != NULL) ? vt->osc133_generation : 0;
}

/* --------------------------------------------------------------------------------------------- */

guint
mcview_vterm_osc7_generation (const mcview_vterm_t *vt)
{
    return (vt != NULL) ? vt->osc7_generation : 0;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_set_far2l (mcview_vterm_t *vt, gboolean allowed)
{
    vt->far2l_allowed = allowed;
    if (!allowed)
        vt->far2l_active = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_vterm_far2l_active (const mcview_vterm_t *vt)
{
    return vt->far2l_active;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_vterm_restore_sync_snapshot (mcview_vterm_t *vt, mcview_terminal_buffer_t *snap_buf,
                                    int snap_cursor_row)
{
    mcview_ansi_state_t blank_ansi;

    /* Initialised, not zeroed: zero is the colour black, and the row wants the
       colours of whoever draws it, which is what the default stands for. */
    mcview_ansi_state_init (&blank_ansi);
    mcview_terminal_buffer_erase_line (snap_buf, snap_cursor_row, vt->term_cols, &blank_ansi);

    mcview_terminal_buffer_free (vt->buf);
    vt->buf = snap_buf; /* takes ownership */
    vt->cursor_row = snap_cursor_row;
    vt->cursor_col = 0;
}

/* --------------------------------------------------------------------------------------------- */
