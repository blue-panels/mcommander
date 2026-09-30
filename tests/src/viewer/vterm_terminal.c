/*
   src/viewer - vterm terminal-mode unit tests

   Regression tests for scroll-region semantics (DECSTBM), VPA, ECH,
   colored erase, cursor clamping, and size-aware reset.

   Copyright (C) 2026
   Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "/src/viewer/vterm_terminal"

#include "tests/mctest.h"

#include "src/viewer/ansi.h"
#include "src/viewer/terminal_buffer.h"
#include "src/viewer/vterm.h"

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

static void
feed_bytes (mcview_vterm_t *vt, const char *data, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++)
    {
        vterm_event_t ev = mcview_vterm_feed (vt, (unsigned char) data[i]);
        mcview_vterm_apply_event (vt, &ev);
    }
}

/* --------------------------------------------------------------------------------------------- */

#define FEED(vt, s) feed_bytes ((vt), (s), sizeof (s) - 1)

/* --------------------------------------------------------------------------------------------- */

static gunichar
cell_ch (mcview_vterm_t *vt, int row, int col)
{
    const mcview_vterm_cell_t *cell;

    cell = mcview_terminal_buffer_get (mcview_vterm_buf (vt), row, col);
    return (cell != NULL) ? cell->ch : 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
cell_bg (mcview_vterm_t *vt, int row, int col)
{
    const mcview_vterm_cell_t *cell;

    cell = mcview_terminal_buffer_get (mcview_vterm_buf (vt), row, col);
    return (cell != NULL) ? cell->attr.bg : MCVIEW_ANSI_COLOR_DEFAULT;
}

/* --------------------------------------------------------------------------------------------- */

static const mcview_vterm_cell_t *
cell_at (mcview_vterm_t *vt, int row, int col)
{
    return mcview_terminal_buffer_get (mcview_vterm_buf (vt), row, col);
}

/* --------------------------------------------------------------------------------------------- */

static char *
canvas_to_text (mcview_vterm_t *vt, int rows, int cols)
{
    GString *s;
    int row, col;

    s = g_string_new ("");
    for (row = 0; row < rows; row++)
    {
        for (col = 0; col < cols; col++)
        {
            gunichar ch = cell_ch (vt, row, col);
            g_string_append_c (s, ch ? (char) ch : ' ');
        }
        g_string_append_c (s, '\n');
    }
    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static char *
buffer_to_text (const mcview_terminal_buffer_t *buffer, int rows, int cols)
{
    GString *s;
    int row, col;

    s = g_string_new ("");
    for (row = 0; row < rows; row++)
    {
        for (col = 0; col < cols; col++)
        {
            const mcview_vterm_cell_t *cell = mcview_terminal_buffer_get (buffer, row, col);

            g_string_append_c (s, cell != NULL && cell->ch != 0 ? (char) cell->ch : ' ');
        }
        g_string_append_c (s, '\n');
    }
    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
feed_with_sync_restore (mcview_vterm_t *vt, const char *data, mcview_terminal_buffer_t **snap_buf,
                        int snap_cursor_row, guint *last_osc7_gen)
{
    size_t i;

    for (i = 0; data[i] != '\0'; i++)
    {
        vterm_event_t ev = mcview_vterm_feed (vt, (unsigned char) data[i]);

        mcview_vterm_apply_event (vt, &ev);
        if (*snap_buf != NULL && mcview_vterm_osc7_generation (vt) != *last_osc7_gen)
        {
            *last_osc7_gen = mcview_vterm_osc7_generation (vt);
            mcview_vterm_restore_sync_snapshot (vt, *snap_buf, snap_cursor_row);
            *snap_buf = NULL;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */
/* Tests *****************************************************************************************/
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_scroll_region_lf_at_bottom_scrolls_up)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 20);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[2;4r"); /* DECSTBM 1-based 2..4 = 0-based 1..3 */
    FEED (vt, "\033[2;1H");
    FEED (vt, "A");
    FEED (vt, "\033[3;1H");
    FEED (vt, "B");
    FEED (vt, "\033[4;1H");
    FEED (vt, "C");
    FEED (vt, "\033[4;1H");
    FEED (vt, "\n");

    ck_assert_uint_eq (cell_ch (vt, 1, 0), 'B');
    ck_assert_uint_eq (cell_ch (vt, 2, 0), 'C');
    ck_assert_uint_eq (cell_ch (vt, 3, 0), ' ');
    ck_assert_uint_eq (cell_ch (vt, 0, 0), 0);
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 3);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_scroll_region_lf_above_region_advances_cursor)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 20);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[2;4r");
    FEED (vt, "\033[2;1H");
    FEED (vt, "X");
    FEED (vt, "\033[1;1H");
    FEED (vt, "\n");

    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 1);
    ck_assert_uint_eq (cell_ch (vt, 1, 0), 'X');

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_vpa_moves_row_only)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 10, 40);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[7C"); /* forward 7 -> col 7 */
    FEED (vt, "\033[4d"); /* VPA row 4 (1-based) = row 3 */

    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 3);
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 7);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_ech_erases_with_attrs_cursor_stays)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 20);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[H");
    FEED (vt, "XYZ");
    FEED (vt, "\033[H");
    FEED (vt, "\033[42m"); /* green bg */
    FEED (vt, "\033[2X");  /* erase 2 chars */

    ck_assert_uint_eq (cell_ch (vt, 0, 0), ' ');
    ck_assert_int_eq (cell_bg (vt, 0, 0), 2);
    ck_assert_uint_eq (cell_ch (vt, 0, 1), ' ');
    ck_assert_int_eq (cell_bg (vt, 0, 1), 2);
    ck_assert_uint_eq (cell_ch (vt, 0, 2), 'Z');
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 0);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_ich_shifts_the_tail_right)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 8);
    mcview_vterm_reset (vt);

    /* readline inserting "222" in front of "11111": ICH makes room, then the text is written */
    FEED (vt, "\033[H");
    FEED (vt, "11111");
    FEED (vt, "\033[H");
    FEED (vt, "\033[3@");
    FEED (vt, "222");

    ck_assert_uint_eq (cell_ch (vt, 0, 0), '2');
    ck_assert_uint_eq (cell_ch (vt, 0, 2), '2');
    ck_assert_uint_eq (cell_ch (vt, 0, 3), '1');
    ck_assert_uint_eq (cell_ch (vt, 0, 7), '1');
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 3);

    /* a shift past the right edge drops what does not fit */
    FEED (vt, "\033[7G");
    FEED (vt, "\033[20@");
    ck_assert_uint_eq (cell_ch (vt, 0, 6), ' ');
    ck_assert_uint_eq (cell_ch (vt, 0, 7), ' ');
    ck_assert_uint_eq (cell_ch (vt, 0, 5), '1');

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_insert_mode_shifts_the_tail_right)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 8);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[H");
    FEED (vt, "11111");
    FEED (vt, "\033[H");
    FEED (vt, "\033[4h");
    FEED (vt, "222");
    FEED (vt, "\033[4l");
    FEED (vt, "X");

    ck_assert_uint_eq (cell_ch (vt, 0, 0), '2');
    ck_assert_uint_eq (cell_ch (vt, 0, 2), '2');
    ck_assert_uint_eq (cell_ch (vt, 0, 3), 'X');
    ck_assert_uint_eq (cell_ch (vt, 0, 4), '1');
    ck_assert_uint_eq (cell_ch (vt, 0, 7), '1');
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 4);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_erase_eol_fills_full_width_with_attrs)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 10);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[43m"); /* yellow bg */
    FEED (vt, "\033[H");
    FEED (vt, "\033[K");

    ck_assert_uint_eq (cell_ch (vt, 0, 0), ' ');
    ck_assert_int_eq (cell_bg (vt, 0, 0), 3);
    ck_assert_uint_eq (cell_ch (vt, 0, 9), ' ');
    ck_assert_int_eq (cell_bg (vt, 0, 9), 3);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_decstbm_out_of_range_bottom_is_clamped)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 20);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[1;999r");
    FEED (vt, "\033[5;1H");
    FEED (vt, "Q");
    FEED (vt, "\033[5;1H");
    FEED (vt, "\n");

    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 4);
    ck_assert_uint_eq (cell_ch (vt, 3, 0), 'Q');

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cursor_fwd_clamps_to_term_cols)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 10);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[999C");

    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 9);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cursor_abs_col_clamps_to_term_cols)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 10);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[1;999H");

    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 9);
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 0);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cursor_abs_row_clamps_to_term_rows)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 10);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[999;1H");

    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 4);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cursor_down_clamps_to_term_rows)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 10);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[999B");

    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 4);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_erase_bol_does_not_expand_past_term_cols)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 10);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[H");
    FEED (vt, "0123456789");
    FEED (vt, "\033[999C");

    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 9);

    FEED (vt, "\033[1K");

    ck_assert_uint_eq (cell_ch (vt, 0, 0), ' ');
    ck_assert_uint_eq (cell_ch (vt, 0, 9), ' ');
    ck_assert_int_eq (mcview_terminal_buffer_max_row (mcview_vterm_buf (vt)), 0);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sync_snapshot_split_osc7_and_prompt)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    mcview_terminal_buffer_t *snap_buf;
    int snap_cursor_row;
    guint last_osc7_gen;
    char *got;

    mcview_vterm_set_size (vt, 6, 40);
    mcview_vterm_reset (vt);

    FEED (vt, "old output\r\nold:/a$ ");
    snap_buf = mcview_terminal_buffer_copy (mcview_vterm_buf (vt));
    snap_cursor_row = mcview_vterm_cursor_row (vt);
    last_osc7_gen = mcview_vterm_osc7_generation (vt);

    FEED (vt, "cd /b\r\n");
    feed_with_sync_restore (vt, "\033]7;file:///b\007", &snap_buf, snap_cursor_row, &last_osc7_gen);
    FEED (vt, "new:/b$ ");

    got = canvas_to_text (vt, 3, 16);
    ck_assert_str_eq (got,
                      "old output      \n"
                      "new:/b$         \n"
                      "                \n");
    g_free (got);
    ck_assert_ptr_null (snap_buf);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sync_snapshot_batched_osc7_and_prompt)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    mcview_terminal_buffer_t *snap_buf;
    int snap_cursor_row;
    guint last_osc7_gen;
    char *got;

    mcview_vterm_set_size (vt, 6, 40);
    mcview_vterm_reset (vt);

    FEED (vt, "old output\r\nold:/a$ ");
    snap_buf = mcview_terminal_buffer_copy (mcview_vterm_buf (vt));
    snap_cursor_row = mcview_vterm_cursor_row (vt);
    last_osc7_gen = mcview_vterm_osc7_generation (vt);

    feed_with_sync_restore (vt, "cd /b\r\n\033]7;file:///b\007new:/b$ ", &snap_buf, snap_cursor_row,
                            &last_osc7_gen);

    got = canvas_to_text (vt, 3, 16);
    ck_assert_str_eq (got,
                      "old output      \n"
                      "new:/b$         \n"
                      "                \n");
    g_free (got);
    ck_assert_ptr_null (snap_buf);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* mcview_vterm_set_size returns TRUE when size changes */

START_TEST (test_set_size_returns_true_on_change)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mctest_assert_true (mcview_vterm_set_size (vt, 44, 187));

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* mcview_vterm_set_size returns FALSE when size is unchanged */

START_TEST (test_set_size_returns_false_on_same_size)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 24, 80);
    mctest_assert_false (mcview_vterm_set_size (vt, 24, 80));

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Golden test: cursor moves, erases, scroll region, colored background.
 *
 * Terminal 6x12.  Operations in order:
 *   - write header/footer with CUP
 *   - DECSTBM restricts scroll to rows 1..4
 *   - fill rows 1..4, then LF at scroll_bottom shifts content up
 *   - navigate with CURSOR_UP/DOWN/FWD, overwrite, erase EOL with red bg
 *
 * Expected canvas after all operations:
 *   ============
 *   BBBB
 *   CCCC **[red]
 *   DDDXX  !
 *   (blank, vacated by scroll)
 *   ------------
 */

START_TEST (test_golden_draw_move_erase)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    char *got;

    mcview_vterm_set_size (vt, 6, 12);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[1;1H");
    FEED (vt, "============");

    FEED (vt, "\033[6;1H");
    FEED (vt, "------------");

    /* scroll region 0-based 1..4 = 1-based 2..5 */
    FEED (vt, "\033[2;5r");

    FEED (vt, "\033[2;1H");
    FEED (vt, "AAAA");
    FEED (vt, "\033[3;1H");
    FEED (vt, "BBBB");
    FEED (vt, "\033[4;1H");
    FEED (vt, "CCCC");
    FEED (vt, "\033[5;1H");
    FEED (vt, "DDDD");

    /* LF at scroll_bottom: region shifts up, row 4 cleared */
    FEED (vt, "\033[5;1H");
    FEED (vt, "\n");

    /* cursor: (4,0) -- up 2 -> row 2, fwd 5 -> col 5 */
    FEED (vt, "\033[2A");
    FEED (vt, "\033[5C");
    FEED (vt, "**");
    /* erase rest of row 2 with red background */
    FEED (vt, "\033[41m");
    FEED (vt, "\033[K");
    FEED (vt, "\033[m");

    /* cursor: (2,7) -- down 1 -> row 3 */
    FEED (vt, "\033[B");
    FEED (vt, "!"); /* col 7 */
    FEED (vt, "\r");
    FEED (vt, "\033[3C"); /* fwd 3 -> col 3 */
    FEED (vt, "XX");      /* overwrite cols 3,4 */

    /* cursor: (3,5) -- up 2 -> row 1, erase EOL */
    FEED (vt, "\033[2A");
    FEED (vt, "\033[K");

    got = canvas_to_text (vt, 6, 12);
    ck_assert_str_eq (got,
                      "============\n"
                      "BBBB        \n"
                      "CCCC **     \n"
                      "DDDXX  !    \n"
                      "            \n"
                      "------------\n");
    g_free (got);

    /* cols 7..11 on row 2 carry red background from the colored erase */
    ck_assert_int_eq (cell_bg (vt, 2, 7), 1);
    ck_assert_int_eq (cell_bg (vt, 2, 11), 1);
    /* cols before the erase have default background */
    ck_assert_int_eq (cell_bg (vt, 2, 6), MCVIEW_ANSI_COLOR_DEFAULT);

    /* header and footer are outside scroll region -- must be untouched */
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 1);
    ck_assert_uint_eq (cell_ch (vt, 0, 0), '=');
    ck_assert_uint_eq (cell_ch (vt, 5, 0), '-');

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_scrollback_canvas_preserves_output_from_top)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    mcview_terminal_buffer_t *canvas;
    char *text;

    mcview_vterm_set_keep_history (vt, TRUE);
    mcview_vterm_set_size (vt, 3, 8);
    mcview_vterm_reset (vt);

    FEED (vt, "first\r\nsecond\r\nthird\r\nfourth");

    mcview_vterm_set_dpy_top_row (vt, 0);
    ck_assert_int_eq (mcview_vterm_resolve_scrollback_top_row (vt, 3), 0);
    canvas = mcview_vterm_compose_scrollback (vt, 0, 3);
    text = buffer_to_text (canvas, 3, 8);
    ck_assert_str_eq (text, "first   \nsecond  \nthird   \n");
    g_free (text);
    mcview_terminal_buffer_free (canvas);

    mcview_vterm_set_dpy_top_row (vt, MCVIEW_VTERM_FOLLOW_END);
    ck_assert_int_eq (mcview_vterm_resolve_scrollback_top_row (vt, 3), 1);
    canvas = mcview_vterm_compose_scrollback (vt, 1, 3);
    text = buffer_to_text (canvas, 3, 8);
    ck_assert_str_eq (text, "second  \nthird   \nfourth  \n");
    g_free (text);
    mcview_terminal_buffer_free (canvas);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_page_up_keeps_the_screen_in_the_history)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    mcview_terminal_buffer_t *canvas;
    char *text;

    mcview_vterm_set_keep_history (vt, TRUE);
    mcview_vterm_set_size (vt, 4, 8);
    mcview_vterm_reset (vt);

    FEED (vt, "first\r\nsecond\r\n$ ls");
    mcview_vterm_page_up (vt, 1);

    // The prompt row is at the bottom of a blank screen, the cursor still on it.
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 3);
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 4);
    ck_assert_int_eq (cell_ch (vt, 0, 0), 0);
    ck_assert_int_eq (cell_ch (vt, 3, 2), 'l');
    // A whole page went up: the rows above the prompt, then blank ones.
    ck_assert_int_eq (mcview_vterm_history_len (vt), 4);

    mcview_vterm_set_dpy_top_row (vt, 0);
    canvas = mcview_vterm_compose_scrollback (vt, 0, 8);
    text = buffer_to_text (canvas, 8, 8);
    ck_assert_str_eq (
        text, "first   \nsecond  \n        \n        \n        \n        \n        \n$ ls    \n");
    g_free (text);
    mcview_terminal_buffer_free (canvas);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_page_up_keeps_a_line_of_several_rows)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_keep_history (vt, TRUE);
    mcview_vterm_set_size (vt, 4, 8);
    mcview_vterm_reset (vt);

    FEED (vt, "out\r\n$ echo x\r\ny");
    mcview_vterm_page_up (vt, 2);

    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 3);
    ck_assert_int_eq (cell_ch (vt, 2, 0), '$');
    ck_assert_int_eq (cell_ch (vt, 3, 0), 'y');
    ck_assert_int_eq (cell_ch (vt, 1, 0), 0);
    ck_assert_int_eq (mcview_vterm_history_len (vt), 4);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_page_up_leaves_the_alternate_screen_alone)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_keep_history (vt, TRUE);
    mcview_vterm_set_size (vt, 3, 8);
    mcview_vterm_reset (vt);

    FEED (vt, "\x1b[?1049hfull");
    mcview_vterm_page_up (vt, 1);

    ck_assert_int_eq (cell_ch (vt, 0, 0), 'f');
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 4);
    ck_assert_int_eq (mcview_vterm_history_len (vt), 0);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_autowrap_breaks_a_long_line)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    char *text;

    mcview_vterm_set_autowrap (vt, TRUE);
    mcview_vterm_set_size (vt, 3, 8);
    mcview_vterm_reset (vt);

    FEED (vt, "0123456789ab");

    text = canvas_to_text (vt, 3, 8);
    ck_assert_str_eq (text, "01234567\n89ab    \n        \n");
    g_free (text);
    ck_assert (mcview_terminal_buffer_is_wrapped (mcview_vterm_buf (vt), 0));
    ck_assert (!mcview_terminal_buffer_is_wrapped (mcview_vterm_buf (vt), 1));
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 1);
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 4);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_autowrap_waits_for_the_next_character)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    char *text;

    mcview_vterm_set_autowrap (vt, TRUE);
    mcview_vterm_set_size (vt, 3, 8);
    mcview_vterm_reset (vt);

    // Exactly the width, then a newline: one row, not a wrapped one plus a blank.
    FEED (vt, "01234567\r\nnext");

    text = canvas_to_text (vt, 3, 8);
    ck_assert_str_eq (text, "01234567\nnext    \n        \n");
    g_free (text);
    ck_assert (!mcview_terminal_buffer_is_wrapped (mcview_vterm_buf (vt), 0));

    // The cursor stays on the last column until something is printed.
    FEED (vt, "\033[1;9H");
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 7);
    FEED (vt, "X");
    ck_assert_int_eq (cell_ch (vt, 0, 7), 'X');
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 0);
    FEED (vt, "\033[K");
    FEED (vt, "Y");
    ck_assert_int_eq (cell_ch (vt, 0, 7), 'Y');
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 0);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_autowrap_off_overwrites_the_last_column)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    char *text;

    mcview_vterm_set_autowrap (vt, TRUE);
    mcview_vterm_set_size (vt, 3, 8);
    mcview_vterm_reset (vt);

    FEED (vt, "\033[?7l0123456789ab");

    text = canvas_to_text (vt, 3, 8);
    ck_assert_str_eq (text, "01234567\n        \n        \n");
    g_free (text);
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 0);
    ck_assert (!mcview_terminal_buffer_is_wrapped (mcview_vterm_buf (vt), 0));

    FEED (vt, "\033[?7h\r\nabcdefghij");
    text = canvas_to_text (vt, 3, 8);
    ck_assert_str_eq (text, "01234567\nabcdefgh\nij      \n");
    g_free (text);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_autowrap_scrolls_and_the_history_keeps_the_break)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    mcview_terminal_buffer_t *canvas;
    char *text;

    mcview_vterm_set_autowrap (vt, TRUE);
    mcview_vterm_set_keep_history (vt, TRUE);
    mcview_vterm_set_size (vt, 2, 8);
    mcview_vterm_reset (vt);

    FEED (vt, "0123456789abcdefgh\r\nlast");

    ck_assert_int_eq (mcview_vterm_history_len (vt), 2);
    ck_assert (mcview_vterm_history_row_wrapped (vt, 0));
    ck_assert (mcview_vterm_history_row_wrapped (vt, 1));
    ck_assert (!mcview_terminal_buffer_is_wrapped (mcview_vterm_buf (vt), 0));

    canvas = mcview_vterm_compose_scrollback (vt, 0, 4);
    text = buffer_to_text (canvas, 4, 8);
    ck_assert_str_eq (text, "01234567\n89abcdef\ngh      \nlast    \n");
    g_free (text);
    mcview_terminal_buffer_free (canvas);

    // Made taller, the screen gets the row back, break and all.
    mcview_vterm_set_size (vt, 3, 8);
    ck_assert_int_eq (mcview_vterm_history_len (vt), 1);
    ck_assert (mcview_terminal_buffer_is_wrapped (mcview_vterm_buf (vt), 0));
    ck_assert (!mcview_terminal_buffer_is_wrapped (mcview_vterm_buf (vt), 1));

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_autowrap_is_off_by_default)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 3, 8);
    mcview_vterm_reset (vt);

    FEED (vt, "0123456789ab");

    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 0);
    ck_assert_int_eq (cell_ch (vt, 0, 11), 'b');

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_wide_char_takes_two_cells)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_autowrap (vt, TRUE);
    mcview_vterm_set_size (vt, 3, 8);
    mcview_vterm_reset (vt);

    FEED (vt,
          "A\xe4\xb8\xad"
          "B");

    ck_assert_int_eq (cell_ch (vt, 0, 0), 'A');
    ck_assert_int_eq (cell_ch (vt, 0, 1), 0x4e2d);
    ck_assert_int_eq (cell_ch (vt, 0, 2), MCVIEW_VTERM_WIDE_TAIL);
    ck_assert_int_eq (cell_ch (vt, 0, 3), 'B');
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 4);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_wide_char_goes_to_the_next_row_whole)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_autowrap (vt, TRUE);
    mcview_vterm_set_size (vt, 3, 4);
    mcview_vterm_reset (vt);

    FEED (vt, "abc\xe4\xb8\xad");

    ck_assert_int_eq (cell_ch (vt, 0, 3), 0);
    ck_assert (mcview_terminal_buffer_is_wrapped (mcview_vterm_buf (vt), 0));
    ck_assert_int_eq (cell_ch (vt, 1, 0), 0x4e2d);
    ck_assert_int_eq (cell_ch (vt, 1, 1), MCVIEW_VTERM_WIDE_TAIL);
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 1);
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 2);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_wide_char_half_overwritten_leaves_a_blank)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_autowrap (vt, TRUE);
    mcview_vterm_set_size (vt, 3, 8);
    mcview_vterm_reset (vt);

    FEED (vt, "\xe4\xb8\xad\xe4\xb8\xad");

    // over the right half of the first one
    FEED (vt, "\033[1;2HX");
    ck_assert_int_eq (cell_ch (vt, 0, 0), ' ');
    ck_assert_int_eq (cell_ch (vt, 0, 1), 'X');

    // over the left half of the second one
    FEED (vt, "Y");
    ck_assert_int_eq (cell_ch (vt, 0, 2), 'Y');
    ck_assert_int_eq (cell_ch (vt, 0, 3), ' ');

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static mcview_vterm_t *
reflow_vterm (int rows, int cols)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_autowrap (vt, TRUE);
    mcview_vterm_set_keep_history (vt, TRUE);
    mcview_vterm_set_size (vt, rows, cols);
    mcview_vterm_reset (vt);
    return vt;
}

/* --------------------------------------------------------------------------------------------- */

static char *
scrollback_text (mcview_vterm_t *vt, int term_rows, int cols)
{
    const int rows = mcview_vterm_history_len (vt) + term_rows;
    mcview_terminal_buffer_t *canvas = mcview_vterm_compose_scrollback (vt, 0, rows);
    char *text = buffer_to_text (canvas, rows, cols);

    mcview_terminal_buffer_free (canvas);
    return text;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reflow_narrower_breaks_the_lines_again)
{
    mcview_vterm_t *vt = reflow_vterm (4, 8);
    char *text;

    FEED (vt, "0123456789\r\nab\r\n$ ");

    mcview_vterm_set_size (vt, 4, 4);

    text = scrollback_text (vt, 4, 4);
    ck_assert_str_eq (text, "0123\n4567\n89  \nab  \n$   \n");
    g_free (text);
    // The screen keeps what fits; one row had to go up.
    ck_assert_int_eq (mcview_vterm_history_len (vt), 1);
    ck_assert (mcview_vterm_history_row_wrapped (vt, 0));
    ck_assert (mcview_terminal_buffer_is_wrapped (mcview_vterm_buf (vt), 0));
    ck_assert (!mcview_terminal_buffer_is_wrapped (mcview_vterm_buf (vt), 1));
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 3);
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 2);

    // A blank tail does not turn into blank rows.
    FEED (vt, "x");
    ck_assert_int_eq (cell_ch (vt, 3, 2), 'x');

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reflow_wider_puts_the_lines_together)
{
    mcview_vterm_t *vt = reflow_vterm (3, 4);
    char *text;

    FEED (vt, "0123456789\r\nab\r\ncd\r\n$ ");
    ck_assert_int_eq (mcview_vterm_history_len (vt), 3);

    mcview_vterm_set_size (vt, 3, 12);

    text = scrollback_text (vt, 3, 12);
    ck_assert_str_eq (text, "0123456789  \nab          \ncd          \n$           \n");
    g_free (text);
    // Only the rows the terminal broke are joined: "ab" and "cd" stay apart.
    ck_assert_int_eq (mcview_vterm_history_len (vt), 1);
    ck_assert (!mcview_vterm_history_row_wrapped (vt, 0));
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 2);
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 2);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reflow_keeps_the_cursor_in_its_line)
{
    mcview_vterm_t *vt = reflow_vterm (3, 8);
    gint64 row;
    int col;

    // The cursor is inside a long line, not at its end.
    FEED (vt, "0123456789abcd\033[3D");
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 1);
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 3);

    mcview_vterm_set_size (vt, 3, 5);
    // Offset 11 of the line: row 2, column 1.
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 2);
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 1);
    FEED (vt, "X");
    ck_assert_int_eq (cell_ch (vt, 2, 1), 'X');

    // The map says the same for the position the caller kept.
    ck_assert (mcview_vterm_reflow_map (vt, 0, 0, &row, &col));
    ck_assert_int_eq ((int) row, 0);
    ck_assert_int_eq (col, 0);
    ck_assert (mcview_vterm_reflow_map (vt, 1, 3, &row, &col));
    ck_assert_int_eq ((int) row, 2);
    ck_assert_int_eq (col, 1);
    ck_assert (!mcview_vterm_reflow_map (vt, 5, 0, &row, &col));

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reflow_narrower_scrolls_the_screen_to_keep_the_cursor)
{
    mcview_vterm_t *vt = reflow_vterm (2, 8);
    char *text;

    FEED (vt, "abcdefgh12345678\r\n$ ");
    ck_assert_int_eq (mcview_vterm_history_len (vt), 1);
    ck_assert_int_eq (mcview_vterm_scrolled_rows (vt), 1);

    mcview_vterm_set_size (vt, 2, 4);

    text = scrollback_text (vt, 2, 4);
    ck_assert_str_eq (text, "abcd\nefgh\n1234\n5678\n$   \n");
    g_free (text);
    ck_assert_int_eq (mcview_vterm_history_len (vt), 3);
    ck_assert_int_eq (mcview_vterm_scrolled_rows (vt), 3);
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 1);
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 2);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reflow_leaves_the_alternate_screen_alone)
{
    mcview_vterm_t *vt = reflow_vterm (3, 8);

    FEED (vt, "0123456789\r\n\033[?1049h\033[Hvim");
    mcview_vterm_set_size (vt, 3, 4);

    ck_assert_int_eq (cell_ch (vt, 0, 0), 'v');
    ck_assert_int_eq (mcview_vterm_history_len (vt), 0);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reflow_back_and_forth_is_the_same_text)
{
    mcview_vterm_t *vt = reflow_vterm (4, 10);
    char *before, *after;

    FEED (vt, "one two three four five\r\n\033[31mred\033[0m and plain\r\nlast\r\n$ ");
    before = scrollback_text (vt, 4, 10);

    mcview_vterm_set_size (vt, 4, 3);
    mcview_vterm_set_size (vt, 4, 7);
    mcview_vterm_set_size (vt, 4, 10);

    after = scrollback_text (vt, 4, 10);
    ck_assert_str_eq (after, before);
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 2);

    g_free (before);
    g_free (after);
    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The scrollback with W for a wide character and > for its right half. */
static char *
wide_text (mcview_vterm_t *vt, int term_rows, int cols)
{
    const int rows = mcview_vterm_history_len (vt) + term_rows;
    mcview_terminal_buffer_t *canvas = mcview_vterm_compose_scrollback (vt, 0, rows);
    GString *s = g_string_new ("");
    int row, col;

    for (row = 0; row < rows; row++)
    {
        for (col = 0; col < cols; col++)
        {
            const mcview_vterm_cell_t *cell = mcview_terminal_buffer_get (canvas, row, col);
            const gunichar ch = cell != NULL ? cell->ch : 0;

            if (ch == MCVIEW_VTERM_WIDE_TAIL)
                g_string_append_c (s, '>');
            else if (ch > 0x7f)
                g_string_append_c (s, 'W');
            else
                g_string_append_c (s, ch != 0 ? (char) ch : ' ');
        }
        g_string_append_c (s, '\n');
    }
    mcview_terminal_buffer_free (canvas);
    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reflow_keeps_a_wide_character_whole)
{
    mcview_vterm_t *vt = reflow_vterm (4, 4);
    char *text;

    FEED (vt,
          "A\xe4\xb8\xad"
          "B");

    mcview_vterm_set_size (vt, 4, 2);
    text = wide_text (vt, 4, 2);
    ck_assert_str_eq (text, "A \nW>\nB \n  \n");
    g_free (text);

    // back: the blank put before the wide character is not kept
    mcview_vterm_set_size (vt, 4, 4);
    text = wide_text (vt, 4, 4);
    ck_assert_str_eq (text, "AW>B\n    \n    \n    \n");
    g_free (text);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reflow_after_a_wide_character_wrapped)
{
    mcview_vterm_t *vt = reflow_vterm (4, 4);
    char *text;

    // no room on the last column: the wide character goes to the next row
    FEED (vt, "abc\xe4\xb8\xad");
    mcview_vterm_set_size (vt, 4, 8);

    text = wide_text (vt, 4, 8);
    ck_assert_str_eq (text, "abcW>   \n        \n        \n        \n");
    g_free (text);
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 0);
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 5);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_erase_of_one_half_blanks_the_wide_character)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_autowrap (vt, TRUE);
    mcview_vterm_set_size (vt, 3, 8);
    mcview_vterm_reset (vt);

    // EL from the right half
    FEED (vt, "\xe4\xb8\xad\033[1;2H\033[K");
    ck_assert_int_eq (cell_ch (vt, 0, 0), ' ');
    ck_assert_int_eq (cell_ch (vt, 0, 1), ' ');

    // ICH between the halves
    FEED (vt, "\033[1;1H\xe4\xb8\xad\xe4\xb8\xad\033[1;2H\033[@");
    ck_assert_int_eq (cell_ch (vt, 0, 0), ' ');
    ck_assert_int_eq (cell_ch (vt, 0, 1), ' ');
    ck_assert_int_eq (cell_ch (vt, 0, 2), ' ');
    ck_assert_int_eq (cell_ch (vt, 0, 3), 0x4e2d);
    ck_assert_int_eq (cell_ch (vt, 0, 4), MCVIEW_VTERM_WIDE_TAIL);

    // DCH of the right half
    FEED (vt, "\033[2K\033[1;1H\xe4\xb8\xad\xe4\xb8\xad\033[1;2H\033[P");
    ck_assert_int_eq (cell_ch (vt, 0, 0), ' ');
    ck_assert_int_eq (cell_ch (vt, 0, 1), 0x4e2d);
    ck_assert_int_eq (cell_ch (vt, 0, 2), MCVIEW_VTERM_WIDE_TAIL);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_oversized_osc_is_dropped_whole)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    GString *osc;
    guint gen_before;

    mcview_vterm_set_size (vt, 6, 40);
    mcview_vterm_reset (vt);

    FEED (vt, "\033]7;file:///good\007");
    gen_before = mcview_vterm_osc7_generation (vt);
    ck_assert_str_eq (mcview_vterm_osc7_raw (vt), "7;file:///good");

    /* Longer than the OSC buffer: it arrives truncated, and a path cut short would name the
       wrong directory. The whole sequence must be dropped instead. */
    osc = g_string_new ("\033]7;file:///");
    while (osc->len < 4096)
        g_string_append (osc, "aaaaaaaaaa");
    g_string_append_c (osc, '\007');

    feed_bytes (vt, osc->str, osc->len);
    g_string_free (osc, TRUE);

    ck_assert_uint_eq (mcview_vterm_osc7_generation (vt), gen_before);
    ck_assert_str_eq (mcview_vterm_osc7_raw (vt), "7;file:///good");

    // and the terminal is back in its senses right after
    FEED (vt, "\033]7;file:///after\007");
    ck_assert_str_eq (mcview_vterm_osc7_raw (vt), "7;file:///after");

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Two bands of 16 sixels each is 16x12 pixels, but the raster attributes say
   16x32, and they win: with 8x16 cells that is 2 columns by 2 rows. */
#define SIXEL_16x32 "\033P0;0;0q\"1;1;16;32#0;2;100;0;0#0!16~-!16~\033\\"

START_TEST (test_sixel_becomes_a_picture_at_the_cursor)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    const mcview_vterm_image_t *image;

    mcview_vterm_set_size (vt, 10, 40);
    mcview_vterm_reset (vt);
    mcview_vterm_set_cell_size (vt, 8, 16);
    mcview_vterm_set_sixel (vt, TRUE);

    FEED (vt, "\033[3;5H");
    FEED (vt, SIXEL_16x32);

    ck_assert_uint_eq (mcview_vterm_images_len (vt), 1);
    image = mcview_vterm_image (vt, 0);
    ck_assert_int_eq (image->row, 2);
    ck_assert_int_eq (image->col, 4);
    ck_assert_int_eq (image->width, 16);
    ck_assert_int_eq (image->height, 32);
    ck_assert_int_eq (image->cols, 2);
    ck_assert_int_eq (image->rows, 2);
    ck_assert_uint_eq (g_bytes_get_size (image->data), sizeof (SIXEL_16x32) - 1);
    ck_assert_int_eq (
        memcmp (g_bytes_get_data (image->data, NULL), SIXEL_16x32, sizeof (SIXEL_16x32) - 1), 0);

    /* The cursor is below the picture, in the column it started at. */
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 4);
    ck_assert_int_eq (mcview_vterm_cursor_col (vt), 4);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sixel_without_raster_attributes_is_measured)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    const mcview_vterm_image_t *image;

    mcview_vterm_set_size (vt, 10, 40);
    mcview_vterm_reset (vt);
    mcview_vterm_set_cell_size (vt, 8, 16);

    /* 3 bands: 20 wide (!20~), then 5 + 10 on one band via $, then 7. */
    FEED (vt, "\033Pq#1;2;0;0;100#1!20~-!5~$!10~-~~~~~~~\033\\");

    image = mcview_vterm_image (vt, 0);
    ck_assert_ptr_nonnull (image);
    ck_assert_int_eq (image->width, 20);
    ck_assert_int_eq (image->height, 18);
    ck_assert_int_eq (image->cols, 3);
    ck_assert_int_eq (image->rows, 2);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sixel_scrolls_with_the_text_and_leaves_at_the_top)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 40);
    mcview_vterm_reset (vt);
    mcview_vterm_set_cell_size (vt, 8, 16);

    FEED (vt, "\033[2;1H");
    FEED (vt, SIXEL_16x32); /* rows 1..2, cursor on row 3 */
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 3);

    FEED (vt, "\n"); /* row 4 */
    FEED (vt, "\n"); /* scroll: picture on rows 0..1 */
    ck_assert_uint_eq (mcview_vterm_images_len (vt), 1);
    ck_assert_int_eq (mcview_vterm_image (vt, 0)->row, 0);

    FEED (vt, "\n"); /* the top row of the picture leaves: so does the picture */
    ck_assert_uint_eq (mcview_vterm_images_len (vt), 0);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sixel_stays_in_the_history)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    const mcview_vterm_image_t *image;
    int i;

    mcview_vterm_set_size (vt, 5, 40);
    mcview_vterm_reset (vt);
    mcview_vterm_set_cell_size (vt, 8, 16);
    mcview_vterm_set_keep_history (vt, TRUE);

    FEED (vt, "\033[2;1H");
    FEED (vt, SIXEL_16x32); /* rows 1..2, cursor on row 3 */
    FEED (vt, "\n");        /* row 4 */
    FEED (vt, "\n");        /* scroll: picture on rows 0..1 */
    ck_assert_uint_eq (mcview_vterm_images_len (vt), 1);
    ck_assert_int_eq (mcview_vterm_image (vt, 0)->row, 0);
    ck_assert_int_eq (mcview_vterm_history_len (vt), 1);

    /* The top row of the picture leaves the screen: the picture goes with the
       row into the history, where its place is the same row as before. */
    for (i = 0; i < 10; i++)
        FEED (vt, "\n");

    ck_assert_uint_eq (mcview_vterm_images_len (vt), 1);
    image = mcview_vterm_image (vt, 0);
    ck_assert_int_eq (image->row, -10);
    ck_assert_int_eq (mcview_vterm_history_len (vt), 11);
    ck_assert_int_eq (mcview_vterm_history_len (vt) + image->row, 1);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sixel_below_the_bottom_scrolls_the_screen)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 40);
    mcview_vterm_reset (vt);
    mcview_vterm_set_cell_size (vt, 8, 16);

    FEED (vt, "\033[1;1H");
    FEED (vt, "top");
    FEED (vt, "\033[5;1H");
    FEED (vt, SIXEL_16x32); /* 2 rows from the last one: one row of scroll */

    ck_assert_uint_eq (cell_ch (vt, 0, 0), 0);
    ck_assert_uint_eq (mcview_vterm_images_len (vt), 1);
    ck_assert_int_eq (mcview_vterm_image (vt, 0)->row, 3);
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 4);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_erase_screen_takes_the_pictures)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    guint generation;

    mcview_vterm_set_size (vt, 10, 40);
    mcview_vterm_reset (vt);

    FEED (vt, SIXEL_16x32);
    ck_assert_uint_eq (mcview_vterm_images_len (vt), 1);
    generation = mcview_vterm_images_generation (vt);

    FEED (vt, "\033[2J");
    ck_assert_uint_eq (mcview_vterm_images_len (vt), 0);
    ck_assert_uint_ne (mcview_vterm_images_generation (vt), generation);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_erase_to_end_of_screen_takes_the_pictures_below)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 10, 40);
    mcview_vterm_reset (vt);

    FEED (vt, SIXEL_16x32); /* rows 0..1 */
    FEED (vt, "\033[6;1H");
    FEED (vt, SIXEL_16x32); /* rows 5..6 */
    FEED (vt, "\033[4;1H");
    FEED (vt, "\033[J");

    ck_assert_uint_eq (mcview_vterm_images_len (vt), 1);
    ck_assert_int_eq (mcview_vterm_image (vt, 0)->row, 0);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_oversized_sixel_is_dropped_whole)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    GString *big = g_string_new ("\033Pq");
    size_t i;

    mcview_vterm_set_size (vt, 10, 40);
    mcview_vterm_reset (vt);

    for (i = 0; i < 4 * 1024 * 1024 + 1; i++)
        g_string_append_c (big, '~');
    g_string_append (big, "\033\\");
    feed_bytes (vt, big->str, big->len);
    g_string_free (big, TRUE);

    ck_assert_uint_eq (mcview_vterm_images_len (vt), 0);
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 0);

    /* And the stream goes on as if nothing happened. */
    FEED (vt, "ok");
    ck_assert_uint_eq (cell_ch (vt, 0, 0), 'o');

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_xtgettcap_is_still_answered)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    static const char query[] = "\033P+q544e\033\\";
    vterm_event_t ev;
    size_t i;

    mcview_vterm_reset (vt);

    for (i = 0; i < sizeof (query) - 1; i++)
        ev = mcview_vterm_feed (vt, (unsigned char) query[i]);

    ck_assert_int_eq (ev.type, VTERM_REPLY);
    ck_assert_uint_eq (mcview_vterm_images_len (vt), 0);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static const char *
reply_to (mcview_vterm_t *vt, const char *query)
{
    vterm_event_t ev = { 0 };
    size_t i;

    for (i = 0; query[i] != '\0'; i++)
    {
        ev = mcview_vterm_feed (vt, (unsigned char) query[i]);
        mcview_vterm_apply_event (vt, &ev);
    }
    return ev.type == VTERM_REPLY ? ev.reply : NULL;
}

START_TEST (test_sixel_terminal_says_so_when_asked)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 24, 80);
    mcview_vterm_reset (vt);
    mcview_vterm_set_cell_size (vt, 10, 20);

    ck_assert_str_eq (reply_to (vt, "\033[c"), "\033[?1;2c");
    ck_assert_ptr_null (reply_to (vt, "\033[16t"));

    mcview_vterm_set_sixel (vt, TRUE);
    ck_assert_str_eq (reply_to (vt, "\033[c"), "\033[?62;4;22c");
    ck_assert_str_eq (reply_to (vt, "\033[16t"), "\033[6;20;10t");
    ck_assert_str_eq (reply_to (vt, "\033[14t"), "\033[4;480;800t");
    ck_assert_str_eq (reply_to (vt, "\033[18t"), "\033[8;24;80t");
    ck_assert_ptr_null (reply_to (vt, "\033[22t"));

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_far2l_extensions_are_asked_for_and_given_up)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 5, 40);
    mcview_vterm_reset (vt);

    // the terminal mc runs in has no extensions: no answer, no mode, and no text of the APC drawn
    ck_assert_ptr_null (reply_to (vt, "\033_far2l1\033\\"));
    ck_assert (!mcview_vterm_far2l_active (vt));
    FEED (vt, "x");
    ck_assert_uint_eq (cell_ch (vt, 0, 0), 'x');

    mcview_vterm_set_far2l (vt, TRUE);
    ck_assert_str_eq (reply_to (vt, "\033_far2l1\033\\"), "\033_far2lok\033\\");
    ck_assert (mcview_vterm_far2l_active (vt));
    ck_assert_str_eq (reply_to (vt, "\033_far2l1\a"), "\033_far2lok\033\\");

    // what else it says on the channel is not for the screen
    FEED (vt, "\033_f2l:AAAA\033\\y\033_far2l1234567890123456789\033\\z");
    ck_assert (mcview_vterm_far2l_active (vt));
    ck_assert_uint_eq (cell_ch (vt, 0, 1), 'y');
    ck_assert_uint_eq (cell_ch (vt, 0, 2), 'z');

    FEED (vt, "\033_far2l0\033\\");
    ck_assert (!mcview_vterm_far2l_active (vt));

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cursor_position_is_reported)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 24, 80);
    mcview_vterm_reset (vt);

    ck_assert_str_eq (reply_to (vt, "\033[5n"), "\033[0n");
    ck_assert_str_eq (reply_to (vt, "\033[6n"), "\033[1;1R");
    ck_assert_str_eq (reply_to (vt, "\033[5;10Habc\033[6n"), "\033[5;13R");
    ck_assert_str_eq (reply_to (vt, "\033[24;80Hx\033[6n"), "\033[24;80R");
    ck_assert_ptr_null (reply_to (vt, "\033[?6n"));

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sixel_keeps_only_what_sixel_is_made_of)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    const mcview_vterm_image_t *image;
    static const char with_controls[] = "\033P0;0;0q\"1;1;16;32#0;2;100;0;0#0\x9b"
                                        "2J"
                                        "\x07!16~\r\n-!16~\033\\";
    static const char clean[] = "\033P0;0;0q\"1;1;16;32#0;2;100;0;0#02J!16~-!16~\033\\";

    mcview_vterm_set_size (vt, 10, 40);
    mcview_vterm_reset (vt);
    mcview_vterm_set_sixel (vt, TRUE);

    feed_bytes (vt, with_controls, sizeof (with_controls) - 1);
    ck_assert_uint_eq (mcview_vterm_images_len (vt), 1);
    image = mcview_vterm_image (vt, 0);
    ck_assert_uint_eq (g_bytes_get_size (image->data), sizeof (clean) - 1);
    ck_assert_int_eq (memcmp (g_bytes_get_data (image->data, NULL), clean, sizeof (clean) - 1), 0);

    /* CAN throws the picture away, and the stream goes on. */
    FEED (vt,
          "\033[2J\033Pq#0!16~\x18"
          "ok");
    ck_assert_uint_eq (mcview_vterm_images_len (vt), 0);
    ck_assert_uint_eq (cell_ch (vt, 0, 0), 'o');

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sixel_without_a_terminal_for_it_takes_its_place_only)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 10, 40);
    mcview_vterm_reset (vt);
    mcview_vterm_set_cell_size (vt, 8, 16);

    FEED (vt, SIXEL_16x32);
    ck_assert_uint_eq (mcview_vterm_images_len (vt), 1);
    ck_assert_ptr_null (mcview_vterm_image (vt, 0)->data);
    ck_assert_int_eq (mcview_vterm_cursor_row (vt), 2);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sixel_wider_than_the_screen_keeps_its_width)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 10, 40);
    mcview_vterm_reset (vt);
    mcview_vterm_set_cell_size (vt, 8, 16);

    FEED (vt, "\033[1;31H");
    FEED (vt, SIXEL_16x32); /* two columns from column 30: past the edge */
    ck_assert_int_eq (mcview_vterm_image (vt, 0)->col, 30);
    ck_assert_int_eq (mcview_vterm_image (vt, 0)->cols, 2);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_the_pictures_kept_are_so_many)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    int i;

    mcview_vterm_set_size (vt, 10, 40);
    mcview_vterm_reset (vt);

    for (i = 0; i < 100; i++)
    {
        FEED (vt, "\033[1;1H");
        FEED (vt, SIXEL_16x32);
    }
    ck_assert_uint_eq (mcview_vterm_images_len (vt), 64);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_screen_made_taller_brings_the_pictures_down_with_the_rows)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 6, 40);
    mcview_vterm_reset (vt);
    mcview_vterm_set_keep_history (vt, TRUE);
    mcview_vterm_set_cell_size (vt, 8, 16);

    FEED (vt, "one\ntwo\nthree\n");
    FEED (vt, SIXEL_16x32); /* rows 3..4, cursor row 5 */
    ck_assert_int_eq (mcview_vterm_image (vt, 0)->row, 3);

    /* Two rows shorter: "one" and "two" go to the history, the picture
       moves up with the rest. */
    mcview_vterm_set_size (vt, 4, 40);
    ck_assert_int_eq (mcview_vterm_image (vt, 0)->row, 1);

    /* And back: the rows come back on top, the picture goes down again. */
    mcview_vterm_set_size (vt, 6, 40);
    ck_assert_uint_eq (cell_ch (vt, 0, 0), 'o');
    ck_assert_int_eq (mcview_vterm_image (vt, 0)->row, 3);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* ESC ( 0 designates the DEC line-drawing set into G0: the letters stand for
   the frame characters until ESC ( B takes it back. */
START_TEST (test_dec_graphics_letters_draw_lines)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 4, 20);

    FEED (vt, "\033(0lqk\033(Bab");

    ck_assert_uint_eq (cell_ch (vt, 0, 0), 0x250C); /* l */
    ck_assert_uint_eq (cell_ch (vt, 0, 1), 0x2500); /* q */
    ck_assert_uint_eq (cell_ch (vt, 0, 2), 0x2510); /* k */
    ck_assert_uint_eq (cell_ch (vt, 0, 3), 'a');
    ck_assert_uint_eq (cell_ch (vt, 0, 4), 'b');

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* SO prints from G1 and SI from G0 again, each set keeping its own designation. */
START_TEST (test_shift_out_prints_from_g1)
{
    mcview_vterm_t *vt = mcview_vterm_new ();

    mcview_vterm_set_size (vt, 4, 20);

    FEED (vt, "\033)0x\016x\017x");

    ck_assert_uint_eq (cell_ch (vt, 0, 0), 'x');    /* G0 is still ASCII */
    ck_assert_uint_eq (cell_ch (vt, 0, 1), 0x2502); /* SO: G1 draws a line */
    ck_assert_uint_eq (cell_ch (vt, 0, 2), 'x');    /* SI: G0 again */

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fish_startup_leaves_the_prompt_plain)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    const mcview_vterm_cell_t *cell;

    mcview_vterm_set_size (vt, 5, 40);
    mcview_vterm_reset (vt);

    /* fish 4 turns on bracketed paste, modifyOtherKeys and the kitty
       keyboard protocol, then draws user@host in green and resets. */
    FEED (vt, "\033[?2004h\033[>4;1m\033[>5u\033[=5u");
    FEED (vt, "\033[32muser@host\033(B\033[m ~> ");

    cell = cell_at (vt, 0, 0);
    ck_assert_ptr_nonnull (cell);
    ck_assert_uint_eq (cell->ch, 'u');
    ck_assert_int_eq (cell->attr.fg, 2);
    ck_assert (!cell->attr.underline);
    ck_assert (!cell->attr.bold);

    cell = cell_at (vt, 0, 10);
    ck_assert_ptr_nonnull (cell);
    ck_assert_uint_eq (cell->ch, '~');
    ck_assert_int_eq (cell->attr.fg, MCVIEW_ANSI_COLOR_DEFAULT);
    ck_assert (!cell->attr.underline);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sgr_after_osc_and_dcs)
{
    mcview_vterm_t *vt = mcview_vterm_new ();
    const mcview_vterm_cell_t *cell;

    mcview_vterm_set_size (vt, 5, 40);
    mcview_vterm_reset (vt);

    /* the terminal takes OSC and DCS itself; the SGR parser must not wait for their end */
    FEED (vt, "\033]7;file:///tmp\007\033[32mg\033P+q544e\033\\\033[1mb");

    cell = cell_at (vt, 0, 0);
    ck_assert_ptr_nonnull (cell);
    ck_assert_uint_eq (cell->ch, 'g');
    ck_assert_int_eq (cell->attr.fg, 2);

    cell = cell_at (vt, 0, 1);
    ck_assert_ptr_nonnull (cell);
    ck_assert_uint_eq (cell->ch, 'b');
    ck_assert (cell->attr.bold);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_scroll_region_lf_at_bottom_scrolls_up);
    tcase_add_test (tc_core, test_scroll_region_lf_above_region_advances_cursor);
    tcase_add_test (tc_core, test_vpa_moves_row_only);
    tcase_add_test (tc_core, test_ech_erases_with_attrs_cursor_stays);
    tcase_add_test (tc_core, test_ich_shifts_the_tail_right);
    tcase_add_test (tc_core, test_insert_mode_shifts_the_tail_right);
    tcase_add_test (tc_core, test_erase_eol_fills_full_width_with_attrs);
    tcase_add_test (tc_core, test_decstbm_out_of_range_bottom_is_clamped);
    tcase_add_test (tc_core, test_cursor_fwd_clamps_to_term_cols);
    tcase_add_test (tc_core, test_cursor_abs_col_clamps_to_term_cols);
    tcase_add_test (tc_core, test_cursor_abs_row_clamps_to_term_rows);
    tcase_add_test (tc_core, test_cursor_down_clamps_to_term_rows);
    tcase_add_test (tc_core, test_erase_bol_does_not_expand_past_term_cols);
    tcase_add_test (tc_core, test_sync_snapshot_split_osc7_and_prompt);
    tcase_add_test (tc_core, test_sync_snapshot_batched_osc7_and_prompt);
    tcase_add_test (tc_core, test_set_size_returns_true_on_change);
    tcase_add_test (tc_core, test_set_size_returns_false_on_same_size);
    tcase_add_test (tc_core, test_golden_draw_move_erase);
    tcase_add_test (tc_core, test_scrollback_canvas_preserves_output_from_top);
    tcase_add_test (tc_core, test_page_up_keeps_the_screen_in_the_history);
    tcase_add_test (tc_core, test_page_up_keeps_a_line_of_several_rows);
    tcase_add_test (tc_core, test_page_up_leaves_the_alternate_screen_alone);
    tcase_add_test (tc_core, test_autowrap_breaks_a_long_line);
    tcase_add_test (tc_core, test_autowrap_waits_for_the_next_character);
    tcase_add_test (tc_core, test_autowrap_off_overwrites_the_last_column);
    tcase_add_test (tc_core, test_autowrap_scrolls_and_the_history_keeps_the_break);
    tcase_add_test (tc_core, test_autowrap_is_off_by_default);
    tcase_add_test (tc_core, test_wide_char_takes_two_cells);
    tcase_add_test (tc_core, test_wide_char_goes_to_the_next_row_whole);
    tcase_add_test (tc_core, test_wide_char_half_overwritten_leaves_a_blank);
    tcase_add_test (tc_core, test_reflow_keeps_a_wide_character_whole);
    tcase_add_test (tc_core, test_reflow_after_a_wide_character_wrapped);
    tcase_add_test (tc_core, test_erase_of_one_half_blanks_the_wide_character);
    tcase_add_test (tc_core, test_reflow_narrower_breaks_the_lines_again);
    tcase_add_test (tc_core, test_reflow_wider_puts_the_lines_together);
    tcase_add_test (tc_core, test_reflow_keeps_the_cursor_in_its_line);
    tcase_add_test (tc_core, test_reflow_narrower_scrolls_the_screen_to_keep_the_cursor);
    tcase_add_test (tc_core, test_reflow_leaves_the_alternate_screen_alone);
    tcase_add_test (tc_core, test_reflow_back_and_forth_is_the_same_text);
    tcase_add_test (tc_core, test_oversized_osc_is_dropped_whole);
    tcase_add_test (tc_core, test_sixel_becomes_a_picture_at_the_cursor);
    tcase_add_test (tc_core, test_sixel_without_raster_attributes_is_measured);
    tcase_add_test (tc_core, test_sixel_scrolls_with_the_text_and_leaves_at_the_top);
    tcase_add_test (tc_core, test_sixel_stays_in_the_history);
    tcase_add_test (tc_core, test_sixel_below_the_bottom_scrolls_the_screen);
    tcase_add_test (tc_core, test_erase_screen_takes_the_pictures);
    tcase_add_test (tc_core, test_erase_to_end_of_screen_takes_the_pictures_below);
    tcase_add_test (tc_core, test_oversized_sixel_is_dropped_whole);
    tcase_add_test (tc_core, test_xtgettcap_is_still_answered);
    tcase_add_test (tc_core, test_far2l_extensions_are_asked_for_and_given_up);
    tcase_add_test (tc_core, test_sixel_terminal_says_so_when_asked);
    tcase_add_test (tc_core, test_cursor_position_is_reported);
    tcase_add_test (tc_core, test_sixel_keeps_only_what_sixel_is_made_of);
    tcase_add_test (tc_core, test_sixel_without_a_terminal_for_it_takes_its_place_only);
    tcase_add_test (tc_core, test_sixel_wider_than_the_screen_keeps_its_width);
    tcase_add_test (tc_core, test_the_pictures_kept_are_so_many);
    tcase_add_test (tc_core, test_a_screen_made_taller_brings_the_pictures_down_with_the_rows);
    tcase_add_test (tc_core, test_dec_graphics_letters_draw_lines);
    tcase_add_test (tc_core, test_shift_out_prints_from_g1);
    tcase_add_test (tc_core, test_fish_startup_leaves_the_prompt_plain);
    tcase_add_test (tc_core, test_sgr_after_osc_and_dcs);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
