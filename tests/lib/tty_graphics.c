/*
   lib - the choice of the picture protocol

   Copyright (C) 2026
   Ilia Maslakov il.smind@gmail.com

   This file is part of M-Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see https://www.gnu.org/licenses/.
 */

#define TEST_SUITE_NAME "/lib/tty"

#include "tests/mctest.h"

#include "lib/tty/graphics.h"

/* --------------------------------------------------------------------------------------------- */

static const struct request_ds
{
    const char *graphics;
    const char *sixel;
    const char *term;
    tty_graphics_t expected;
} request_ds[] = {
    { NULL, NULL, "xterm-256color", TTY_GRAPHICS_AUTO },
    { "", NULL, "xterm", TTY_GRAPHICS_AUTO },
    { "auto", NULL, "xterm", TTY_GRAPHICS_AUTO },
    { "none", NULL, "xterm", TTY_GRAPHICS_NONE },
    { "sixel", NULL, "xterm", TTY_GRAPHICS_SIXEL },
    { "kitty", NULL, "xterm-kitty", TTY_GRAPHICS_KITTY },
    { "bogus", NULL, "xterm", TTY_GRAPHICS_AUTO },
    // MC_SIXEL is the old name
    { NULL, "0", "xterm", TTY_GRAPHICS_NONE },
    { NULL, "1", "xterm", TTY_GRAPHICS_SIXEL },
    // MC_GRAPHICS wins over MC_SIXEL
    { "kitty", "1", "xterm", TTY_GRAPHICS_KITTY },
    { "auto", "0", "xterm", TTY_GRAPHICS_AUTO },
    // multiplexers: none unless a protocol is named
    { NULL, NULL, "screen-256color", TTY_GRAPHICS_NONE },
    { NULL, NULL, "tmux-256color", TTY_GRAPHICS_NONE },
    { "auto", NULL, "tmux", TTY_GRAPHICS_NONE },
    { "sixel", NULL, "tmux", TTY_GRAPHICS_SIXEL },
    { NULL, "1", "screen", TTY_GRAPHICS_SIXEL },
    { NULL, NULL, NULL, TTY_GRAPHICS_AUTO },
};

START_PARAMETRIZED_TEST (test_request, request_ds)
{
    ck_assert_int_eq (tty_graphics_request (data->graphics, data->sixel, data->term),
                      data->expected);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

static const struct pick_ds
{
    tty_graphics_t request;
    gboolean sixel;
    gboolean kitty;
    tty_graphics_t expected;
} pick_ds[] = {
    { TTY_GRAPHICS_AUTO, FALSE, FALSE, TTY_GRAPHICS_NONE },
    { TTY_GRAPHICS_AUTO, TRUE, FALSE, TTY_GRAPHICS_SIXEL },
    { TTY_GRAPHICS_AUTO, FALSE, TRUE, TTY_GRAPHICS_KITTY },
    // sixel when the terminal has it: Konsole and WezTerm answer both
    { TTY_GRAPHICS_AUTO, TRUE, TRUE, TTY_GRAPHICS_SIXEL },
    { TTY_GRAPHICS_NONE, TRUE, TRUE, TTY_GRAPHICS_NONE },
    { TTY_GRAPHICS_SIXEL, FALSE, FALSE, TTY_GRAPHICS_SIXEL },
    { TTY_GRAPHICS_KITTY, TRUE, FALSE, TTY_GRAPHICS_KITTY },
};

START_PARAMETRIZED_TEST (test_pick, pick_ds)
{
    ck_assert_int_eq (tty_graphics_pick (data->request, data->sixel, data->kitty), data->expected);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_ask_kitty)
{
    ck_assert (tty_graphics_ask_kitty (TTY_GRAPHICS_AUTO, "xterm-kitty"));
    ck_assert (tty_graphics_ask_kitty (TTY_GRAPHICS_AUTO, NULL));
    ck_assert (!tty_graphics_ask_kitty (TTY_GRAPHICS_AUTO, "linux"));
    ck_assert (!tty_graphics_ask_kitty (TTY_GRAPHICS_KITTY, "xterm-kitty"));
    ck_assert (!tty_graphics_ask_kitty (TTY_GRAPHICS_NONE, "xterm"));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_draws_pictures)
{
    tty_graphics_set_backend (TTY_GRAPHICS_SIXEL);
    ck_assert (tty_draws_pictures ());

    // detected, not drawn yet
    tty_graphics_set_backend (TTY_GRAPHICS_KITTY);
    ck_assert_int_eq (tty_graphics_backend (), TTY_GRAPHICS_KITTY);
    ck_assert (!tty_draws_pictures ());

    tty_graphics_set_backend (TTY_GRAPHICS_AUTO);
    ck_assert_int_eq (tty_graphics_backend (), TTY_GRAPHICS_NONE);
    ck_assert (!tty_draws_pictures ());
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_request, request_ds);
    mctest_add_parameterized_test (tc_core, test_pick, pick_ds);
    tcase_add_test (tc_core, test_ask_kitty);
    tcase_add_test (tc_core, test_draws_pictures);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
