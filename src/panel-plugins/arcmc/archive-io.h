/** \file archive-io.h
 *  \brief Header: archive read/write/extract and extfs helpers
 */

#ifndef ARCMC_ARCHIVE_IO_H
#define ARCMC_ARCHIVE_IO_H

#include "arcmc-types.h"
#include "arcmc-ext.h"

/*** declarations (functions)
 * **********************************************************************/

/* Builtin format table */
arcmc_builtin_format_t *arcmc_find_builtin_format (const char *path);
gboolean arcmc_builtin_can_pack (int fmt);
const char *arcmc_backend_name (arcmc_backend_t b);
arcmc_backend_t arcmc_backend_from_name (const char *s, arcmc_backend_t def);
gboolean arcmc_backend_possible (arcmc_backend_t b, gboolean lib, const char *bin);
const char *arcmc_builtin_tool (const arcmc_builtin_format_t *f);
const char *arcmc_resolve_tool (const char *bin);

/* Path utilities */
char *get_parent_dir (const char *current_dir);
char *build_child_path (const char *current_dir, const char *name);
const char *is_direct_child (const char *entry_path, const char *dir);
gboolean is_under_dir (const char *entry_path, const char *dir);
const arcmc_entry_t *arcmc_find_entry (GPtrArray *entries, const char *full_path);
const arcmc_entry_t *arcmc_data_find_entry (arcmc_data_t *data, const char *full_path);
void arcmc_data_entries_changed (arcmc_data_t *data);
void arcmc_data_free_entries (arcmc_data_t *data);
gboolean arcmc_compute_dir_size (GPtrArray *entries, const char *full_path);
void arcmc_compute_level_dir_sizes (GPtrArray *entries, const char *dir);
guint arcmc_compute_named_dir_sizes (GPtrArray *entries, const char *dir, GHashTable *names);
void arcmc_resolve_links (GPtrArray *entries);
char *arcmc_target_path (arcmc_data_t *data, const char *name);

/* Reading */
char *arcmc_find_extfs_helper (const char *archive_path);
gboolean arcmc_read_archive (arcmc_data_t *data);
gboolean arcmc_read_archive_extfs (arcmc_data_t *data);
gboolean arcmc_try_open (arcmc_data_t *data);

#ifdef HAVE_LIBMAGIC
gboolean arcmc_is_archive_by_content (const char *path);
#endif

/* Writing */
off_t arcmc_entries_total_size (GPtrArray *all_entries);
gboolean arcmc_archive_add_file (const char *archive_path, const char *local_path,
                                 const char *archive_name, const char *password,
                                 arcmc_progress_t *p);
gboolean arcmc_archive_delete (const char *archive_path, const char **del_paths, int del_count,
                               const char *password, arcmc_progress_t *p);
gboolean arcmc_do_pack (const arcmc_pack_opts_t *opts, const char *cwd, GPtrArray *files,
                        char **error_msg);

/* Extraction */
mc_pp_result_t arcmc_extract_entry (arcmc_data_t *data, const char *target_path, char **local_path,
                                    arcmc_progress_t *p);
mc_pp_result_t arcmc_extract_subtree (arcmc_data_t *data, const char *src_dir,
                                      const char *dest_path, arcmc_progress_t *p);
mc_pp_result_t arcmc_extract_entry_extfs (arcmc_data_t *data, const char *target_path,
                                          char **local_path);
gboolean arcmc_extfs_run_cmd (const char *helper, const char *cmd_name, const char *archive_path,
                              const char *stored_name, const char *local_name,
                              const char *password);
mc_pp_result_t arcmc_extract_to_temp (arcmc_data_t *data, const char *name, char **local_path);
mc_pp_result_t arcmc_push_nested (arcmc_data_t *data, char *local_path, const char *display_name);

/* External archiver operations */
gboolean arcmc_check_bin_available (const char *bin_name);
gboolean arcmc_ext_pack (const arcmc_ext_archiver_t *archiver, const char *archive_path,
                         const char *cwd, GPtrArray *files, const char *password, char **error_msg);
gboolean arcmc_ext_unpack (const arcmc_ext_archiver_t *archiver, const char *archive_path,
                           const char *dest_dir, const char *password);
gboolean arcmc_ext_unpack_files (const arcmc_ext_archiver_t *archiver, const char *archive_path,
                                 const char *dest_dir, GPtrArray *files, const char *password);
gboolean arcmc_ext_test (const arcmc_ext_archiver_t *archiver, const char *archive_path,
                         const char *password);

#endif /* ARCMC_ARCHIVE_IO_H */
