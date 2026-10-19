
/** \file scrollbar.h
 *  \brief Header: WScrollBar widget
 */

#ifndef MC__WIDGET_SCROLLBAR_H
#define MC__WIDGET_SCROLLBAR_H

#include "lib/global.h"  // GLib types

// forward declarations needed by widget-common.h
struct Widget;
typedef struct Widget Widget;
struct WGroup;
typedef struct WGroup WGroup;

#include "lib/skin.h"                  // mc_skin_scrollbar_t
#include "lib/widget/rect.h"           // WRect
#include "lib/widget/widget-common.h"  // Widget, mouse_msg_t

/*** typedefs(not structures) and defined constants **********************************************/

#define SCROLLBAR(x)       ((WScrollBar *) (x))
#define CONST_SCROLLBAR(x) ((const WScrollBar *) (x))

/* How a scrollbar is drawn: the arrows, the fill of the track, the thumb and its length */
typedef mc_skin_scrollbar_t scrollbar_look_t;

/*** enums ***************************************************************************************/

typedef enum
{
    SCROLLBAR_VERTICAL = 0,
    SCROLLBAR_HORIZONTAL
} scrollbar_orientation_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/*
 * A scrollbar: an arrow at each end, a track between them and a thumb in the track.  It shows a
 * view of visible of total at pos, and moves it with the mouse: an arrow steps, the track beside
 * the thumb pages, the thumb is dragged.  Each move is told to the client of the bar, the owner
 * when it has none, by MSG_NOTIFY with the bar as the sender; scrollbar_get_pos() is where the
 * view is asked to go.  The client moves the view there, or as near as it can, and gives the bar
 * the range again.  A bar takes no focus and no keys; it draws nothing while the view holds all
 * of the text.
 */
typedef struct
{
    Widget widget;
    scrollbar_orientation_t orientation;
    scrollbar_look_t look;  // with own_look; else the one of the skin, read when it is drawn
    gboolean own_look;
    int color;       // the color it is drawn in
    long total;      // the length of the text
    long visible;    // how much of it the view holds
    long pos;        // the first of it in the view
    Widget *client;  // told of the moves; NULL for the owner
    int drag;        // where in the thumb the mouse holds it, -1 when it does not
} WScrollBar;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* A bar len cells long at y, x, drawn as the skin has it for its orientation */
WScrollBar *scrollbar_new (int y, int x, int len, scrollbar_orientation_t orientation);
void scrollbar_set_look (WScrollBar *b, const scrollbar_look_t *look);
void scrollbar_show (WScrollBar *b, const WRect *r, long total, long visible, long pos, int color);
void scrollbar_set_color (WScrollBar *b, int color);
void scrollbar_set_client (WScrollBar *b, Widget *client);
/* The view of visible of total at pos; the bar is drawn again when it changes */
void scrollbar_set_range (WScrollBar *b, long total, long visible, long pos);
long scrollbar_get_pos (const WScrollBar *b);
/* Whether the view does not hold all of the text, and the bar has something to show */
gboolean scrollbar_needed (const WScrollBar *b);

/* For a client that is no group and keeps its bar to itself: the bar taking a mouse event at
   the cell at of its length, from 0 */
void scrollbar_mouse (WScrollBar *b, mouse_msg_t msg, int at);

/*** inline functions ****************************************************************************/

#endif
