/** \file usermenu_ini.h
 *  \brief Header: the user menu that edits itself
 */

#ifndef MC__USERMENU_INI_H
#define MC__USERMENU_INI_H

#include "lib/global.h"

/*** structures declarations (and typedefs of structures)*****************************************/

/**
 * One entry of the menu.  The label is the name of its group in the file, and
 * the level says which of the two files it came from.
 */
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
} user_menu_entry_t;

/*** declarations of public functions ************************************************************/

gboolean user_menu_ini_preferred (void);
gboolean user_menu_ini_cmd (void);

/* A menu of the new kind the user keeps: whether he has one, and where it is.
   local = TRUE is the file of the current directory. */
gboolean user_menu_ini_own_exists (void);
char *user_menu_ini_path (gboolean local);

void user_menu_entry_free (user_menu_entry_t *entry);

/* The commands of an entry in one line, and back. */
char *user_menu_ini_escape (const char *command);
char *user_menu_ini_unescape (const char *text);

/* Entries of a menu file written by hand, conditions and masks dropped. */
guint user_menu_ini_import_file (GPtrArray *entries, const char *file, int level);

/* One file: the paths of the three levels are decided elsewhere. */
gboolean user_menu_ini_load_file (GPtrArray *entries, const char *file, int level, GError **error);
gboolean user_menu_ini_save_file (const char *file, GPtrArray *entries, int level, GError **error);

#endif
