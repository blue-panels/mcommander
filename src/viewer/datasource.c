/*
   Internal file viewer for the Midnight Commander
   Functions for datasources

   Copyright (C) 1994-2025
   Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995, 1998
   Janne Kukonlehto, 1994, 1995
   Jakub Jelinek, 1995
   Joseph M. Hinkle, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Roland Illig <roland.illig@gmx.de>, 2004, 2005
   Slava Zanko <slavazanko@google.com>, 2009
   Andrew Borodin <aborodin@vmail.ru>, 2009
   Ilia Maslakov <il.smind@gmail.com>, 2009, 2026

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

/*
   The data source provides the viewer with data from either a file, a
   string or the output of a command. The mcview_get_byte() function can be
   used to get the value of a byte at a specific offset. If the offset
   is out of range, -1 is returned. The function mcview_get_byte_indexed(a,b)
   returns the byte at the offset a+b, or -1 if a+b is out of range.

   The mcview_set_byte() function has the effect that later calls to
   mcview_get_byte() will return the specified byte for this offset. This
   function is designed only for use by the hexedit component after
   saving its changes. Inspect the source before you want to use it for
   other purposes.

   The mcview_get_filesize() function returns the current size of the
   data source. If the growing buffer is used, this size may increase
   later on. Use the mcview_may_still_grow() function when you want to
   know if the size can change later.
 */

#include <config.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/vfs/vfs.h"
#include "lib/util.h"
#include "lib/tty/key.h"  // add_select_channel(), delete_select_channel()
#include "lib/widget.h"   // D_NORMAL, D_ERROR

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_stdio_pipe (WView *view, mc_pipe_t *p)
{
    p->out.len = MC_PIPE_BUFSIZE;
    p->out.null_term = FALSE;
    p->err.len = MC_PIPE_BUFSIZE;
    p->err.null_term = TRUE;
    view->datasource = DS_STDIO_PIPE;
    view->ds_stdio_pipe = p;
    view->pipe_first_err_msg = TRUE;

    mcview_growbuf_init (view);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_none (WView *view)
{
    view->datasource = DS_NONE;
}

/* --------------------------------------------------------------------------------------------- */

off_t
mcview_get_filesize (WView *view)
{
    switch (view->datasource)
    {
    case DS_STDIO_PIPE:
    case DS_VFS_PIPE:
    case DS_RAW_PIPE:
    case DS_GENERATOR:
        return mcview_growbuf_filesize (view);
    case DS_FILE:
        return view->ds_file_filesize;
    case DS_STRING:
        return view->ds_string_len;
    default:
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_update_filesize (WView *view)
{
    if (view->datasource == DS_FILE)
    {
        struct stat st;
        if (mc_fstat (view->ds_file_fd, &st) != -1 && st.st_size != view->ds_file_filesize)
        {
            view->ds_file_filesize = st.st_size;
            // cached line boundaries and widths may no longer match the file
            mcview_lcache_flush (view);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
mcview_get_ptr_file (WView *view, off_t byte_index)
{
    g_assert (view->datasource == DS_FILE);

    mcview_file_load_data (view, byte_index);
    if (mcview_already_loaded (view->ds_file_offset, byte_index, view->ds_file_datalen))
        return (char *) (view->ds_file_data + (byte_index - view->ds_file_offset));
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* Invalid UTF-8 is reported as negative integers (one for each byte),
 * see ticket 3783. */
gboolean
mcview_get_utf (WView *view, off_t byte_index, int *ch, int *ch_len)
{
    gchar *str = NULL;
    int res;
    gchar utf8buf[MB_LEN_MAX + 1];

    switch (view->datasource)
    {
    case DS_STDIO_PIPE:
    case DS_VFS_PIPE:
    case DS_RAW_PIPE:
    case DS_GENERATOR:
        str = mcview_get_ptr_growing_buffer (view, byte_index);
        break;
    case DS_FILE:
        str = mcview_get_ptr_file (view, byte_index);
        break;
    case DS_STRING:
        str = mcview_get_ptr_string (view, byte_index);
        break;
    case DS_NONE:
    default:
        break;
    }

    *ch = 0;

    if (str == NULL)
        return FALSE;

    res = g_utf8_get_char_validated (str, -1);

    if (res < 0)
    {
        // Retry with explicit bytes to make sure it's not a buffer boundary
        int i;

        for (i = 0; i < MB_LEN_MAX; i++)
        {
            if (mcview_get_byte (view, byte_index + i, &res))
                utf8buf[i] = res;
            else
            {
                utf8buf[i] = '\0';
                break;
            }
        }
        utf8buf[MB_LEN_MAX] = '\0';
        str = utf8buf;
        res = g_utf8_get_char_validated (str, -1);
    }

    if (res < 0)
    {
        // Implicit conversion from signed char to signed int keeps negative values.
        *ch = *str;
        *ch_len = 1;
    }
    else
    {
        gchar *next_ch = NULL;

        *ch = res;
        // Calculate UTF-8 char length
        next_ch = g_utf8_next_char (str);
        *ch_len = next_ch - str;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcview_get_ptr_string (WView *view, off_t byte_index)
{
    g_assert (view->datasource == DS_STRING);

    if (byte_index >= 0 && byte_index < (off_t) view->ds_string_len)
        return (char *) (view->ds_string_data + byte_index);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_get_byte_string (WView *view, off_t byte_index, int *retval)
{
    char *p;

    if (retval != NULL)
        *retval = -1;

    p = mcview_get_ptr_string (view, byte_index);
    if (p == NULL)
        return FALSE;

    if (retval != NULL)
        *retval = (unsigned char) (*p);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_get_byte_none (WView *view, off_t byte_index, int *retval)
{
    (void) &view;
    (void) byte_index;

    g_assert (view->datasource == DS_NONE);

    if (retval != NULL)
        *retval = -1;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_byte (WView *view, off_t offset, byte b)
{
    (void) &b;
    (void) offset;

    g_assert (offset < mcview_get_filesize (view));
    g_assert (view->datasource == DS_FILE);

    view->ds_file_datalen = 0;  // just force reloading
    mcview_lcache_flush (view);
}

/* --------------------------------------------------------------------------------------------- */

/*static */
void
mcview_file_load_data (WView *view, off_t byte_index)
{
    off_t blockoffset;
    ssize_t res;
    size_t bytes_read;

    g_assert (view->datasource == DS_FILE);

    if (mcview_already_loaded (view->ds_file_offset, byte_index, view->ds_file_datalen))
        return;

    if (byte_index >= view->ds_file_filesize)
        return;

    blockoffset = mcview_offset_rounddown (byte_index, view->ds_file_datasize);
    if (mc_lseek (view->ds_file_fd, blockoffset, SEEK_SET) == -1)
        goto error;

    bytes_read = 0;
    while (bytes_read < view->ds_file_datasize)
    {
        res = mc_read (view->ds_file_fd, view->ds_file_data + bytes_read,
                       view->ds_file_datasize - bytes_read);
        if (res == -1)
            goto error;
        if (res == 0)
            break;
        bytes_read += (size_t) res;
    }
    view->ds_file_offset = blockoffset;
    if ((off_t) bytes_read > view->ds_file_filesize - view->ds_file_offset)
    {
        // the file has grown in the meantime -- stick to the old size
        view->ds_file_datalen = view->ds_file_filesize - view->ds_file_offset;
    }
    else
    {
        view->ds_file_datalen = bytes_read;
    }
    return;

error:
    view->ds_file_datalen = 0;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_close_datasource (WView *view)
{
    switch (view->datasource)
    {
    case DS_NONE:
        break;
    case DS_GENERATOR:
        mcview_generator_stop (view);
        if (!view->growbuf_finished)
            mcview_source_state_notify (view, MCVIEW_SOURCE_CANCELLED, -1, 0);
        mcview_generator_unref (view->generator);
        view->generator = NULL;
        mcview_growbuf_free (view);
        break;
    case DS_STDIO_PIPE:
        if (view->ds_stdio_pipe != NULL)
        {
            mcview_growbuf_done (view, MCVIEW_SOURCE_CANCELLED);
            mcview_display (view);
        }
        mcview_growbuf_free (view);
        break;
    case DS_VFS_PIPE:
        if (view->ds_vfs_pipe != -1)
            mcview_growbuf_done (view, MCVIEW_SOURCE_CANCELLED);
        mcview_growbuf_free (view);
        break;
    case DS_RAW_PIPE:
        if (view->ds_raw_pipe != -1)
            mcview_growbuf_done (view, MCVIEW_SOURCE_CANCELLED);
        mcview_growbuf_free (view);
        break;
    case DS_FILE:
        (void) mc_close (view->ds_file_fd);
        view->ds_file_fd = -1;
        MC_PTR_FREE (view->ds_file_data);
        break;
    case DS_STRING:
        MC_PTR_FREE (view->ds_string_data);
        break;
    default:
        break;
    }
    view->datasource = DS_NONE;
    mcview_lcache_flush (view);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_file (WView *view, int fd, const struct stat *st)
{
    view->datasource = DS_FILE;
    view->ds_file_fd = fd;
    view->ds_file_filesize = st->st_size;
    view->ds_file_offset = 0;
    view->ds_file_data = g_malloc (4096);
    view->ds_file_datalen = 0;
    view->ds_file_datasize = 4096;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_load_command_output (WView *view, const char *command)
{
    mc_pipe_t *p;
    GError *error = NULL;

    mcview_close_datasource (view);

    p = mc_popen (command, TRUE, TRUE, &error);
    if (p == NULL)
    {
        mcview_display (view);
        mcview_show_error (view, NULL, error->message);
        g_error_free (error);
        return FALSE;
    }

    // Check if filter produced any output
    mcview_set_datasource_stdio_pipe (view, p);
    if (!mcview_get_byte (view, 0, NULL))
    {
        mcview_close_datasource (view);
        mcview_display (view);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_vfs_pipe (WView *view, int fd)
{
    g_assert (fd != -1);

    view->datasource = DS_VFS_PIPE;
    view->ds_vfs_pipe = fd;

    mcview_growbuf_init (view);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_raw_pipe (WView *view, int fd)
{
    g_assert (fd != -1);

    view->datasource = DS_RAW_PIPE;
    view->ds_raw_pipe = fd;

    mcview_growbuf_init (view);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_string (WView *view, const char *s)
{
    view->datasource = DS_STRING;
    view->ds_string_len = strlen (s);
    view->ds_string_data = (byte *) g_strndup (s, view->ds_string_len);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Idle hook for deferred stream redraw.
 * Runs from frontend_dlg_run() idle path, safe to call mcview_update().
 */

static void mcview_stream_redraw_hook (void *v);

/* Each view owns its queued redraw: cancelling one must not remove another's. */
static void
mcview_stream_remove_redraw (WView *view)
{
    hook_t **hook = &idle_hook;

    while (*hook != NULL)
    {
        hook_t *current = *hook;

        if (current->hook_fn == mcview_stream_redraw_hook && current->hook_data == view)
        {
            *hook = current->next;
            g_free (current);
        }
        else
            hook = &current->next;
    }
    view->stream_redraw_queued = FALSE;
}

static void
mcview_stream_redraw_hook (void *v)
{
    WView *view = (WView *) v;

    mcview_stream_remove_redraw (view);

    if (view->dirty > 0)
    {
        mcview_update (view);
        /* mcview_update writes directly into the viewer area. If a modal
           dialog (filter, source-options, etc.) is open over the viewer,
           the new bytes overwrite its frame. Refresh the dialog stack so
           any covering dialog is repainted on top. */
        do_refresh ();
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Select channel callback for streaming mode.
 * Called from tty input loop when pipe fd is readable.
 * Reads available data and queues deferred redraw via idle hook.
 * Returns 1 to hint tty_get_event() to return EV_NONE.
 */

static int
mcview_stream_ready (int fd, void *info)
{
    WView *view = (WView *) info;

    (void) fd;

    if (mcview_growbuf_read_available (view))
    {
        view->dirty++;

        if (!view->stream_redraw_queued)
        {
            add_hook (&idle_hook, mcview_stream_redraw_hook, view);
            view->stream_redraw_queued = TRUE;
        }

        return 1;
    }

    return 0;
}

/* Drain stderr independently to prevent child blockage and keep viewer input stdout-only. */
static int
mcview_stream_stderr_ready (int fd, void *info)
{
    char buffer[4096];
    ssize_t count;

    (void) info;
    do
        count = read (fd, buffer, sizeof (buffer));
    while (count > 0 || (count < 0 && errno == EINTR));
    if (count == 0)
        delete_select_channel (fd);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_stream_start (WView *view)
{
    int flags;

    g_assert (view->datasource == DS_STDIO_PIPE);
    g_assert (view->ds_stdio_pipe != NULL);

    flags = fcntl (view->ds_stdio_pipe->out.fd, F_GETFL);
    fcntl (view->ds_stdio_pipe->out.fd, F_SETFL, flags | O_NONBLOCK);

    add_select_channel (view->ds_stdio_pipe->out.fd, mcview_stream_ready, view);
    if (view->ds_stdio_pipe->err.fd >= 0)
    {
        flags = fcntl (view->ds_stdio_pipe->err.fd, F_GETFL);
        fcntl (view->ds_stdio_pipe->err.fd, F_SETFL, flags | O_NONBLOCK);
        add_select_channel (view->ds_stdio_pipe->err.fd, mcview_stream_stderr_ready, view);
    }
    view->streaming = TRUE;
    view->stream_active = TRUE;

    /* Force one deferred redraw after dialog startup even if data is already buffered
       before the first select callback gets a chance to run. */
    if (!view->stream_redraw_queued)
    {
        add_hook (&idle_hook, mcview_stream_redraw_hook, view);
        view->stream_redraw_queued = TRUE;
    }
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_stream_stop (WView *view)
{
    if (view->stream_active)
    {
        delete_select_channel (view->ds_stdio_pipe->out.fd);
        if (view->ds_stdio_pipe->err.fd >= 0)
            delete_select_channel (view->ds_stdio_pipe->err.fd);
        view->stream_active = FALSE;
    }

    if (view->stream_redraw_queued)
    {
        mcview_stream_remove_redraw (view);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* A source keeps its produced bytes across raw/cooked switches. Cloned specs
   share this state; only the installed source is scheduled. */
struct mcview_generator
{
    guint refs;
    GString *bytes;
    gboolean (*next) (void *, GString *, gboolean *);
    void *data;
    GDestroyNotify destroy;
    gboolean done;
    gboolean failed;
};

mcview_generator_t *
mcview_generator_new (const char *initial, gsize length,
                      gboolean (*next) (void *, GString *, gboolean *), void *data,
                      GDestroyNotify destroy)
{
    mcview_generator_t *generator = g_new0 (mcview_generator_t, 1);

    generator->refs = 1;
    generator->bytes = g_string_new_len (initial, length);
    generator->next = next;
    generator->data = data;
    generator->destroy = destroy;
    return generator;
}

mcview_generator_t *
mcview_generator_ref (mcview_generator_t *generator)
{
    if (generator != NULL)
        generator->refs++;
    return generator;
}

void
mcview_generator_unref (mcview_generator_t *generator)
{
    if (generator == NULL || --generator->refs != 0)
        return;
    generator->destroy (generator->data);
    g_string_free (generator->bytes, TRUE);
    g_free (generator);
}

void
mcview_generator_stop (WView *view)
{
    if (view->generator_wakeup[0] >= 0)
    {
        delete_select_channel (view->generator_wakeup[0]);
        close (view->generator_wakeup[0]);
        close (view->generator_wakeup[1]);
        view->generator_wakeup[0] = view->generator_wakeup[1] = -1;
    }
    if (view->stream_redraw_queued)
    {
        mcview_stream_remove_redraw (view);
    }
}

static void
mcview_generator_wake (WView *view)
{
    ssize_t written;

    do
        written = write (view->generator_wakeup[1], "x", 1);
    while (written < 0 && errno == EINTR);
}

void
mcview_generator_step (WView *view)
{
    mcview_generator_t *generator = view->generator;
    const gint64 deadline = g_get_monotonic_time () + 4000;
    GString *chunk;
    char wake;
    ssize_t count;
    guint steps = 0;

    if (view->datasource != DS_GENERATOR || view->growbuf_finished || view->generator_wakeup[0] < 0)
        return;
    chunk = g_string_new (NULL);
    do
        count = read (view->generator_wakeup[0], &wake, 1);
    while (count < 0 && errno == EINTR);
    do
    {
        g_string_truncate (chunk, 0);
        if (!generator->next (generator->data, chunk, &generator->done)
            || chunk->len > 64U * 1024U * 1024U - generator->bytes->len)
        {
            generator->failed = generator->done = TRUE;
            break;
        }
        g_string_append_len (generator->bytes, chunk->str, chunk->len);
        mcview_growbuf_append (view, chunk->str, chunk->len);
    }
    while (!generator->done && ++steps < 64 && g_get_monotonic_time () < deadline);
    g_string_free (chunk, TRUE);
    if (generator->done)
    {
        view->growbuf_finished = TRUE;
        mcview_generator_stop (view);
        mcview_source_state_notify (
            view, generator->failed ? MCVIEW_SOURCE_FAILED : MCVIEW_SOURCE_FINISHED,
            generator->failed ? -1 : 0, 0);
    }
    else
        mcview_generator_wake (view);
    /* Rendering can produce many small blocks; cap progress redraws at 30 Hz.
       Keys still redraw immediately through the normal viewer callback. */
    if (!generator->done && g_get_monotonic_time () < view->generator_redraw_at)
        return;
    view->generator_redraw_at = g_get_monotonic_time () + 33000;
    view->dirty++;
    if (!view->stream_redraw_queued)
    {
        add_hook (&idle_hook, mcview_stream_redraw_hook, view);
        view->stream_redraw_queued = TRUE;
    }
}

static int
mcview_generator_ready (int fd, void *data)
{
    (void) fd;
    mcview_generator_step (data);
    return 1;
}

void
mcview_set_datasource_generator (WView *view, mcview_generator_t *generator, int wakeup[2])
{
    view->datasource = DS_GENERATOR;
    view->generator = mcview_generator_ref (generator);
    view->generator_wakeup[0] = wakeup[0];
    view->generator_wakeup[1] = wakeup[1];
    view->generator_redraw_at = g_get_monotonic_time () + 33000;
    mcview_growbuf_init (view);
    mcview_growbuf_append (view, generator->bytes->str, generator->bytes->len);
    view->growbuf_finished = generator->done;
    if (generator->done)
        mcview_generator_stop (view);
    else
    {
        add_select_channel (wakeup[0], mcview_generator_ready, view);
        mcview_generator_wake (view);
    }
}
