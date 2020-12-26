#ifndef CFM_VIEW_H_
#define CFM_VIEW_H_

#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

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
  struct savedpos* backstack;
};

/* TODO: view functions */

#endif /* CFM_VIEW_H_ */
