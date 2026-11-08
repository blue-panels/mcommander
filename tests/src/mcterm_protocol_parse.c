/*
   tests/src/mcterm_protocol_parse.c -- unit tests for the OSC 133 prompt marks

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

#define TEST_SUITE_NAME "/src/mcterm_protocol_parse"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "src/mcterm/mcterm_proto.h"

#define TOKEN "deadbeefcafe0000"

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_marks_are_read)
{
    mcterm_osc133_t m;

    ck_assert (mcterm_osc133_parse ("133;A;mc=" TOKEN, TOKEN, &m));
    ck_assert_int_eq (m.mark, MCTERM_MARK_PROMPT_START);

    ck_assert (mcterm_osc133_parse ("133;B;mc=" TOKEN, TOKEN, &m));
    ck_assert_int_eq (m.mark, MCTERM_MARK_PROMPT_END);

    ck_assert (mcterm_osc133_parse ("133;R;mc=" TOKEN, TOKEN, &m));
    ck_assert_int_eq (m.mark, MCTERM_MARK_RPROMPT_START);

    ck_assert (mcterm_osc133_parse ("133;S;mc=" TOKEN, TOKEN, &m));
    ck_assert_int_eq (m.mark, MCTERM_MARK_RPROMPT_END);

    ck_assert (mcterm_osc133_parse ("133;C;mc=" TOKEN, TOKEN, &m));
    ck_assert_int_eq (m.mark, MCTERM_MARK_COMMAND_START);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_exit_code_of_done_mark)
{
    mcterm_osc133_t m;

    ck_assert (mcterm_osc133_parse ("133;D;0;mc=" TOKEN, TOKEN, &m));
    ck_assert_int_eq (m.mark, MCTERM_MARK_COMMAND_DONE);
    ck_assert_int_eq (m.exit_code, 0);

    ck_assert (mcterm_osc133_parse ("133;D;130;mc=" TOKEN, TOKEN, &m));
    ck_assert_int_eq (m.exit_code, 130);

    // a shell that names no code still reports the command as done
    ck_assert (mcterm_osc133_parse ("133;D;mc=" TOKEN, TOKEN, &m));
    ck_assert_int_eq (m.mark, MCTERM_MARK_COMMAND_DONE);
    ck_assert_int_eq (m.exit_code, -1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_nonsense_exit_code_is_no_code)
{
    mcterm_osc133_t m;

    ck_assert (mcterm_osc133_parse ("133;D;wat;mc=" TOKEN, TOKEN, &m));
    ck_assert_int_eq (m.exit_code, -1);

    ck_assert (mcterm_osc133_parse ("133;D;12x;mc=" TOKEN, TOKEN, &m));
    ck_assert_int_eq (m.exit_code, -1);

    ck_assert (mcterm_osc133_parse ("133;D;9999;mc=" TOKEN, TOKEN, &m));
    ck_assert_int_eq (m.exit_code, -1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_foreign_marks_are_refused)
{
    mcterm_osc133_t m;

    // the shell integration of a session across ssh, or the output of a command
    ck_assert (!mcterm_osc133_parse ("133;A", TOKEN, &m));
    ck_assert (!mcterm_osc133_parse ("133;A;mc=0badc0de", TOKEN, &m));
    ck_assert (!mcterm_osc133_parse ("133;A;mc=", TOKEN, &m));
    ck_assert (!mcterm_osc133_parse ("133;A;aid=7", TOKEN, &m));
    ck_assert (!mcterm_osc133_parse ("133;A;mc=" TOKEN "extra", TOKEN, &m));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_malformed_payloads_are_refused)
{
    mcterm_osc133_t m;

    ck_assert (!mcterm_osc133_parse (NULL, TOKEN, &m));
    ck_assert (!mcterm_osc133_parse ("", TOKEN, &m));
    ck_assert (!mcterm_osc133_parse ("7;file:///tmp", TOKEN, &m));
    ck_assert (!mcterm_osc133_parse ("133;", TOKEN, &m));
    ck_assert (!mcterm_osc133_parse ("133;Z;mc=" TOKEN, TOKEN, &m));
    // a letter of ours with something stuck to it is not a mark of ours
    ck_assert (!mcterm_osc133_parse ("133;AA;mc=" TOKEN, TOKEN, &m));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_without_token_any_mark_is_taken)
{
    mcterm_osc133_t m;

    ck_assert (mcterm_osc133_parse ("133;B", NULL, &m));
    ck_assert_int_eq (m.mark, MCTERM_MARK_PROMPT_END);

    ck_assert (mcterm_osc133_parse ("133;D;3", NULL, &m));
    ck_assert_int_eq (m.exit_code, 3);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_refused_payload_leaves_the_result_alone)
{
    mcterm_osc133_t m = { MCTERM_MARK_PROMPT_END, 42 };

    ck_assert (!mcterm_osc133_parse ("133;A;mc=0badc0de", TOKEN, &m));
    ck_assert_int_eq (m.mark, MCTERM_MARK_PROMPT_END);
    ck_assert_int_eq (m.exit_code, 42);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    str_init_strings ("UTF-8");

    tc_core = tcase_create ("Core");
    tcase_add_test (tc_core, test_marks_are_read);
    tcase_add_test (tc_core, test_exit_code_of_done_mark);
    tcase_add_test (tc_core, test_nonsense_exit_code_is_no_code);
    tcase_add_test (tc_core, test_foreign_marks_are_refused);
    tcase_add_test (tc_core, test_malformed_payloads_are_refused);
    tcase_add_test (tc_core, test_without_token_any_mark_is_taken);
    tcase_add_test (tc_core, test_refused_payload_leaves_the_result_alone);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
