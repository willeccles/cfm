#ifndef CFM_DRAWING_H_
#define CFM_DRAWING_H_

#include "types.h"

/*
 * Draws one element to the screen.
 */
extern void drawentry(struct cfm_listelem* e, bool selected);

/*
 * Draws the status line at the bottom of the screen.
 */
extern void drawstatusline(struct cfm_listelem* l, size_t n, size_t s, size_t m, size_t p);

/*
 * Draws the statusline with an error message in it.
 */
extern void drawstatuslineerror(const char* prefix, const char* error, size_t p);

/*
 * Draws the whole screen (redraw).
 * Use sparingly.
 */
extern void drawscreen(char* wd, struct cfm_listelem* l, size_t n, size_t s, size_t o, size_t m, int v);

#endif
