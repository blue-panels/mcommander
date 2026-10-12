/*
   Panel plugin acl for the M-Commander
   Windows security descriptors on cifs and NTFS mounts

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

/** \file acl-nt.h
 *  \brief Header: Windows security descriptors on cifs and NTFS mounts
 */

#ifndef MC__PANEL_PLUGIN_ACL_NT_H
#define MC__PANEL_PLUGIN_ACL_NT_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* ACE types (MS-DTYP 2.4.4.1) */
#define ACL_NT_ACE_ALLOWED 0
#define ACL_NT_ACE_DENIED  1

/* ACE flags (MS-DTYP 2.4.4.1) */
#define ACL_NT_OBJECT_INHERIT       0x01
#define ACL_NT_CONTAINER_INHERIT    0x02
#define ACL_NT_NO_PROPAGATE_INHERIT 0x04
#define ACL_NT_INHERIT_ONLY         0x08
#define ACL_NT_INHERITED            0x10

#define ACL_NT_PRESET_COUNT         5
#define ACL_NT_RIGHT_COUNT          13
#define ACL_NT_SYNCHRONIZE          0x00100000

/*** enums ***************************************************************************************/

typedef enum
{
    ACL_NT_WRITE_FAILED = 0,
    ACL_NT_WRITE_OK,
    ACL_NT_WRITE_CHANGED /* the server stored the list in a form of its own */
} acl_nt_write_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    guint32 mask;
    const char *name;
    const char *label;
} acl_nt_preset_t;

typedef struct
{
    guint32 bit;
    const char *file;
    const char *dir;
} acl_nt_right_t;

typedef struct
{
    guint8 type;  /* ACL_NT_ACE_ALLOWED, ACL_NT_ACE_DENIED or another */
    guint8 flags; /* ACL_NT_* inheritance flags */
    guint32 mask; /* access mask */
    char *sid;    /* "S-1-5-32-544", or NULL for an ACE that carries none */
    GBytes *raw;  /* original bytes of an ACE whose format we do not edit */
} acl_nt_ace_t;

typedef struct
{
    guint16 control;
    char *owner; /* SID, or NULL */
    char *group; /* SID, or NULL */
    gboolean has_dacl;
    guint8 dacl_revision; /* 2 for ordinary ACEs, 4 for object ACEs */
    GPtrArray *aces; /* acl_nt_ace_t of the DACL */
} acl_nt_sd_t;

/*** declarations of public functions ************************************************************/

extern const acl_nt_preset_t acl_nt_presets[ACL_NT_PRESET_COUNT];
extern const acl_nt_right_t acl_nt_rights[ACL_NT_RIGHT_COUNT];

acl_nt_sd_t *acl_nt_parse (const guint8 *data, gsize size);
void acl_nt_free (acl_nt_sd_t *sd);

acl_nt_sd_t *acl_nt_read (const char *path, const char **attr, int *err);

char *acl_nt_sid_name (const char *sid);
const char *acl_nt_rights_preset (guint32 mask);
int acl_nt_preset_index (guint32 mask);
guint32 acl_nt_map_generic (guint32 mask);
GPtrArray *acl_nt_rights_list (guint32 mask, gboolean directory);
const char *acl_nt_applies_to (guint8 flags);

guint8 *acl_nt_build (const acl_nt_sd_t *sd, gsize *size);
acl_nt_write_t acl_nt_write (const char *path, const char *attr, const acl_nt_sd_t *before,
                             const acl_nt_sd_t *sd, int *err);
gboolean acl_nt_dacl_equal (const acl_nt_sd_t *a, const acl_nt_sd_t *b);
gboolean acl_nt_dacl_same_entries (const acl_nt_sd_t *a, const acl_nt_sd_t *b);

char *acl_nt_sid_for (const char *name, gboolean group, const char *attr);
gchar **acl_nt_split_names (const char *text);
void acl_nt_insert_ace (acl_nt_sd_t *sd, acl_nt_ace_t *ace);
void acl_nt_replace_ace (acl_nt_sd_t *sd, guint index, acl_nt_ace_t *ace);
acl_nt_ace_t *acl_nt_ace_new (guint8 type, guint8 flags, guint32 mask, const char *sid);

/*** inline functions ****************************************************************************/

#endif
