/*
   tests/src/panel-plugins/mongo/mongo_scram_test.c -- unit tests for the SCRAM
   conversation and the URI without credentials

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

#define TEST_SUITE_NAME "/src/panel-plugins/mongo"

#include "tests/mctest.h"

#include <string.h>

#include <bson/bson.h>

#include "src/panel-plugins/mongo/mongo_conn.h"
#include "src/panel-plugins/mongo/mongo_scram.h"

/* --------------------------------------------------------------------------------------------- */
/* mongo_scram_client_first_bare                                                                 */
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_first_bare_plain)
{
    char *s;

    s = mongo_scram_client_first_bare ("user", "rOprNGfwEbeRWgbNEkqO");
    mctest_assert_str_eq (s, "n=user,r=rOprNGfwEbeRWgbNEkqO");
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_first_bare_escapes)
{
    char *s;

    s = mongo_scram_client_first_bare ("a=b,c", "x");
    mctest_assert_str_eq (s, "n=a=3Db=2Cc,r=x");
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* mongo_scram_client_final                                                                      */
/* --------------------------------------------------------------------------------------------- */

/* the SCRAM-SHA-256 example of RFC 7677 */
START_TEST (test_final_rfc7677)
{
    char *final, *server, *err = NULL;
    gboolean ok;

    ok = mongo_scram_client_final (
        "pencil", "n=user,r=rOprNGfwEbeRWgbNEkqO",
        "r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,s=W22ZaJ0SNY7soEsUEjb6gQ==,i=4096",
        &final, &server, &err);
    mctest_assert_true (ok);
    mctest_assert_null (err);
    mctest_assert_str_eq (final,
                          "c=biws,r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,"
                          "p=dHzbZapWIk4jUhN+Ute9ytag9zjfMHgsqmmiz7AndVQ=");
    mctest_assert_str_eq (server, "v=6rriTRBi23WpRR/wtup+mMhUZUn/dB5nLTJRsjl95G4=");
    g_free (final);
    g_free (server);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* a 12-byte salt, which libmongoc refuses */
START_TEST (test_final_short_salt)
{
    char *final, *server, *err = NULL;
    gboolean ok;

    ok = mongo_scram_client_final (
        "noop", "n=noop,r=eE/05rra6O3KuV8uqkYSqyUIymtxLZeq",
        "r=eE/05rra6O3KuV8uqkYSqyUIymtxLZeqgU7M43rzI1enu3gOP6mFMcWc2WksGqlL,"
        "s=c2VydmVyLW5vbmNl,i=4096",
        &final, &server, &err);
    mctest_assert_true (ok);
    mctest_assert_str_eq (
        final,
        "c=biws,r=eE/05rra6O3KuV8uqkYSqyUIymtxLZeqgU7M43rzI1enu3gOP6mFMcWc2WksGqlL,"
        "p=DhjSzFkmL3zNdm1rk5BSk8L1mijz0EiAhW2EYOv0MuU=");
    mctest_assert_str_eq (server, "v=BCGcpO4Ijj3OMsyfLt1+MeH4uWKZ9u/pEa+QcXYa02Y=");
    g_free (final);
    g_free (server);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static const struct reject_ds
{
    const char *server_first;
} reject_ds[] = {
    /* the server nonce does not extend the client one */
    { "r=other,s=c2FsdA==,i=4096" },
    /* nothing added to the client nonce */
    { "r=abc,s=c2FsdA==,i=4096" },
    /* too few iterations */
    { "r=abcdef,s=c2FsdA==,i=1000" },
    /* so many iterations that the screen would hang */
    { "r=abcdef,s=c2FsdA==,i=2147483647" },
    /* no salt */
    { "r=abcdef,i=4096" },
    /* a mandatory extension */
    { "m=ext,r=abcdef,s=c2FsdA==,i=4096" },
};

START_PARAMETRIZED_TEST (test_final_rejects, reject_ds)
{
    char *final, *server, *err = NULL;
    gboolean ok;

    ok = mongo_scram_client_final ("pw", "n=u,r=abc", data->server_first, &final, &server, &err);
    mctest_assert_false (ok);
    mctest_assert_not_null (err);
    mctest_assert_null (final);
    mctest_assert_null (server);
    g_free (err);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */
/* mongo_conn_uri_without_auth                                                                   */
/* --------------------------------------------------------------------------------------------- */

static const struct uri_ds
{
    const char *in;
    const char *out;
} uri_ds[] = {
    { "mongodb://noop:noop@127.0.0.1:27018/?directConnection=true",
      "mongodb://127.0.0.1:27018/?directConnection=true" },
    { "mongodb://u:p%40ss@h1,h2:27017/"
      "db?authSource=admin&replicaSet=rs&authMechanism=SCRAM-SHA-256",
      "mongodb://h1,h2:27017/db?replicaSet=rs" },
    { "mongodb://u@h/?AUTHMECHANISM=SCRAM-SHA-256&tls=true", "mongodb://h/?tls=true" },
    { "mongodb+srv://u:p@cluster.example.net", "mongodb+srv://cluster.example.net" },
    { "mongodb://h:1/?authSource=x", "mongodb://h:1/" },
    { "mongodb://h", "mongodb://h" },
};

START_PARAMETRIZED_TEST (test_uri_without_auth, uri_ds)
{
    char *s;

    s = mongo_conn_uri_without_auth (data->in);
    mctest_assert_str_eq (s, data->out);
    g_free (s);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_first_bare_plain);
    tcase_add_test (tc_core, test_first_bare_escapes);
    tcase_add_test (tc_core, test_final_rfc7677);
    tcase_add_test (tc_core, test_final_short_salt);
    mctest_add_parameterized_test (tc_core, test_final_rejects, reject_ds);
    mctest_add_parameterized_test (tc_core, test_uri_without_auth, uri_ds);

    return mctest_run_all (tc_core);
}
