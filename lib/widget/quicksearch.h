/** \file quicksearch.h
 *  \brief Header: the text of a quick search, as the panels and the tables type it
 */

#ifndef MC__WIDGET_QUICKSEARCH_H
#define MC__WIDGET_QUICKSEARCH_H

#include "lib/global.h"
#include "lib/search.h" /* mc_search_t */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* Take one byte of a key typed into the quick search @text: the bytes of a multibyte
   character wait in @ch (@ch_size bytes, @chpoint of them used) until it is whole;
   KEY_BACKSPACE takes the last character away.  TRUE when @text has changed. */
gboolean quick_search_take_key (GString *text, char *ch, size_t ch_size, int *chpoint, int key);
/* Take the last character, a whole one, out of @text */
void quick_search_drop_char (GString *text);
/* A search for names that begin with @text, or have it anywhere with @contains */
mc_search_t *quick_search_new (const char *text, gboolean contains, gboolean case_sensitive);
/* A fuzzy match: the characters of @query are in @name in their order, not side by side;
   0 when they are not, else a score that grows when they are side by side and begin words */
int quick_search_fuzzy_score (const char *name, const char *query, gboolean case_sensitive);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_QUICKSEARCH_H */
