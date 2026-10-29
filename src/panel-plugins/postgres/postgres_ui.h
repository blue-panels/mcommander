/*
   Panel plugin postgres for the M-Commander
   messages and the status window around a wait for the server

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

/** \file postgres_ui.h
 *  \brief PostgreSQL plugin: messages and the status window around a wait for the server.
 */

#ifndef MC_PANEL_POSTGRES_UI_H
#define MC_PANEL_POSTGRES_UI_H

#include "lib/global.h"
#include "lib/widget.h"        // simple_status_msg_t
#include "lib/panel-plugin.h"  // mc_panel_host_t

/*** structures declarations (and typedefs of structures)*****************************************/

/* The status window shown while the plugin waits for the server */
typedef struct
{
    simple_status_msg_t base;
    gboolean active;
} postgres_status_t;

/*** declarations of public functions ************************************************************/

/* A message of the host, word-wrapped so that a long line does not stretch the dialog across the
   whole screen. */
void postgres_message (mc_panel_host_t *host, gboolean error, const char *format, ...)
    G_GNUC_PRINTF (3, 4);

void postgres_status_open (postgres_status_t *st, const char *title);
void postgres_status_set (postgres_status_t *st, const char *text);
void postgres_status_close (postgres_status_t *st);

#endif
