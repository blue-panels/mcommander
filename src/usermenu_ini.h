/** \file usermenu_ini.h
 *  \brief Header: the user menu that edits itself
 */

#ifndef MC__USERMENU_INI_H
#define MC__USERMENU_INI_H

#include <sys/types.h>  // mode_t

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

// from a key of the current panel to the same key of the other one
#define UM_COND_OTHER (UM_COND_OTHER_PATH - UM_COND_PATH)

/*** enums ***************************************************************************************/

// the files of the menu: .mc6menu of the directory, menu.ini of the user
typedef enum
{
    MENU_LEVEL_LOCAL = 0,
    MENU_LEVEL_USER,
    MENU_LEVEL_COUNT
} menu_level_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/**
 * One entry of the menu.  The label is the name of its group in the file, and
 * the level says which of the two files it came from.
 */
/* The keys that decide where an entry is shown; other.* look at the other panel. */
typedef enum
{
    UM_COND_PATH = 0,  // masks of the path under the cursor, the rules of .gitignore
    UM_COND_ON,
    UM_COND_EXEC,
    UM_COND_MARKED,
    UM_COND_PATH_RE,  // path~=: a regular expression instead of masks
    UM_COND_PANEL_LAST = UM_COND_PATH_RE,
    UM_COND_NEEDS,
    UM_COND_OTHER_PATH,
    UM_COND_OTHER_ON,
    UM_COND_OTHER_EXEC,
    UM_COND_OTHER_MARKED,
    UM_COND_OTHER_PATH_RE,
    UM_COND_COUNT
} um_cond_t;

typedef struct
{
    char *label;
    char hotkey;
    char *command;
    gboolean view;
    gboolean silent;
    gboolean is_submenu;  // a container of other entries, not a command
    char *parent;         // the label of the submenu it belongs to; NULL at the top
    int level;
    char *group;                 // the name of its group in the file as read; NULL if new
    char *comment;               // lines written above a new entry, '#' included
    char *cond[UM_COND_COUNT];   // the values as written; NULL where the key is absent
    char *dcond[UM_COND_COUNT];  // default.*: where the menu opens on the entry
    gboolean is_default;         // default=true: the menu opens on it wherever it is shown
} user_menu_entry_t;

/*** global variables defined in .c file *********************************************************/

// the names of the keys of the conditions, in the order of um_cond_t
extern const char *const user_menu_ini_cond_keys[UM_COND_COUNT];
// the same under "default.", for the conditions of the entry the menu opens on
extern const char *const user_menu_ini_default_keys[UM_COND_COUNT];

/*** declarations of public functions ************************************************************/

gboolean user_menu_ini_preferred (void);
gboolean user_menu_ini_cmd (void);

/* A menu of the new kind the user keeps: whether he has one, and where it is.
   local = TRUE is the file of the current directory. */
gboolean user_menu_ini_own_exists (void);
char *user_menu_ini_path (gboolean local);

void user_menu_entry_free (user_menu_entry_t *entry);

/* Entries of a menu file written by hand; conditions the keys cannot say become comments. */
guint user_menu_ini_import_file (GPtrArray *entries, const char *file, int level,
                                 guint *not_converted);
char *user_menu_ini_regex_to_glob (const char *regex);

/* Conditions: masks and types separated by ';', any of them matches, '!' turns one over. */
gboolean user_menu_ini_path_match (const char *masks, const char *path, gboolean is_dir,
                                   const char *base);
gboolean user_menu_ini_on_match (const char *on, mode_t mode, gboolean link_to_dir,
                                 gboolean stale_link);
gboolean user_menu_ini_on_check (const char *on, char **bad);
gboolean user_menu_ini_regex_match (const char *regex, const char *str);
gboolean user_menu_ini_entry_visible (const user_menu_entry_t *entry);
gboolean user_menu_ini_entry_default (const user_menu_entry_t *entry);

/* A label that can stand as the name of a group, and a number where it repeats. */
void user_menu_ini_label_fix (GPtrArray *entries, user_menu_entry_t *entry);

/* One file: the paths of the three levels are decided elsewhere. */
gboolean user_menu_ini_load_file (GPtrArray *entries, const char *file, int level, GError **error);
/* A menu file without the format line, and its conversion to the format. */
gboolean user_menu_ini_needs_conversion (const char *file);
gboolean user_menu_ini_convert_file (const char *file, GError **error);
gboolean user_menu_ini_save_file (const char *file, GPtrArray *entries, int level, GError **error);

#endif
