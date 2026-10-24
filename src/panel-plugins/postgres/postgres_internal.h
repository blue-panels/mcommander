/*
   Panel plugin postgres for the M-Commander
   the state of a panel and the functions its modules share

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

/** \file postgres_internal.h
 *  \brief PostgreSQL plugin: the state of a panel and the functions its modules share.
 */

#ifndef MC_PANEL_POSTGRES_INTERNAL_H
#define MC_PANEL_POSTGRES_INTERNAL_H

#include "lib/global.h"
#include "lib/panel-plugin.h"  // mc_panel_host_t, mc_pp_result_t

#include "postgres_config.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define POSTGRES_HELP_NODE "[PostgreSQL Plugin]"
#define POSTGRES_HELP_FILE MC_PLUGIN_DIR "/postgres_panel.md"

/*** enums ***************************************************************************************/

typedef enum
{
    POSTGRES_LEVEL_CONNECTIONS = 0,
} postgres_level_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    mc_panel_host_t *host;
    postgres_config_t *cfg;
    postgres_level_t level;

    char *focus_after_up;  // the entry the cursor goes to when the list is read next
} postgres_data_t;

/*** declarations of public functions ************************************************************/

/* postgres_connections.c */

mc_pp_result_t postgres_create_connection (postgres_data_t *data);
mc_pp_result_t postgres_edit_connection (postgres_data_t *data);
mc_pp_result_t postgres_delete_connections (postgres_data_t *data, const char **names, int count);
void postgres_configure (void);

#endif
