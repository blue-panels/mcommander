/** \file resurrect.h
 *  \brief Header: an mc that lost its terminal, and a new mc that takes it over
 *
 *  When the terminal of an mc is gone (the SSH connection dropped, the window
 *  was closed) mc does not have to die with it.  It can wait in a socket for a
 *  new mc, started in another terminal, to hand it the new terminal, and go on
 *  from where it was.  The same idea is called "immortal" in far2l and f4.
 *
 *  This file is the plumbing and knows nothing about mc's screen: a server
 *  that waits and a client that hands over the descriptors of its terminal.
 *  The part that redraws the screen is in src/resurrect.c.
 *
 *  A waiting mc leaves two files in a directory of its user, srv-<pid>.sock
 *  (a stream socket of the AF_UNIX family) and srv-<pid>.info (what a new mc
 *  shows to choose from).  The client connects and sends one message with the
 *  descriptors of its terminal (input, output) and of the write end of a pipe
 *  attached to it (SCM_RIGHTS); the answer is one byte, 'Y' with the pid of the
 *  server after it, or 'N'.  The client then stays as a shim: it passes the
 *  window size changes and the interrupt to the server, and exits with the
 *  code that the server writes to the pipe when it finishes.  A connection
 *  that sends nothing is a probe: it tells the client the server is alive.
 */

#ifndef MC__TTY_RESURRECT_H
#define MC__TTY_RESURRECT_H

#include <glib.h>
#include <sys/types.h>

/*** typedefs(not structures) and defined constants **********************************************/

/* The server touches its files this often (seconds): tmp cleaners remove the old ones. */
#define RESURRECT_KEEPALIVE_SEC 51

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* what a waiting mc tells about itself */
typedef struct
{
    char *sock_path;
    char *info_path;
    pid_t pid;
    char *term;
    char *title;
} resurrect_entry_t;

typedef struct resurrect_server_t resurrect_server_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* FALSE when this build or system cannot pass descriptors between processes */
gboolean resurrect_available (void);

/* The directory of the files of the waiting mc-s of the user, made if needed; NULL if it cannot be
   made or is not safe. */
char *resurrect_dir (void);

/* Start waiting: the socket and the info file are made in the directory. */
resurrect_server_t *resurrect_server_new (const char *dir, const char *term, const char *title);

/* Wait for a client with the right TERM; TRUE, and the three descriptors are ours to close. Does
   not return while there is no one; FALSE only if the socket broke. */
gboolean resurrect_server_wait (resurrect_server_t *srv, int *in_fd, int *out_fd, int *notify_fd);

/* Stop waiting, and remove the files. */
void resurrect_server_free (resurrect_server_t *srv);

/* The waiting mc-s of the directory that are alive and were started with this TERM. The files of
   the dead ones are removed on the way. Free with resurrect_entries_free(). */
GPtrArray *resurrect_list (const char *dir, const char *term);
void resurrect_entries_free (GPtrArray *entries);

/* Hand the terminal to a waiting mc. On success the pid of the server is stored. */
gboolean resurrect_visit (const resurrect_entry_t *entry, const char *term, int in_fd, int out_fd,
                          int notify_wr_fd, pid_t *server_pid);

/* The shim: stays until the server tells its exit code (the result) or is gone (failure). */
int resurrect_shim (int notify_rd_fd, pid_t server_pid);

/* The server writes its exit code to the pipe of the client that took it last. */
void resurrect_send_exit_code (int notify_fd, int code);

/*** inline functions ****************************************************************************/

#endif
