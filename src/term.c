#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "term.h"

static bool interactive = false;
static int _rows, _cols;
static struct termios old_term;

int backupterm(void) {
  if (!interactive) return 0;
  if (tcgetattr(STDIN_FILENO, &old_term) < 0) {
    perror("tcgetattr");
    return 1;
  }
  return 0;
}

int termsize(int* rows, int* cols) {
  if (!interactive) return 0;
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0) {
    perror("ioctl");
    return 1;
  }

  _rows = ws.ws_row;
  _cols = ws.ws_col;

  *rows = _rows;
  *cols = _cols;

  return 0;
}

int setupterm(void) {
  interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

  if (!interactive) return 0;

  setvbuf(stdout, NULL, _IOFBF, 0);

  struct termios new_term = old_term;
  new_term.c_oflag &= ~OPOST;
  new_term.c_lflag &= ~(ECHO | ICANON);

  if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) < 0) {
    perror("tcsetattr");
    return 1;
  }

  printf(
      "\033[?1049h" // use alternative screen buffer
      "\033[?7l"    // disable line wrapping
      "\033[?25l"   // hide cursor
      "\033[2J"     // clear screen
      "\033[2;%dr", // limit scrolling to our rows
      _rows-1);

  return 0;
}

void resetterm(void) {
  if (!interactive) return;

  setvbuf(stdout, NULL, _IOLBF, 0);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term) < 0) {
    perror("tcsetattr");
    return;
  }

  printf(
      "\033[?7h"    // enable line wrapping
      "\033[?25h"   // unhide cursor
      "\033[r"     // reset scroll region
      "\033[?1049l" // restore main screen
      );

  fflush(stdout);
}

bool isinteractive(void) {
  return interactive;
}

int getrows() {
  return _rows;
}

int getcols() {
  return _cols;
}
