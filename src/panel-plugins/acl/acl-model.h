/*
   Panel plugin acl for the M-Commander
   POSIX ACL entries, read and written through libacl

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

/** \file acl-model.h
 *  \brief Header: POSIX ACL entries, read and written through libacl
 */

#ifndef MC__PANEL_PLUGIN_ACL_MODEL_H
#define MC__PANEL_PLUGIN_ACL_MODEL_H

#include <sys/types.h>

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define ACL_PERM_READ    4u
#define ACL_PERM_WRITE   2u
#define ACL_PERM_EXECUTE 1u
#define ACL_PERM_ALL     (ACL_PERM_READ | ACL_PERM_WRITE | ACL_PERM_EXECUTE)

/*** enums ***************************************************************************************/

/* Canonical POSIX ACL order; the dialog places the base entries first. */
typedef enum
{
    ACLM_OWNER = 0, /* ACL_USER_OBJ */
    ACLM_USER,      /* ACL_USER, named */
    ACLM_GROUP_OBJ, /* ACL_GROUP_OBJ, the group of the file */
    ACLM_GROUP,     /* ACL_GROUP, named */
    ACLM_MASK,
    ACLM_OTHER
} aclm_kind_t;

typedef enum
{
    ACLM_ACCESS = 0,
    ACLM_DEFAULT
} aclm_type_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    aclm_kind_t kind;
    unsigned int id; /* uid or gid of a named entry, 0 otherwise */
    unsigned int perm;
} aclm_entry_t;

/* One list: the access ACL or the default ACL of a directory. */
typedef struct
{
    GArray *entries; /* aclm_entry_t, kept in list order */
} aclm_list_t;

/* A change made in the dialog, replayed on other files by "Changes to all". */
typedef struct
{
    gboolean remove;
    aclm_type_t type;
    aclm_entry_t entry;
} aclm_change_t;

/*** declarations of public functions ************************************************************/

aclm_list_t *aclm_list_new (void);
aclm_list_t *aclm_list_copy (const aclm_list_t *list);
void aclm_list_free (aclm_list_t *list);

gboolean aclm_list_is_empty (const aclm_list_t *list);
gboolean aclm_is_named (aclm_kind_t kind);
gboolean aclm_has_named (const aclm_list_t *list);

aclm_entry_t *aclm_list_find (const aclm_list_t *list, aclm_kind_t kind, unsigned int id);
void aclm_list_set (aclm_list_t *list, aclm_kind_t kind, unsigned int id, unsigned int perm);
gboolean aclm_list_remove (aclm_list_t *list, aclm_kind_t kind, unsigned int id);
void aclm_list_sort (aclm_list_t *list);

void aclm_calc_mask (aclm_list_t *list);
void aclm_drop_unused_mask (aclm_list_t *list);
unsigned int aclm_effective (const aclm_list_t *list, const aclm_entry_t *entry);

void aclm_list_base_from (aclm_list_t *list, const aclm_list_t *access);

GArray *aclm_diff (const aclm_list_t *before, const aclm_list_t *after, aclm_type_t type,
                   GArray *changes);
void aclm_apply (aclm_list_t *list, const GArray *changes, aclm_type_t type, gboolean recalc_mask);

void aclm_perm_to_str (unsigned int perm, char out[4]);
char *acl_name_of (aclm_kind_t kind, unsigned int id);
gboolean acl_id_of (gboolean group, const char *name, unsigned int *id);

aclm_list_t *aclm_read (const char *path, aclm_type_t type, int *err);
gboolean aclm_write (const char *path, aclm_type_t type, const aclm_list_t *list, int *err);

/*** inline functions ****************************************************************************/

#endif
