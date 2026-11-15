/*
   Tests for the M-Commander
   The characters of the editor buffer at the end of a chunk

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

#define TEST_SUITE_NAME "/src/editor"

#include "tests/mctest.h"

#include "lib/global.h"
#include "src/editor/edit-impl.h"
#include "src/editor/editwidget.h"  // edit_buffer_t, through editbuffer.h

/* the size of a chunk of the buffer: EDIT_BUF_SIZE of editbuffer.c */
#define CHUNK 65536

/* --------------------------------------------------------------------------------------------- */

/* A character whose first byte ends a chunk of the buffer: its rest is in the next chunk, and
   nothing is read past the end of the first */
START_TEST (test_utf_across_chunks)
{
    edit_buffer_t buf;
    int len = -1;
    off_t i;

    edit_buffer_init (&buf, 0);
    for (i = 0; i < CHUNK - 1; i++)
        edit_buffer_insert (&buf, 'a');
    edit_buffer_insert (&buf, 0xC3);  // e acute: C3 A9
    edit_buffer_insert (&buf, 0xA9);

    ck_assert_int_eq (edit_buffer_get_utf (&buf, CHUNK - 1, &len), 0xE9);
    ck_assert_int_eq (len, 2);
    ck_assert_int_eq (edit_buffer_get_utf (&buf, CHUNK - 2, &len), 'a');

    edit_buffer_clean (&buf);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The same after the cursor, where the chunks hold the text from their end */
START_TEST (test_utf_across_chunks_after_cursor)
{
    edit_buffer_t buf;
    int len = -1;
    off_t i;

    edit_buffer_init (&buf, 0);
    // inserted ahead of the cursor, the text comes in from its end: e acute and CHUNK - 1 'a'
    for (i = 0; i < CHUNK - 1; i++)
        edit_buffer_insert_ahead (&buf, 'a');
    edit_buffer_insert_ahead (&buf, 0xA9);
    edit_buffer_insert_ahead (&buf, 0xC3);

    ck_assert_int_eq (edit_buffer_get_utf (&buf, 0, &len), 0xE9);
    ck_assert_int_eq (len, 2);
    ck_assert_int_eq (edit_buffer_get_utf (&buf, 2, &len), 'a');

    edit_buffer_clean (&buf);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_test (tc_core, test_utf_across_chunks);
    tcase_add_test (tc_core, test_utf_across_chunks_after_cursor);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
