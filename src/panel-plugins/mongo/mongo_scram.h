/** \file mongo_scram.h
 *  \brief MongoDB plugin: SCRAM-SHA-256 that tolerates any salt length.
 */

#ifndef MC_PANEL_MONGO_SCRAM_H
#define MC_PANEL_MONGO_SCRAM_H

#include "lib/global.h"

struct _mongoc_stream_t;
typedef struct _mongoc_stream_t mongoc_stream_t;

/*** enums *************************************************************************************/

typedef enum
{
    MONGO_SCRAM_OK = 0,
    /* the server refused the proof sent for its short salt */
    MONGO_SCRAM_FAILED,
    /* the workaround did not apply: no short salt came, so no proof was sent; the server
       refused the conversation, or sent a salt libmongoc takes. libmongoc's own verdict is the
       one to trust */
    MONGO_SCRAM_NOT_APPLIED
} mongo_scram_result_t;

/*** structures declarations (and typedefs of structures)***************************************/

/* The salted password and what it was derived from, so that a reconnect to
   the same server skips the key derivation. Zero-initialize before use. */
typedef struct
{
    guint8 *salt;
    gsize salt_len;
    int iterations;
    guint8 salted[32];
} mongo_scram_cache_t;

/*** API ***************************************************************************************/

/* Wipe and free what @cache holds; @cache itself stays usable. */
void mongo_scram_cache_clear (mongo_scram_cache_t *cache);

/* "n=<user>,r=<nonce>" with '=' and ',' of @user escaped. Caller g_free. */
char *mongo_scram_client_first_bare (const char *user, const char *nonce);

/* Answer @server_first: the client-final message to send and the "v=..." the
   server must send back. FALSE with @err_out set on a malformed message. */
gboolean mongo_scram_client_final (const char *password, const char *client_first_bare,
                                   const char *server_first, char **client_final,
                                   char **server_final, char **err_out);

/* Authenticate a fresh, not yet handshaken @stream as @user on @source.
   libmongoc refuses a salt shorter than the hash minus four bytes, which
   some proxies send; this conversation accepts it and sends its proof only
   for a salt of another length. Its isMaster is the stream's first hello,
   after which a genuine mongod refuses the client metadata of libmongoc's
   handshake: this is for servers libmongoc cannot authenticate with, not a
   replacement of its own authentication. @cache may be NULL. */
mongo_scram_result_t mongo_scram_auth_stream (mongoc_stream_t *stream, const char *user,
                                              const char *password, const char *source,
                                              int timeout_ms, mongo_scram_cache_t *cache,
                                              char **err_out);

#endif /* MC_PANEL_MONGO_SCRAM_H */
