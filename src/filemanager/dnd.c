/*
   Files dropped on the terminal window.

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

/** \file dnd.c
 *  \brief Source: files dropped on the terminal window
 *
 *  A far2l terminal that has files dropped on it tells mc where (lib/tty/far2l.h).  The files
 *  are not sent unasked: mc lists them and reads the ranges it wants, one chunk per request,
 *  which is what lets the same protocol serve a local mc and one behind SSH.  The copy is done
 *  here, by mc's own file access, into the directory of the panel under the drop - so a panel
 *  that shows an archive or a remote host is not a special case.
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "src/mcterm/mcterm.h"

#include "filemanager.h"
#include "layout.h"
#include "mcterm_overlay.h"
#include "panel.h"

#include "dnd.h"

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef enum
{
    OVERWRITE_ASK,
    OVERWRITE_ALL
} overwrite_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static const char *
status_text (int status)
{
    switch (status)
    {
    case FAR2L_E_DENIED:
        return _ ("access denied");
    case FAR2L_E_OFFER_GONE:
        return _ ("the drop is no longer available");
    case FAR2L_E_UNKNOWN_ITEM:
        return _ ("unknown item");
    case FAR2L_E_CHANGED:
        return _ ("the source file changed");
    case FAR2L_E_LIMIT:
        return _ ("too much at once");
    case FAR2L_E_CANCELLED:
        return _ ("cancelled");
    case FAR2L_E_TIMEOUT:
        return _ ("the terminal did not answer");
    case FAR2L_E_PROTOCOL:
        return _ ("bad answer of the terminal");
    case FAR2L_E_NO_DND:
        return _ ("the terminal does not support drag and drop");
    default:
        return _ ("I/O error");
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
point_in_widget (const Widget *w, int x, int y)
{
    return x >= w->rect.x && x < w->rect.x + w->rect.cols && y >= w->rect.y
        && y < w->rect.y + w->rect.lines;
}

/* --------------------------------------------------------------------------------------------- */

/* The listing panel that covers the cell, if any. */

static WPanel *
panel_at (int x, int y)
{
    int i;

    for (i = 0; i < 2; i++)
    {
        WPanel *p = i == 0 ? left_panel : right_panel;

        if (p != NULL && get_panel_type (i) == view_listing
            && widget_get_state (CONST_WIDGET (p), WST_VISIBLE)
            && point_in_widget (CONST_WIDGET (p), x, y))
            return p;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* Pull the bytes of one item into the file. */

static int
receive_file (const guint8 *offer, const far2l_dnd_entry_t *entry, const vfs_path_t *target)
{
    guint64 offset = 0;
    int fd, status = FAR2L_OK;
    guint32 chunk = far2l_dnd_max_chunk ();

    fd = mc_open (target, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return FAR2L_E_IO;

    while (TRUE)
    {
        guint8 *data = NULL;
        gsize len = 0;
        gboolean eof = FALSE;
        gsize done = 0;

        status = far2l_dnd_read (offer, entry->item_id, offset, chunk, &data, &len, &eof);
        if (status != FAR2L_OK)
            break;

        while (done < len)
        {
            ssize_t w = mc_write (fd, data + done, len - done);

            if (w < 0)
            {
                if (errno == EINTR)
                    continue;
                status = FAR2L_E_IO;
                break;
            }
            done += (gsize) w;
        }
        g_free (data);
        if (status != FAR2L_OK)
            break;

        offset += len;
        if (eof || len == 0)
            break;
    }

    if (mc_close (fd) != 0 && status == FAR2L_OK)
        status = FAR2L_E_IO;
    /* a half-received file is worse than none */
    if (status != FAR2L_OK)
        (void) mc_unlink (target);
    return status;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
filemanager_dnd_drop (const far2l_drop_t *drop)
{
    GPtrArray *listed, *files;
    WPanel *dest = NULL;
    overwrite_t overwrite = OVERWRITE_ASK;
    guint i, received = 0, failed = 0;
    gboolean cancelled = FALSE;
    int st;

    /* only the file manager itself takes files: not while a dialog, the viewer or the editor
       is on top, and not while the panels are hidden */
    if (top_dlg == NULL || top_dlg->data != filemanager || current_panel == NULL
        || mc_global.mc_run_mode != MC_RUN_FULL)
    {
        far2l_dnd_close (drop->offer, FAR2L_DND_CLOSE_REJECTED);
        return;
    }

    if (drop->x >= 0 && drop->y >= 0)
    {
        dest = panel_at (drop->x, drop->y);
        if (dest == NULL)
        {
            /* On mc's own terminal it is the program's: a program that has bound drop reception
               is told, and its LIST and READ are answered from the offer of ours. */
            WMcTerm *term = mcterm_overlay_terminal_at (drop->x, drop->y);

            if (term != NULL && mcterm_far2l_take_drop (term, drop))
                return;

            /* it did not land on a panel */
            far2l_dnd_close (drop->offer, FAR2L_DND_CLOSE_REJECTED);
            return;
        }
    }

    listed = g_ptr_array_new_with_free_func (far2l_dnd_entry_free);
    st = far2l_dnd_list (drop->offer, listed);
    if (st != FAR2L_OK)
    {
        message (D_ERROR, _ ("Drag and drop"), _ ("Cannot get the list of dropped files:\n%s"),
                 status_text (st));
        far2l_dnd_close (drop->offer, FAR2L_DND_CLOSE_FAILED);
        g_ptr_array_free (listed, TRUE);
        return;
    }

    /* what can be taken: plain files that can be read from the start, with a name that is a
       name; directories and the rest are not part of the first version of the protocol */
    files = g_ptr_array_new ();
    for (i = 0; i < listed->len; i++)
    {
        far2l_dnd_entry_t *e = g_ptr_array_index (listed, i);

        if (e->kind == FAR2L_DND_KIND_FILE && (e->flags & FAR2L_DND_ITEM_STREAM) != 0
            && far2l_dnd_name_is_safe (e->name))
            g_ptr_array_add (files, e);
    }

    if (files->len == 0)
    {
        message (D_ERROR, _ ("Drag and drop"), "%s",
                 _ ("There is nothing to copy: only files can be dropped."));
        far2l_dnd_close (drop->offer, FAR2L_DND_CLOSE_REJECTED);
    }
    else
    {
        if (dest == NULL)
        {
            /* the terminal does not know where the drop landed: ask, do not guess (0,0) */
            char *text = g_strdup_printf (_ ("Copy %u dropped file(s) to\n%s?"), files->len,
                                          vfs_path_as_str (current_panel->cwd_vpath));
            int answer =
                query_dialog (_ ("Drag and drop"), text, D_NORMAL, 2, _ ("&Yes"), _ ("&No"));

            g_free (text);
            if (answer == 0)
                dest = current_panel;
        }

        if (dest == NULL)
            far2l_dnd_close (drop->offer, FAR2L_DND_CLOSE_REJECTED);
        else
        {
            for (i = 0; i < files->len && !cancelled; i++)
            {
                const far2l_dnd_entry_t *e = g_ptr_array_index (files, i);
                vfs_path_t *target = vfs_path_append_new (dest->cwd_vpath, e->name, (char *) NULL);
                struct stat sb;
                WDialog *progress;

                if (overwrite == OVERWRITE_ASK && mc_stat (target, &sb) == 0)
                {
                    char *text = g_strdup_printf (_ ("%s already exists in\n%s"), e->name,
                                                  vfs_path_as_str (dest->cwd_vpath));
                    int answer =
                        query_dialog (_ ("Drag and drop"), text, D_ERROR, 4, _ ("&Overwrite"),
                                      _ ("&Skip"), _ ("Overwrite &all"), _ ("&Cancel"));

                    g_free (text);
                    if (answer == 1)
                    {
                        vfs_path_free (target, TRUE);
                        continue;
                    }
                    if (answer == 2)
                        overwrite = OVERWRITE_ALL;
                    else if (answer != 0)
                    {
                        vfs_path_free (target, TRUE);
                        cancelled = TRUE;
                        break;
                    }
                }

                progress =
                    create_message (D_NORMAL, _ ("Drag and drop"), _ ("Receiving %s"), e->name);
                mc_refresh ();
                st = receive_file (drop->offer, e, target);
                dlg_run_done (progress);
                widget_destroy (WIDGET (progress));
                vfs_path_free (target, TRUE);

                if (st == FAR2L_OK)
                    received++;
                else
                {
                    failed++;
                    message (D_ERROR, _ ("Drag and drop"), _ ("Cannot receive %s:\n%s"), e->name,
                             status_text (st));
                    /* the offer is gone or the terminal is not answering: the rest will not
                       come either */
                    if (st == FAR2L_E_OFFER_GONE || st == FAR2L_E_TIMEOUT || st == FAR2L_E_PROTOCOL
                        || st == FAR2L_E_CANCELLED)
                        break;
                }
            }

            far2l_dnd_close (
                drop->offer,
                failed != 0 ? FAR2L_DND_CLOSE_FAILED
                            : (cancelled ? FAR2L_DND_CLOSE_CANCELLED : FAR2L_DND_CLOSE_PROCESSED));
            if (received != 0)
                update_panels (UP_RELOAD, UP_KEEPSEL);
        }
    }

    g_ptr_array_free (files, TRUE);
    g_ptr_array_free (listed, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
