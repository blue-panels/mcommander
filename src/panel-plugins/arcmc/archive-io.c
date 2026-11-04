/*
   Panel plugin arcmc for the M-Commander
   Archive read, write and extract, and the extfs helpers

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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <archive.h>
#include <archive_entry.h>

#ifdef HAVE_LIBMAGIC
#include <magic.h>
#endif

#include "lib/global.h"
#include "lib/panel-plugin.h"
#include "lib/tty/tty.h"
#include "lib/widget.h"
#include "lib/util.h"        /* mc_popen, mc_pread, mc_pstream_get_string, mc_pclose, name_quote */
#include "lib/vfs/utilvfs.h" /* vfs_parse_ls_lga */
#include "lib/mcconfig.h"    /* mc_config_get_data_path */

#include "arcmc-types.h"
#include "arcmc-config.h"
#include "arcmc-reader.h"
#include "progress.h"
#include "archive-io.h"

/*** file scope type declarations ****************************************************************/

/* Outcome of reading the table of contents with libarchive. */
typedef enum
{
    ARCMC_READ_OK = 0,       /* contents read */
    ARCMC_READ_FAILED,       /* unreadable: not an archive, unknown format, I/O error */
    ARCMC_READ_ENCRYPTED,    /* encrypted: a password is needed */
    ARCMC_READ_ENCRYPTED_7Z, /* encrypted 7z: only the external 7z program can read it */
    ARCMC_READ_ABORTED       /* the user stopped the reading */
} arcmc_read_result_t;

/* 7z binaries that understand the -p switch, in order of preference.
   7zr is left out: it cannot handle encrypted archives. */
static const char *const p7zip_bins[] = { "7z", "7zz", "7za" };

/*** global variables ****************************************************************************/

/* First ARCMC_FMT_COUNT rows are in ARCMC_FMT_* order. Only 7z has an external
   tool: libarchive cannot do its encryption, see arcmc_7z_pack(). */
arcmc_builtin_format_t arcmc_builtin_formats[] = {
    /* name, key, ext, lib pack/unpack, pack bin, pack args, unpack bin, helper, pack, unpack, on */
    { "ZIP", "zip", ".zip", TRUE, TRUE, NULL, NULL, NULL, NULL, ARCMC_BACKEND_BUILTIN,
      ARCMC_BACKEND_BUILTIN, TRUE },
    { "7Z", "7z", ".7z", TRUE, TRUE, "7z", "a -y -t7z", "7z", "u7z", ARCMC_BACKEND_BOTH,
      ARCMC_BACKEND_BOTH, TRUE },
    { "TAR.GZ", "tar.gz", ".tar.gz", TRUE, TRUE, NULL, NULL, NULL, NULL, ARCMC_BACKEND_BUILTIN,
      ARCMC_BACKEND_BUILTIN, TRUE },
    { "TAR.BZ2", "tar.bz2", ".tar.bz2", TRUE, TRUE, NULL, NULL, NULL, NULL, ARCMC_BACKEND_BUILTIN,
      ARCMC_BACKEND_BUILTIN, TRUE },
    { "TAR.XZ", "tar.xz", ".tar.xz", TRUE, TRUE, NULL, NULL, NULL, NULL, ARCMC_BACKEND_BUILTIN,
      ARCMC_BACKEND_BUILTIN, TRUE },
    { "TAR", "tar", ".tar", TRUE, TRUE, NULL, NULL, NULL, NULL, ARCMC_BACKEND_BUILTIN,
      ARCMC_BACKEND_BUILTIN, TRUE },
    { "CPIO", "cpio", ".cpio", TRUE, TRUE, NULL, NULL, NULL, NULL, ARCMC_BACKEND_BUILTIN,
      ARCMC_BACKEND_BUILTIN, TRUE },
    /* libarchive reads these but arcmc does not write them */
    { "TAR.ZST", "tar.zst", ".tar.zst", FALSE, TRUE, NULL, NULL, NULL, NULL, ARCMC_BACKEND_OFF,
      ARCMC_BACKEND_BUILTIN, TRUE },
    { "TAR.LZ", "tar.lz", ".tar.lz", FALSE, TRUE, NULL, NULL, NULL, NULL, ARCMC_BACKEND_OFF,
      ARCMC_BACKEND_BUILTIN, TRUE },
    { "TAR.LZMA", "tar.lzma", ".tar.lzma", FALSE, TRUE, NULL, NULL, NULL, NULL, ARCMC_BACKEND_OFF,
      ARCMC_BACKEND_BUILTIN, TRUE },
    { "ISO", "iso", ".iso", FALSE, TRUE, NULL, NULL, "7z", "u7z", ARCMC_BACKEND_OFF,
      ARCMC_BACKEND_BUILTIN, TRUE },
    { "XAR", "xar", ".xar", FALSE, TRUE, NULL, NULL, NULL, NULL, ARCMC_BACKEND_OFF,
      ARCMC_BACKEND_BUILTIN, TRUE },
    { "CAB", "cab", ".cab", FALSE, TRUE, NULL, NULL, NULL, NULL, ARCMC_BACKEND_OFF,
      ARCMC_BACKEND_BUILTIN, TRUE },
};

const size_t arcmc_builtin_formats_count = G_N_ELEMENTS (arcmc_builtin_formats);

/*** file scope variables ************************************************************************/

/* Extensions that name the same format as a row in arcmc_builtin_formats[] */
static const struct
{
    const char *ext;
    int fmt;
} builtin_ext_aliases[] = {
    { ".tgz", ARCMC_FMT_TAR_GZ },   { ".tbz2", ARCMC_FMT_TAR_BZ2 }, { ".txz", ARCMC_FMT_TAR_XZ },
    { ".jar", ARCMC_FMT_ZIP },      { ".war", ARCMC_FMT_ZIP },      { ".ear", ARCMC_FMT_ZIP },
    { ".tzst", ARCMC_FMT_TAR_ZST }, { ".tlz", ARCMC_FMT_TAR_LZ },
};

/*** forward declarations (file scope functions) *************************************************/

static gboolean arcmc_ext_run_with_status (const char *title, const char *cmd_str,
                                           char **error_msg);

/*** file scope functions ************************************************************************/

/* Check that a path from an archive entry is safe (no absolute paths, no ".." components). */
static gboolean
is_path_safe (const char *path)
{
    const char *p = path;

    if (p[0] == '/')
        return FALSE;

    while (*p != '\0')
    {
        if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\0'))
            return FALSE;
        p = strchr (p, '/');
        if (p == NULL)
            break;
        p++;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Strip trailing slashes from a path string (in-place). */
static void
strip_trailing_slashes (char *path)
{
    size_t len;

    if (path == NULL)
        return;

    len = strlen (path);
    while (len > 0 && path[len - 1] == '/')
        path[--len] = '\0';
}

/* --------------------------------------------------------------------------------------------- */

/* The size of the file the archive is read from, or 0 if it is not a local file. */
static off_t
source_size (const arcmc_data_t *data)
{
    const char *local;
    struct stat st;

    local = data->input_stream == NULL ? data->archive_path
                                       : mc_pp_input_stream_local_path (data->input_stream, NULL);
    if (local != NULL && stat (local, &st) == 0 && S_ISREG (st.st_mode))
        return st.st_size;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* The progress of reading the table of contents, shown when it takes longer than 4 seconds.
   With @measured, the share of the archive read is counted from the size of its file. */
static arcmc_progress_t *
read_progress_new (const arcmc_data_t *data, gboolean measured)
{
    arcmc_progress_t *p;

    p = arcmc_progress_create (_ ("Reading archive"),
                               data->display_path != NULL ? data->display_path : data->archive_path,
                               measured ? source_size (data) : 0);
    arcmc_progress_set_delay (p, 4 * G_USEC_PER_SEC);
    return p;
}

/* --------------------------------------------------------------------------------------------- */

/* What the dialog of a pass counts */
typedef enum
{
    PASS_TOC,  /* the table of contents is read: the bytes of the archive read */
    PASS_SEEK, /* entries are looked for: the position in the archive file */
    PASS_COPY  /* entries are copied out: the bytes copied, told by the copy itself */
} pass_mode_t;

/* A pass over the archive that tells a progress dialog how far it has got. */
typedef struct
{
    struct archive *a;
    arcmc_progress_t *p;
    off_t size; /* bytes of the archive file, 0 if it is not a local file */
    pass_mode_t mode;
    gint64 next; /* when the dialog is told again */
} pass_t;

static void
pass_init (pass_t *pass, const arcmc_data_t *data, struct archive *a, arcmc_progress_t *p,
           pass_mode_t mode)
{
    pass->a = a;
    pass->p = p;
    pass->size = p != NULL ? source_size (data) : 0;
    pass->mode = mode;
    pass->next = 0;
}

/* Tell the dialog of the pass a few times a second, whatever the size of the entries.
   FALSE if the user stopped the pass. */
static gboolean
pass_report (pass_t *pass, const char *pathname)
{
    gint64 now;
    off_t pos;

    if (pass->p == NULL)
        return TRUE;

    now = g_get_monotonic_time ();
    if (now < pass->next)
        return TRUE;
    pass->next = now + G_USEC_PER_SEC / 20;

    pos = archive_filter_bytes (pass->a, -1);
    switch (pass->mode)
    {
    case PASS_TOC:
        return arcmc_progress_update (pass->p, pathname, 0, 0, pos, 0);
    case PASS_SEEK:
        return arcmc_progress_seek (pass->p, pathname, pos, pass->size);
    default:
        // the numbers stay those of the last copy, only the buttons are looked at
        return arcmc_progress_update (pass->p, NULL, pass->p->file_bytes, pass->p->file_done,
                                      pass->p->done_bytes, pass->p->written_bytes);
    }
}

/* TRUE if reading the data of the entry costs no more than skipping it: the plain data of
   a tar or a cpio inside a compressed stream or a stream that cannot be seeked. A zip,
   a 7z or a rar skips the packed data of an entry without unpacking it. */
static gboolean
pass_reads_anyway (const pass_t *pass)
{
    int format = archive_format (pass->a) & ARCHIVE_FORMAT_BASE_MASK;

    if (format != ARCHIVE_FORMAT_TAR && format != ARCHIVE_FORMAT_CPIO)
        return FALSE;
    return pass->size == 0 || archive_filter_code (pass->a, 0) != ARCHIVE_FILTER_NONE;
}

/* Pass over the data of the entry the reader stands on. When it is read anyway, it is read
   block by block, and the dialog is told between the blocks. FALSE if the user stopped
   the pass. */
static gboolean
pass_skip (pass_t *pass, const char *pathname)
{
    if (pass->p == NULL || !pass_reads_anyway (pass))
    {
        archive_read_data_skip (pass->a);
        return TRUE;
    }

    for (;;)
    {
        const void *buf;
        size_t len;
        la_int64_t offset;

        // the end of the data, or an error the next header meets again
        if (archive_read_data_block (pass->a, &buf, &len, &offset) != ARCHIVE_OK)
            return TRUE;
        if (!pass_report (pass, pathname))
            return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Add @e to @entries and to @by_path, the entries by their path while the archive is read. */
static void
add_entry (GPtrArray *entries, GHashTable *by_path, arcmc_entry_t *e)
{
    g_ptr_array_add (entries, e);
    g_hash_table_insert (by_path, e->full_path, e);
}

/* --------------------------------------------------------------------------------------------- */

/* Ensure all parent directories of `full_path` exist as virtual dir entries. */
static void
ensure_parent_dirs (GPtrArray *entries, GHashTable *by_path, const char *full_path)
{
    char *tmp;
    char *slash;

    tmp = g_strdup (full_path);

    for (slash = strchr (tmp, '/'); slash != NULL; slash = strchr (slash + 1, '/'))
    {
        char saved;
        char *basename_ptr;
        arcmc_entry_t *dir_entry;

        saved = *slash;
        *slash = '\0';

        if (tmp[0] == '\0' || g_hash_table_contains (by_path, tmp))
        {
            *slash = saved;
            continue;
        }

        dir_entry = g_new0 (arcmc_entry_t, 1);
        dir_entry->full_path = g_strdup (tmp);

        basename_ptr = strrchr (dir_entry->full_path, '/');
        dir_entry->name = g_strdup (basename_ptr != NULL ? basename_ptr + 1 : dir_entry->full_path);

        dir_entry->mode = S_IFDIR | 0755;
        dir_entry->size = 0;
        dir_entry->mtime = time (NULL);
        dir_entry->is_virtual_dir = TRUE;

        add_entry (entries, by_path, dir_entry);

        *slash = saved;
    }

    g_free (tmp);
}

/* --------------------------------------------------------------------------------------------- */

/* Recursively calculate the total size of files for packing. */
static off_t
arcmc_calculate_total_size (const char *cwd, GPtrArray *files)
{
    guint i;
    off_t total = 0;

    for (i = 0; i < files->len; i++)
    {
        const char *name = (const char *) g_ptr_array_index (files, i);
        char *full_path;
        struct stat st;

        full_path = g_build_filename (cwd, name, NULL);

        if (lstat (full_path, &st) == 0)
        {
            if (S_ISDIR (st.st_mode))
            {
                GDir *dir;

                total += st.st_size;
                dir = g_dir_open (full_path, 0, NULL);
                if (dir != NULL)
                {
                    const gchar *child;
                    GPtrArray *children;

                    children = g_ptr_array_new_with_free_func (g_free);
                    while ((child = g_dir_read_name (dir)) != NULL)
                        g_ptr_array_add (children, g_strconcat (name, "/", child, NULL));
                    g_dir_close (dir);

                    total += arcmc_calculate_total_size (cwd, children);
                    g_ptr_array_free (children, TRUE);
                }
            }
            else
                total += st.st_size;
        }

        g_free (full_path);
    }

    return total;
}

/* --------------------------------------------------------------------------------------------- */

/* Add a single file to the archive. `disk_path` is the real filesystem path,
   `archive_name` is the name stored inside the archive.
   `p` is an optional progress context (may be NULL). */
static gboolean
arcmc_pack_add_file (struct archive *a, const char *disk_path, const char *archive_name,
                     arcmc_progress_t *p)
{
    struct stat st;
    struct archive_entry *entry;
    int fd;
    char buf[8192];
    ssize_t bytes_read;

    if (lstat (disk_path, &st) != 0)
        return FALSE;

    entry = archive_entry_new ();
    archive_entry_set_pathname (entry, archive_name);
    archive_entry_copy_stat (entry, &st);

    if (archive_write_header (a, entry) != ARCHIVE_OK)
    {
        archive_entry_free (entry);
        return FALSE;
    }

    if (S_ISREG (st.st_mode))
    {
        off_t file_done = 0;

        fd = open (disk_path, O_RDONLY);
        if (fd >= 0)
        {
            while ((bytes_read = read (fd, buf, sizeof (buf))) > 0)
            {
                la_ssize_t written;

                written = archive_write_data (a, buf, (size_t) bytes_read);
                if (written < 0)
                {
                    close (fd);
                    archive_entry_free (entry);
                    return FALSE;
                }
                file_done += bytes_read;

                if (p != NULL)
                {
                    p->done_bytes += bytes_read;
                    p->written_bytes += written;

                    if (!arcmc_progress_update (p, archive_name, st.st_size, file_done,
                                                p->done_bytes, p->written_bytes))
                    {
                        close (fd);
                        archive_entry_free (entry);
                        return FALSE;
                    }
                }
            }

            if (bytes_read < 0)
            {
                close (fd);
                archive_entry_free (entry);
                return FALSE;
            }

            close (fd);
        }
        else
        {
            archive_entry_free (entry);
            return FALSE;
        }
    }

    archive_entry_free (entry);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Recursively add a directory tree to the archive.
   `base_dir` is the filesystem path, `archive_prefix` is the path prefix inside the archive.
   `p` is an optional progress context (may be NULL). */
static gboolean
arcmc_pack_add_directory (struct archive *a, const char *base_dir, const char *archive_prefix,
                          arcmc_progress_t *p)
{
    GDir *dir;
    const gchar *name;

    /* add the directory entry itself */
    if (!arcmc_pack_add_file (a, base_dir, archive_prefix, p))
        return FALSE;

    dir = g_dir_open (base_dir, 0, NULL);
    if (dir == NULL)
        return FALSE;

    while ((name = g_dir_read_name (dir)) != NULL)
    {
        char *full_path;
        char *arc_name;
        struct stat st;

        full_path = g_build_filename (base_dir, name, NULL);
        arc_name = g_strconcat (archive_prefix, "/", name, NULL);

        if (lstat (full_path, &st) == 0)
        {
            gboolean ok;

            if (S_ISDIR (st.st_mode))
                ok = arcmc_pack_add_directory (a, full_path, arc_name, p);
            else
                ok = arcmc_pack_add_file (a, full_path, arc_name, p);

            if (!ok)
            {
                g_free (full_path);
                g_free (arc_name);
                g_dir_close (dir);
                return FALSE;
            }
        }

        g_free (full_path);
        g_free (arc_name);
    }

    g_dir_close (dir);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Detect ARCMC_FMT_* from archive filename extension.  Returns -1 if unknown. */
static int
arcmc_detect_fmt_id (const char *filename)
{
    static const struct
    {
        const char *ext;
        int fmt;
    } map[] = {
        { ".tar.gz", ARCMC_FMT_TAR_GZ },   { ".tgz", ARCMC_FMT_TAR_GZ },
        { ".tar.bz2", ARCMC_FMT_TAR_BZ2 }, { ".tbz2", ARCMC_FMT_TAR_BZ2 },
        { ".tar.xz", ARCMC_FMT_TAR_XZ },   { ".txz", ARCMC_FMT_TAR_XZ },
        { ".tar", ARCMC_FMT_TAR },         { ".zip", ARCMC_FMT_ZIP },
        { ".jar", ARCMC_FMT_ZIP },         { ".war", ARCMC_FMT_ZIP },
        { ".ear", ARCMC_FMT_ZIP },         { ".7z", ARCMC_FMT_7Z },
        { ".cpio", ARCMC_FMT_CPIO },
    };

    size_t flen, i;

    flen = strlen (filename);

    for (i = 0; i < G_N_ELEMENTS (map); i++)
    {
        size_t elen = strlen (map[i].ext);

        if (flen >= elen && g_ascii_strcasecmp (filename + flen - elen, map[i].ext) == 0)
            return map[i].fmt;
    }

    return -1;
}

/* --------------------------------------------------------------------------------------------- */

/* Configure archive_write object for the given format.  Returns TRUE on success. */
static gboolean
arcmc_write_set_format (struct archive *a, int fmt)
{
    switch (fmt)
    {
    case ARCMC_FMT_ZIP:
        archive_write_set_format_zip (a);
        break;
    case ARCMC_FMT_7Z:
        archive_write_set_format_7zip (a);
        break;
    case ARCMC_FMT_TAR_GZ:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_gzip (a);
        break;
    case ARCMC_FMT_TAR_BZ2:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_bzip2 (a);
        break;
    case ARCMC_FMT_TAR_XZ:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_xz (a);
        break;
    case ARCMC_FMT_TAR:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_none (a);
        break;
    case ARCMC_FMT_CPIO:
        archive_write_set_format_cpio (a);
        archive_write_add_filter_none (a);
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Check whether `path` (with trailing slashes stripped) should be excluded.
   An entry is excluded if it matches one of `del_paths` exactly, or is a child of one. */
static gboolean
arcmc_path_is_deleted (const char *path, const char **del_paths, int del_count)
{
    int i;
    size_t plen;

    plen = strlen (path);

    for (i = 0; i < del_count; i++)
    {
        size_t dlen = strlen (del_paths[i]);

        if (plen == dlen && strcmp (path, del_paths[i]) == 0)
            return TRUE;
        /* child: path starts with del_path + '/' */
        if (plen > dlen && strncmp (path, del_paths[i], dlen) == 0 && path[dlen] == '/')
            return TRUE;
    }

    return FALSE;
}

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/* The path inside the archive that @name in the current directory stands for: for
   a link to a file, the file it leads to, as the content is that file's. */
char *
arcmc_target_path (arcmc_data_t *data, const char *name)
{
    char *path;
    const arcmc_entry_t *e;

    path = build_child_path (data->current_dir, name);
    e = arcmc_data_find_entry (data, path);

    if (e != NULL && S_ISLNK (e->mode) && e->link_path != NULL && !e->link_to_dir)
    {
        g_free (path);
        return g_strdup (e->link_path);
    }

    return path;
}

/* --------------------------------------------------------------------------------------------- */

/* Get the parent directory of current_dir.
   Returns a newly allocated string, or "" for root. */
char *
get_parent_dir (const char *current_dir)
{
    char *slash;

    if (current_dir == NULL || current_dir[0] == '\0')
        return g_strdup ("");

    slash = strrchr (current_dir, '/');
    if (slash == NULL)
        return g_strdup ("");

    return g_strndup (current_dir, (gsize) (slash - current_dir));
}

/* --------------------------------------------------------------------------------------------- */

/* Build the full path for a child entry within the current directory. */
char *
build_child_path (const char *current_dir, const char *name)
{
    if (current_dir == NULL || current_dir[0] == '\0')
        return g_strdup (name);

    return g_strdup_printf ("%s/%s", current_dir, name);
}

/* --------------------------------------------------------------------------------------------- */

/* Build a full path to an extfs helper by name, or NULL if it is not installed. */
static char *
arcmc_extfs_helper_path (const char *helper_name)
{
    char *path;

    if (helper_name == NULL)
        return NULL;

    /* try user data dir first */
    path = g_build_filename (mc_config_get_data_path (), "extfs.d", helper_name, NULL);
    if (g_file_test (path, G_FILE_TEST_IS_EXECUTABLE))
        return path;
    g_free (path);

    /* try system libexecdir */
    path = g_build_filename (LIBEXECDIR, "extfs.d", helper_name, NULL);
    if (g_file_test (path, G_FILE_TEST_IS_EXECUTABLE))
        return path;
    g_free (path);

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* Check that `path` ends with `ext` (case-insensitive). */
static gboolean
arcmc_has_ext (const char *path, const char *ext)
{
    size_t plen, elen;

    plen = strlen (path);
    elen = strlen (ext);

    return (plen >= elen && g_ascii_strcasecmp (path + plen - elen, ext) == 0);
}

/* --------------------------------------------------------------------------------------------- */

/* Find the builtin format that owns the given file name, or NULL. */
arcmc_builtin_format_t *
arcmc_find_builtin_format (const char *path)
{
    const char *base;
    size_t i;

    if (path == NULL)
        return NULL;

    base = strrchr (path, '/');
    base = base != NULL ? base + 1 : path;

    for (i = 0; i < arcmc_builtin_formats_count; i++)
        if (arcmc_has_ext (base, arcmc_builtin_formats[i].ext))
            return &arcmc_builtin_formats[i];

    for (i = 0; i < G_N_ELEMENTS (builtin_ext_aliases); i++)
        if (arcmc_has_ext (base, builtin_ext_aliases[i].ext))
            return &arcmc_builtin_formats[builtin_ext_aliases[i].fmt];

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* Whether the pack dialog may offer this builtin format. */
gboolean
arcmc_builtin_can_pack (int fmt)
{
    const arcmc_builtin_format_t *f;

    if (fmt < 0 || fmt >= (int) arcmc_builtin_formats_count)
        return FALSE;

    f = &arcmc_builtin_formats[fmt];

    return (f->enabled && f->pack != ARCMC_BACKEND_OFF);
}

/* --------------------------------------------------------------------------------------------- */

/* Name of a backend value as shown in the settings dialog. */
const char *
arcmc_backend_name (arcmc_backend_t b)
{
    switch (b)
    {
    case ARCMC_BACKEND_BUILTIN:
        return "builtin";
    case ARCMC_BACKEND_BOTH:
        return "both";
    case ARCMC_BACKEND_EXTERN:
        return "extern";
    default:
        return "off";
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Parse a backend value written by arcmc_config_save(). */
arcmc_backend_t
arcmc_backend_from_name (const char *s, arcmc_backend_t def)
{
    if (s == NULL || s[0] == '\0')
        return def;
    if (strcmp (s, "builtin") == 0)
        return ARCMC_BACKEND_BUILTIN;
    if (strcmp (s, "both") == 0)
        return ARCMC_BACKEND_BOTH;
    if (strcmp (s, "extern") == 0)
        return ARCMC_BACKEND_EXTERN;
    if (strcmp (s, "off") == 0)
        return ARCMC_BACKEND_OFF;

    return def;
}

/* --------------------------------------------------------------------------------------------- */

/* Check whether a backend value can be used for one direction of a format.
   `lib` tells whether libarchive handles it, `bin` is the external tool. */
gboolean
arcmc_backend_possible (arcmc_backend_t b, gboolean lib, const char *bin)
{
    switch (b)
    {
    case ARCMC_BACKEND_OFF:
        return (lib || bin != NULL);
    case ARCMC_BACKEND_BUILTIN:
        return lib;
    case ARCMC_BACKEND_BOTH:
        return (lib && bin != NULL);
    case ARCMC_BACKEND_EXTERN:
        return (bin != NULL);
    default:
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Find an installed 7z program, or NULL when there is none. */
static const char *
arcmc_7z_bin (void)
{
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (p7zip_bins); i++)
        if (arcmc_check_bin_available (p7zip_bins[i]))
            return p7zip_bins[i];

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* Resolve a configured tool name to the program to run, NULL when there is none. */
const char *
arcmc_resolve_tool (const char *bin)
{
    if (bin == NULL)
        return NULL;

    /* p7zip is shipped under several names */
    if (strcmp (bin, "7z") == 0)
    {
        const char *found = arcmc_7z_bin ();

        if (found != NULL)
            return found;
    }

    return bin;
}

/* --------------------------------------------------------------------------------------------- */

const char *
arcmc_builtin_tool (const arcmc_builtin_format_t *f)
{
    return arcmc_resolve_tool (f->pack_bin != NULL ? f->pack_bin : f->unpack_bin);
}

/* --------------------------------------------------------------------------------------------- */

/* Program that reads this format, NULL when none of the candidates is installed. */
static const char *
arcmc_unpack_tool (const arcmc_builtin_format_t *f)
{
    const char *bin;

    bin = arcmc_resolve_tool (f->unpack_bin);

    return (bin != NULL && arcmc_check_bin_available (bin)) ? bin : NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* Find an extfs helper for the given archive path.
   Returns an allocated full path to the helper executable, or NULL if not found. */
char *
arcmc_find_extfs_helper (const char *archive_path)
{
    const arcmc_builtin_format_t *fmt;
    const arcmc_ext_archiver_t *ext;

    /* a builtin format uses a helper only when its settings allow the external tool */
    fmt = arcmc_find_builtin_format (archive_path);
    if (fmt != NULL)
    {
        if (!fmt->enabled
            || (fmt->unpack != ARCMC_BACKEND_BOTH && fmt->unpack != ARCMC_BACKEND_EXTERN))
            return NULL;

        return arcmc_extfs_helper_path (fmt->extfs_helper);
    }

    ext = arcmc_find_ext_archiver (archive_path);
    if (ext != NULL)
    {
        if (!ext->enabled)
            return NULL;

        return arcmc_extfs_helper_path (ext->extfs_helper);
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_LIBMAGIC
/* Check whether a file is a supported archive by content (MIME type).
   Returns TRUE if the file looks like an archive that arcmc can handle. */
gboolean
arcmc_is_archive_by_content (const char *path)
{
    magic_t mag;
    const char *mime;
    gboolean result = FALSE;

    /* archive MIME types that libarchive or extfs helpers can handle */
    static const char *const archive_mimes[] = {
        "application/zip",       "application/x-7z-compressed",
        "application/gzip",      "application/x-gzip",
        "application/x-bzip2",   "application/x-xz",
        "application/zstd",      "application/x-zstd",
        "application/x-tar",     "application/x-cpio",
        "application/x-archive", "application/x-debian-package",
        "application/x-rpm",     "application/x-rar",
        "application/vnd.rar",   "application/x-arj",
        "application/x-ace",     "application/x-zoo",
        "application/x-lha",     "application/x-lzh-compressed",
        "application/x-alz",     "application/x-arc",
    };

    mag = magic_open (MAGIC_MIME_TYPE | MAGIC_SYMLINK | MAGIC_ERROR);
    if (mag == NULL)
        return FALSE;

    if (magic_load (mag, MC_LIBMAGIC_FILE) != 0)
    {
        magic_close (mag);
        return FALSE;
    }

    mime = magic_file (mag, path);
    if (mime != NULL)
    {
        size_t i;

        for (i = 0; i < G_N_ELEMENTS (archive_mimes); i++)
        {
            if (strcmp (mime, archive_mimes[i]) == 0)
            {
                result = TRUE;
                break;
            }
        }
    }

    magic_close (mag);
    return result;
}
#endif /* HAVE_LIBMAGIC */

/* --------------------------------------------------------------------------------------------- */

/* Quote for /bin/sh. Unlike name_quote(), safe for a password starting with '-'. */
static char *
arcmc_shell_quote (const char *s)
{
    GString *q;

    q = g_string_new ("'");

    for (; *s != '\0'; s++)
    {
        if (*s == '\'')
            g_string_append (q, "'\\''");
        else
            g_string_append_c (q, *s);
    }

    g_string_append_c (q, '\'');

    return g_string_free (q, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* Environment prefix for an extfs helper: password and program to run.
   Returns an allocated string, empty when there is nothing to pass. */
static char *
arcmc_extfs_env (const char *password, const char *archive_path)
{
    const arcmc_builtin_format_t *fmt;
    GString *env;
    char *quoted;

    env = g_string_new ("");

    if (password != NULL && password[0] != '\0')
    {
        quoted = arcmc_shell_quote (password);
        g_string_append_printf (env, "MC_EXTFS_PASSWORD=%s ", quoted);
        g_free (quoted);
    }

    fmt = arcmc_find_builtin_format (archive_path);
    if (fmt != NULL && fmt->unpack_bin != NULL && strcmp (fmt->unpack_bin, "7z") != 0)
    {
        quoted = arcmc_shell_quote (fmt->unpack_bin);
        g_string_append_printf (env, "MC_EXTFS_P7ZIP=%s ", quoted);
        g_free (quoted);
    }

    return g_string_free (env, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* Read archive contents using an extfs helper's "list" command.
   Returns TRUE on success, FALSE on error; `*stopped` tells that the user stopped it. */
static gboolean
read_archive_extfs (arcmc_data_t *data, gboolean *stopped)
{
    char *quoted_archive;
    char *env;
    char *cmd;
    mc_pipe_t *pip;
    GError *error = NULL;
    GString *remain_line = NULL;
    GHashTable *by_path;
    arcmc_progress_t *progress;
    GPtrArray *entries;
    guint lines = 0;
    gboolean aborted = FALSE;

    *stopped = FALSE;
    quoted_archive = name_quote (data->archive_path, FALSE);
    env = arcmc_extfs_env (data->password, data->archive_path);
    cmd = g_strconcat (env, data->extfs_helper, " list ", quoted_archive, (char *) NULL);
    g_free (quoted_archive);
    g_free (env);

    pip = mc_popen (cmd, TRUE, TRUE, &error);
    g_free (cmd);

    if (pip == NULL)
    {
        if (error != NULL)
            g_error_free (error);
        return FALSE;
    }

    // the old list stays until the new one is read in full
    entries = g_ptr_array_new_with_free_func (arcmc_entry_free);
    by_path = g_hash_table_new (g_str_hash, g_str_equal);
    // the helper says nothing of how far it is
    progress = read_progress_new (data, FALSE);

    while (!aborted)
    {
        GString *buffer;

        pip->out.len = MC_PIPE_BUFSIZE;
        pip->err.len = MC_PIPE_BUFSIZE;

        mc_pread (pip, &error);

        if (error != NULL)
        {
            g_error_free (error);
            break;
        }

        if (pip->out.len == MC_PIPE_STREAM_EOF)
            break;

        if (pip->out.len <= 0)
            continue;

        while ((buffer = mc_pstream_get_string (&pip->out)) != NULL)
        {
            if (buffer->str[buffer->len - 1] == '\n')
            {
                g_string_truncate (buffer, buffer->len - 1);

                if (remain_line != NULL)
                {
                    g_string_append_len (remain_line, buffer->str, buffer->len);
                    g_string_free (buffer, TRUE);
                    buffer = remain_line;
                    remain_line = NULL;
                }
            }
            else
            {
                if (remain_line == NULL)
                    remain_line = buffer;
                else
                {
                    g_string_append_len (remain_line, buffer->str, buffer->len);
                    g_string_free (buffer, TRUE);
                }
                continue;
            }

            /* parse the ls -l line */
            {
                struct stat st;
                char *filename = NULL;
                char *linkname = NULL;

                if (vfs_parse_ls_lga (buffer->str, &st, &filename, &linkname, NULL)
                    && filename != NULL && filename[0] != '\0')
                {
                    char *clean;
                    char *basename_ptr;
                    arcmc_entry_t *e;

                    /* skip leading "./" or "/" */
                    clean = filename;
                    if (clean[0] == '.' && clean[1] == '/')
                        clean += 2;
                    else if (clean[0] == '/')
                        clean++;

                    clean = g_strdup (clean);
                    strip_trailing_slashes (clean);

                    if (clean[0] != '\0' && !g_hash_table_contains (by_path, clean))
                    {
                        ensure_parent_dirs (entries, by_path, clean);

                        e = g_new0 (arcmc_entry_t, 1);
                        e->full_path = clean;

                        basename_ptr = strrchr (clean, '/');
                        e->name = g_strdup (basename_ptr != NULL ? basename_ptr + 1 : clean);

                        e->mode = st.st_mode;
                        if (S_ISLNK (e->mode))
                            e->linkname = g_strdup (linkname);
                        else if (!S_ISDIR (e->mode))
                            e->mode = S_IFREG | (e->mode & 07777);

                        e->size = st.st_size;
                        e->mtime = st.st_mtime;
                        e->is_virtual_dir = FALSE;

                        add_entry (entries, by_path, e);

                        if (++lines % 64 == 0
                            && !arcmc_progress_update (progress, e->full_path, 0, 0, 0, 0))
                            aborted = TRUE;
                    }
                    else
                        g_free (clean);
                }

                g_free (filename);
                g_free (linkname);
            }

            g_string_free (buffer, TRUE);
            if (aborted)
                break;
        }
    }

    arcmc_progress_destroy (progress);

    if (remain_line != NULL)
        g_string_free (remain_line, TRUE);

    // a helper that still writes ends on the closed pipe, one that does not on the signal
    if (aborted)
        kill (pip->child_pid, SIGTERM);
    mc_pclose (pip, NULL);
    g_hash_table_destroy (by_path);

    // an archive with nothing left in it is a full list too, only not a success to open
    if (!aborted)
    {
        arcmc_resolve_links (entries);
        arcmc_data_free_entries (data);
        data->all_entries = entries;
    }
    else
        g_ptr_array_free (entries, TRUE);

    *stopped = aborted;
    return !aborted && data->all_entries->len > 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Read archive contents using an extfs helper, TRUE on success. */
gboolean
arcmc_read_archive_extfs (arcmc_data_t *data)
{
    gboolean stopped;

    return read_archive_extfs (data, &stopped);
}

/* --------------------------------------------------------------------------------------------- */

/* Close an mc_pipe_t and return the child process exit status.
   Returns the exit code (0 = success) or -1 on error. */
static int
mc_pclose_get_status (mc_pipe_t *p)
{
    int status = -1;
    int res;

    if (p == NULL)
        return -1;

    if (p->out.fd >= 0)
        close (p->out.fd);
    if (p->err.fd >= 0)
        close (p->err.fd);

    do
        res = waitpid (p->child_pid, &status, 0);
    while (res < 0 && errno == EINTR);

    g_free (p);

    if (res > 0 && WIFEXITED (status))
        return WEXITSTATUS (status);
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

/* Extract a file from the archive using extfs helper's "copyout" command.
   Returns MC_PPR_OK on success. */
mc_pp_result_t
arcmc_extract_entry_extfs (arcmc_data_t *data, const char *target_path, char **local_path)
{
    GError *error = NULL;
    int fd;
    char *quoted_archive, *quoted_file, *quoted_local;
    char *env;
    char *cmd;
    mc_pipe_t *pip;
    char *tmp_path = NULL;
    gboolean failed;

    fd = g_file_open_tmp ("mc-arcmc-XXXXXX", &tmp_path, &error);
    if (fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        return MC_PPR_FAILED;
    }
    close (fd);

    *local_path = tmp_path;
    mc_pp_rename_with_ext (local_path, target_path);

    quoted_archive = name_quote (data->archive_path, FALSE);
    quoted_file = name_quote (target_path, FALSE);
    quoted_local = name_quote (*local_path, FALSE);
    env = arcmc_extfs_env (data->password, data->archive_path);

    cmd = g_strconcat (env, data->extfs_helper, " copyout ", quoted_archive, " ", quoted_file, " ",
                       quoted_local, (char *) NULL);

    g_free (quoted_archive);
    g_free (quoted_file);
    g_free (quoted_local);
    g_free (env);

    pip = mc_popen (cmd, FALSE, TRUE, &error);
    g_free (cmd);

    if (pip == NULL)
    {
        if (error != NULL)
            g_error_free (error);
        unlink (*local_path);
        g_free (*local_path);
        *local_path = NULL;
        return MC_PPR_FAILED;
    }

    pip->out.len = MC_PIPE_STREAM_UNREAD;
    pip->err.len = MC_PIPE_BUFSIZE;

    mc_pread (pip, &error);

    failed = error != NULL || pip->err.len > 0;
    if (error != NULL)
        g_error_free (error);

    // The helper scripts answer 0 even where copyout gave up, so check the file too.
    if (mc_pclose_get_status (pip) != 0)
        failed = TRUE;
    else if (!failed)
    {
        const arcmc_entry_t *e;
        struct stat st;

        e = arcmc_data_find_entry (data, target_path);
        failed = stat (*local_path, &st) != 0 || (e != NULL && e->size > 0 && st.st_size == 0);
    }

    if (failed)
    {
        unlink (*local_path);
        g_free (*local_path);
        *local_path = NULL;
        return MC_PPR_FAILED;
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* Run an extfs helper command (copyin / rm).
   Returns TRUE on success (exit code 0). */
gboolean
arcmc_extfs_run_cmd (const char *helper, const char *cmd_name, const char *archive_path,
                     const char *stored_name, const char *local_name, const char *password)
{
    char *quoted_archive, *quoted_stored, *env, *cmd;
    mc_pipe_t *pip;
    GError *error = NULL;

    quoted_archive = name_quote (archive_path, FALSE);
    quoted_stored = name_quote (stored_name, FALSE);
    env = arcmc_extfs_env (password, archive_path);

    if (local_name != NULL)
    {
        char *quoted_local = name_quote (local_name, FALSE);

        cmd = g_strconcat (env, helper, cmd_name, quoted_archive, " ", quoted_stored, " ",
                           quoted_local, (char *) NULL);
        g_free (quoted_local);
    }
    else
        cmd =
            g_strconcat (env, helper, cmd_name, quoted_archive, " ", quoted_stored, (char *) NULL);

    g_free (quoted_archive);
    g_free (quoted_stored);
    g_free (env);

    pip = mc_popen (cmd, FALSE, TRUE, &error);
    g_free (cmd);

    if (pip == NULL)
    {
        if (error != NULL)
            g_error_free (error);
        return FALSE;
    }

    pip->out.len = MC_PIPE_STREAM_UNREAD;
    pip->err.len = MC_PIPE_BUFSIZE;

    mc_pread (pip, &error);

    if (error != NULL)
    {
        g_error_free (error);
        mc_pclose_get_status (pip);
        return FALSE;
    }

    if (pip->err.len > 0)
    {
        mc_pclose_get_status (pip);
        return FALSE;
    }

    return (mc_pclose_get_status (pip) == 0);
}

/* --------------------------------------------------------------------------------------------- */

/* Open the archive and read its table of contents into all_entries.
   Returns the outcome, see arcmc_read_result_t. */
static arcmc_read_result_t
arcmc_read_archive_res (arcmc_data_t *data)
{
    struct archive *a;
    struct archive_entry *entry;
    arcmc_read_result_t res = ARCMC_READ_OK;
    arcmc_archive_reader_ctx_t *reader_ctx;
    GHashTable *by_path;
    GPtrArray *entries;
    arcmc_progress_t *progress;
    pass_t pass;
    int r;

    a = arcmc_archive_reader_open (data, &reader_ctx);
    if (a == NULL)
        return ARCMC_READ_FAILED;

    // the old list stays until the new one is read in full
    entries = g_ptr_array_new_with_free_func (arcmc_entry_free);
    by_path = g_hash_table_new (g_str_hash, g_str_equal);
    progress = read_progress_new (data, TRUE);
    pass_init (&pass, data, a, progress, PASS_TOC);

    while ((r = archive_read_next_header (a, &entry)) == ARCHIVE_OK)
    {
        const char *pathname;
        char *clean_path;
        char *basename_ptr;
        arcmc_entry_t *e;
        mode_t entry_mode;

        pathname = archive_entry_pathname (entry);

        if (!pass_report (&pass, pathname))
        {
            res = ARCMC_READ_ABORTED;
            break;
        }

        if (pathname == NULL || pathname[0] == '\0')
            continue;

        clean_path = g_strdup (pathname);
        strip_trailing_slashes (clean_path);

        if (clean_path[0] == '\0')
        {
            g_free (clean_path);
            continue;
        }

        /* reject path traversal attempts (Zip Slip) */
        if (!is_path_safe (clean_path))
        {
            g_free (clean_path);
            continue;
        }

        /* skip if duplicate */
        if (g_hash_table_contains (by_path, clean_path))
        {
            g_free (clean_path);
            continue;
        }

        /* ensure parent directories exist */
        ensure_parent_dirs (entries, by_path, clean_path);

        e = g_new0 (arcmc_entry_t, 1);
        e->full_path = clean_path;

        basename_ptr = strrchr (clean_path, '/');
        e->name = g_strdup (basename_ptr != NULL ? basename_ptr + 1 : clean_path);

        entry_mode = archive_entry_mode (entry);
        if (S_ISDIR (entry_mode))
            e->mode = S_IFDIR | (entry_mode & 07777);
        else if (archive_entry_filetype (entry) == AE_IFLNK)
        {
            e->mode = S_IFLNK | (entry_mode & 07777);
            e->linkname = g_strdup (archive_entry_symlink (entry));
        }
        else
            e->mode = S_IFREG | (entry_mode & 07777);

        e->size = archive_entry_size (entry);
        e->mtime = archive_entry_mtime (entry);
        e->is_virtual_dir = FALSE;

        add_entry (entries, by_path, e);

        if (!pass_skip (&pass, pathname))
        {
            res = ARCMC_READ_ABORTED;
            break;
        }
    }

    arcmc_progress_destroy (progress);

    /* Check if we failed due to encryption */
    if (res == ARCMC_READ_OK && r != ARCHIVE_EOF)
    {
        const char *err_str;

        err_str = archive_error_string (a);
        if (err_str != NULL
            && (strstr (err_str, "passphrase") != NULL || strstr (err_str, "password") != NULL
                || strstr (err_str, "ncrypt") != NULL))
            res = ARCMC_READ_ENCRYPTED;
    }

    /* entries may be listed while their content stays out of reach; a 7z always
       goes this way, no password given to libarchive ever unlocks it */
    if (res == ARCMC_READ_OK && archive_read_has_encrypted_entries (a) > 0
        && (data->password == NULL || archive_format (a) == ARCHIVE_FORMAT_7ZIP))
        res = ARCMC_READ_ENCRYPTED;

    /* libarchive decrypts neither 7z headers nor 7z content, whatever the password */
    if (res == ARCMC_READ_ENCRYPTED && archive_format (a) == ARCHIVE_FORMAT_7ZIP)
        res = ARCMC_READ_ENCRYPTED_7Z;

    arcmc_archive_reader_close (a, reader_ctx);
    g_hash_table_destroy (by_path);

    if (res == ARCMC_READ_OK)
    {
        arcmc_resolve_links (entries);
        arcmc_data_free_entries (data);
        data->all_entries = entries;
    }
    else
        g_ptr_array_free (entries, TRUE);
    return res;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
arcmc_read_archive (arcmc_data_t *data)
{
    return (arcmc_read_archive_res (data) == ARCMC_READ_OK);
}

/* --------------------------------------------------------------------------------------------- */

/* Try to open the archive, requesting password on encryption errors. */
gboolean
arcmc_try_open (arcmc_data_t *data)
{
    const arcmc_builtin_format_t *fmt;
    arcmc_read_result_t res;

    fmt = arcmc_find_builtin_format (data->archive_path);

    /* first attempt without password via libarchive, unless it is switched off */
    if (fmt != NULL && fmt->unpack == ARCMC_BACKEND_EXTERN)
        res = ARCMC_READ_FAILED;
    else
        res = arcmc_read_archive_res (data);

    if (res == ARCMC_READ_OK)
        return TRUE;
    if (res == ARCMC_READ_ABORTED)
        return FALSE;

    /* An external helper needs a filename. A file-backed stream has one; an
       arbitrary stream does not. */
    if (data->input_stream == NULL
        || mc_pp_input_stream_local_path (data->input_stream, NULL) != NULL)
    {
        /* try extfs helper as fallback */
        data->extfs_helper = arcmc_find_extfs_helper (data->archive_path);

        /* the archive may be a 7z named without the .7z extension */
        if (res == ARCMC_READ_ENCRYPTED_7Z && data->extfs_helper == NULL && fmt == NULL)
        {
            const arcmc_builtin_format_t *sevenzip = &arcmc_builtin_formats[ARCMC_FMT_7Z];

            if (sevenzip->enabled
                && (sevenzip->unpack == ARCMC_BACKEND_BOTH
                    || sevenzip->unpack == ARCMC_BACKEND_EXTERN))
                data->extfs_helper = arcmc_extfs_helper_path (sevenzip->extfs_helper);
        }
    }

    if (res == ARCMC_READ_FAILED)
    {
        if (data->extfs_helper != NULL && arcmc_read_archive_extfs (data))
            return TRUE;

        /* not an encryption problem, a password would not help */
        MC_PTR_FREE (data->extfs_helper);
        return FALSE;
    }

    /* encrypted 7z can only be read by the external 7z program */
    if (res == ARCMC_READ_ENCRYPTED_7Z && data->extfs_helper == NULL)
    {
        message (D_ERROR, MSG_ERROR, "%s",
                 _ ("Encrypted 7z needs the external tool, it is off in the archiver settings"));
        return FALSE;
    }

    if (res == ARCMC_READ_ENCRYPTED_7Z
        && arcmc_unpack_tool (fmt != NULL ? fmt : &arcmc_builtin_formats[ARCMC_FMT_7Z]) == NULL)
    {
        MC_PTR_FREE (data->extfs_helper);
        message (D_ERROR, MSG_ERROR, "%s",
                 _ ("Encrypted 7z archives need the 7z program to be installed"));
        return FALSE;
    }

    for (;;)
    {
        char *pw;

        pw = input_dialog (_ ("Archive password"), _ ("Enter password:"), "arcmc-password",
                           INPUT_PASSWORD, INPUT_COMPLETE_NONE);
        if (pw == NULL)
            break;

        g_free (data->password);
        data->password = pw;

        res = arcmc_read_archive_res (data);
        if (res == ARCMC_READ_OK)
            return TRUE;
        if (res == ARCMC_READ_ABORTED)
            break;

        if (data->extfs_helper != NULL)
        {
            gboolean stopped = FALSE;

            if (read_archive_extfs (data, &stopped))
                return TRUE;
            if (stopped)
                break;
        }

        message (D_ERROR, MSG_ERROR, "%s", _ ("Wrong password"));
    }

    MC_PTR_FREE (data->extfs_helper);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* Calculate total size of entries in an existing archive. */
off_t
arcmc_entries_total_size (GPtrArray *all_entries)
{
    guint i;
    off_t total = 0;

    if (all_entries == NULL)
        return 0;

    for (i = 0; i < all_entries->len; i++)
    {
        const arcmc_entry_t *e = (const arcmc_entry_t *) g_ptr_array_index (all_entries, i);

        if (!S_ISDIR (e->mode))
            total += e->size;
    }

    return total;
}

/* --------------------------------------------------------------------------------------------- */

/* Add a file to an existing archive by rewriting it:
   1. Read all entries from the old archive
   2. Write them + the new file to a temp archive
   3. Replace the old archive with the temp one
   `p` is an optional progress context (may be NULL).
   Returns TRUE on success. */
gboolean
arcmc_archive_add_file (const char *archive_path, const char *local_path, const char *archive_name,
                        const char *password, arcmc_progress_t *p)
{
    struct archive *reader, *writer;
    struct archive_entry *entry;
    int fmt;
    char *tmp_path;
    char buf[8192];
    gboolean ok = TRUE;

    fmt = arcmc_detect_fmt_id (archive_path);
    if (fmt < 0)
        return FALSE;

    tmp_path = g_strdup_printf ("%s.arcmc-tmp", archive_path);

    /* set up writer */
    writer = archive_write_new ();
    if (!arcmc_write_set_format (writer, fmt))
    {
        archive_write_free (writer);
        g_free (tmp_path);
        return FALSE;
    }

    if (password != NULL && password[0] != '\0')
    {
        archive_write_set_passphrase (writer, password);
        if (fmt == ARCMC_FMT_ZIP)
            archive_write_set_options (writer, "zip:encryption=aes256");
    }

    if (archive_write_open_filename (writer, tmp_path) != ARCHIVE_OK)
    {
        archive_write_free (writer);
        g_free (tmp_path);
        return FALSE;
    }

    /* copy existing entries */
    reader = archive_read_new ();
    archive_read_support_filter_all (reader);
    archive_read_support_format_all (reader);
    if (password != NULL)
        archive_read_add_passphrase (reader, password);

    if (archive_read_open_filename (reader, archive_path, 10240) == ARCHIVE_OK)
    {
        while (archive_read_next_header (reader, &entry) == ARCHIVE_OK)
        {
            const char *epath;
            la_ssize_t bytes;
            off_t entry_size, entry_done = 0;

            if (archive_write_header (writer, entry) != ARCHIVE_OK)
            {
                ok = FALSE;
                break;
            }
            epath = archive_entry_pathname (entry);
            entry_size = archive_entry_size (entry);

            while ((bytes = archive_read_data (reader, buf, sizeof (buf))) > 0)
            {
                la_ssize_t written;

                written = archive_write_data (writer, buf, (size_t) bytes);
                if (written < 0)
                {
                    ok = FALSE;
                    break;
                }
                entry_done += bytes;

                if (p != NULL)
                {
                    p->done_bytes += bytes;
                    p->written_bytes += written;

                    if (!arcmc_progress_update (p, epath, entry_size, entry_done, p->done_bytes,
                                                p->written_bytes))
                    {
                        ok = FALSE;
                        break;
                    }
                }
            }

            if (!ok)
                break;
        }
    }
    else
        ok = FALSE;
    archive_read_free (reader);

    /* add the new file */
    if (ok)
    {
        if (!arcmc_pack_add_file (writer, local_path, archive_name, p))
            ok = FALSE;
    }

    archive_write_close (writer);
    archive_write_free (writer);

    if (ok)
    {
        if (rename (tmp_path, archive_path) != 0)
            ok = FALSE;
    }

    if (!ok)
        unlink (tmp_path);

    g_free (tmp_path);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Rewrite the archive excluding entries whose paths match `del_paths`.
   `p` is an optional progress context (may be NULL).
   Returns TRUE on success. */
gboolean
arcmc_archive_delete (const char *archive_path, const char **del_paths, int del_count,
                      const char *password, arcmc_progress_t *p)
{
    struct archive *reader, *writer;
    struct archive_entry *entry;
    int fmt;
    char *tmp_path;
    char buf[8192];
    gboolean ok = TRUE;

    fmt = arcmc_detect_fmt_id (archive_path);
    if (fmt < 0)
        return FALSE;

    tmp_path = g_strdup_printf ("%s.arcmc-tmp", archive_path);

    writer = archive_write_new ();
    if (!arcmc_write_set_format (writer, fmt))
    {
        archive_write_free (writer);
        g_free (tmp_path);
        return FALSE;
    }

    if (password != NULL && password[0] != '\0')
    {
        archive_write_set_passphrase (writer, password);
        if (fmt == ARCMC_FMT_ZIP)
            archive_write_set_options (writer, "zip:encryption=aes256");
    }

    if (archive_write_open_filename (writer, tmp_path) != ARCHIVE_OK)
    {
        archive_write_free (writer);
        g_free (tmp_path);
        return FALSE;
    }

    reader = archive_read_new ();
    archive_read_support_filter_all (reader);
    archive_read_support_format_all (reader);
    if (password != NULL)
        archive_read_add_passphrase (reader, password);

    if (archive_read_open_filename (reader, archive_path, 10240) == ARCHIVE_OK)
    {
        while (archive_read_next_header (reader, &entry) == ARCHIVE_OK)
        {
            const char *pathname;
            char *clean;

            pathname = archive_entry_pathname (entry);
            if (pathname == NULL)
            {
                archive_read_data_skip (reader);
                continue;
            }

            clean = g_strdup (pathname);
            strip_trailing_slashes (clean);

            if (arcmc_path_is_deleted (clean, del_paths, del_count))
            {
                g_free (clean);
                archive_read_data_skip (reader);
                continue;
            }

            g_free (clean);

            if (archive_write_header (writer, entry) != ARCHIVE_OK)
            {
                ok = FALSE;
                break;
            }
            {
                la_ssize_t bytes;
                off_t entry_size, entry_done = 0;

                entry_size = archive_entry_size (entry);

                while ((bytes = archive_read_data (reader, buf, sizeof (buf))) > 0)
                {
                    la_ssize_t written;

                    written = archive_write_data (writer, buf, (size_t) bytes);
                    if (written < 0)
                    {
                        ok = FALSE;
                        break;
                    }
                    entry_done += bytes;

                    if (p != NULL)
                    {
                        p->done_bytes += bytes;
                        p->written_bytes += written;

                        if (!arcmc_progress_update (p, pathname, entry_size, entry_done,
                                                    p->done_bytes, p->written_bytes))
                        {
                            ok = FALSE;
                            break;
                        }
                    }
                }
            }

            if (!ok)
                break;
        }
    }
    else
        ok = FALSE;
    archive_read_free (reader);

    archive_write_close (writer);
    archive_write_free (writer);

    if (ok)
    {
        if (rename (tmp_path, archive_path) != 0)
            ok = FALSE;
    }

    if (!ok)
        unlink (tmp_path);

    g_free (tmp_path);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Create an encrypted 7z archive with the external 7z program.
   libarchive accepts a passphrase for 7z but writes the archive unencrypted. */
static gboolean
arcmc_7z_pack (const arcmc_pack_opts_t *opts, const char *cwd, GPtrArray *files, char **error_msg)
{
    const arcmc_builtin_format_t *fmt = &arcmc_builtin_formats[ARCMC_FMT_7Z];
    const char *bin;
    const char *args;
    GString *cmd;
    char *quoted;
    guint i;
    gboolean ok;

    /* the configured program wins; "7z" is looked up under its other names too */
    bin = fmt->pack_bin;
    if (bin == NULL || strcmp (bin, "7z") == 0)
        bin = arcmc_7z_bin ();

    if (bin == NULL || !arcmc_check_bin_available (bin))
    {
        if (error_msg != NULL)
            *error_msg = g_strdup (_ ("Encrypted 7z archives need the 7z program to be installed"));
        return FALSE;
    }

    /* -t7z: the format follows the chosen one, not the extension of the name */
    args = fmt->pack_args != NULL ? fmt->pack_args : "a -y -t7z";

    cmd = g_string_new ("");

    quoted = name_quote (cwd, FALSE);
    g_string_append_printf (cmd, "cd %s && %s %s", quoted, bin, args);
    g_free (quoted);

    switch (opts->compression)
    {
    case 0:
        g_string_append (cmd, " -mx=0");
        break;
    case 1:
        g_string_append (cmd, " -mx=1");
        break;
    case 3:
        g_string_append (cmd, " -mx=9");
        break;
    default:
        g_string_append (cmd, " -mx=5");
        break;
    }

    /* the tool also packs without a password when it is the chosen backend */
    if (opts->password != NULL && opts->password[0] != '\0')
    {
        quoted = arcmc_shell_quote (opts->password);
        g_string_append_printf (cmd, " -p%s", quoted);
        g_free (quoted);

        if (opts->encrypt_header)
            g_string_append (cmd, " -mhe=on");
    }

    quoted = name_quote (opts->archive_path, FALSE);
    g_string_append_printf (cmd, " %s", quoted);
    g_free (quoted);

    for (i = 0; i < files->len; i++)
    {
        quoted = name_quote ((const char *) g_ptr_array_index (files, i), FALSE);
        g_string_append_printf (cmd, " %s", quoted);
        g_free (quoted);
    }

    g_string_append (cmd, " 2>&1");

    ok = arcmc_ext_run_with_status (_ ("Creating archive..."), cmd->str, error_msg);
    g_string_free (cmd, TRUE);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Create an archive from the given file list.
   `cwd` is the current working directory for resolving relative names.
   `files` is an array of file/dir names (relative to cwd).
   Returns TRUE on success. */
gboolean
arcmc_do_pack (const arcmc_pack_opts_t *opts, const char *cwd, GPtrArray *files, char **error_msg)
{
    struct archive *a;
    guint i;
    arcmc_progress_t *progress;
    off_t total_size;
    gboolean aborted = FALSE;
    gboolean pack_error = FALSE;

    if (error_msg != NULL)
        *error_msg = NULL;

    /* the tool packs when it is the only backend, or when libarchive cannot
       do what was asked - 7z encryption */
    if (opts->format >= 0 && opts->format < (int) arcmc_builtin_formats_count)
    {
        const arcmc_builtin_format_t *fmt = &arcmc_builtin_formats[opts->format];
        gboolean has_password = (opts->password != NULL && opts->password[0] != '\0');

        if (opts->format == ARCMC_FMT_7Z
            && (fmt->pack == ARCMC_BACKEND_EXTERN
                || (fmt->pack == ARCMC_BACKEND_BOTH && has_password)))
            return arcmc_7z_pack (opts, cwd, files, error_msg);

        if (has_password && fmt->pack == ARCMC_BACKEND_BUILTIN && opts->format == ARCMC_FMT_7Z)
        {
            if (error_msg != NULL)
                *error_msg =
                    g_strdup (_ ("7z encryption needs the external tool, it is off in the archiver "
                                 "settings"));
            return FALSE;
        }
    }

    total_size = arcmc_calculate_total_size (cwd, files);
    progress = arcmc_progress_create (_ ("Creating archive..."), opts->archive_path, total_size);

    a = archive_write_new ();

    /* set archive format */
    switch (opts->format)
    {
    case ARCMC_FMT_ZIP:
        archive_write_set_format_zip (a);
        break;
    case ARCMC_FMT_7Z:
        archive_write_set_format_7zip (a);
        break;
    case ARCMC_FMT_TAR_GZ:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_gzip (a);
        break;
    case ARCMC_FMT_TAR_BZ2:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_bzip2 (a);
        break;
    case ARCMC_FMT_TAR_XZ:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_xz (a);
        break;
    case ARCMC_FMT_TAR:
        archive_write_set_format_pax_restricted (a);
        archive_write_add_filter_none (a);
        break;
    case ARCMC_FMT_CPIO:
        archive_write_set_format_cpio (a);
        archive_write_add_filter_none (a);
        break;
    default:
        archive_write_free (a);
        return FALSE;
    }

    /* set compression options */
    if (opts->format == ARCMC_FMT_ZIP || opts->format == ARCMC_FMT_7Z)
    {
        const char *level = NULL;

        switch (opts->compression)
        {
        case 0:
            level = "0";
            break;
        case 1:
            level = "1";
            break;
        case 2:
            level = "6";
            break;
        case 3:
            level = "9";
            break;
        default:
            break;
        }

        if (level != NULL)
        {
            char *opt_str;

            if (opts->format == ARCMC_FMT_ZIP)
                opt_str = g_strdup_printf ("zip:compression-level=%s", level);
            else
                opt_str = g_strdup_printf ("7zip:compression-level=%s", level);

            archive_write_set_options (a, opt_str);
            g_free (opt_str);
        }
    }

    /* set encryption */
    if (opts->password != NULL && opts->password[0] != '\0')
    {
        archive_write_set_passphrase (a, opts->password);

        if (opts->format == ARCMC_FMT_ZIP)
        {
            archive_write_set_options (a, "zip:encryption=aes256");
        }
    }

    /* resolve archive path relative to cwd */
    {
        char *archive_path;

        if (g_path_is_absolute (opts->archive_path))
            archive_path = g_strdup (opts->archive_path);
        else
            archive_path = g_build_filename (cwd, opts->archive_path, NULL);

        if (archive_write_open_filename (a, archive_path) != ARCHIVE_OK)
        {
            g_free (archive_path);
            archive_write_free (a);
            return FALSE;
        }
        g_free (archive_path);
    }

    /* add files */
    for (i = 0; i < files->len; i++)
    {
        const char *name = (const char *) g_ptr_array_index (files, i);
        char *full_path;
        struct stat st;
        const char *arc_name;

        full_path = g_build_filename (cwd, name, NULL);

        if (lstat (full_path, &st) != 0)
        {
            g_free (full_path);
            continue;
        }

        arc_name = opts->store_paths ? name : strrchr (name, '/');
        if (arc_name == NULL || !opts->store_paths)
            arc_name = name;

        {
            gboolean ok;

            if (S_ISDIR (st.st_mode))
                ok = arcmc_pack_add_directory (a, full_path, arc_name, progress);
            else
                ok = arcmc_pack_add_file (a, full_path, arc_name, progress);

            if (!ok)
            {
                if (progress->aborted)
                    aborted = TRUE;
                else
                    pack_error = TRUE;
                g_free (full_path);
                break;
            }
        }

        g_free (full_path);
    }

    archive_write_close (a);
    archive_write_free (a);

    arcmc_progress_destroy (progress);

    if (aborted || pack_error)
    {
        /* remove partial archive */
        char *archive_path;

        if (g_path_is_absolute (opts->archive_path))
            archive_path = g_strdup (opts->archive_path);
        else
            archive_path = g_build_filename (cwd, opts->archive_path, NULL);

        unlink (archive_path);
        g_free (archive_path);

        /* abort is not an error (user chose to cancel), but pack_error is */
        return aborted;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Extract the entry `target_path` from the current archive to a temp file.
   `p` is an optional progress context (may be NULL). */
mc_pp_result_t
arcmc_extract_entry (arcmc_data_t *data, const char *target_path, char **local_path,
                     arcmc_progress_t *p)
{
    struct archive *a;
    struct archive_entry *entry;
    arcmc_archive_reader_ctx_t *reader_ctx;
    pass_t pass;
    int r;

    a = arcmc_archive_reader_open (data, &reader_ctx);
    if (a == NULL)
        return MC_PPR_FAILED;

    pass_init (&pass, data, a, p, PASS_SEEK);

    while (archive_read_next_header (a, &entry) == ARCHIVE_OK)
    {
        const char *pathname;
        char *clean;

        pathname = archive_entry_pathname (entry);

        if (!pass_report (&pass, pathname))
        {
            arcmc_archive_reader_close (a, reader_ctx);
            return MC_PPR_FAILED;
        }

        if (pathname == NULL)
            continue;

        clean = g_strdup (pathname);
        strip_trailing_slashes (clean);

        if (strcmp (clean, target_path) != 0)
        {
            g_free (clean);
            if (!pass_skip (&pass, pathname))
            {
                arcmc_archive_reader_close (a, reader_ctx);
                return MC_PPR_FAILED;
            }
            continue;
        }

        g_free (clean);

        /* found the entry - extract it to a temp file */
        {
            GError *error = NULL;
            int fd;
            off_t file_size, file_done = 0;
            char *tmp_path = NULL;

            file_size = archive_entry_size (entry);

            fd = g_file_open_tmp ("mc-arcmc-XXXXXX", &tmp_path, &error);
            if (fd == -1)
            {
                if (error != NULL)
                    g_error_free (error);
                arcmc_archive_reader_close (a, reader_ctx);
                return MC_PPR_FAILED;
            }

            *local_path = tmp_path;
            mc_pp_rename_with_ext (local_path, target_path);

            for (;;)
            {
                const void *buff;
                size_t len;
                la_int64_t offset;

                r = archive_read_data_block (a, &buff, &len, &offset);
                if (r == ARCHIVE_EOF)
                    break;
                if (r != ARCHIVE_OK)
                {
                    close (fd);
                    unlink (*local_path);
                    g_free (*local_path);
                    *local_path = NULL;
                    arcmc_archive_reader_close (a, reader_ctx);
                    return MC_PPR_FAILED;
                }

                {
                    const char *wbuf = (const char *) buff;
                    size_t remaining = len;

                    while (remaining > 0)
                    {
                        ssize_t nw = write (fd, wbuf, remaining);

                        if (nw <= 0)
                        {
                            close (fd);
                            unlink (*local_path);
                            g_free (*local_path);
                            *local_path = NULL;
                            arcmc_archive_reader_close (a, reader_ctx);
                            return MC_PPR_FAILED;
                        }
                        wbuf += nw;
                        remaining -= (size_t) nw;
                    }
                }

                file_done += (off_t) len;

                if (p != NULL)
                {
                    p->done_bytes += (off_t) len;
                    p->written_bytes += (off_t) len;

                    if (!arcmc_progress_update (p, target_path, file_size, file_done, p->done_bytes,
                                                p->written_bytes))
                    {
                        close (fd);
                        unlink (*local_path);
                        g_free (*local_path);
                        *local_path = NULL;
                        arcmc_archive_reader_close (a, reader_ctx);
                        return MC_PPR_FAILED;
                    }
                }
            }

            close (fd);
        }

        arcmc_archive_reader_close (a, reader_ctx);
        return MC_PPR_OK;
    }

    /* entry not found */
    arcmc_archive_reader_close (a, reader_ctx);
    return MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

/* Extract the entry `name` from the current archive to a temp file.
   Returns MC_PPR_OK on success with `local_path` set. */
mc_pp_result_t
arcmc_extract_to_temp (arcmc_data_t *data, const char *name, char **local_path)
{
    char *target_path;
    mc_pp_result_t r;

    target_path = arcmc_target_path (data, name);

    if (data->extfs_helper != NULL)
        r = arcmc_extract_entry_extfs (data, target_path, local_path);
    else
    {
        arcmc_progress_t *progress;
        off_t file_size = 0;
        const arcmc_entry_t *e = arcmc_data_find_entry (data, target_path);

        if (e != NULL)
            file_size = e->size;

        progress = arcmc_progress_create (_ ("Extracting..."), data->archive_path, file_size);
        r = arcmc_extract_entry (data, target_path, local_path, progress);
        arcmc_progress_destroy (progress);
    }

    g_free (target_path);
    return r;
}

/* --------------------------------------------------------------------------------------------- */

/* Write the entry the reader stands on into `out_path`.
   `p` is an optional progress context (may be NULL). */
static gboolean
extract_current_to (struct archive *a, struct archive_entry *entry, const char *out_path,
                    arcmc_progress_t *p)
{
    off_t entry_size, done = 0;
    int fd;

    entry_size = archive_entry_size (entry);

    fd = open (out_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd == -1)
        return FALSE;

    for (;;)
    {
        const void *buff;
        size_t len, block_len;
        la_int64_t offset;
        const char *wbuf;
        int r;

        r = archive_read_data_block (a, &buff, &len, &offset);
        if (r == ARCHIVE_EOF)
            break;
        if (r != ARCHIVE_OK)
        {
            close (fd);
            return FALSE;
        }

        block_len = len;

        for (wbuf = (const char *) buff; len > 0;)
        {
            ssize_t nw = write (fd, wbuf, len);

            if (nw <= 0)
            {
                close (fd);
                return FALSE;
            }
            wbuf += nw;
            len -= (size_t) nw;
            done += (off_t) nw;
        }

        if (p != NULL)
        {
            p->done_bytes += (off_t) block_len;
            p->written_bytes += (off_t) block_len;

            if (!arcmc_progress_update (p, archive_entry_pathname (entry), entry_size, done,
                                        p->done_bytes, p->written_bytes))
            {
                close (fd);
                return FALSE;
            }
        }
    }

    close (fd);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Unpack everything below `src_dir` into `dest_path` in one pass over the
   archive. Entry by entry costs a fresh read of the archive each time, which on
   a compressed one means decompressing it again. Directories are left at 0755:
   the caller puts their modes back once the files are in place.
   `p` is an optional progress context (may be NULL). */
mc_pp_result_t
arcmc_extract_subtree (arcmc_data_t *data, const char *src_dir, const char *dest_path,
                       arcmc_progress_t *p)
{
    struct archive *a;
    struct archive_entry *entry;
    arcmc_archive_reader_ctx_t *reader_ctx;
    size_t dir_len;
    char *last_dir = NULL;
    mc_pp_result_t result = MC_PPR_OK;
    pass_t pass;

    a = arcmc_archive_reader_open (data, &reader_ctx);
    if (a == NULL)
        return MC_PPR_FAILED;

    dir_len = strlen (src_dir);
    pass_init (&pass, data, a, p, PASS_COPY);

    while (result == MC_PPR_OK && archive_read_next_header (a, &entry) == ARCHIVE_OK)
    {
        const char *pathname;
        char *clean, *out_path;
        mode_t mode;

        pathname = archive_entry_pathname (entry);
        if (pathname == NULL)
            continue;

        clean = g_strdup (pathname);
        strip_trailing_slashes (clean);

        if (!is_under_dir (clean, src_dir))
        {
            g_free (clean);
            if (!pass_report (&pass, pathname) || !pass_skip (&pass, pathname))
                result = MC_PPR_FAILED;
            continue;
        }

        mode = archive_entry_mode (entry);
        out_path =
            g_build_filename (dest_path, clean + (dir_len == 0 ? 0 : dir_len + 1), (char *) NULL);

        if (S_ISDIR (mode))
        {
            if (g_mkdir_with_parents (out_path, 0755) != 0)
                result = MC_PPR_FAILED;
        }
        else
        {
            char *out_dir;

            /* Listings are grouped by directory, so the same parent repeats. */
            out_dir = g_path_get_dirname (out_path);
            if (last_dir == NULL || strcmp (last_dir, out_dir) != 0)
            {
                if (g_mkdir_with_parents (out_dir, 0755) != 0)
                    result = MC_PPR_FAILED;
                g_free (last_dir);
                last_dir = out_dir;
            }
            else
                g_free (out_dir);

            if (result == MC_PPR_OK)
            {
                if (!extract_current_to (a, entry, out_path, p))
                    result = MC_PPR_FAILED;
                else if ((mode & 0777) != 0)
                    chmod (out_path, mode & 0777);
            }
        }

        g_free (out_path);
        g_free (clean);
    }

    g_free (last_dir);
    arcmc_archive_reader_close (a, reader_ctx);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/* Extract a file from the current archive and push it as a nested archive.
   `local_path` is the already-extracted temp file (takes ownership).
   Returns MC_PPR_OK on success, MC_PPR_FAILED on failure (temp file cleaned up). */
mc_pp_result_t
arcmc_push_nested (arcmc_data_t *data, char *local_path, const char *display_name)
{
    arcmc_nest_frame_t *frame;

    /* push current state onto the nest stack */
    frame = g_new0 (arcmc_nest_frame_t, 1);
    frame->prev = data->nest_stack;
    frame->archive_path = data->archive_path;
    frame->display_path = data->display_path;
    frame->current_dir = data->current_dir;
    frame->password = data->password;
    frame->extfs_helper = data->extfs_helper;
    frame->all_entries = data->all_entries;
    arcmc_data_entries_changed (data);
    frame->input_stream = data->input_stream;
    frame->temp_file = local_path;
    data->nest_stack = frame;

    /* switch to the nested archive */
    data->archive_path = g_strdup (local_path);
    data->display_path = g_strdup (display_name != NULL ? display_name : local_path);
    data->current_dir = g_strdup ("");
    data->password = NULL;
    data->extfs_helper = NULL;
    data->all_entries = NULL;
    data->input_stream = NULL;

    if (!arcmc_try_open (data))
    {
        /* failed -pop the stack and restore */
        arcmc_nest_frame_t *f = data->nest_stack;

        g_free (data->archive_path);
        g_free (data->display_path);
        g_free (data->current_dir);
        g_free (data->password);
        g_free (data->extfs_helper);
        arcmc_data_free_entries (data);

        data->archive_path = f->archive_path;
        data->display_path = f->display_path;
        data->current_dir = f->current_dir;
        data->password = f->password;
        data->extfs_helper = f->extfs_helper;
        data->all_entries = f->all_entries;
        data->input_stream = f->input_stream;
        data->nest_stack = f->prev;

        unlink (f->temp_file);
        g_free (f->temp_file);
        g_free (f);

        return MC_PPR_FAILED;
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* Check if a binary is available in PATH. */
gboolean
arcmc_check_bin_available (const char *bin_name)
{
    /* the settings dialog asks per row per redraw, and a miss walks the whole PATH */
    static GHashTable *cache = NULL;
    gpointer known;
    char *full_path;
    gboolean found;

    if (bin_name == NULL || bin_name[0] == '\0')
        return FALSE;

    if (cache == NULL)
        cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    known = g_hash_table_lookup (cache, bin_name);
    if (known != NULL)
        return (GPOINTER_TO_INT (known) > 0);

    full_path = g_find_program_in_path (bin_name);
    found = (full_path != NULL);
    g_free (full_path);

    g_hash_table_insert (cache, g_strdup (bin_name), GINT_TO_POINTER (found ? 1 : -1));

    return found;
}

/* --------------------------------------------------------------------------------------------- */

/* Status dialog for external archiver operations -shows last N lines of output. */

#define EXT_STATUS_MAX_LINES          10
#define EXT_STATUS_UPDATE_INTERVAL_US 100000 /* 100 ms */
#define EXT_STATUS_DLG_WIDTH          60

typedef struct
{
    simple_status_msg_t status_msg;
    GString *log;
    Widget *hline_w;
    Widget *button_w;
    gint64 last_update_time;
    gboolean dirty;
} arcmc_ext_status_msg_t;

/* --------------------------------------------------------------------------------------------- */

static void
arcmc_ext_status_init_cb (status_msg_t *sm)
{
    simple_status_msg_t *ssm = SIMPLE_STATUS_MSG (sm);
    arcmc_ext_status_msg_t *esm = (arcmc_ext_status_msg_t *) sm;
    Widget *wd = WIDGET (sm->dlg);
    WGroup *wg = GROUP (sm->dlg);
    WRect r;

    const char *b_name = _ ("&Abort");
    int wd_width, y;

    wd_width = EXT_STATUS_DLG_WIDTH;

    y = 2;
    ssm->label = label_new (y++, 3, _ ("Please wait..."));
    group_add_widget (wg, ssm->label);

    esm->hline_w = WIDGET (hline_new (y++, -1, -1));
    group_add_widget (wg, esm->hline_w);

    esm->button_w = WIDGET (button_new (y++, 3, B_CANCEL, NORMAL_BUTTON, b_name, NULL));
    group_add_widget_autopos (wg, esm->button_w, WPOS_KEEP_TOP | WPOS_CENTER_HORZ, NULL);

    r = wd->rect;
    r.lines = y + 2;
    r.cols = wd_width;
    widget_set_size_rect (wd, &r);
}

/* --------------------------------------------------------------------------------------------- */

static int
arcmc_ext_status_update_cb (status_msg_t *sm)
{
    simple_status_msg_t *ssm = SIMPLE_STATUS_MSG (sm);
    arcmc_ext_status_msg_t *esm = (arcmc_ext_status_msg_t *) sm;
    Widget *wd = WIDGET (sm->dlg);
    Widget *lw = WIDGET (ssm->label);
    const char *text;
    int label_lines;
    WRect r;

    text = (esm->log != NULL && esm->log->len > 0) ? esm->log->str : _ ("Please wait...");
    label_set_text (ssm->label, text);

    label_lines = lw->rect.lines;
    r = wd->rect;
    r.lines = MAX (r.lines, label_lines + 6);
    r.cols = EXT_STATUS_DLG_WIDTH;
    r.y = (LINES - r.lines) / 2;
    r.x = (COLS - r.cols) / 2;
    widget_set_size_rect (wd, &r);

    /* keep label within dialog bounds */
    {
        WRect lr = lw->rect;

        lr.x = r.x + 3;
        lr.cols = r.cols - 6;
        widget_set_size_rect (lw, &lr);
    }

    /* reposition hline below label */
    if (esm->hline_w != NULL)
    {
        WRect hr = esm->hline_w->rect;

        hr.y = r.y + 2 + label_lines;
        widget_set_size_rect (esm->hline_w, &hr);
    }

    /* reposition button below hline */
    if (esm->button_w != NULL)
    {
        WRect br = esm->button_w->rect;

        br.y = r.y + 3 + label_lines;
        br.x = r.x + (r.cols - br.cols) / 2;
        widget_set_size_rect (esm->button_w, &br);
    }

    return status_msg_common_update (sm);
}

/* --------------------------------------------------------------------------------------------- */

static void
arcmc_ext_status_deinit_cb (status_msg_t *sm)
{
    (void) sm;
}

/* --------------------------------------------------------------------------------------------- */

/* Update the log with a new line, keeping only the last EXT_STATUS_MAX_LINES lines.
   Redraws the dialog at most once per EXT_STATUS_UPDATE_INTERVAL_US. */
static gboolean
arcmc_ext_status_add_line (arcmc_ext_status_msg_t *esm, const char *line)
{
    gsize len = strlen (line);
    gint64 now;
    int max_width = EXT_STATUS_DLG_WIDTH - 6; /* 3 padding each side */

    /* trim trailing whitespace */
    while (len > 0
           && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' '
               || line[len - 1] == '\t'))
        len--;

    if (len == 0)
        return TRUE;

    /* trim leading whitespace */
    while (len > 0 && (*line == ' ' || *line == '\t'))
    {
        line++;
        len--;
    }

    if (len == 0)
        return TRUE;

    if (esm->log->len > 0)
        g_string_append_c (esm->log, '\n');

    /* truncate to fit dialog, then right-align with padding */
    {
        char *tmp = g_strndup (line, len);
        int tw = str_term_width1 (tmp);

        if (tw > max_width)
        {
            /* cut from the left: find the suffix that fits */
            char *p = tmp;

            while (str_term_width1 (p) > max_width - 1 && *p != '\0')
                str_next_char (&p);

            g_string_append_c (esm->log, '~');
            g_string_append (esm->log, p);
        }
        else
        {
            /* right-align: pad with spaces on the left */
            int pad = max_width - tw;
            int i;

            for (i = 0; i < pad; i++)
                g_string_append_c (esm->log, ' ');
            g_string_append (esm->log, tmp);
        }

        g_free (tmp);
    }

    /* keep only last N lines */
    {
        const char *s = esm->log->str;
        int count = 0;
        const char *p;

        for (p = s + esm->log->len - 1; p >= s; p--)
        {
            if (*p == '\n')
            {
                count++;
                if (count >= EXT_STATUS_MAX_LINES)
                {
                    g_string_erase (esm->log, 0, (gssize) (p - s + 1));
                    break;
                }
            }
        }
    }

    esm->dirty = TRUE;

    /* throttle redraws */
    now = g_get_monotonic_time ();
    if (now - esm->last_update_time < EXT_STATUS_UPDATE_INTERVAL_US)
        return TRUE;

    esm->last_update_time = now;
    esm->dirty = FALSE;

    return (STATUS_MSG (esm)->update (STATUS_MSG (esm)) != B_CANCEL);
}

/* --------------------------------------------------------------------------------------------- */

/* Flush pending update if dirty (call after read loop ends or between read chunks). */
static gboolean
arcmc_ext_status_flush (arcmc_ext_status_msg_t *esm)
{
    if (!esm->dirty)
        return TRUE;

    esm->dirty = FALSE;
    esm->last_update_time = g_get_monotonic_time ();

    return (STATUS_MSG (esm)->update (STATUS_MSG (esm)) != B_CANCEL);
}

/* --------------------------------------------------------------------------------------------- */

/* Run an external command with a status dialog showing its output log.
   Returns TRUE on success. On failure, *error_msg receives a description. */
static gboolean
arcmc_ext_run_with_status (const char *title, const char *cmd_str, char **error_msg)
{
    mc_pipe_t *pip;
    GError *error = NULL;
    gboolean ok = TRUE;
    gboolean cancelled = FALSE;
    arcmc_ext_status_msg_t esm;
    GString *err_buf;

    if (error_msg != NULL)
        *error_msg = NULL;

    pip = mc_popen (cmd_str, TRUE, TRUE, &error);
    if (pip == NULL)
    {
        if (error_msg != NULL)
            *error_msg = g_strdup (error != NULL ? error->message : "failed to run command");
        if (error != NULL)
            g_error_free (error);
        return FALSE;
    }

    memset (&esm, 0, sizeof (esm));
    esm.log = g_string_new (_ ("Running external archiver, please wait..."));

    status_msg_init (STATUS_MSG (&esm), title, 0.0, arcmc_ext_status_init_cb,
                     arcmc_ext_status_update_cb, arcmc_ext_status_deinit_cb);

    err_buf = g_string_new ("");

    while (TRUE)
    {
        pip->out.len = MC_PIPE_BUFSIZE;
        pip->out.null_term = TRUE;
        pip->err.len = MC_PIPE_BUFSIZE;

        mc_pread (pip, &error);
        if (error != NULL)
        {
            ok = FALSE;
            break;
        }

        /* collect stderr for error reporting */
        if (pip->err.len > 0)
            g_string_append_len (err_buf, pip->err.buf, pip->err.len);

        /* show stdout lines in log */
        if (pip->out.len > 0)
        {
            GString *line;

            while ((line = mc_pstream_get_string (&pip->out)) != NULL)
            {
                if (!arcmc_ext_status_add_line (&esm, line->str))
                {
                    cancelled = TRUE;
                    g_string_free (line, TRUE);
                    break;
                }
                g_string_free (line, TRUE);
            }
        }

        if (cancelled)
        {
            kill (pip->child_pid, SIGTERM);
            ok = FALSE;
            break;
        }

        if (pip->out.len == MC_PIPE_STREAM_EOF && pip->err.len == MC_PIPE_STREAM_EOF)
            break;
    }

    /* final flush to show last output */
    if (!cancelled)
        arcmc_ext_status_flush (&esm);

    status_msg_deinit (STATUS_MSG (&esm));

    if (cancelled)
    {
        if (error_msg != NULL)
            *error_msg = g_strdup (_ ("Operation cancelled"));
    }
    else if (!ok && error_msg != NULL)
    {
        if (err_buf->len > 0)
            *error_msg = g_string_free (err_buf, FALSE);
        else if (error != NULL)
            *error_msg = g_strdup (error->message);
        else
            *error_msg = g_strdup (_ ("Unknown error"));
        err_buf = NULL;
    }

    if (error != NULL)
        g_error_free (error);

    {
        int exit_status = mc_pclose_get_status (pip);

        if (ok && exit_status != 0)
        {
            ok = FALSE;
            if (error_msg != NULL)
            {
                if (err_buf != NULL && err_buf->len > 0)
                    *error_msg = g_string_free (err_buf, FALSE);
                else
                    *error_msg = g_strdup_printf ("Command exited with status %d", exit_status);
                err_buf = NULL;
            }
        }
    }

    if (err_buf != NULL)
        g_string_free (err_buf, TRUE);
    g_string_free (esm.log, TRUE);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Write file names to a temporary file (one per line).
   Returns the path to the temp file, or NULL on error. Caller must g_free(). */
static char *
write_file_list (GPtrArray *files)
{
    char *tpl;
    int fd;
    guint i;

    tpl = g_build_filename (g_get_tmp_dir (), "mc-filelist-XXXXXX", (char *) NULL);
    fd = mkstemp (tpl);
    if (fd < 0)
    {
        g_free (tpl);
        return NULL;
    }

    for (i = 0; i < files->len; i++)
    {
        const char *name = (const char *) g_ptr_array_index (files, i);
        size_t nlen = strlen (name);

        if (write (fd, name, nlen) != (ssize_t) nlen || write (fd, "\n", 1) != 1)
        {
            close (fd);
            unlink (tpl);
            g_free (tpl);
            return NULL;
        }
    }

    close (fd);
    return tpl;
}

/* --------------------------------------------------------------------------------------------- */

/* Pack files using an external archiver binary.
   If error_msg is not NULL, it receives a newly-allocated error string on failure. */
gboolean
arcmc_ext_pack (const arcmc_ext_archiver_t *archiver, const char *archive_path, const char *cwd,
                GPtrArray *files, const char *password, char **error_msg)
{
    GString *cmd;
    char *quoted;
    guint i;
    char *title;
    gboolean ok;

    (void) password;

    if (error_msg != NULL)
        *error_msg = NULL;

    if (archiver->pack_bin == NULL)
    {
        if (error_msg != NULL)
            *error_msg = g_strdup_printf ("%s: packing not supported", archiver->name);
        return FALSE;
    }

    if (!arcmc_check_bin_available (archiver->pack_bin))
    {
        if (error_msg != NULL)
            *error_msg = g_strdup_printf ("'%s' not found in PATH", archiver->pack_bin);
        return FALSE;
    }

    cmd = g_string_new ("");
    if (cwd != NULL)
    {
        quoted = name_quote (cwd, FALSE);
        g_string_append_printf (cmd, "cd %s && ", quoted);
        g_free (quoted);
    }

    g_string_append (cmd, archiver->pack_bin);
    if (archiver->pack_args != NULL && archiver->pack_args[0] != '\0')
        g_string_append_printf (cmd, " %s", archiver->pack_args);

    quoted = name_quote (archive_path, FALSE);
    g_string_append_printf (cmd, " %s", quoted);
    g_free (quoted);

    {
        size_t est_len = cmd->len;
        char *list_file = NULL;

        for (i = 0; i < files->len; i++)
            est_len += strlen ((const char *) g_ptr_array_index (files, i)) + 4;

        /* if command is too long and file-list is supported, use it */
        if (est_len > 131072 && archiver->list_file_arg != NULL)
        {
            char *list_arg;

            list_file = write_file_list (files);
            if (list_file != NULL)
            {
                list_arg = g_strdup_printf (archiver->list_file_arg, list_file);
                quoted = name_quote (list_arg, FALSE);
                g_string_append_printf (cmd, " %s", quoted);
                g_free (quoted);
                g_free (list_arg);
            }
        }

        if (list_file == NULL)
        {
            for (i = 0; i < files->len; i++)
            {
                const char *name = (const char *) g_ptr_array_index (files, i);

                quoted = name_quote (name, FALSE);
                g_string_append_printf (cmd, " %s", quoted);
                g_free (quoted);
            }
        }

        g_string_append (cmd, " 2>&1");

        title = g_strdup_printf (_ ("Packing: %s"), archiver->name);
        ok = arcmc_ext_run_with_status (title, cmd->str, error_msg);
        g_free (title);
        g_string_free (cmd, TRUE);

        if (list_file != NULL)
        {
            unlink (list_file);
            g_free (list_file);
        }
    }

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Extract an archive using an external archiver binary. */
gboolean
arcmc_ext_unpack (const arcmc_ext_archiver_t *archiver, const char *archive_path,
                  const char *dest_dir, const char *password)
{
    GString *cmd;
    char *quoted;
    char *title;
    gboolean ok;

    (void) password;

    if (archiver->unpack_bin == NULL)
        return FALSE;

    cmd = g_string_new (archiver->unpack_bin);
    if (archiver->unpack_args != NULL && archiver->unpack_args[0] != '\0')
        g_string_append_printf (cmd, " %s", archiver->unpack_args);

    quoted = name_quote (archive_path, FALSE);
    g_string_append_printf (cmd, " %s", quoted);
    g_free (quoted);

    if (dest_dir != NULL)
    {
        quoted = name_quote (dest_dir, FALSE);
        g_string_append_printf (cmd, " %s", quoted);
        g_free (quoted);
    }

    g_string_append (cmd, " 2>&1");

    title = g_strdup_printf (_ ("Extracting: %s"), archiver->name);
    ok = arcmc_ext_run_with_status (title, cmd->str, NULL);
    g_free (title);
    g_string_free (cmd, TRUE);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Extract specific files from an archive using an external archiver binary.
   Supports file-list argument (@filelist) when command line would be too long. */
gboolean
arcmc_ext_unpack_files (const arcmc_ext_archiver_t *archiver, const char *archive_path,
                        const char *dest_dir, GPtrArray *files, const char *password)
{
    GString *cmd;
    char *quoted;
    char *title;
    gboolean ok;
    guint i;
    size_t est_len;
    char *list_file = NULL;

    (void) password;

    if (archiver->unpack_bin == NULL || files == NULL || files->len == 0)
        return FALSE;

    /* estimate command length */
    est_len = strlen (archiver->unpack_bin) + strlen (archive_path) + 64;
    if (archiver->unpack_args != NULL)
        est_len += strlen (archiver->unpack_args);
    if (dest_dir != NULL)
        est_len += strlen (dest_dir) + 4;
    for (i = 0; i < files->len; i++)
        est_len += strlen ((const char *) g_ptr_array_index (files, i)) + 4;

    cmd = g_string_new ("");

    /* cd to dest dir so all archivers extract there regardless of syntax */
    if (dest_dir != NULL)
    {
        quoted = name_quote (dest_dir, FALSE);
        g_string_append_printf (cmd, "cd %s && ", quoted);
        g_free (quoted);
    }

    g_string_append (cmd, archiver->unpack_bin);
    if (archiver->unpack_args != NULL && archiver->unpack_args[0] != '\0')
        g_string_append_printf (cmd, " %s", archiver->unpack_args);

    quoted = name_quote (archive_path, FALSE);
    g_string_append_printf (cmd, " %s", quoted);
    g_free (quoted);

    /* if command is too long and file-list is supported, use it */
    if (est_len > 131072 && archiver->list_file_arg != NULL)
    {
        char *list_arg;

        list_file = write_file_list (files);
        if (list_file != NULL)
        {
            list_arg = g_strdup_printf (archiver->list_file_arg, list_file);
            quoted = name_quote (list_arg, FALSE);
            g_string_append_printf (cmd, " %s", quoted);
            g_free (quoted);
            g_free (list_arg);
        }
    }

    /* no file list used - append files inline */
    if (list_file == NULL)
    {
        for (i = 0; i < files->len; i++)
        {
            const char *name = (const char *) g_ptr_array_index (files, i);

            quoted = name_quote (name, FALSE);
            g_string_append_printf (cmd, " %s", quoted);
            g_free (quoted);
        }
    }

    g_string_append (cmd, " 2>&1");

    title = g_strdup_printf (_ ("Extracting: %s"), archiver->name);
    ok = arcmc_ext_run_with_status (title, cmd->str, NULL);
    g_free (title);

    g_string_free (cmd, TRUE);

    if (list_file != NULL)
    {
        unlink (list_file);
        g_free (list_file);
    }

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Test an archive using an external archiver binary. */
gboolean
arcmc_ext_test (const arcmc_ext_archiver_t *archiver, const char *archive_path,
                const char *password)
{
    GString *cmd;
    char *quoted;
    char *title;
    gboolean ok;

    (void) password;

    if (archiver->test_bin == NULL)
        return FALSE;

    cmd = g_string_new (archiver->test_bin);
    if (archiver->test_args != NULL && archiver->test_args[0] != '\0')
        g_string_append_printf (cmd, " %s", archiver->test_args);

    quoted = name_quote (archive_path, FALSE);
    g_string_append_printf (cmd, " %s", quoted);
    g_free (quoted);

    g_string_append (cmd, " 2>&1");

    title = g_strdup_printf (_ ("Testing: %s"), archiver->name);
    ok = arcmc_ext_run_with_status (title, cmd->str, NULL);
    g_free (title);
    g_string_free (cmd, TRUE);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */
