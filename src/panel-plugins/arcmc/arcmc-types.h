/** \file arcmc-types.h
 *  \brief Header: shared archive browser types and constants
 */

#ifndef ARCMC_TYPES_H
#define ARCMC_TYPES_H

#include "lib/global.h"
#include "lib/panel-plugin.h"
#include "lib/widget.h"

/*** typedefs (not langstruc strstruc)
 * *************************************************************/

typedef struct
{
    char *full_path; /* full path inside the archive */
    char *name;      /* basename for display */
    mode_t mode;     /* S_IFDIR, S_IFREG or S_IFLNK | perms */
    off_t size;
    time_t mtime;
    gboolean is_virtual_dir; /* TRUE if synthesized from path components */
    char *linkname;          /* what a symbolic link says it points to */
    char *link_path;         /* the entry a link leads to, or NULL */
    gboolean link_to_dir;    /* the link leads to a directory */
    gboolean stale_link;     /* the link leads nowhere in the archive */
    off_t dir_size;          /* recursive size after Ctrl-Space */
    gboolean dir_size_computed;
} arcmc_entry_t;

/* Format indices for arcmc_pack_opts_t.format */
enum
{
    ARCMC_FMT_ZIP = 0,
    ARCMC_FMT_7Z = 1,
    ARCMC_FMT_TAR_GZ = 2,
    ARCMC_FMT_TAR_BZ2 = 3,
    ARCMC_FMT_TAR_XZ = 4,
    ARCMC_FMT_TAR = 5,
    ARCMC_FMT_CPIO = 6,
    /* read-only formats, not offered in the pack dialog */
    ARCMC_FMT_TAR_ZST = 7,
    ARCMC_FMT_TAR_LZ = 8,
    ARCMC_FMT_TAR_LZMA = 9,
    ARCMC_FMT_ISO = 10,
    ARCMC_FMT_XAR = 11,
    ARCMC_FMT_CAB = 12,
    ARCMC_FMT_OTHER_COUNT = 5 /* number of "Other" formats (tar.gz .. cpio) */
};

#define ARCMC_FMT_COUNT    (ARCMC_FMT_CPIO + 1)
#define ARCMC_FMT_EXT_BASE 100 /* external archiver formats start here */

typedef struct
{
    char *archive_path;
    int format;      /* ARCMC_FMT_* */
    int compression; /* 0=store, 1=fastest, 2=normal, 3=maximum */
    char *password;
    gboolean encrypt_files;
    gboolean encrypt_header;
    gboolean store_paths;
    gboolean delete_after;
} arcmc_pack_opts_t;

/* Saved state for nested archive browsing (stack frame). */
typedef struct arcmc_nest_frame_t
{
    struct arcmc_nest_frame_t *prev;
    char *archive_path; /* path to the outer archive on disk */
    char *display_path; /* stable user-visible name of the outer archive */
    char *current_dir;  /* dir inside the outer archive where we were */
    char *password;
    char *extfs_helper;
    GPtrArray *all_entries;
    mc_pp_input_stream_t *input_stream; /* owned source of the outer archive, or NULL */
    char *temp_file;                    /* extracted temp file (to unlink on pop), or NULL */
} arcmc_nest_frame_t;

typedef struct
{
    mc_panel_host_t *host;
    char *archive_path;                 /* path to the archive on disk */
    char *display_path;                 /* stable user-visible name, never a temporary path */
    char *current_dir;                  /* current directory inside the archive ("" = root) */
    char *password;                     /* password (if encrypted), NULL otherwise */
    GPtrArray *all_entries;             /* flat list of all entries from the archive */
    GHashTable *entry_index;            /* all_entries by full_path, built on demand, or NULL */
    char *title_buf;                    /* buffer for get_title */
    char *extfs_helper;                 /* full path to extfs helper, or NULL for libarchive mode */
    mc_pp_input_stream_t *input_stream; /* owned non-local archive source, or NULL */
    arcmc_nest_frame_t *nest_stack;     /* stack of outer archives for nested browsing */
    GHashTable *bulk_cache;             /* bulk extract cache: filename -> local_path */
    char *bulk_temp_dir;                /* temp directory for bulk-extracted files */
    gboolean history_enabled;           /* FALSE for non-reconstructable input streams */
} arcmc_data_t;

/* Progress dialog context for pack/extract operations */
typedef struct
{
    WDialog *dlg;
    WHLine *hline_top;      /* top separator -shows "{32%} title" */
    WLabel *lbl_archive;    /* archive path */
    WLabel *lbl_file;       /* current file path */
    WLabel *lbl_file_size;  /* "184 KB / 1.06 MB" */
    WGauge *gauge_file;     /* per-file progress */
    WLabel *lbl_total_size; /* "182 MB / 559 MB @ 17 MB/s -00:00:22" */
    WLabel *lbl_ratio;      /* "182 MB -> 9 MB = 5%" */
    WGauge *gauge_total;    /* total progress */

    /* tracking state */
    char *op_title;      /* operation title (e.g. "Creating archive...") */
    off_t total_bytes;   /* total bytes to process */
    off_t done_bytes;    /* bytes processed so far */
    off_t written_bytes; /* compressed bytes written */
    off_t file_bytes;    /* current file total size */
    off_t file_done;     /* current file bytes done */
    gint64 start_time;   /* g_get_monotonic_time() at start */
    int gauge_cols;      /* visual bar length in characters */
    int last_total_col;  /* last drawn column of total gauge */
    int last_file_col;   /* last drawn column of file gauge */
    gboolean aborted;    /* user pressed Abort/Esc */
    gboolean visible;    /* dlg_init() called */
} arcmc_progress_t;

/* Which backend serves a builtin format */
typedef enum
{
    ARCMC_BACKEND_OFF = 0, /* format is not handled in this direction */
    ARCMC_BACKEND_BUILTIN, /* libarchive only */
    ARCMC_BACKEND_BOTH,    /* libarchive, external tool where libarchive cannot cope */
    ARCMC_BACKEND_EXTERN,  /* external tool only */
    ARCMC_BACKEND_COUNT
} arcmc_backend_t;

/* Builtin (libarchive) format descriptor.
   The first ARCMC_FMT_COUNT entries are in ARCMC_FMT_* order. */
typedef struct
{
    const char *name;         /* display name, e.g. "7Z" */
    const char *key;          /* config key, e.g. "7z" */
    const char *ext;          /* default extension, e.g. ".7z" */
    gboolean lib_pack;        /* libarchive can write this format */
    gboolean lib_unpack;      /* libarchive can read this format */
    const char *pack_bin;     /* external packer, NULL = none wired up */
    const char *pack_args;    /* arguments of the packer, e.g. "a -y -t7z" */
    const char *unpack_bin;   /* external unpacker, NULL = none wired up */
    const char *extfs_helper; /* helper used for browsing with an external tool */
    arcmc_backend_t pack;     /* current setting */
    arcmc_backend_t unpack;   /* current setting */
    gboolean enabled;         /* format handled at all */
} arcmc_builtin_format_t;

extern arcmc_builtin_format_t arcmc_builtin_formats[];
extern const size_t arcmc_builtin_formats_count;

/* External archiver tool descriptor */
typedef struct
{
    char *name;          /* display name, e.g. "RAR" */
    char *ext;           /* filename suffix, e.g. ".rar" */
    char *pack_bin;      /* pack binary, e.g. "rar", NULL = no pack support */
    char *pack_args;     /* pack arguments template, e.g. "a -r -o+" */
    char *unpack_bin;    /* unpack binary, e.g. "unrar" */
    char *unpack_args;   /* unpack arguments template, e.g. "x -o+" */
    char *test_bin;      /* test binary, e.g. "unrar", NULL = no test */
    char *test_args;     /* test arguments template, e.g. "t" */
    char *extfs_helper;  /* extfs helper name for browsing, e.g. "urar" */
    char *list_file_arg; /* file-list argument template, e.g. "@%s" for RAR/7z, NULL = not
                            supported */
    gboolean enabled;    /* format is available for dispatch and settings */
} arcmc_ext_archiver_t;

/* External archivers registry.  arcmc.ini may append rows at startup. */
extern arcmc_ext_archiver_t *ext_archivers;
extern size_t ext_archivers_count;

/*** declarations (functions)
 * **********************************************************************/

void arcmc_entry_free (gpointer p);

/* The help of the plugin, beside the plugin: F1 in its panel and in its dialogs
   opens that file and not the help of the file manager. */
const char *arcmc_help_file (void);

#endif /* ARCMC_TYPES_H */
