/** \file session_proto.h
 *  \brief Header: what the session plugin, its holder and its client say to each other
 */

#ifndef MC__SESSION_PROTO_H
#define MC__SESSION_PROTO_H

#include <glib.h>

/*** typedefs(not structures) and defined constants **********************************************/

#define SESSION_PROTO_VERSION 1

/* A frame is the type byte, the length of the data (4 bytes, big endian) and the data */
#define SESSION_FRAME_HEADER 5
#define SESSION_FRAME_MAX    (1 << 20)

/* client -> holder */
#define SF_HELLO 'H' /* "version\nTERM\nrows\ncols\n", then the environment as for SF_ATTACHED */
#define SF_INFO  'I' /* nothing; the answer is SF_INFO with "key=value\n" lines */
#define SF_WINCH 'W' /* "rows cols" */
/* both ways */
#define SF_DATA 'D'
/* holder -> client */
#define SF_EXIT     'X' /* the exit code of mc */
#define SF_KICKED   'K' /* another terminal has taken the session */
#define SF_SWITCH   'S' /* mc has let the terminal go: to the session of this id, or a new one */
#define SF_DETACHED 'Q' /* mc has let the terminal go: back to the shell */
#define SF_REFUSED  'R' /* why the session cannot be taken */
/* holder <-> mc */
#define SF_ATTACHED 'A' /* a new terminal is behind mc; "NAME=value\n" or "NAME\n" for unset */
#define SF_CWD      'C' /* the directory of the active panel */
/* a client of the list -> holder -> mc, and the answer back: "ok", "busy" or why not */
#define SF_END 'E'

/* What mc writes to its terminal when it has let it go: the holder forwards what comes before,
   and nothing after. An APC string, which a terminal that sees it by mistake ignores. The word
   starts with the token of the session, so that output of a program that only looks like a mark
   goes to the terminal as it is. */
#define SESSION_MARK_HEAD     "\033_mc-session;"
#define SESSION_MARK_TAIL     "\033\\"
#define SESSION_MARK_DETACH   "detach"
#define SESSION_MARK_NEW      "new"
#define SESSION_MARK_SWITCH   "switch;" /* and the id of the session */
#define SESSION_MARK_WORD_MAX 96        /* the token, ";switch;" and an id fit */

/* Hex digits of the id of a session and of its token */
#define SESSION_ID_LEN    16
#define SESSION_TOKEN_LEN 32

/* The environment of the processes of a session */
#define SESSION_ENV_ID    "MC_SESSION"
#define SESSION_ENV_FD    "MC_SESSION_FD"
#define SESSION_ENV_TOKEN "MC_SESSION_TOKEN"

/* How long the list waits for mc to answer whether it ends */
#define SESSION_END_WAIT_MS (5 * 1000)

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    char *path;
    char *id;
    char *term;
    char *cwd;
    long pid;
    long started;
    gboolean attached;
    gboolean lost;  // attached, but its client does not take what it is sent
} session_info_t;

typedef struct
{
    GString *buf;
    gboolean bad;  // a frame longer than any of ours came: the stream is not to be trusted
} session_reader_t;

/*** global variables defined in .c file *******************************************************/

/* What a new terminal brings to mc: the way to its agents and its display */
extern const char *const session_env_names[];

/*** declarations of public functions ************************************************************/

GString *session_frame_new (char type, const void *data, gsize len);
gboolean session_frame_write (int fd, char type, const void *data, gsize len);
/* The same for a descriptor that may not take it all at once: waits at most timeout_ms in all */
gboolean session_frame_write_timed (int fd, char type, const void *data, gsize len, int timeout_ms);
char *session_random_hex (int digits);

void session_reader_init (session_reader_t *reader);
void session_reader_free (session_reader_t *reader);
/* Reads what fd has; FALSE on the end of the stream or an error. */
gboolean session_reader_fill (session_reader_t *reader, int fd);
/* Takes one complete frame out of the reader; FALSE when there is none yet, or the stream is bad */
gboolean session_reader_next (session_reader_t *reader, char *type, GString *data);

/* The directory of the sockets, made if it is not there; NULL when it is not safe to use. */
char *session_dir (void);

/* -1 also when the session does not take connections now: errno tells which */
int session_connect (const char *path);
/* One frame, waiting at most timeout_ms in all for it */
gboolean session_read_frame (int fd, session_reader_t *reader, char *type, GString *data,
                             int timeout_ms);
const char *session_info_value (char **lines, const char *key);
void session_info_free (gpointer data);
/* The lock a holder keeps on <id>.lock for as long as it lives: -1 when it is taken. The
   descriptor is close-on-exec. */
int session_lock (const char *lock_path);
/* Every session of the directory, oldest first; the files of dead ones are removed */
GPtrArray *session_list (const char *dir);
/* Asks the mc of a session to end: "ok", "busy" or why not */
char *session_end (const char *path);

/*** inline functions ****************************************************************************/

#endif
