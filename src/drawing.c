#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "drawing.h"
#include "term.h"
#include "userconfig.h"

void drawentry(struct cfm_listelem* e, bool selected) {
  if (!isinteractive()) return;
  printf("\033[2K"); // clear line

#if BOLD_POINTER
# define PBOLD printf("\033[1m")
#else
# define PBOLD
#endif
  if (e->marked) {
    printf("\033[35m");
    PBOLD;
  } else {
    switch (e->type) {
      case ELEM_EXEC:
        printf("\033[33m");
        PBOLD;
        break;
      case ELEM_DIR:
        printf("\033[32m");
        PBOLD;
        break;
      case ELEM_DIRLINK:
        printf("\033[36m");
        PBOLD;
        break;
      case ELEM_LINK:
        printf("\033[36m");
        break;
      case ELEM_FILE:
      default:
        printf("\033[37m");
        break;
    }
  }
#undef PBOLD

#if INVERT_SELECTION && INVERT_FULL_SELECTION
  if (selected) {
    printf("\033[7m");
  }
#endif

#if INDENT_SELECTION
  if (selected) {
    printf("%s", POINTER);
  }
#else
  printf("%-*s", pointerwidth, selected ? POINTER : "");
#endif

#if !BOLD_POINTER
  if (e->marked) {
    printf("\033[1m");
    if (e->type == ELEM_EXEC
        || e->type == ELEM_DIR
        || e->type == ELEM_DIRLINK) {
      printf("\033[1m");
    }
  }
#endif

#if INVERT_SELECTION
  if (selected) {
# if INVERT_FULL_SELECTION
    printf(" %s%-*s", e->name, getcols(), E_DIR(e->type) ? "/" : "");
# else
    printf(" \033[7m%s%s", e->name, E_DIR(e->type) ? "/" : "");
# endif
  } else {
    printf(" %s", e->name);
    if (E_DIR(e->type)) {
      printf("/");
    }
  }
#else
  printf(" %s", e->name);
  if (E_DIR(e->type)) {
    printf("/");
  }
#endif

  if (e->marked && !selected) {
    printf("\r%c", MARK_SYMBOL);
  }

  printf("\r\033[m"); // cursor to column 1
}

void drawstatusline(struct cfm_listelem* l, size_t n, size_t s, size_t m, size_t p) {
  if (!isinteractive()) return;
  printf("\033[%d;H" // go to the bottom row
         //"\033[2K" // clear the row
         "\033[37;7;1m", // inverse + bold
         getrows());

  int count;
  if (!m) {
    count = printf(" %zu/%zu", n ? s+1 : n, n);
  } else {
    count = printf(" %zu/%zu (%zu marked)", n ? s+1 : n, n, m);
  }
  // print the type of the file
  printf("%*s \r", getcols()-count-1, elemtypestrings[l->type]);
  printf("\033[m\n\033[%zu;H", p+2); // move cursor back and reset formatting
}

void drawstatuslineerror(const char* prefix, const char* error, size_t p) {
  if (!isinteractive()) {
    // instead print to stderr
    fprintf(stderr, "%s: %s\n", prefix, error);
    exit(EXIT_FAILURE);
    return;
  }

  printf("\033[%d;H"
         //"\033[2K"
         "\033[31;7;1m",
         getrows());
  int count = printf(" %s: ", prefix);
  printf("%-*s \r", getcols()-count-1, error);
  printf("\033[m\033[%zu;H", p+2);
}

void drawscreen(char* wd, struct cfm_listelem* l, size_t n, size_t s, size_t o, size_t m, int v) {
  if (!isinteractive()) return;

  // clear the screen except for the top and bottom lines
  // this gets rid of the flashing when redrawing
  for (int i = 2; i < getrows(); i++) {
    printf("\033[%dH" // row i
           "\033[m"
           "\033[K", i); // clear row
  }

  // go to the top and print the info bar
  printf("\033[H" // top left
         "\033[37;7;1m"); // style

  int count;
#if VIEW_COUNT > 1
  count = printf(" %d: %s", v+1, wd);
#else
  (void)v;
  count = printf(" %s", wd);
#endif

  printf("%-*s", (int)(getcols() - count), (wd[1] == '\0') ? "" : "/");

  printf("\033[m"); // reset formatting

  for (size_t i = s - o; i < n && (int)(i - (s - o)) < getrows() - 2; i++) {
    printf("\r\n");
    drawentry(&(l[i]), (bool)(i == s));
  }

  drawstatusline(&(l[s]), n, s, m, o);
}
