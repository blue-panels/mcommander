/*
   Panel plugin acl for the M-Commander
   the access control list dialogs

   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file acl-dialog.h
 *  \brief Header: the access control list dialogs
 */

#ifndef MC__PANEL_PLUGIN_ACL_DIALOG_H
#define MC__PANEL_PLUGIN_ACL_DIALOG_H

#include <sys/types.h>

#include "acl-model.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

typedef enum
{
    ACL_DLG_CANCEL = 0,
    ACL_DLG_SET,
    ACL_DLG_SET_ALL,
    ACL_DLG_CHANGES_TO_ALL
} acl_dlg_result_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    const char *shown_path; /* the path, with "-> target" for a symbolic link */
    int index;              /* 1-based number of this file among the marked ones */
    int count;              /* how many files the dialog goes through */
    uid_t owner;
    gid_t group;
    mode_t mode;
    gboolean is_dir;
    gboolean read_only;

    aclm_list_t *lists[2]; /* ACLM_ACCESS, ACLM_DEFAULT (directories only) */
    gboolean recalc_mask;
} acl_dlg_state_t;

/*** declarations of public functions ************************************************************/

acl_dlg_result_t acl_dialog_run (acl_dlg_state_t *st);

const char *acl_help_file (void);

char *acl_choose (gboolean group, const char *current);
gchar **acl_split_names (const char *text);

/*** inline functions ****************************************************************************/

#endif
