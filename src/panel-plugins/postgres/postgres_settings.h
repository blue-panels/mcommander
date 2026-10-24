/*
   Panel plugin postgres for the M-Commander
   the dialogs of a connection and of the general options

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

/** \file postgres_settings.h
 *  \brief PostgreSQL plugin: the dialogs of a connection and of the general options.
 */

#ifndef MC_PANEL_POSTGRES_SETTINGS_H
#define MC_PANEL_POSTGRES_SETTINGS_H

#include "lib/global.h"

#include "postgres_config.h"

/*** declarations of public functions ************************************************************/

/* The editor of one connection: on OK with a valid name @c is changed in place and TRUE is
   returned; the caller saves it. @is_new only chooses the title. */
gboolean postgres_connection_dialog_run (postgres_connection_t *c, gboolean is_new);

/* The general options; on OK @cfg is changed and [General] of postgres.ini written. */
gboolean postgres_settings_dialog_run (postgres_config_t *cfg);

#endif
