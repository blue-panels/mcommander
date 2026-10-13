/*
   libshfs - internals shared inside the library.

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

/**
 * \file
 * \brief Header: libshfs internals
 *
 * Not part of the public contract in shfs.h. The shell VFS includes it while
 * the two live side by side, so that the protocol layer can move into the
 * library one piece at a time instead of in one jump. When the VFS goes away
 * this header stops having users outside the library.
 */

#ifndef MC__SHFS_PRIV_H
#define MC__SHFS_PRIV_H

#include "shfs.h"

#ifdef ENABLE_SHELL_SSH2
#include "shell_ssh2.h"
#endif

/*** typedefs(not structures) and defined constants **********************************************/

/* Values the protocol carries in the port field. Historic, but part of the
   wire format: a URL says ":C" or ":r" and it arrives here. */
#define SHELL_FLAG_COMPRESSED 1
#define SHELL_FLAG_RSH        2

/* What the remote host turned out to have, as reported by helpers/info. The
   values are a wire format too: the same numbers appear in the script. */
#define SHELL_HAVE_HEAD      1
#define SHELL_HAVE_SED       2
#define SHELL_HAVE_AWK       4
#define SHELL_HAVE_PERL      8
#define SHELL_HAVE_LSQ       16
#define SHELL_HAVE_DATE_MDYT 32
#define SHELL_HAVE_TAIL      64
#define SHELL_HAVE_SHA256    128
#define SHELL_HAVE_MD5       256
#define SHELL_HAVE_NOTRUNC   512
#define SHELL_HAVE_DD        1024

/* Reply classes, as produced by shfs_decode_reply(). Same meaning as the FTP
   reply classes the protocol borrowed them from. */
#define SHFS_REPLY_PRELIM    1
#define SHFS_REPLY_COMPLETE  2
#define SHFS_REPLY_CONTINUE  3
#define SHFS_REPLY_TRANSIENT 4
#define SHFS_REPLY_ERROR     5

/*** structures declarations (and typedefs of structures)*****************************************/

struct shfs_conn_t
{
    int sockr;
    int sockw;
#ifdef ENABLE_SHELL_SSH2
    shell_ssh2_t *ssh2;  // NULL in the pipe+ssh mode
#endif

    /** Cleared by anything that leaves the channel out of step with the
        protocol: a cancellation, a broken reply, a dead peer. Never set again;
        the caller opens a new connection. */
    gboolean alive;

    /** What the remote host can run, probed at connect. */
    shfs_digest_algo_t digest_algos;
    int host_flags;

    /** Prologue prepended to every helper script: the SHELL_HAVE_* exports. */
    GString *env;

    /** The helper scripts, as loaded for this host. Text, not paths: the
        protocol sends the script itself down the channel and the remote
        /bin/sh runs it. */
    char *scr_ls;
    char *scr_exists;
    char *scr_mkdir;
    char *scr_unlink;
    char *scr_chown;
    char *scr_chmod;
    char *scr_utime;
    char *scr_rmdir;
    char *scr_ln;
    char *scr_mv;
    char *scr_hardlink;
    char *scr_get;
    char *scr_send;
    char *scr_append;
    char *scr_info;
    char *scr_putat;
    char *scr_cksumrange;
    char *scr_blockdigests;

    /* Overrides that declare an older revision than this build expects.
       Collected when the scripts are loaded. */
    GPtrArray *stale_helpers;

    /** Asked while the library waits on the channel. */
    gboolean (*cancelled) (void *user_data);
    void *cancel_data;

    /** State of the transfer in progress; one connection carries one at a
        time. */
    gint64 xfer_left;
    gboolean xfer_reading;
    gboolean xfer_writing;
    shfs_digest_algo_t xfer_algo;

    /** Last non-reply line the peer sent, kept for the callers that want it. */
    char reply_str[BUF_MEDIUM];
};

/*** declarations of public functions ************************************************************/

/**
 * Load the helper scripts for @hostname.
 *
 * Each is looked up in the user override directory first, then in the system
 * set, and falls back to the copy compiled into the binary.
 */
void shfs_conn_load_scripts (shfs_conn_t *conn, const char *hostname);

/** Install the cancellation probe used by every operation on this connection. */
void shfs_conn_set_cancel_cb (shfs_conn_t *conn, gboolean (*cancelled) (void *), void *user_data);

/** Read one line, giving up if the cancellation probe says so.
    Returns 1 on a line, 0 on end of stream, EINTR when cancelled. */
int shfs_get_line_cancellable (shfs_conn_t *conn, char *buffer, int size);

/** Parse one line of the ls helper output into @ent. */
void shfs_parse_ls (char *buffer, shfs_entry_t *ent);

/** Set the SHELL_HAVE_* prologue from a host capability mask. */
void shfs_conn_set_env (shfs_conn_t *conn, int host_flags);

/* --- raw channel --- */

ssize_t shfs_write (shfs_conn_t *conn, const void *buf, size_t len);
ssize_t shfs_read (shfs_conn_t *conn, void *buf, size_t len);

/** Read one line ending with @term. FALSE on end of stream. */
gboolean shfs_get_line (shfs_conn_t *conn, char *buffer, int size, char term);

/* --- protocol --- */

int shfs_decode_reply (char *s, gboolean was_garbage);

/** Read replies until a "### " line arrives; returns its class. Any line
    before it is kept in @string_buf when that is not NULL. */
int shfs_get_reply (shfs_conn_t *conn, char *string_buf, int string_len);

/** Send @cmd. When @wait_reply is set, waits for the reply and returns its
    class, otherwise returns SHFS_REPLY_COMPLETE. */
int shfs_command (shfs_conn_t *conn, gboolean wait_reply, gboolean want_string, const char *cmd,
                  size_t cmd_len);

/** Prepend the environment prologue, expand @vars, append @scr, send it. */
int shfs_command_v (shfs_conn_t *conn, gboolean wait_reply, gboolean want_string, const char *scr,
                    const char *vars, ...) G_GNUC_PRINTF (5, 6);
int shfs_command_va (shfs_conn_t *conn, gboolean wait_reply, gboolean want_string, const char *scr,
                     const char *vars, va_list ap) G_GNUC_PRINTF (5, 0);

/** The line the peer sent before its last reply, or an empty string. */
const char *shfs_last_reply_str (const shfs_conn_t *conn);

/*** inline functions ****************************************************************************/

#endif
