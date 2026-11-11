/** \file panel_plugin_cache.h
 *  \brief Header: what is known about panel plugin modules without opening them
 */

#ifndef MC__PANEL_PLUGIN_CACHE_H
#define MC__PANEL_PLUGIN_CACHE_H

#include <sys/stat.h>

#include "lib/panel-plugin.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* The callbacks the core may want before a panel of the plugin is open. */
typedef enum
{
    MC_PP_OFFERS_OPEN_INPUT_STREAM = 1 << 0,
    MC_PP_OFFERS_OPEN_FILE_LIST = 1 << 1,
    MC_PP_OFFERS_CONFIGURE = 1 << 2
} mc_pp_offers_t;

typedef enum
{
    MC_PP_OP_OFFERS_MAY_OPEN_NAME = 1 << 0,
    MC_PP_OP_OFFERS_OPEN_INPUT_STREAM = 1 << 1,
    MC_PP_OP_OFFERS_VIEW_INPUT_STREAM = 1 << 2,
    MC_PP_OP_OFFERS_SHOW = 1 << 3
} mc_pp_op_offers_t;

typedef struct mc_pp_cache_t mc_pp_cache_t;

/*** declarations of public functions ************************************************************/

/* What tells one module file from another; the caller frees it. */
char *mc_pp_cache_file_id (const struct stat *st);

int mc_pp_plugin_offers (const mc_panel_plugin_t *plugin);
int mc_pp_operation_offers (const mc_pp_file_operation_t *operation);

/* NULL only when @path is NULL; a missing or damaged file is an empty cache. */
mc_pp_cache_t *mc_pp_cache_open (const char *path);

/* A plugin with the descriptive fields of the module at @so_path and no callbacks, NULL when the
   cache has no usable entry for this very file.  @offers gets mc_pp_offers_t bits; @op_offers
   gets mc_pp_op_offers_t bits per operation; @description, if not NULL, gets the text
   mc_pp_cache_describe() gives for the module.  The caller frees both. */
mc_panel_plugin_t *mc_pp_cache_make_stub (mc_pp_cache_t *cache, const char *so_path,
                                          const struct stat *st, int *offers, int **op_offers,
                                          char **description);
void mc_pp_cache_free_stub (mc_panel_plugin_t *stub);

/* All that the cache keeps about @plugin, as text to compare. */
char *mc_pp_cache_describe (const mc_panel_plugin_t *plugin);

void mc_pp_cache_store (mc_pp_cache_t *cache, const char *so_path, const struct stat *st,
                        const mc_panel_plugin_t *plugin);

/* Writes the file if it changed; entries neither made into stubs nor stored are dropped. */
void mc_pp_cache_close (mc_pp_cache_t *cache);

/*** inline functions ****************************************************************************/

#endif
