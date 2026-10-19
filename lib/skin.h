#ifndef MC_SKIN_H
#define MC_SKIN_H

#include "lib/global.h"

#include "lib/mcconfig.h"

#include "lib/tty/color.h"
#include "lib/tty/tty.h"  // mc_tty_char_t

/*** typedefs(not structures) and defined constants **********************************************/

/* Beware! When using Slang with color, not all the indexes are free.
   See color-slang.h (A_*) */

/* cache often used colors */
#define CORE_DEFAULT_COLOR         mc_skin_color__cache[0]
#define CORE_NORMAL_COLOR          mc_skin_color__cache[1]
#define CORE_MARKED_COLOR          mc_skin_color__cache[2]
#define CORE_SELECTED_COLOR        mc_skin_color__cache[3]
#define CORE_MARKED_SELECTED_COLOR mc_skin_color__cache[4]
#define CORE_DISABLED_COLOR        mc_skin_color__cache[5]
#define CORE_REVERSE_COLOR         mc_skin_color__cache[6]
#define CORE_COMMAND_MARK_COLOR    mc_skin_color__cache[7]
#define CORE_HEADER_COLOR          mc_skin_color__cache[8]
#define CORE_SHADOW_COLOR          mc_skin_color__cache[9]
#define CORE_FRAME_COLOR           mc_skin_color__cache[10]

/* Dialog colors */
#define DIALOG_NORMAL_COLOR          mc_skin_color__cache[11]
#define DIALOG_FOCUS_COLOR           mc_skin_color__cache[12]
#define DIALOG_HOT_NORMAL_COLOR      mc_skin_color__cache[13]
#define DIALOG_HOT_FOCUS_COLOR       mc_skin_color__cache[14]
#define DIALOG_SELECTED_NORMAL_COLOR mc_skin_color__cache[15]
#define DIALOG_SELECTED_FOCUS_COLOR  mc_skin_color__cache[16]
#define DIALOG_TITLE_COLOR           mc_skin_color__cache[17]
#define DIALOG_FRAME_COLOR           mc_skin_color__cache[18]
#define DIALOG_CELL_FOCUS_COLOR      mc_skin_color__cache[91]

/* Error dialog colors */
#define ERROR_NORMAL_COLOR     mc_skin_color__cache[19]
#define ERROR_FOCUS_COLOR      mc_skin_color__cache[20]
#define ERROR_HOT_NORMAL_COLOR mc_skin_color__cache[21]
#define ERROR_HOT_FOCUS_COLOR  mc_skin_color__cache[22]
#define ERROR_TITLE_COLOR      mc_skin_color__cache[23]
#define ERROR_FRAME_COLOR      mc_skin_color__cache[24]

/* Menu colors */
#define MENU_ENTRY_COLOR    mc_skin_color__cache[25]
#define MENU_SELECTED_COLOR mc_skin_color__cache[26]
#define MENU_HOT_COLOR      mc_skin_color__cache[27]
#define MENU_HOTSEL_COLOR   mc_skin_color__cache[28]
#define MENU_INACTIVE_COLOR mc_skin_color__cache[29]
#define MENU_FRAME_COLOR    mc_skin_color__cache[30]

/* Popup menu colors */
#define PMENU_ENTRY_COLOR      mc_skin_color__cache[31]
#define PMENU_SELECTED_COLOR   mc_skin_color__cache[32]
#define PMENU_HOT_COLOR        mc_skin_color__cache[33]  // unused: not implemented yet
#define PMENU_HOTSEL_COLOR     mc_skin_color__cache[34]  // unused: not implemented yet
#define PMENU_TITLE_COLOR      mc_skin_color__cache[35]
#define PMENU_FRAME_COLOR      mc_skin_color__cache[36]

#define BUTTONBAR_HOTKEY_COLOR mc_skin_color__cache[37]
#define BUTTONBAR_BUTTON_COLOR mc_skin_color__cache[38]

#define STATUSBAR_COLOR        mc_skin_color__cache[39]

/*
 * This should be selectable independently. Default has to be black background
 * foreground does not matter at all.
 */
#define CORE_GAUGE_COLOR             mc_skin_color__cache[40]
#define CORE_INPUT_COLOR             mc_skin_color__cache[41]
#define CORE_INPUT_UNCHANGED_COLOR   mc_skin_color__cache[42]
#define CORE_INPUT_MARK_COLOR        mc_skin_color__cache[43]
#define CORE_INPUT_HISTORY_COLOR     mc_skin_color__cache[44]
#define CORE_COMMAND_HISTORY_COLOR   mc_skin_color__cache[45]

#define HELP_NORMAL_COLOR            mc_skin_color__cache[46]
#define HELP_ITALIC_COLOR            mc_skin_color__cache[47]
#define HELP_BOLD_COLOR              mc_skin_color__cache[48]
#define HELP_LINK_COLOR              mc_skin_color__cache[49]
#define HELP_SLINK_COLOR             mc_skin_color__cache[50]
#define HELP_TITLE_COLOR             mc_skin_color__cache[51]
#define HELP_FRAME_COLOR             mc_skin_color__cache[52]

#define VIEWER_NORMAL_COLOR          mc_skin_color__cache[53]
#define VIEWER_BOLD_COLOR            mc_skin_color__cache[54]
#define VIEWER_UNDERLINED_COLOR      mc_skin_color__cache[55]
#define VIEWER_BOLD_UNDERLINED_COLOR mc_skin_color__cache[56]
#define VIEWER_SELECTED_COLOR        mc_skin_color__cache[57]
#define VIEWER_FRAME_COLOR           mc_skin_color__cache[58]
#define VIEWER_HEADING_COLOR         mc_skin_color__cache[79]
#define MCTREE_KEY_COLOR             mc_skin_color__cache[80]
#define MCTREE_VALUE_COLOR           mc_skin_color__cache[81]
#define MCTREE_MARKER_COLOR          mc_skin_color__cache[89]
#define MCTREE_SELECTED_COLOR        mc_skin_color__cache[90]

// embedded terminal
#define MCTERM_NORMAL_COLOR   mc_skin_color__cache[82]
#define MCTERM_SELECTED_COLOR mc_skin_color__cache[83]

/*
 * editor colors - only 4 for normal, search->found, select, and whitespace
 * respectively
 * Last is defined to view color.
 */
#define EDITOR_NORMAL_COLOR       mc_skin_color__cache[59]
#define EDITOR_NONPRINTABLE_COLOR mc_skin_color__cache[60]
#define EDITOR_BOLD_COLOR         mc_skin_color__cache[61]
#define EDITOR_MARKED_COLOR       mc_skin_color__cache[62]
#define EDITOR_WHITESPACE_COLOR   mc_skin_color__cache[63]
#define EDITOR_RIGHT_MARGIN_COLOR mc_skin_color__cache[64]
#define EDITOR_BACKGROUND_COLOR   mc_skin_color__cache[65]
#define EDITOR_FRAME_COLOR        mc_skin_color__cache[66]
#define EDITOR_FRAME_ACTIVE_COLOR mc_skin_color__cache[67]
#define EDITOR_FRAME_DRAG_COLOR   mc_skin_color__cache[68]
/* color of left 8 char status per line */
#define EDITOR_LINE_STATE_COLOR     mc_skin_color__cache[69]
#define EDITOR_BOOKMARK_COLOR       mc_skin_color__cache[70]
#define EDITOR_BOOKMARK_FOUND_COLOR mc_skin_color__cache[71]

/* Diff colors */
#define DIFFVIEWER_ADDED_COLOR       mc_skin_color__cache[72]
#define DIFFVIEWER_CHANGEDLINE_COLOR mc_skin_color__cache[73]
#define DIFFVIEWER_CHANGEDNEW_COLOR  mc_skin_color__cache[74]
#define DIFFVIEWER_CHANGED_COLOR     mc_skin_color__cache[75]
#define DIFFVIEWER_REMOVED_COLOR     mc_skin_color__cache[76]
#define DIFFVIEWER_ERROR_COLOR       mc_skin_color__cache[77]
#define DIFFVIEWER_CONFLICT_COLOR    mc_skin_color__cache[92]
#define DIFFVIEWER_LINEMARK_COLOR    mc_skin_color__cache[93]
#define DIFFVIEWER_DIMFRAME_COLOR    mc_skin_color__cache[94]

#define FILEHIGHLIGHT_DEFAULT_COLOR  mc_skin_color__cache[78]

/* permission bits, one color per kind of bit */
#define CORE_PERM_READ_COLOR      mc_skin_color__cache[84]
#define CORE_PERM_WRITE_COLOR     mc_skin_color__cache[85]
#define CORE_PERM_EXEC_COLOR      mc_skin_color__cache[86]
#define CORE_PERM_SPECIAL_COLOR   mc_skin_color__cache[87]
#define CORE_PERM_NONE_COLOR      mc_skin_color__cache[88]

#define MC_SKIN_COLOR_CACHE_COUNT 95

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* How a scrollbar is drawn: the arrows at its start and its end, the fill of its track, its
   thumb, and the cells of the thumb when it fits; [scrollbar] of the skin */
typedef struct
{
    mc_tty_char_t start;
    mc_tty_char_t end;
    mc_tty_char_t track;
    mc_tty_char_t thumb;
    int thumb_len;
} mc_skin_scrollbar_t;

typedef struct mc_skin_struct
{
    gchar *name;
    gchar *description;
    mc_config_t *config;
    GHashTable *colors;
    gboolean have_256_colors;
    gboolean have_true_colors;
} mc_skin_t;

/*** global variables defined in .c file *********************************************************/

extern int mc_skin_color__cache[];
extern mc_skin_t mc_skin__default;
/* the scrollbars of the skin, the vertical one and the horizontal one */
extern mc_skin_scrollbar_t mc_skin_scrollbar_vert;
extern mc_skin_scrollbar_t mc_skin_scrollbar_horiz;

/*** declarations of public functions ************************************************************/

gboolean mc_skin_init (const gchar *skin_override, GError **error);
/* replace the loaded skin by @config, the engine's from here on; @name NULL keeps the current */
gboolean mc_skin_load_from_config (mc_config_t *config, const char *name, GError **error);

/* keys filled in when a skin does not name them: @fg on the group's bg, or @src_group/@src_key
   with @attrs, when given, in place of its attributes */
typedef struct
{
    const char *group;
    const char *key;
    const char *fg;
    const char *src_group;
    const char *src_key;
    const char *attrs;
} mc_skin_fallback_t;

const mc_skin_fallback_t *mc_skin_fallbacks (size_t *count);
void mc_skin_deinit (void);

int mc_skin_color_get (const gchar *group, const gchar *name);

void mc_skin_lines_parse_ini_file (mc_skin_t *mc_skin);

gchar *mc_skin_get (const gchar *group, const gchar *key, const gchar *default_value);

const char *mc_skin_spinner_frame (unsigned int index);

GPtrArray *mc_skin_list (void);

#endif
