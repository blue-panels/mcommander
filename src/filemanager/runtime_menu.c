/*
   File manager for the M-Commander
   Menu entries of runtime plugins

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

/** \file runtime_menu.c
 *  \brief Source: menu entries of runtime plugins in the file manager
 */

#include <config.h>

#include "lib/global.h"
#include "lib/extension-runtime.h"
#include "lib/widget.h"

#include "runtime_menu.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* Above the commands of the panel plugins (CK_PluginActionBase and up) */
#define RUNTIME_MENU_ACTION_BASE 1000000L

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *runtime_name;
    char *id;
    char *menu_path;
    char *label;
    char *shortcut;
    gint position;
    guint registration_order;
} runtime_menu_action_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static GPtrArray *runtime_menu_actions = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
runtime_menu_action_free (gpointer data)
{
    runtime_menu_action_t *action = (runtime_menu_action_t *) data;

    g_free (action->runtime_name);
    g_free (action->id);
    g_free (action->menu_path);
    g_free (action->label);
    g_free (action->shortcut);
    g_free (action);
}

/* --------------------------------------------------------------------------------------------- */

static void
runtime_menu_collect (const char *runtime_name, const char *id, const char *menu_path,
                      const char *label, const char *shortcut, gint position, gpointer user_data)
{
    GPtrArray *actions = (GPtrArray *) user_data;
    runtime_menu_action_t *action;

    if (runtime_name == NULL || id == NULL || menu_path == NULL || menu_path[0] == '\0'
        || label == NULL || label[0] == '\0')
        return;

    action = g_new0 (runtime_menu_action_t, 1);
    action->runtime_name = g_strdup (runtime_name);
    action->id = g_strdup (id);
    action->menu_path = g_strdup (menu_path);
    action->label = g_strdup (label);
    action->shortcut = g_strdup (shortcut);
    action->position = position;
    action->registration_order = actions->len;
    g_ptr_array_add (actions, action);
}

/* --------------------------------------------------------------------------------------------- */

static gint
runtime_menu_action_compare (gconstpointer left, gconstpointer right)
{
    const runtime_menu_action_t *a = *(const runtime_menu_action_t *const *) left;
    const runtime_menu_action_t *b = *(const runtime_menu_action_t *const *) right;
    int result;

    result = g_strcmp0 (a->menu_path, b->menu_path);
    if (result != 0)
        return result;
    if (a->position != b->position)
        return a->position < b->position ? -1 : 1;
    if (a->registration_order != b->registration_order)
        return a->registration_order < b->registration_order ? -1 : 1;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
runtime_menu_reload (void)
{
    g_clear_pointer (&runtime_menu_actions, g_ptr_array_unref);
    runtime_menu_actions = g_ptr_array_new_with_free_func (runtime_menu_action_free);
    mc_runtime_plugins_enumerate_menu_actions (RUNTIME_MENU_WORKSPACE, runtime_menu_collect,
                                               runtime_menu_actions);
    g_ptr_array_sort (runtime_menu_actions, runtime_menu_action_compare);
}

/* --------------------------------------------------------------------------------------------- */

GList *
runtime_menu_prepend_entries (GList *entries, const char *menu_path)
{
    gboolean first = TRUE;
    guint i;

    for (i = 0; runtime_menu_actions != NULL && i < runtime_menu_actions->len; i++)
    {
        const runtime_menu_action_t *action =
            (const runtime_menu_action_t *) g_ptr_array_index (runtime_menu_actions, i);
        menu_entry_t *entry;

        if (g_strcmp0 (action->menu_path, menu_path) != 0)
            continue;

        if (first)
        {
            entries = g_list_prepend (entries, menu_separator_new ());
            first = FALSE;
        }

        entry = menu_entry_new (action->label, RUNTIME_MENU_ACTION_BASE + (long) i);
        if (action->shortcut != NULL)
            menu_entry_set_shortcut (entry, action->shortcut);
        entries = g_list_prepend (entries, entry);
    }

    return entries;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
runtime_menu_execute (long command)
{
    const runtime_menu_action_t *action;
    const char *error = NULL;
    guint index;

    if (command < RUNTIME_MENU_ACTION_BASE)
        return FALSE;

    index = (guint) (command - RUNTIME_MENU_ACTION_BASE);
    if (runtime_menu_actions == NULL || index >= runtime_menu_actions->len)
        return TRUE;

    action = (const runtime_menu_action_t *) g_ptr_array_index (runtime_menu_actions, index);
    if (!mc_runtime_plugins_invoke_action (action->runtime_name, RUNTIME_MENU_WORKSPACE, action->id,
                                           &error))
        message (D_ERROR, _ ("Plugin action"), "%s", error != NULL ? error : _ ("Action failed"));
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
