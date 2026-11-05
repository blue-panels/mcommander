/** \file mcmagic.h
 *  \brief Plugin file-operation associations from magic.ini
 */

#ifndef MC__MCMAGIC_H
#define MC__MCMAGIC_H

#include "lib/panel-plugin.h"

typedef mc_pp_result_t (*mc_magic_get_local_copy_t) (void *data, char **local_path);

typedef enum
{
    MC_MAGIC_ACTION_NONE,
    MC_MAGIC_ACTION_FOUND,
    MC_MAGIC_ACTION_ERROR,
    MC_MAGIC_ACTION_SKIPPED /* the user stopped the fetch of the file: nothing more to do */
} mc_magic_action_state_t;

typedef struct
{
    const char *display_name;
    const char *local_path;
    mc_magic_get_local_copy_t get_local_copy;
    void *data;
} mc_magic_source_t;

typedef struct
{
    char *plugin_id;
    char *submodule_id;
    char *operation_id;
    char *magic_group;
    char *mime_type;
} mc_magic_action_t;

mc_magic_action_state_t mc_magic_find_action (const mc_magic_source_t *source, const char *action,
                                              char **local_copy, mc_magic_action_t *result);
void mc_magic_action_clear (mc_magic_action_t *action);
void mc_magic_flush (void);

#endif /* MC__MCMAGIC_H */
