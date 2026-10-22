/*
   Panel plugin mongo for the M-Commander
   SCRAM-SHA-256 authentication that tolerates any salt length

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

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <mongoc/mongoc.h>
#include <bson/bson.h>

#include "lib/global.h"

#include "mongo_scram.h"

/*** file scope macro definitions ****************************************************************/

#define SCRAM_MECHANISM   "SCRAM-SHA-256"
#define SCRAM_HASH_LEN    32
#define SCRAM_NONCE_BYTES 24
/* the salt length libmongoc insists on */
#define SCRAM_STD_SALT_LEN (SCRAM_HASH_LEN - 4)
/* libmongoc's own floor for SCRAM-SHA-256 */
#define SCRAM_MIN_ITER 4096
/* a ceiling, so that a broken server cannot hold the screen for minutes */
#define SCRAM_MAX_ITER 1000000

#define OP_REPLY       1
#define OP_QUERY       2004
#define OP_MSG         2013
#define MSG_HEADER_LEN 16
#define MSG_MAX_LEN    (48 * 1024 * 1024)

/*** file scope functions ************************************************************************/

/* memset that the compiler may not drop as a dead store */
static void
wipe (void *p, gsize n)
{
    volatile guint8 *v = (volatile guint8 *) p;

    while (n-- != 0)
        *v++ = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
hmac_sha256 (const guint8 *key, gsize key_len, const guint8 *data, gsize data_len,
             guint8 out[SCRAM_HASH_LEN])
{
    GHmac *h;
    gsize len = SCRAM_HASH_LEN;

    h = g_hmac_new (G_CHECKSUM_SHA256, key, key_len);
    g_hmac_update (h, data, data_len);
    g_hmac_get_digest (h, out, &len);
    g_hmac_unref (h);
}

/* --------------------------------------------------------------------------------------------- */

/* PBKDF2-HMAC-SHA256 for one block: the derived key is exactly one hash long. */
static void
salt_password (const char *password, const guint8 *salt, gsize salt_len, int iterations,
               guint8 out[SCRAM_HASH_LEN])
{
    GHmac *keyed;
    guint8 *first;
    guint8 u[SCRAM_HASH_LEN];
    int i, k;

    /* the key is set up once; each round starts from a copy of it */
    keyed = g_hmac_new (G_CHECKSUM_SHA256, (const guint8 *) password, strlen (password));

    first = g_malloc (salt_len + 4);
    memcpy (first, salt, salt_len);
    memcpy (first + salt_len, "\0\0\0\1", 4);

    for (i = 0; i < iterations; i++)
    {
        GHmac *h;
        gsize len = SCRAM_HASH_LEN;

        h = g_hmac_copy (keyed);
        if (i == 0)
            g_hmac_update (h, first, salt_len + 4);
        else
            g_hmac_update (h, u, SCRAM_HASH_LEN);
        g_hmac_get_digest (h, u, &len);
        g_hmac_unref (h);

        if (i == 0)
            memcpy (out, u, SCRAM_HASH_LEN);
        else
            for (k = 0; k < SCRAM_HASH_LEN; k++)
                out[k] ^= u[k];
    }

    g_hmac_unref (keyed);
    g_free (first);
    wipe (u, sizeof (u));
}

/* --------------------------------------------------------------------------------------------- */

/* NULL when the system gives no random bytes: a nonce that can be guessed is no nonce. */
static char *
random_nonce (void)
{
    guint8 raw[SCRAM_NONCE_BYTES];
    FILE *f;
    gboolean got = FALSE;
    char *nonce;

    f = fopen ("/dev/urandom", "rb");
    if (f != NULL)
    {
        got = fread (raw, 1, sizeof (raw), f) == sizeof (raw);
        fclose (f);
    }
    nonce = got ? g_base64_encode (raw, sizeof (raw)) : NULL;
    wipe (raw, sizeof (raw));
    return nonce;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
send_command (mongoc_stream_t *stream, int32_t opcode, const bson_t *cmd, int timeout_ms,
              char **err_out)
{
    static int32_t request_id = 0;
    GByteArray *msg;
    int32_t v;
    mongoc_iovec_t iov;
    ssize_t n;

    msg = g_byte_array_new ();
    /* the length, patched below */
    v = 0;
    g_byte_array_append (msg, (const guint8 *) &v, 4);
    v = GINT32_TO_LE (++request_id);
    g_byte_array_append (msg, (const guint8 *) &v, 4);
    v = 0;
    g_byte_array_append (msg, (const guint8 *) &v, 4);
    v = GINT32_TO_LE (opcode);
    g_byte_array_append (msg, (const guint8 *) &v, 4);

    if (opcode == OP_QUERY)
    {
        v = 0;
        g_byte_array_append (msg, (const guint8 *) &v, 4);
        g_byte_array_append (msg, (const guint8 *) "admin.$cmd", sizeof ("admin.$cmd"));
        g_byte_array_append (msg, (const guint8 *) &v, 4);
        v = GINT32_TO_LE (-1);
        g_byte_array_append (msg, (const guint8 *) &v, 4);
    }
    else
    {
        /* flag bits, then a section of kind 0 */
        v = 0;
        g_byte_array_append (msg, (const guint8 *) &v, 4);
        g_byte_array_append (msg, (const guint8 *) "", 1);
    }
    g_byte_array_append (msg, bson_get_data (cmd), cmd->len);

    v = GINT32_TO_LE ((int32_t) msg->len);
    memcpy (msg->data, &v, 4);

    iov.iov_base = msg->data;
    iov.iov_len = msg->len;
    n = mongoc_stream_writev (stream, &iov, 1, timeout_ms);
    if (n != (ssize_t) msg->len)
    {
        *err_out = g_strdup ("Authentication: could not send to the server.");
        g_byte_array_free (msg, TRUE);
        return FALSE;
    }

    g_byte_array_free (msg, TRUE);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Read one reply and copy its document into @reply (left uninitialized on failure). */
static gboolean
recv_reply (mongoc_stream_t *stream, int timeout_ms, bson_t *reply, char **err_out)
{
    guint8 header[MSG_HEADER_LEN];
    guint8 *body;
    int32_t len, opcode, doc_len;
    gsize body_len, off;
    bson_t view;

    if (mongoc_stream_read (stream, header, sizeof (header), sizeof (header), timeout_ms)
        != (ssize_t) sizeof (header))
    {
        *err_out = g_strdup ("Authentication: the server closed the connection.");
        return FALSE;
    }

    memcpy (&len, header, 4);
    len = GINT32_FROM_LE (len);
    memcpy (&opcode, header + 12, 4);
    opcode = GINT32_FROM_LE (opcode);
    if (len <= MSG_HEADER_LEN || len > MSG_MAX_LEN || (opcode != OP_REPLY && opcode != OP_MSG))
    {
        *err_out = g_strdup ("Authentication: malformed reply from the server.");
        return FALSE;
    }

    body_len = (gsize) len - MSG_HEADER_LEN;
    body = g_malloc (body_len);
    if (mongoc_stream_read (stream, body, body_len, body_len, timeout_ms) != (ssize_t) body_len)
    {
        g_free (body);
        *err_out = g_strdup ("Authentication: the server closed the connection.");
        return FALSE;
    }

    /* OP_REPLY: flags, cursor id, starting from, number returned.
       OP_MSG: flag bits and the kind byte of the body section. */
    off = opcode == OP_REPLY ? 20 : 5;
    if (body_len < off + 5 || (opcode == OP_MSG && body[4] != 0))
    {
        g_free (body);
        *err_out = g_strdup ("Authentication: malformed reply from the server.");
        return FALSE;
    }
    memcpy (&doc_len, body + off, 4);
    doc_len = GINT32_FROM_LE (doc_len);
    if (doc_len < 5 || (gsize) doc_len > body_len - off
        || !bson_init_static (&view, body + off, (size_t) doc_len))
    {
        g_free (body);
        *err_out = g_strdup ("Authentication: malformed reply from the server.");
        return FALSE;
    }

    bson_copy_to (&view, reply);
    g_free (body);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Send @cmd and check "ok" of the reply; @reply is initialized on success only. */
static gboolean
run_command (mongoc_stream_t *stream, int32_t opcode, const bson_t *cmd, int timeout_ms,
             bson_t *reply, char **err_out)
{
    bson_iter_t it;

    if (!send_command (stream, opcode, cmd, timeout_ms, err_out)
        || !recv_reply (stream, timeout_ms, reply, err_out))
        return FALSE;

    if (bson_iter_init_find (&it, reply, "ok") && bson_iter_as_bool (&it))
        return TRUE;

    if (bson_iter_init_find (&it, reply, "errmsg") && BSON_ITER_HOLDS_UTF8 (&it))
        *err_out = g_strdup (bson_iter_utf8 (&it, NULL));
    else
        *err_out = g_strdup ("Authentication failed.");
    bson_destroy (reply);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* Pull conversationId and the payload text out of a saslStart/saslContinue reply. */
static gboolean
sasl_reply_parts (const bson_t *doc, bson_value_t *conv_id, char **payload, gboolean *done,
                  char **err_out)
{
    bson_iter_t it;
    const uint8_t *data;
    uint32_t len;
    bson_subtype_t subtype;

    if (!bson_iter_init_find (&it, doc, "payload") || !BSON_ITER_HOLDS_BINARY (&it))
    {
        *err_out = g_strdup ("Authentication: the server sent no SASL payload.");
        return FALSE;
    }
    bson_iter_binary (&it, &subtype, &len, &data);
    *payload = g_strndup ((const char *) data, len);

    if (conv_id != NULL)
    {
        if (!bson_iter_init_find (&it, doc, "conversationId"))
        {
            g_free (*payload);
            *payload = NULL;
            *err_out = g_strdup ("Authentication: the server sent no SASL conversation id.");
            return FALSE;
        }
        bson_value_copy (bson_iter_value (&it), conv_id);
    }

    if (done != NULL)
        *done = bson_iter_init_find (&it, doc, "done") && bson_iter_as_bool (&it);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static bson_t *
sasl_start_doc (const char *client_first, const char *source, gboolean speculative)
{
    bson_t *doc;
    bson_t opts;

    doc = bson_new ();
    BSON_APPEND_INT32 (doc, "saslStart", 1);
    BSON_APPEND_UTF8 (doc, "mechanism", SCRAM_MECHANISM);
    BSON_APPEND_BINARY (doc, "payload", BSON_SUBTYPE_BINARY, (const uint8_t *) client_first,
                        (uint32_t) strlen (client_first));
    BSON_APPEND_INT32 (doc, "autoAuthorize", 1);
    BSON_APPEND_DOCUMENT_BEGIN (doc, "options", &opts);
    BSON_APPEND_BOOL (&opts, "skipEmptyExchange", true);
    bson_append_document_end (doc, &opts);
    BSON_APPEND_UTF8 (doc, speculative ? "db" : "$db", source);
    return doc;
}

/* --------------------------------------------------------------------------------------------- */

static bson_t *
sasl_continue_doc (const bson_value_t *conv_id, const char *payload, const char *source)
{
    bson_t *doc;

    doc = bson_new ();
    BSON_APPEND_INT32 (doc, "saslContinue", 1);
    BSON_APPEND_VALUE (doc, "conversationId", conv_id);
    BSON_APPEND_BINARY (doc, "payload", BSON_SUBTYPE_BINARY, (const uint8_t *) payload,
                        (uint32_t) strlen (payload));
    BSON_APPEND_UTF8 (doc, "$db", source);
    return doc;
}

/* --------------------------------------------------------------------------------------------- */

/* "v=..." up to an extension, if any. */
static gboolean
server_final_matches (const char *got, const char *expected)
{
    const size_t n = strlen (expected);

    return strncmp (got, expected, n) == 0 && (got[n] == '\0' || got[n] == ',');
}

/* --------------------------------------------------------------------------------------------- */

/* mongo_scram_client_final with a @cache of the salted password; with
   @refuse_standard_salt a salt of libmongoc's length makes no proof. */
static mongo_scram_result_t
client_final_impl (const char *password, const char *client_first_bare, const char *server_first,
                   mongo_scram_cache_t *cache, gboolean refuse_standard_salt, char **client_final,
                   char **server_final, char **err_out)
{
    const char *client_nonce;
    const char *r = NULL, *s = NULL, *i = NULL;
    char **parts;
    guint8 *salt;
    gsize salt_len;
    gint64 iterations;
    guint8 salted[SCRAM_HASH_LEN], client_key[SCRAM_HASH_LEN], stored_key[SCRAM_HASH_LEN];
    guint8 server_key[SCRAM_HASH_LEN], client_sig[SCRAM_HASH_LEN], server_sig[SCRAM_HASH_LEN];
    gsize len;
    char *without_proof, *auth_message, *b64;
    int k;

    *client_final = NULL;
    *server_final = NULL;

    client_nonce = strstr (client_first_bare, ",r=");
    if (client_nonce == NULL)
    {
        *err_out = g_strdup ("Authentication: bad client-first message.");
        return MONGO_SCRAM_FAILED;
    }
    client_nonce += 3;

    parts = g_strsplit (server_first, ",", -1);
    for (char **p = parts; *p != NULL; p++)
        if (strncmp (*p, "r=", 2) == 0 && r == NULL)
            r = *p + 2;
        else if (strncmp (*p, "s=", 2) == 0 && s == NULL)
            s = *p + 2;
        else if (strncmp (*p, "i=", 2) == 0 && i == NULL)
            i = *p + 2;
        else if (strncmp (*p, "m=", 2) == 0)
        {
            g_strfreev (parts);
            *err_out = g_strdup ("Authentication: the server requires an unknown SCRAM extension.");
            return MONGO_SCRAM_FAILED;
        }

    if (r == NULL || s == NULL || i == NULL || !g_str_has_prefix (r, client_nonce)
        || strlen (r) == strlen (client_nonce))
    {
        g_strfreev (parts);
        *err_out = g_strdup ("Authentication: bad server-first message.");
        return MONGO_SCRAM_FAILED;
    }

    iterations = g_ascii_strtoll (i, NULL, 10);
    salt = g_base64_decode (s, &salt_len);
    if (iterations < SCRAM_MIN_ITER || iterations > SCRAM_MAX_ITER || salt_len == 0)
    {
        g_free (salt);
        g_strfreev (parts);
        *err_out = g_strdup ("Authentication: bad salt or iteration count from the server.");
        return MONGO_SCRAM_FAILED;
    }

    if (refuse_standard_salt && salt_len == SCRAM_STD_SALT_LEN)
    {
        g_free (salt);
        g_strfreev (parts);
        *err_out = g_strdup ("Authentication: the server's salt needs no workaround.");
        return MONGO_SCRAM_NOT_APPLIED;
    }

    if (cache != NULL && cache->salt != NULL && cache->salt_len == salt_len
        && cache->iterations == (int) iterations && memcmp (cache->salt, salt, salt_len) == 0)
        memcpy (salted, cache->salted, SCRAM_HASH_LEN);
    else
    {
        salt_password (password, salt, salt_len, (int) iterations, salted);
        if (cache != NULL)
        {
            mongo_scram_cache_clear (cache);
            cache->salt = g_memdup2 (salt, salt_len);
            cache->salt_len = salt_len;
            cache->iterations = (int) iterations;
            memcpy (cache->salted, salted, SCRAM_HASH_LEN);
        }
    }
    g_free (salt);

    hmac_sha256 (salted, SCRAM_HASH_LEN, (const guint8 *) "Client Key", 10, client_key);
    len = SCRAM_HASH_LEN;
    {
        GChecksum *c;

        c = g_checksum_new (G_CHECKSUM_SHA256);
        g_checksum_update (c, client_key, SCRAM_HASH_LEN);
        g_checksum_get_digest (c, stored_key, &len);
        g_checksum_free (c);
    }

    /* "biws" is base64 of the GS2 header "n,," */
    without_proof = g_strdup_printf ("c=biws,r=%s", r);
    g_strfreev (parts);
    auth_message = g_strdup_printf ("%s,%s,%s", client_first_bare, server_first, without_proof);

    hmac_sha256 (stored_key, SCRAM_HASH_LEN, (const guint8 *) auth_message, strlen (auth_message),
                 client_sig);
    for (k = 0; k < SCRAM_HASH_LEN; k++)
        client_key[k] ^= client_sig[k];
    b64 = g_base64_encode (client_key, SCRAM_HASH_LEN);
    *client_final = g_strdup_printf ("%s,p=%s", without_proof, b64);
    g_free (b64);

    hmac_sha256 (salted, SCRAM_HASH_LEN, (const guint8 *) "Server Key", 10, server_key);
    hmac_sha256 (server_key, SCRAM_HASH_LEN, (const guint8 *) auth_message, strlen (auth_message),
                 server_sig);
    b64 = g_base64_encode (server_sig, SCRAM_HASH_LEN);
    *server_final = g_strdup_printf ("v=%s", b64);
    g_free (b64);

    wipe (salted, sizeof (salted));
    wipe (client_key, sizeof (client_key));
    wipe (stored_key, sizeof (stored_key));
    wipe (server_key, sizeof (server_key));
    g_free (auth_message);
    g_free (without_proof);
    return MONGO_SCRAM_OK;
}

/*** public functions ****************************************************************************/

void
mongo_scram_cache_clear (mongo_scram_cache_t *cache)
{
    if (cache->salt != NULL)
        wipe (cache->salt, cache->salt_len);
    g_free (cache->salt);
    cache->salt = NULL;
    cache->salt_len = 0;
    cache->iterations = 0;
    wipe (cache->salted, sizeof (cache->salted));
}

/* --------------------------------------------------------------------------------------------- */

char *
mongo_scram_client_first_bare (const char *user, const char *nonce)
{
    GString *s;

    s = g_string_new ("n=");
    for (const char *p = user; *p != '\0'; p++)
        if (*p == '=')
            g_string_append (s, "=3D");
        else if (*p == ',')
            g_string_append (s, "=2C");
        else
            g_string_append_c (s, *p);
    g_string_append_printf (s, ",r=%s", nonce);
    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mongo_scram_client_final (const char *password, const char *client_first_bare,
                          const char *server_first, char **client_final, char **server_final,
                          char **err_out)
{
    return client_final_impl (password, client_first_bare, server_first, NULL, FALSE, client_final,
                              server_final, err_out)
        == MONGO_SCRAM_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

mongo_scram_result_t
mongo_scram_auth_stream (mongoc_stream_t *stream, const char *user, const char *password,
                         const char *source, int timeout_ms, mongo_scram_cache_t *cache,
                         char **err_out)
{
    char *nonce, *first_bare, *client_first;
    char *server_first = NULL, *client_final = NULL, *server_final = NULL, *got = NULL;
    bson_value_t conv_id;
    gboolean have_conv = FALSE, done = FALSE;
    // until a proof goes out for a short salt, nothing is the workaround's doing
    mongo_scram_result_t result = MONGO_SCRAM_NOT_APPLIED;
    bson_t *cmd, *sasl;
    bson_t reply;
    bson_iter_t it;

    *err_out = NULL;

    nonce = random_nonce ();
    if (nonce == NULL)
    {
        *err_out = g_strdup ("Authentication: /dev/urandom cannot be read for the nonce.");
        return MONGO_SCRAM_FAILED;
    }
    first_bare = mongo_scram_client_first_bare (user, nonce);
    client_first = g_strconcat ("n,,", first_bare, NULL);

    /* Speculative authentication rides on the first isMaster; some proxies
       accept no other way. A server that ignores it gets a plain saslStart. */
    cmd = bson_new ();
    BSON_APPEND_INT32 (cmd, "isMaster", 1);
    sasl = sasl_start_doc (client_first, source, TRUE);
    BSON_APPEND_DOCUMENT (cmd, "speculativeAuthenticate", sasl);
    bson_destroy (sasl);
    if (!run_command (stream, OP_QUERY, cmd, timeout_ms, &reply, err_out))
        goto out;
    bson_destroy (cmd);
    cmd = NULL;

    if (bson_iter_init_find (&it, &reply, "speculativeAuthenticate")
        && BSON_ITER_HOLDS_DOCUMENT (&it))
    {
        const uint8_t *data;
        uint32_t len;
        bson_t spec;

        bson_iter_document (&it, &len, &data);
        if (bson_init_static (&spec, data, len))
            have_conv = sasl_reply_parts (&spec, &conv_id, &server_first, NULL, err_out);
        else
            *err_out = g_strdup ("Authentication: malformed reply from the server.");
        bson_destroy (&reply);
        if (!have_conv)
            goto out;
    }
    else
    {
        bson_destroy (&reply);
        cmd = sasl_start_doc (client_first, source, FALSE);
        if (!run_command (stream, OP_MSG, cmd, timeout_ms, &reply, err_out))
            goto out;
        have_conv = sasl_reply_parts (&reply, &conv_id, &server_first, NULL, err_out);
        bson_destroy (&reply);
        if (!have_conv)
            goto out;
        bson_destroy (cmd);
        cmd = NULL;
    }

    if (client_final_impl (password, first_bare, server_first, cache, TRUE, &client_final,
                           &server_final, err_out)
        != MONGO_SCRAM_OK)
        goto out;
    result = MONGO_SCRAM_FAILED;

    cmd = sasl_continue_doc (&conv_id, client_final, source);
    if (!run_command (stream, OP_MSG, cmd, timeout_ms, &reply, err_out))
        goto out;
    bson_destroy (cmd);
    cmd = NULL;
    if (!sasl_reply_parts (&reply, NULL, &got, &done, err_out))
    {
        bson_destroy (&reply);
        goto out;
    }
    bson_destroy (&reply);

    // the server refused the proof: a wrong password, say
    if (strncmp (got, "e=", 2) == 0)
    {
        *err_out = g_strdup_printf ("Authentication failed: %s.", got + 2);
        goto out;
    }
    if (!server_final_matches (got, server_final))
    {
        *err_out = g_strdup ("Authentication: the server signature does not match.");
        goto out;
    }

    /* A server that ignored skipEmptyExchange wants one more empty round. */
    if (!done)
    {
        cmd = sasl_continue_doc (&conv_id, "", source);
        if (!run_command (stream, OP_MSG, cmd, timeout_ms, &reply, err_out))
            goto out;
        done = bson_iter_init_find (&it, &reply, "done") && bson_iter_as_bool (&it);
        bson_destroy (&reply);
        if (!done)
        {
            *err_out = g_strdup ("Authentication: the server did not finish the conversation.");
            goto out;
        }
    }

    result = MONGO_SCRAM_OK;

out:
    if (cmd != NULL)
        bson_destroy (cmd);
    if (have_conv)
        bson_value_destroy (&conv_id);
    g_free (got);
    g_free (server_final);
    g_free (client_final);
    g_free (server_first);
    g_free (client_first);
    g_free (first_bare);
    g_free (nonce);
    return result;
}
