/*
   tests/src/syntax/context_rules.c -- unit tests for context and keyword matching

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

#define TEST_SUITE_NAME "/src/syntax"

#include "tests/mctest.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lib/global.h"
#include "src/syntax/syntax.h"

/* A rule set is only ever built from a real syntax file, so the tests write one.
   Nothing else here needs mc to be running: the scanner reads its bytes through
   the accessor below. */
static char *tmpdir = NULL;
static char *syntax_file = NULL;
static syntax_rules_t *rules = NULL;

/* the text under test, and the byte source over it */
static const char *text = NULL;
static off_t text_len = 0;

/* --------------------------------------------------------------------------------------------- */

static int
get_byte (void *data, off_t byte_index)
{
    (void) data;

    return byte_index < 0 || byte_index >= text_len ? '\n' : (unsigned char) text[byte_index];
}

/* --------------------------------------------------------------------------------------------- */

static void
write_file (const char *path, const char *content)
{
    FILE *f = fopen (path, "w");

    ck_assert_msg (f != NULL, "cannot write %s", path);
    fputs (content, f);
    fclose (f);
}

/* --------------------------------------------------------------------------------------------- */

/** Build a rule set out of @body, the part of a syntax file below its first line. */
static void
load (const char *body)
{
    char *lang;
    char *top;
    syntax_select_t sel;
    int res;

    lang = g_build_filename (tmpdir, "tested.syntax", (char *) NULL);
    write_file (lang, body);

    top = g_strdup_printf ("file .\\* Tested\ninclude %s\n", lang);
    write_file (syntax_file, top);
    g_free (top);
    g_free (lang);

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";
    syntax_rules_unref (rules);
    res = syntax_rules_load (syntax_file, &sel, &rules, NULL);
    ck_assert_int_eq (res, 0);
    ck_assert_ptr_nonnull (rules);
}

/* --------------------------------------------------------------------------------------------- */

/** Load @body and give back what the parser made of it: 0, or the line it stopped at. */
static int
load_result (const char *body)
{
    char *lang;
    char *top;
    syntax_select_t sel;
    int res;

    lang = g_build_filename (tmpdir, "tested.syntax", (char *) NULL);
    write_file (lang, body);

    top = g_strdup_printf ("file .\\* Tested\ninclude %s\n", lang);
    write_file (syntax_file, top);
    g_free (top);
    g_free (lang);

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";
    syntax_rules_unref (rules);
    res = syntax_rules_load (syntax_file, &sel, &rules, NULL);

    return res;
}

/* --------------------------------------------------------------------------------------------- */

/** Load a whole Syntax file of its own, rules and all. */
static int
load_toplevel (const char *content)
{
    syntax_select_t sel;

    write_file (syntax_file, content);

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";
    syntax_rules_unref (rules);

    return syntax_rules_load (syntax_file, &sel, &rules, NULL);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * What the rules make of @s, a character to a byte: the first letter of the
 * foreground color, or '.' where the byte is left alone.
 */
static char *
mask_len (const char *s, off_t len)
{
    syntax_scanner_t *sc;
    GString *out;
    off_t i;

    text = s;
    text_len = len;

    sc = syntax_scanner_new (rules, get_byte, NULL, text_len);
    out = g_string_sized_new ((gsize) text_len);

    for (i = 0; i < text_len; i++)
    {
        guint color;
        const char *fg = NULL;
        const char *bg;
        const char *attrs;

        color = syntax_rules_color_of (rules, syntax_state_at (sc, i));
        if (color != 0)
            syntax_rules_color_spec (rules, color, &fg, &bg, &attrs);

        g_string_append_c (out, fg == NULL || *fg == '\0' ? '.' : *fg);
    }

    syntax_scanner_free (sc);

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static char *
mask (const char *s)
{
    return mask_len (s, (off_t) strlen (s));
}

/* --------------------------------------------------------------------------------------------- */

static void
check_mask_len (const char *s, off_t len, const char *expected)
{
    char *got = mask_len (s, len);

    ck_assert_msg (strcmp (got, expected) == 0, "text [%s]\n  expected [%s]\n  got      [%s]", s,
                   expected, got);
    g_free (got);
}

/* --------------------------------------------------------------------------------------------- */

static void
check_mask (const char *s, const char *expected)
{
    char *got = mask (s);

    ck_assert_msg (strcmp (got, expected) == 0, "text [%s]\n  expected [%s]\n  got      [%s]", s,
                   expected, got);
    g_free (got);
}

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    tmpdir = g_dir_make_tmp ("mc-syntax-XXXXXX", NULL);
    ck_assert_ptr_nonnull (tmpdir);
    syntax_file = g_build_filename (tmpdir, "Syntax", (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown (void)
{
    char *lang;

    syntax_rules_unref (rules);
    rules = NULL;

    lang = g_build_filename (tmpdir, "tested.syntax", (char *) NULL);
    unlink (lang);
    g_free (lang);

    unlink (syntax_file);
    g_free (syntax_file);
    syntax_file = NULL;

    rmdir (tmpdir);
    g_free (tmpdir);
    tmpdir = NULL;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_plain_keyword)
{
    load ("context default\n"
          "  keyword int red\n");

    // a literal keyword, and a byte of it that does not match
    check_mask ("int x", "rrr..");
    check_mask ("ins x", ".....");
    // without "whole" the keyword is found inside a word as well
    check_mask ("print", "..rrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_whole_word)
{
    load ("context default\n"
          "  keyword whole int red\n"
          "  keyword wholeleft do green\n"
          "  keyword wholeright if yellow\n");

    check_mask ("int x", "rrr..");
    // a word character on either side turns the match down
    check_mask ("print", ".....");
    check_mask ("into", "....");
    // wholeleft only looks behind, wholeright only ahead
    check_mask ("do done", "gg.gg..");
    check_mask ("if iffy", "yy.....");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_line_start)
{
    load ("context default\n"
          "  keyword linestart end red\n");

    check_mask ("end\nthe end", "rrr........");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_star)
{
    load ("context default\n"
          "  keyword a*z red\n"
          "  keyword wholeright q* green\n");

    // the star swallows what lies between
    check_mask ("abcz .", "rrrr..");
    check_mask ("az .", "rr..");
    // and stops at the end of the line
    check_mask ("abc\nz", ".....");
    // a star at the end of a keyword runs to the end of the word
    check_mask ("qwe .", "gggg.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_plus)
{
    load ("context default\n"
          "  keyword a+z red\n"
          "  keyword whole i+ green\n");

    // the plus eats the word characters between
    check_mask ("abcz .", "rrrr..");
    // and is satisfied by none of them, which is what the engine does today
    check_mask ("az .", "rr..");
    // a plus at the end takes the rest of the word
    check_mask ("ivar .", "gggg..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_bracket)
{
    load ("context default\n"
          "  keyword a\\[0123456789\\]z red\n");

    // the bracket eats a run of the characters it names
    check_mask ("a12z .", "rrrr..");
    check_mask ("a1z .", "rrr..");
    // and nothing else
    check_mask ("abz .", ".....");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_brace)
{
    load ("context default\n"
          "  keyword a\\{xy\\}z red\n");

    // the brace eats exactly one of the characters it names
    check_mask ("axz .", "rrr..");
    check_mask ("ayz .", "rrr..");
    check_mask ("abz .", ".....");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_starting_with_bracket)
{
    load ("context default\n"
          "  keyword \\[abc\\]z red\n");

    // the set eats a run, and then the rest of the word has to be there
    check_mask ("abz .", "rrr..");
    check_mask ("cz .", "rr..");
    /* an empty run will do, so the word can begin with what stands after the
       set: this is why a set at the start of a word does not say which bytes
       the word can begin with all by itself */
    check_mask ("z .", "r..");
    check_mask ("qz .", ".r..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_starting_with_two_brackets)
{
    load ("context default\n"
          "  keyword \\[ab\\]\\[cd\\]z red\n");

    check_mask ("abcdz .", "rrrrr..");
    // the first set gives up and the second one takes over
    check_mask ("cdz .", "rrr..");
    // both give up, and the word is what is left of it
    check_mask ("z .", "r..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_starting_with_star)
{
    load ("context default\n"
          "  keyword *z red\n");

    // the star runs over anything, so the word can begin on any byte at all
    check_mask ("abz .", "rrr..");
    // an empty run will do
    check_mask ("z .", "r..");
    // but the line break stops it
    check_mask ("ab\nz .", "...r..");
    check_mask ("q .", "...");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_starting_with_plus)
{
    load ("context default\n"
          "  keyword +z red\n");

    // the plus runs over the bytes that are part of a word
    check_mask ("abz .", "rrr..");
    // and at the start of a word it wants at least one of them
    check_mask ("z .", "...");
    check_mask ("q .", "...");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_starting_with_brace)
{
    load ("context default\n"
          "  keyword \\{xy\\}z red\n");

    check_mask ("xz .", "rr..");
    // exactly one byte out of the set, and an empty one will not do
    check_mask ("z .", "...");
    check_mask ("qz .", "....");
    // the set eats one byte only, so the word begins on the second x here
    check_mask ("xyz .", ".rr..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_two_keywords_that_both_fit)
{
    load ("context default\n"
          "  keyword \\{ab\\}c red\n"
          "  keyword ac green\n");

    // the word named first wins
    check_mask ("ac .", "rr..");

    load ("context default\n"
          "  keyword ac green\n"
          "  keyword \\{ab\\}c red\n");

    // and it is the order they are named in, not the kind of word
    check_mask ("ac .", "gg..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_case_insensitive)
{
    load ("caseinsensitive\n"
          "context default\n"
          "  keyword int red\n");

    check_mask ("INT x", "rrr..");
    check_mask ("Int x", "rrr..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_context)
{
    load ("context default\n"
          "context /\\* \\*/ green\n");

    // the delimiters belong to the context
    check_mask ("a /* b */ c", "..ggggggg..");
    // and it runs to the end of the text when the right side never comes
    check_mask ("a /* b", "..gggg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_context_exclusive)
{
    load ("context default\n"
          "context exclusive /\\* \\*/ green\n");

    // exclusive colors what lies between the delimiters, not the delimiters
    check_mask ("a /* b */ c", "....ggg....");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_context_oneline)
{
    load ("context default\n"
          "  keyword int red\n"
          "context oneline `` `` green\n"
          "context exclusive oneline < > yellow\n");

    // a single backtick inside is no end of the context
    check_mask ("a ``b ` c`` int", "..ggggggggg.rrr");
    // left open, the context ends with its line
    check_mask ("a ``b\nint ``c``",
                "..gggg"
                "rrr.ggggg");
    check_mask ("x <a\nint",
                "...yy"
                "rrr");
    // and the next line can start a context on its first byte
    check_mask ("``a\n``b``",
                "gggg"
                "ggggg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_context_linestart)
{
    load ("context default\n"
          "context linestart # \\n green\n");

    check_mask ("# one\nx # two\n", "gggggg........");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_in_context)
{
    load ("context default\n"
          "context /\\* \\*/ green\n"
          "  keyword TODO red\n");

    check_mask ("/* TODO x */", "gggrrrrggggg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_plus_corners)
{
    load ("context default\n"
          "  keyword a+a red\n"
          "  keyword b+xy green\n"
          "  keyword c+z yellow\n");

    // the character after the plus is the one the keyword starts with
    check_mask ("aba .", "rrr..");
    // the plus lets go where the character at hand turns up further down the
    // keyword, and what follows no longer lines up
    check_mask ("bxy .", ".....");
    // and a word that ends before the plus found anything
    check_mask ("c z", "...");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_bracket_corners)
{
    load ("context default\n"
          "  keyword a\\[xy\\]\\[xy\\]z red\n"
          "  keyword q\\[wer green\n");

    // two sets in a row, the second one starting where the first gave up
    check_mask ("axyz .", "rrrr..");
    // a set that is never closed matches nothing
    check_mask ("qwer .", "......");
    // the character after the set is the last one the set ate
    load ("context default\n"
          "  keyword a\\[xy\\]yz red\n");
    check_mask ("axyz .", "rrrr..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_over_newline)
{
    load ("context default\n"
          "  keyword a\\nb red\n");

    // the keyword reaches over the line break, and is dropped on the next line
    check_mask ("a\nb", "rr.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_nul_byte)
{
    load ("context default\n"
          "  keyword int red\n");

    // a NUL byte is a byte the rules are not applied at; what follows is read on
    check_mask_len ("a\0int", 5, "..rrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_exclusive_empty)
{
    load ("context default\n"
          "context exclusive < > green\n");

    // the right delimiter arrives where the left one ended
    check_mask ("a<>b", "....");
    check_mask ("a<x>b", "..g..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_exclusive_first_byte)
{
    load ("context default\n"
          "context exclusive < > green\n"
          "context { } red\n");

    // the body of an exclusive context is its own from the first byte on
    check_mask ("a<{x}>b", "..ggg..");
    check_mask ("a<x{y}>b", "..gggg..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_newline_keyword_in_newline_context)
{
    load ("context default\n"
          "context # \\n green\n"
          "  keyword TODO\\n red\n");

    // both the context and the keyword end with the line break, and the keyword
    // gives it back so that the context is not carried into the next line
    check_mask ("# TODO\nx", "ggrrrrg.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_at_context_start)
{
    load ("context default\n"
          "context < > green\n"
          "  keyword <x red\n");

    // the context turns on and a keyword of it starts on the same byte
    check_mask ("a<xy>", ".rrgg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_context_over_keyword_starting_with_it)
{
    load ("context default\n"
          "  keyword << red\n"
          "context <<< >>> green\n");

    // the longer context wins over the keyword, and is entered
    check_mask ("a<<<x>>>b<<c", ".ggggggg.rr.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_newline_keyword_at_context_start)
{
    load ("context default\n"
          "context # \\n green\n"
          "  keyword #TODO\\n red\n");

    /* the same keyword that ends with a line break, but starting on the byte
       the context starts on: here the keyword keeps the line break and the
       context is carried into the next line.  Compare with
       test_newline_keyword_in_newline_context, where it is not. */
    check_mask ("#TODO\nx", "rrrrrrg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_wholechars)
{
    // the characters a word is made of are the file's to choose
    load ("wholechars ab\n"
          "context default\n"
          "  keyword whole ab red\n");
    check_mask ("ab ", "rr.");
    check_mask ("aba", "...");

    load ("wholechars left xy\n"
          "wholechars right z\n"
          "context default\n"
          "  keyword whole q red\n");
    // only x and y stop a word on the left, only z on the right
    check_mask ("aqa", ".r.");
    check_mask ("xq", "..");
    check_mask ("qz", "..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_context_word_borders)
{
    load ("context default\n"
          "context whole ab cd green\n");
    // the delimiters are words of their own
    check_mask ("ab cd", "ggggg");
    check_mask ("xab cd", "......");

    load ("context default\n"
          "context wholeleft ab cd green\n");
    check_mask ("ab cd", "ggggg");

    load ("context default\n"
          "context wholeright ab cd green\n");
    check_mask ("ab cd", "ggggg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_context_linestart_right)
{
    load ("context default\n"
          "context < linestart > green\n");

    // the right delimiter counts only at the start of a line
    check_mask ("a<b>c\n>d", ".gggggg.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_spellcheck_and_comments)
{
    load ("# a comment\n"
          "\n"
          "context default\n"
          "  spellcheck\n"
          "  keyword int red\n");

    check_mask ("int", "rrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_color_inherited)
{
    load ("context default green\n"
          "  keyword int red\n"
          "  keyword char\n");

    // a keyword with no color of its own falls back to the context's
    check_mask ("int char", "rrrggggg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_define)
{
    load ("define types int char\n"
          "context default\n"
          "  keyword whole int red\n"
          "  keyword whole char red\n");

    check_mask ("int char", "rrr.rrrr");

    // a define given twice keeps the last
    load ("define types int\n"
          "define types char\n"
          "context default\n"
          "  keyword whole char green\n");
    check_mask ("char", "gggg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_parse_errors)
{
    // the parser reports the line it stopped at, and never a dialog
    ck_assert_int_eq (load_result ("context\n"), 1);
    ck_assert_int_eq (load_result ("context default\n  keyword\n"), 2);
    ck_assert_int_eq (load_result ("wholechars\n"), 1);
    ck_assert_int_eq (load_result ("context something else\n"), 1);
    ck_assert_int_eq (load_result ("keyword int red\n"), 1);
    ck_assert_int_eq (load_result ("define\n"), 1);
    ck_assert_int_eq (load_result ("define one\n"), 1);
    ck_assert_int_eq (load_result ("line-local extra\n"), 1);
    ck_assert_int_eq (load_result ("context default\nline-local\n"), 2);
    ck_assert_int_eq (load_result ("number 16 red\n"), 1);
    ck_assert_int_eq (load_result ("line-local\nnumber\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nnumber 0 red\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nnumber 16zz red\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nnumber 16\n"), 2);
    ck_assert_int_eq (load_result ("string \" red\n"), 1);
    ck_assert_int_eq (load_result ("line-local\nstring\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nstring xx red\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nstring x red\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nstring \"\n"), 2);
    ck_assert_int_eq (load_result ("symbols ,. red\n"), 1);
    ck_assert_int_eq (load_result ("line-local\nsymbols\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nsymbols ,.\n"), 2);
    ck_assert_int_eq (load_result ("include\n"), 1);
    ck_assert_int_eq (load_result ("include /nowhere/at/all.syntax\n"), 1);
    ck_assert_int_eq (load_result ("context default\n  spellcheck extra\n"), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_rules_without_include)
{
    // the rules can stand in the Syntax file itself
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "  keyword int red\n"),
                      0);
    check_mask ("int", "rrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_include_that_is_not_there)
{
    // an include of a file that cannot be opened stops at its line
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "include /nowhere/at/all.syntax\n"),
                      2);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_second_file_line_ends_the_rules)
{
    // a second "file" line belongs to the next rule set, not to this one
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "  keyword int red\n"
                                     "file \\.c$ C\n"
                                     "context default\n"),
                      0);
    check_mask ("int", "rrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_full_colors)
{
    // a color is a foreground, a background and attributes, for contexts as well
    // as for keywords
    load ("context default green black bold\n"
          "  keyword int red black underline\n");

    check_mask ("int x", "rrrgg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_more_parse_errors)
{
    ck_assert_int_eq (load_result ("line-local\ncontext default\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nspellcheck\n"), 2);
    ck_assert_int_eq (load_result ("spellcheck\n"), 1);
    ck_assert_int_eq (load_result ("line-local\nkeyword int red\n"), 2);
    ck_assert_int_eq (load_result ("context default\n  keyword linestart whole int red\n"), 2);
    ck_assert_int_eq (load_result ("context default\n  keyword linestart\n"), 2);
    ck_assert_int_eq (load_result ("nonsense here\n"), 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_error_file_names_the_include)
{
    char *lang;
    char *top;
    char *error_file = NULL;
    syntax_select_t sel;

    lang = g_build_filename (tmpdir, "tested.syntax", (char *) NULL);
    write_file (lang, "context default\n  nonsense\n");

    top = g_strdup_printf ("file .\\* Tested\ninclude %s\n", lang);
    write_file (syntax_file, top);
    g_free (top);

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";

    // the included file counts its own lines and gives its own name away
    ck_assert_int_eq (syntax_rules_load (syntax_file, &sel, &rules, &error_file), 2);
    ck_assert_str_eq (error_file, lang);
    g_free (error_file);
    g_free (lang);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_truncated_context_and_keyword_lines)
{
    // a context line that stops before every word of it has been read
    ck_assert_int_eq (load_result ("context default\ncontext exclusive\n"), 2);
    ck_assert_int_eq (load_result ("context default\ncontext oneline\n"), 2);
    ck_assert_int_eq (load_result ("context default\ncontext exclusive oneline\n"), 2);
    ck_assert_int_eq (load_result ("context default\ncontext whole\n"), 2);
    ck_assert_int_eq (load_result ("context default\ncontext linestart\n"), 2);
    ck_assert_int_eq (load_result ("context default\ncontext <\n"), 2);
    ck_assert_int_eq (load_result ("context default\ncontext < linestart\n"), 2);

    // and a keyword line that says which side of it is a word border, and nothing else
    ck_assert_int_eq (load_result ("context default\n  keyword whole\n"), 2);
    ck_assert_int_eq (load_result ("context default\n  keyword wholeleft\n"), 2);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_no_syntax_file_at_all)
{
    syntax_select_t sel;
    char *missing;

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";

    missing = g_build_filename (tmpdir, "not-here", (char *) NULL);
    // neither the file asked for nor the one mc ships with
    ck_assert_int_eq (syntax_rules_load (missing, &sel, &rules, NULL), -1);
    g_free (missing);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_type_chosen_by_caller)
{
    syntax_select_t sel;

    write_file (syntax_file,
                "\n"
                "# the first lines say nothing\n"
                "file \\.c$ TheC\n"
                "context default\n"
                "  keyword int red\n"
                "file \\.sh$ TheShell\n"
                "context default\n"
                "  keyword echo green\n");

    sel.type = "TheShell";
    sel.filename = "whatever.c";
    sel.first_line = "";
    ck_assert_int_eq (syntax_rules_load (syntax_file, &sel, &rules, NULL), 0);
    ck_assert_str_eq (syntax_rules_type (rules), "TheShell");

    // the name it was asked for wins over the name of the file
    check_mask ("echo int", "gggg....");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_type_chosen_by_first_line)
{
    syntax_select_t sel;

    write_file (syntax_file,
                "file \\.c$ TheC\n"
                "context default\n"
                "  keyword int red\n"
                "file unmatchable TheShell ^#!.\\*sh\n"
                "context default\n"
                "  keyword echo green\n");

    // the name matches nothing, the first line of the text does
    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "#!/bin/sh";
    ck_assert_int_eq (syntax_rules_load (syntax_file, &sel, &rules, NULL), 0);
    ck_assert_str_eq (syntax_rules_type (rules), "TheShell");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_nothing_to_choose_by)
{
    syntax_select_t sel;

    write_file (syntax_file,
                "file \\.c$ TheC\n"
                "context default\n"
                "  keyword int red\n");

    // no name asked for and no file to match against: no rule set is chosen
    sel.type = NULL;
    sel.filename = NULL;
    sel.first_line = "";
    ck_assert_int_eq (syntax_rules_load (syntax_file, &sel, &rules, NULL), -1);
    ck_assert_ptr_null (rules);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_list_of_types)
{
    GPtrArray *names;

    write_file (syntax_file,
                "file \\.c$ TheC\n"
                "context default\n"
                "file \\.sh$ TheShell\n"
                "context default\n");

    names = g_ptr_array_new_with_free_func (g_free);
    ck_assert_int_eq (syntax_rules_list_types (syntax_file, names), 0);
    ck_assert_int_eq ((int) names->len, 2);
    ck_assert_str_eq (g_ptr_array_index (names, 0), "TheC");
    ck_assert_str_eq (g_ptr_array_index (names, 1), "TheShell");
    g_ptr_array_free (names, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_include_before_any_file_line)
{
    char *lang;
    syntax_select_t sel;

    lang = g_build_filename (tmpdir, "tested.syntax", (char *) NULL);
    write_file (lang, "context default\n  keyword int red\n");
    g_free (lang);

    // an include above every "file" line is the rule set, whatever the file is
    write_file (syntax_file, "include tested.syntax\n");

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";
    // the include is looked for where mc keeps its own, so this one is not found
    ck_assert_int_eq (syntax_rules_load (syntax_file, &sel, &rules, NULL), 1);

    ck_assert_int_eq (load_toplevel ("include\n"), 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_include_that_opens)
{
    char *lang;
    char *content;
    syntax_select_t sel;

    lang = g_build_filename (tmpdir, "included.syntax", (char *) NULL);
    write_file (lang, "context default\n  keyword int red\n");

    // an include above every "file" line names the rule set for any file
    content = g_strdup_printf ("include %s\n", lang);
    write_file (syntax_file, content);
    g_free (content);
    g_free (lang);

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";
    ck_assert_int_eq (syntax_rules_load (syntax_file, &sel, &rules, NULL), 0);
    check_mask ("int", "rrr");

    lang = g_build_filename (tmpdir, "included.syntax", (char *) NULL);
    unlink (lang);
    g_free (lang);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_include_then_a_file_line)
{
    char *lang;
    char *content;
    syntax_select_t sel;

    lang = g_build_filename (tmpdir, "included.syntax", (char *) NULL);
    write_file (lang, "context default\n  keyword int blue\n");

    content = g_strdup_printf ("include %s\n"
                               "file .\\* Tested\n"
                               "context default\n"
                               "  keyword int red\n",
                               lang);
    write_file (syntax_file, content);
    g_free (content);

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";
    ck_assert_int_eq (syntax_rules_load (syntax_file, &sel, &rules, NULL), 0);

    // the rules are read twice, and the ones of the "file" line are what is kept
    ck_assert_str_eq (syntax_rules_type (rules), "Tested");
    check_mask ("int", "rrr");

    unlink (lang);
    g_free (lang);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_rules_without_any_keyword)
{
    // a rule set of nothing but its default context colors nothing, so it is
    // thrown away and the caller told there is none
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"),
                      -1);
    ck_assert_ptr_null (rules);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_broken_file_line) { ck_assert_int_eq (load_toplevel ("file onlyone\n"), 1); }
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_error_inside_the_chosen_rules)
{
    // the line the parser stopped at is counted from the top of the Syntax file
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "  nonsense\n"),
                      3);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The rules the tests below embed: a keyword and a string of their own. */
#define INNER_RULES                                                                                \
    "file \\.none$ Inner\n"                                                                        \
    "context default\n"                                                                            \
    "  keyword whole int yellow\n"                                                                 \
    "context \" \" cyan\n"

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed)
{
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "  keyword whole int red\n"
                                     "context <s> </s> green\n"
                                     "  embed Inner\n" INNER_RULES),
                      0);

    // the delimiters are the context's, what stands between them the embedded rules'
    check_mask ("int <s>int \"x\"</s> int", "rrr.gggyyy.cccgggg.rrr");
    // and the embedded rules end with the body, not before
    check_mask ("<s>int\nint</s>int", "gggyyy.yyyggggrrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_escape)
{
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context <s> </s> green\n"
                                     "  embed Inner\n" INNER_RULES),
                      0);

    // the right delimiter ends the body in a string of the embedded rules as well
    check_mask ("<s>\"a</s>b", "gggccgggg.");
    // a body with nothing in it
    check_mask ("<s></s>b", "ggggggg.");
    // and one that never ends runs to the end of the text
    check_mask ("<s>int", "gggyyy");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_exclusive)
{
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context exclusive <s> </s> green\n"
                                     "  embed Inner\n" INNER_RULES),
                      0);

    // exclusive leaves the delimiters to the scope around
    check_mask ("<s>int x</s>.", "...yyy.......");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_case)
{
    // the rules around ignore case, the embedded ones do not; the type is found
    // though the line naming it was folded
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "caseinsensitive\n"
                                     "context default\n"
                                     "context <s> </s> green\n"
                                     "  embed Inner\n" INNER_RULES),
                      0);

    check_mask ("<S>int INT</S>.", "gggyyy....gggg.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_nested)
{
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "  keyword whole int red\n"
                                     "context <a> </a> green\n"
                                     "  embed Middle\n"
                                     "file \\.none$ Middle\n"
                                     "context default\n"
                                     "  keyword whole int cyan\n"
                                     "context <b> </b> magenta\n"
                                     "  embed Inner\n" INNER_RULES),
                      0);

    // each body ends with its own delimiter, and the scope around takes over
    check_mask ("<a><b>x</b>int</a>int",
                "gggmmm.mmmmccc"
                "gggg"
                "rrr");
    // the outer delimiter ends every body inside it at once
    check_mask ("<a><b>\"int</a>int",
                "gggmmmcccc"
                "gggg"
                "rrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_itself)
{
    /* a rule set inside itself is copied in so many levels deep, and then left
       plain; each parenthesis is closed by its own */
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "  keyword whole int red\n"
                                     "context ( ) green\n"
                                     "  embed Tested\n"),
                      0);

    check_mask ("(int(int))int", "grrrgrrrggrrr");
    check_mask ("(((((x)))))int", "gggggggggggrrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_shared)
{
    syntax_scanner_t *sc;
    syntax_state_t a, b, c;

    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context <a> </s> green\n"
                                     "  embed Inner\n"
                                     "context <b> </s> green\n"
                                     "  embed Inner\n"
                                     "context <c> </c> green\n"
                                     "  embed Inner\n" INNER_RULES),
                      0);

    check_mask ("<a>int</s><b>int</s><c>int</c>", "gggyyygggggggyyygggggggyyygggg");

    // the bodies that end alike are read in one scope, the other one in its own
    sc = syntax_scanner_new (rules, get_byte, NULL, text_len);
    a = syntax_state_at (sc, 3);
    b = syntax_state_at (sc, 13);
    c = syntax_state_at (sc, 23);
    syntax_scanner_free (sc);
    ck_assert_int_eq (a.context, b.context);
    ck_assert_int_ne (a.context, c.context);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_left_ends_like_right)
{
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context run\\s' ' green\n"
                                     "  embed Inner\n" INNER_RULES),
                      0);

    // the quote that ends the left delimiter does not end the body it opens
    check_mask ("run 'int'.", "gggggyyyg.");
    check_mask ("run ''.", "gggggg.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_soft)
{
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context {{ }} green\n"
                                     "  embed soft Inner\n"
                                     "context <s> </s> magenta\n"
                                     "  embed Inner\n" INNER_RULES),
                      0);

    // a soft delimiter in a string of the embedded rules is part of the string
    check_mask ("{{\"}}\"}}.", "ggccccgg.");
    // and ends the body from the default context of those rules as ever
    check_mask ("{{int}}.", "ggyyygg.");
    check_mask ("{{}}.", "gggg.");
    // a hard one ends it anywhere
    check_mask ("<s>\"</s>\".", "mmmcmmmm..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_soft_keyword)
{
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context { } green\n"
                                     "  embed soft Escapes\n"
                                     "file \\.none$ Escapes\n"
                                     "context default\n"
                                     "  keyword \\\\} yellow\n"),
                      0);

    // a soft delimiter inside a keyword of the embedded rules is part of it
    check_mask ("{a\\}b}.", "g.yy.g.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_soft_nested)
{
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context {{ }} green\n"
                                     "  embed soft Middle\n"
                                     "file \\.none$ Middle\n"
                                     "context default\n"
                                     "context <b> </b> magenta\n"
                                     "  embed Inner\n" INNER_RULES),
                      0);

    // the body of <b> is a context of the rules {{ embeds: its "}}" is no end
    check_mask ("{{<b>}}</b>}}.", "ggmmm..mmmmgg.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_unknown_type)
{
    // a type no Syntax file names leaves the context in its own colors
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "  keyword whole int red\n"
                                     "context < > green\n"
                                     "  embed Nowhere\n" INNER_RULES),
                      0);
    check_mask ("int <int> int", "rrr.ggggg.rrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_from_installed_syntax)
{
    char *saved_share = mc_global.share_data_dir;
    char *share, *dir, *installed, *lang, *content;

    /* the Syntax file the user keeps is older than the rules and does not know
       the type they embed; the one installed with the program does */
    share = g_build_filename (tmpdir, "share", (char *) NULL);
    dir = g_build_filename (share, "syntax", (char *) NULL);
    g_mkdir_with_parents (dir, 0700);
    lang = g_build_filename (dir, "inner.syntax", (char *) NULL);
    write_file (lang,
                "context default\n"
                "  keyword whole int yellow\n");
    installed = g_build_filename (dir, "Syntax", (char *) NULL);
    content = g_strdup_printf ("file \\.none$ Inner\ninclude %s\n", lang);
    write_file (installed, content);
    mc_global.share_data_dir = share;

    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context < > green\n"
                                     "  embed Inner\n"),
                      0);
    check_mask ("<int>", "gyyyg");

    mc_global.share_data_dir = saved_share;
    unlink (installed);
    unlink (lang);
    rmdir (dir);
    rmdir (share);
    g_free (content);
    g_free (installed);
    g_free (lang);
    g_free (dir);
    g_free (share);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/**
 * An installed Syntax file under a share directory of the test's own:
 * .bar files are Bar, .foo files InstalledFoo.  Gives back the directory.
 */
static char *
make_installed_syntax (void)
{
    char *share, *dir, *path, *lang, *content;

    share = g_build_filename (tmpdir, "share", (char *) NULL);
    dir = g_build_filename (share, "syntax", (char *) NULL);
    g_mkdir_with_parents (dir, 0700);

    lang = g_build_filename (dir, "installed.syntax", (char *) NULL);
    write_file (lang,
                "context default\n"
                "  keyword whole int yellow\n");
    path = g_build_filename (dir, "Syntax", (char *) NULL);
    content = g_strdup_printf ("file \\.bar$ Bar\ninclude %s\n"
                               "file \\.foo$ InstalledFoo\ninclude %s\n",
                               lang, lang);
    write_file (path, content);

    g_free (content);
    g_free (path);
    g_free (lang);
    g_free (dir);

    return share;
}

/* --------------------------------------------------------------------------------------------- */

static void
remove_installed_syntax (char *share)
{
    char *dir, *path;

    dir = g_build_filename (share, "syntax", (char *) NULL);
    path = g_build_filename (dir, "Syntax", (char *) NULL);
    unlink (path);
    g_free (path);
    path = g_build_filename (dir, "installed.syntax", (char *) NULL);
    unlink (path);
    g_free (path);
    rmdir (dir);
    rmdir (share);
    g_free (dir);
    g_free (share);
}

/* --------------------------------------------------------------------------------------------- */

/** The type the Syntax file of the user and the installed one choose for @filename. */
static const char *
type_for (const char *filename)
{
    syntax_select_t sel;

    sel.type = NULL;
    sel.filename = filename;
    sel.first_line = "";
    syntax_rules_unref (rules);
    rules = NULL;
    if (syntax_rules_load (syntax_file, &sel, &rules, NULL) != 0)
        return NULL;

    return syntax_rules_type (rules);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_user_syntax_inherits_installed)
{
    char *saved_share = mc_global.share_data_dir;
    char *share;

    share = make_installed_syntax ();
    mc_global.share_data_dir = share;

    // the file of the user names .foo files, the installed one the rest
    write_file (syntax_file,
                "file \\.foo$ UserFoo\n"
                "context default\n"
                "  keyword whole int red\n");
    ck_assert_str_eq (type_for ("x.foo"), "UserFoo");
    ck_assert_str_eq (type_for ("x.bar"), "Bar");
    ck_assert_ptr_null (type_for ("x.none"));

    // a file of the user that names nothing leaves everything to the installed one
    write_file (syntax_file, "# nothing here yet\n");
    ck_assert_str_eq (type_for ("x.foo"), "InstalledFoo");
    ck_assert_str_eq (type_for ("x.bar"), "Bar");

    mc_global.share_data_dir = saved_share;
    remove_installed_syntax (share);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_user_syntax_types_listed_with_installed)
{
    char *saved_share = mc_global.share_data_dir;
    char *share;
    GPtrArray *names;

    share = make_installed_syntax ();
    mc_global.share_data_dir = share;

    write_file (syntax_file,
                "file \\.foo$ UserFoo\n"
                "context default\n"
                "file \\.baz$ Bar\n"
                "context default\n");

    // the types of the user first, then the installed ones not named yet
    names = g_ptr_array_new_with_free_func (g_free);
    ck_assert_int_eq (syntax_rules_list_types (syntax_file, names), 0);
    ck_assert_int_eq ((int) names->len, 3);
    ck_assert_str_eq (g_ptr_array_index (names, 0), "UserFoo");
    ck_assert_str_eq (g_ptr_array_index (names, 1), "Bar");
    ck_assert_str_eq (g_ptr_array_index (names, 2), "InstalledFoo");
    g_ptr_array_free (names, TRUE);

    mc_global.share_data_dir = saved_share;
    remove_installed_syntax (share);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_errors)
{
    // the default context has no delimiters to embed between
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "  embed Inner\n" INNER_RULES),
                      3);
    // one type, named once
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context < > green\n"
                                     "  embed\n"),
                      4);
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context < > green\n"
                                     "  embed Inner Inner\n"),
                      4);
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context < > green\n"
                                     "  embed soft\n"),
                      4);
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context < > green\n"
                                     "  embed Inner\n"
                                     "  embed Inner\n" INNER_RULES),
                      5);
    // rules that keep no state between lines have no contexts to copy in
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context < > green\n"
                                     "  embed Plain\n"
                                     "file \\.none$ Plain\n"
                                     "line-local\n"
                                     "number 4 red\n"),
                      4);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_embed_state_from_checkpoints)
{
    GString *s;
    syntax_scanner_t *sc;
    char *forward;
    off_t i;

    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "context <s> </s> green\n"
                                     "  embed Inner\n" INNER_RULES),
                      0);

    // a body long enough to be checkpointed on the way, walked back to front
    s = g_string_new ("x <s>");
    for (i = 0; i < 300; i++)
        g_string_append (s, "int \"a\" ");
    g_string_append (s, "</s> x");

    forward = mask (s->str);

    sc = syntax_scanner_new (rules, get_byte, NULL, text_len);
    for (i = text_len - 1; i >= 0; i--)
    {
        syntax_state_t st = syntax_state_at (sc, i);
        const char *fg = NULL, *bg, *attrs;
        guint color;

        color = syntax_rules_color_of (rules, st);
        if (color != 0)
            syntax_rules_color_spec (rules, color, &fg, &bg, &attrs);
        ck_assert_int_eq (fg == NULL || *fg == '\0' ? '.' : *fg, forward[i]);
    }
    syntax_scanner_free (sc);

    g_free (forward);
    g_string_free (s, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static void
add_tests (TCase *tc_core)
{
    tcase_add_test (tc_core, test_plain_keyword);
    tcase_add_test (tc_core, test_whole_word);
    tcase_add_test (tc_core, test_line_start);
    tcase_add_test (tc_core, test_star);
    tcase_add_test (tc_core, test_plus);
    tcase_add_test (tc_core, test_bracket);
    tcase_add_test (tc_core, test_brace);
    tcase_add_test (tc_core, test_keyword_starting_with_bracket);
    tcase_add_test (tc_core, test_keyword_starting_with_two_brackets);
    tcase_add_test (tc_core, test_keyword_starting_with_star);
    tcase_add_test (tc_core, test_keyword_starting_with_plus);
    tcase_add_test (tc_core, test_keyword_starting_with_brace);
    tcase_add_test (tc_core, test_two_keywords_that_both_fit);
    tcase_add_test (tc_core, test_case_insensitive);
    tcase_add_test (tc_core, test_context);
    tcase_add_test (tc_core, test_context_exclusive);
    tcase_add_test (tc_core, test_context_oneline);
    tcase_add_test (tc_core, test_context_linestart);
    tcase_add_test (tc_core, test_keyword_in_context);
    tcase_add_test (tc_core, test_plus_corners);
    tcase_add_test (tc_core, test_bracket_corners);
    tcase_add_test (tc_core, test_keyword_over_newline);
    tcase_add_test (tc_core, test_nul_byte);
    tcase_add_test (tc_core, test_exclusive_empty);
    tcase_add_test (tc_core, test_exclusive_first_byte);
    tcase_add_test (tc_core, test_newline_keyword_in_newline_context);
    tcase_add_test (tc_core, test_keyword_at_context_start);
    tcase_add_test (tc_core, test_context_over_keyword_starting_with_it);
    tcase_add_test (tc_core, test_newline_keyword_at_context_start);
    tcase_add_test (tc_core, test_wholechars);
    tcase_add_test (tc_core, test_context_word_borders);
    tcase_add_test (tc_core, test_context_linestart_right);
    tcase_add_test (tc_core, test_spellcheck_and_comments);
    tcase_add_test (tc_core, test_keyword_color_inherited);
    tcase_add_test (tc_core, test_define);
    tcase_add_test (tc_core, test_parse_errors);
    tcase_add_test (tc_core, test_rules_without_include);
    tcase_add_test (tc_core, test_include_that_is_not_there);
    tcase_add_test (tc_core, test_second_file_line_ends_the_rules);
    tcase_add_test (tc_core, test_full_colors);
    tcase_add_test (tc_core, test_more_parse_errors);
    tcase_add_test (tc_core, test_error_file_names_the_include);
    tcase_add_test (tc_core, test_truncated_context_and_keyword_lines);
    tcase_add_test (tc_core, test_no_syntax_file_at_all);
    tcase_add_test (tc_core, test_type_chosen_by_caller);
    tcase_add_test (tc_core, test_type_chosen_by_first_line);
    tcase_add_test (tc_core, test_nothing_to_choose_by);
    tcase_add_test (tc_core, test_list_of_types);
    tcase_add_test (tc_core, test_include_before_any_file_line);
    tcase_add_test (tc_core, test_include_that_opens);
    tcase_add_test (tc_core, test_include_then_a_file_line);
    tcase_add_test (tc_core, test_rules_without_any_keyword);
    tcase_add_test (tc_core, test_broken_file_line);
    tcase_add_test (tc_core, test_error_inside_the_chosen_rules);
    tcase_add_test (tc_core, test_embed);
    tcase_add_test (tc_core, test_embed_escape);
    tcase_add_test (tc_core, test_embed_exclusive);
    tcase_add_test (tc_core, test_embed_case);
    tcase_add_test (tc_core, test_embed_nested);
    tcase_add_test (tc_core, test_embed_itself);
    tcase_add_test (tc_core, test_embed_shared);
    tcase_add_test (tc_core, test_embed_left_ends_like_right);
    tcase_add_test (tc_core, test_embed_soft);
    tcase_add_test (tc_core, test_embed_soft_keyword);
    tcase_add_test (tc_core, test_embed_soft_nested);
    tcase_add_test (tc_core, test_embed_unknown_type);
    tcase_add_test (tc_core, test_embed_from_installed_syntax);
    tcase_add_test (tc_core, test_user_syntax_inherits_installed);
    tcase_add_test (tc_core, test_user_syntax_types_listed_with_installed);
    tcase_add_test (tc_core, test_embed_errors);
    tcase_add_test (tc_core, test_embed_state_from_checkpoints);
}

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);
    add_tests (tc_core);

    return mctest_run_all (tc_core);
}
