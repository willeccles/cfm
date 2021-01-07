#ifndef CFM_VIEW_H_
#define CFM_VIEW_H_

#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

#include "files.h"
#include "types.h"

/*
 * Represents a saved position for a view (i.e. when coming out of a
 * directory). Can be chained.
 */
struct cfm_savedpos {
  size_t pos, sel; // position and selection
  struct cfm_savedpos* prev;
};

/*
 * Structure representing a single directory view in cfm.
 *
 * TODO: error things should no longer be a part of the view; instead, they
 * should be made part of the GUI functionality
 */
struct cfm_view {
  char wd[PATH_MAX + 1];

  // TODO: remove these
  const char* eprefix;
  const char* emsg;
  bool errorshown;

  size_t selection;
  size_t pos;
  size_t marks;
  struct cfm_savedpos* backstack;

  // TODO: encapsulate in a file list structure which could then be pointed to
  // by the view rather than whatever this is
  struct cfm_listelem* list;
  size_t listcount; // used to be dcount
};

/* TODO: view functions */
extern void view_movedown(struct cfm_view* view);
extern void view_moveup(struct cfm_view* view);
extern bool view_pagedown(struct cfm_view* view);
extern bool view_pageup(struct cfm_view* view);
extern bool view_gototop(struct cfm_view* view);
extern bool view_gotobottom(struct cfm_view* view);
extern bool view_enter(struct cfm_view* view);
extern bool view_direxit(struct cfm_view* view);
extern void view_togglemark(struct cfm_view* view);
extern void view_update(struct cfm_view* view);

#endif /* CFM_VIEW_H_ */
