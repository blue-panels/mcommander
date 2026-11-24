/*
   Interface to the terminal controlling library for the M-Commander
   Commands of the kitty graphics protocol

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

#include <string.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "lib/global.h"

#include "kitty_graphics.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define APC_START ESC_STR "_G"
#define APC_END   ESC_STR "\\"

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tty_kitty_transmit (GString *out, guint32 id, const guint8 *rgba, int width, int height)
{
    const gsize size = (gsize) width * (gsize) height * 4;
    const guint8 *payload = rgba;
    gsize payload_len = size;
    gboolean compressed = FALSE;
    guint8 *packed = NULL;
    gchar *b64;
    gsize b64_len, pos;

#ifdef HAVE_ZLIB
    {
        uLongf packed_len = compressBound ((uLong) size);

        packed = g_malloc (packed_len);
        if (compress2 (packed, &packed_len, rgba, (uLong) size, Z_BEST_SPEED) == Z_OK)
        {
            payload = packed;
            payload_len = packed_len;
            compressed = TRUE;
        }
    }
#endif

    b64 = g_base64_encode (payload, payload_len);
    b64_len = strlen (b64);
    g_free (packed);

    for (pos = 0; pos == 0 || pos < b64_len; pos += TTY_KITTY_CHUNK)
    {
        const gsize n = MIN (TTY_KITTY_CHUNK, b64_len - pos);
        const gboolean more = pos + n < b64_len;

        g_string_append (out, APC_START);
        if (pos == 0)
            g_string_append_printf (out, "a=t,i=%u,f=32,s=%d,v=%d,%s", id, width, height,
                                    compressed ? "o=z," : "");
        g_string_append_printf (out, "m=%d,q=2;", more ? 1 : 0);
        g_string_append_len (out, b64 + pos, (gssize) n);
        g_string_append (out, APC_END);
    }

    g_free (b64);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_kitty_put (GString *out, guint32 id, guint32 pid)
{
    g_string_append_printf (out, APC_START "a=p,i=%u,p=%u,C=1,q=2" APC_END, id, pid);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_kitty_delete_placement (GString *out, guint32 id, guint32 pid)
{
    g_string_append_printf (out, APC_START "a=d,d=i,i=%u,p=%u,q=2" APC_END, id, pid);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_kitty_free (GString *out, guint32 id)
{
    g_string_append_printf (out, APC_START "a=d,d=I,i=%u,q=2" APC_END, id);
}

/* --------------------------------------------------------------------------------------------- */
