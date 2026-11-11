/*
   lib - a panel plugin for the tests of lazy loading

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

#include "lib/global.h"
#include "lib/panel-plugin.h"

#ifndef FIXTURE_NAME
#define FIXTURE_NAME "fixture"
#endif

/* --------------------------------------------------------------------------------------------- */

static void *
fixture_open (mc_panel_host_t *host, const char *open_path)
{
    (void) host;
    (void) open_path;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
fixture_close (void *plugin_data)
{
    (void) plugin_data;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
fixture_get_items (void *plugin_data, void *list)
{
    (void) plugin_data;
    (void) list;
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
fixture_may_open_name (const char *display_name)
{
    return g_str_has_suffix (display_name, ".fix");
}

/* --------------------------------------------------------------------------------------------- */

static void *
fixture_open_input_stream (mc_panel_host_t *host, const char *display_name,
                           mc_pp_input_stream_t *stream)
{
    (void) host;
    (void) display_name;
    (void) stream;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
fixture_configure (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* the test sees this from its own process */
static void
fixture_shutdown (void)
{
    g_setenv ("FIXTURE_SHUTDOWN", FIXTURE_NAME, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static const mc_pp_file_operation_t fixture_operations[] = {
    {
        .name = "open",
        .kind = MC_PP_FILE_OPERATION_OPEN,
        .may_open_name = fixture_may_open_name,
        .open_input_stream = fixture_open_input_stream,
    },
};

static void *
fixture_action (mc_panel_host_t *host, const char *open_path)
{
    (void) host;
    (void) open_path;
    return NULL;
}

static const mc_pp_action_t fixture_actions[] = {
    { "Do it", fixture_action },
#ifdef FIXTURE_V2
    { "Do more", fixture_action },
#endif
};

static const mc_pp_cmd_menu_entry_t fixture_menu[] = {
    { "&Do it", 0, "S-F1", 1234, MC_PP_MENU_PANEL },
};

static const mc_panel_plugin_t fixture_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = FIXTURE_NAME,
    .display_name = "Fixture",
    .prefix = "fixture:",
#ifdef FIXTURE_EAGER
    .flags = MC_PPF_NAVIGATE | MC_PPF_EAGER_LOAD,
#else
    .flags = MC_PPF_NAVIGATE,
#endif
    .open = fixture_open,
    .close = fixture_close,
    .get_items = fixture_get_items,
    .file_operations = fixture_operations,
    .file_operation_count = 1,
    .actions = fixture_actions,
    .action_count = G_N_ELEMENTS (fixture_actions),
    .cmd_menu_entries = fixture_menu,
    .cmd_menu_entry_count = 1,
    .configure = fixture_configure,
    .shutdown = fixture_shutdown,
};

/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    static int calls = 0;
    char count[16];

    g_snprintf (count, sizeof (count), "%d", ++calls);
    g_setenv ("FIXTURE_REGISTER_" FIXTURE_NAME, count, TRUE);
    return &fixture_plugin;
}

/* --------------------------------------------------------------------------------------------- */
