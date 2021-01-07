#include "view.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drawing.h"
#include "files.h"
#include "term.h"
#include "util.h"

void view_movedown(struct cfm_view* view) {
  if (view->selection < view->listcount - 1) {
    view->errorshown = false;

    drawentry(&view->list[view->selection], false);

    view->selection++;
    printf("\n");

    drawentry(&view->list[view->selection], true);

    if (view->pos < (size_t)getrows() - 3) {
      view->pos++;
    }

    drawstatusline(&view->list[view->selection], view->listcount,
                   view->selection, view->marks, view->pos);
  }
}

void view_moveup(struct cfm_view* view) {
  if (view->selection > 0) {
    view->errorshown = false;

    drawentry(&view->list[view->selection], false);
    view->selection--;

    if (view->pos > 0) {
      view->pos--;
      printf("\r\033[A");
    } else {
      printf("\r\033[L");
    }

    drawentry(&view->list[view->selection], true);
    drawstatusline(&view->list[view->selection], view->listcount,
                   view->selection, view->marks, view->pos);
  }
}

bool view_pagedown(struct cfm_view* view) {
  // don't do anything if we are too low to page down
  if ((size_t)getrows() - 2 + view->selection - view->pos < view->listcount) {
    // 1. move the view down so the last item is now the top item
    // 2. select that one
    // 3. if we are within view of the bottom
    view->selection += (size_t)getrows() - 2 - view->pos - 1;
    view->pos = 0;
    view->errorshown = false;

    return true; // redraw
  }

  return false; // nothing to redraw
}

bool view_pageup(struct cfm_view* view) {
  // do nothing if we are in the top "page"
  if (view->pos != view->selection) {
    // 1. move view up so that the top item is now the last item
    // 2. select that one
    // 3. if we are within view of the top, don't go up
    if ((size_t)getrows() - 2 > view->selection - view->pos) {
      view->selection = getrows() - 3;
    } else {
      view->selection -= view->pos;
    }

    view->pos = getrows() - 3;
    view->errorshown = false;

    return true;
  }

  return false;
}

bool view_gototop(struct cfm_view* view) {
  view->errorshown = false;

  if (view->pos != view->selection) {
    // if the top is not within sight, we don't have to do much here
    view->pos = 0;
    view->selection = 0;
    return true;
  } else {
    drawentry(&view->list[view->selection], false);
    view->pos = 0;
    view->selection = 0;
    printf("\033[%zu;1H", view->pos+2);
    drawentry(&view->list[view->selection], true);
    drawstatusline(&view->list[view->selection], view->listcount,
                   view->selection, view->marks, view->pos);
    return false;
  }
}

bool view_gotobottom(struct cfm_view* view) {
  view->selection = view->listcount - 1;

  if (view->listcount > (size_t)getrows() - 2) {
    view->pos = getrows() - 3;
  } else {
    view->pos = view->selection;
  }

  view->errorshown = false;

  return true;
}

bool view_enter(struct cfm_view* view) {
  if (E_DIR(view->list[view->selection].type)) {
    struct cfm_savedpos* sp = malloc(sizeof(struct cfm_savedpos));
    sp->pos = view->pos;
    sp->sel = view->selection;
    sp->prev = view->backstack;
    view->backstack = sp;

    if (view->wd[1] != '\0') {
      strcat(view->wd, "/");
    }

    strncat(view->wd, view->list[view->selection].name, PATH_MAX - strlen(view->wd) - 2);

    view->selection = 0;
    view->pos = 0;

    // TODO: replace update = true with something like view_update()
    update = true;
  } else {
    if (editor[0]) {
      execcmd(view->wd, editor, view->list[view->selection].name);
      update = true;
    }
  }

  view->errorshown = false;

  return false;
}

bool view_direxit(struct cfm_view* view) {
  char* bn = cfm_basename(view->wd);
  strncpy(lastname, bn, NAME_MAX);
  if (parentdir(view->wd)) {
    view->errorshown = false;
    if (view->backstack) {
      view->pos = view->backstack->pos;
      view->selection = view->backstack->sel;
      struct savedpos* s = view->backstack;
      view->backstack = s->prev;
      free(s);
    } else {
      view->pos = 0;
      view->selection = 0;
    }
    update = true;
  }
  return false;
}

void view_togglemark(struct cfm_view* view) {
  view->list[view->selection].marked = !(view->list[view->selection].marked);

  if (view->list[view->selection].marked) {
    view->marks++;
  } else {
    view->marks--;
  }

  drawstatusline(&view->list[view->selection], view->listcount, view->selection, view->marks, view->pos);
  drawentry(&view->list[view->selection], true);
}

// is this necessary?
void view_update(struct cfm_view* view) {
}
