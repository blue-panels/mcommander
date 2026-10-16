/** \file filechooser.h
 *  \brief Header: WFileChooser, the dialog that picks a file or a directory
 */

#ifndef MC__WIDGET_FILECHOOSER_H
#define MC__WIDGET_FILECHOOSER_H

#include "lib/global.h"
#include "lib/vfs/vfs.h" /* vfs_path_t */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

typedef enum
{
    FILE_CHOOSER_OPEN = 0, /* an existing file */
    FILE_CHOOSER_SAVE,     /* a file that may not exist yet; an existing one asks first */
    FILE_CHOOSER_DIR       /* a directory */
} file_chooser_mode_t;

typedef enum
{
    FILE_CHOOSER_HIDDEN = 1 << 0,      /* list the names that start with a dot */
    FILE_CHOOSER_SEARCH_CASE = 1 << 1, /* the quick search tells upper from lower case */
    FILE_CHOOSER_NEW_OK = 1 << 2       /* FILE_CHOOSER_OPEN takes a name that does not exist */
} file_chooser_flags_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/* One line of the list of a directory */
typedef struct
{
    char *name;
    gboolean is_dir; /* a directory, or a link to one */
    uintmax_t size;
    time_t mtime;
    char *sort_key; /* the key of the name while the list is sorted, else NULL */
} file_chooser_entry_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* The entries of @dir, directories first, then by name; ".." is not among them.  @keep is listed
   even when it is hidden.  NULL when the directory cannot be read. */
GPtrArray *file_chooser_read_dir (const vfs_path_t *dir, gboolean show_hidden, gboolean dirs_only,
                                  const char *keep);

/* A dialog to pick a file or a directory, starting at @start (a directory, or a file to start
   on); @history_name keeps the paths typed.  The chosen path, to free, or NULL on cancel. */
char *file_chooser_dialog (const char *title, const char *start, const char *history_name,
                           file_chooser_mode_t mode, file_chooser_flags_t flags);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_FILECHOOSER_H */
