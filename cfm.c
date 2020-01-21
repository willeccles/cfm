#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

// include user configs
#include "config.h"

#ifdef __GNUC__
#define UNUSED(x) UNUSED_##x __attribute__((unused))
#else
#define UNUSED(x) UNUSED_##x
#endif

#ifndef SIGWINCH
#define SIGWINCH 28
#endif

#define K_ESC '\033'
#define ESC_UP 'A'
#define ESC_DOWN 'B'
#define ESC_LEFT 'D'
#define ESC_RIGHT 'C'
#define ESC_BKSP 127

enum elemtype {
    ELEM_DIR,
    ELEM_LINK,
    ELEM_DIRLINK,
    ELEM_EXEC,
    ELEM_FILE,
};

struct listelem {
    enum elemtype type;
    char name[NAME_MAX + 1];
    uint8_t selected;
};

static struct termios old_term;

/*
 * Get editor.
 */
static const char* geteditor() {
#ifdef EDITOR
    return EDITOR;
#else
    return getenv("EDITOR");
#endif /* EDITOR */
}

/*
 * Get shell.
 */
static const char* getshell() {
#ifdef SHELL
    return SHELL " -i";
#else
    const char* res = getenv("SHELL");
    if (!res) {
        return NULL;
    }
    char* s = malloc(PATH_MAX);
    snprintf(s, PATH_MAX, "%s -i", res);
    return s;
#endif /* SHELL */
}

/*
 * Save the default terminal settings.
 * Returns 0 on success.
 */
static int backupterm() {
    if (tcgetattr(STDIN_FILENO, &old_term) < 0) {
        perror("tcgetattr");
        return 1;
    }
    return 0;
}

/*
 * Get the size of the terminal.
 * Returns 0 on success.
 */
static int termsize(int* r, int* c) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0) {
        perror("ioctl");
        return 1;
    }

    *r = ws.ws_row;
    *c = ws.ws_col;

    return 0;
}

/*
 * Sets up the terminal for TUI.
 * Return 0 on success.
 */
static int setupterm(int r) {
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
        "\033[3;%dr", // limit scrolling to our rows
        r);

    return 0;
}

/*
 * Resets the terminal to how it was before we ruined it.
 */
static void resetterm() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term) < 0) {
        perror("tcsetattr");
        return;
    }

    printf(
        "\033[?7h"    // enable line wrapping
        "\033[?25h"   // unhide cursor
        "\033[;r"     // reset scroll region
        "\033[?1049l" // restore main screen
    );
}

/*
 * Get a key. Wraps getchar() and returns hjkl instead of arrow keys.
 * Also, returns 
 */
static int getkey() {
    char c[3];
    
    ssize_t n = read(STDIN_FILENO, c, 3);
    if (n <= 0) {
        return -1;
    }

    if (n < 3) {
        return c[0];
    }

    switch (c[2]) {
        case ESC_UP:
            return 'k';
        case ESC_DOWN:
            return 'j';
        case ESC_RIGHT:
            return 'l';
        case ESC_LEFT:
            return 'h';
        case ESC_BKSP:
            return c[2];
        default:
            return -1;
    }
}

/*
 * Draws the whole screen (redraw).
 * Use sparingly.
 */
static int drawscreen(char* wd, struct listelem* l, size_t n, size_t s, size_t o, ) {

}

/*
 * Signal handler for SIGINT/SIGTERM.
 */
static void sigdie(int UNUSED(sig)) {
    exit(EXIT_SUCCESS);
}

/*
 * Signal handler for window resize.
 */
static void sigresize(int UNUSED(sig)) {

}

int main(int argc, char** argv) {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "isatty: not connected to a tty\n");
        exit(EXIT_FAILURE);
    }

    char* wd = malloc(PATH_MAX);
    memset(wd, 0, PATH_MAX);

    if (argc == 1) {
        if (NULL == getcwd(wd, PATH_MAX)) {
            perror("getcwd");
            exit(EXIT_FAILURE);
        }
    } else {
        wd = argv[1];
        if (NULL == realpath(argv[1], wd)) {
            exit(EXIT_FAILURE);
        }
    }

    const char* editor = geteditor();
    const char* shell = getshell();

    int rows, cols;
    if (termsize(&rows, &cols)) {
        exit(EXIT_FAILURE);
    }

    struct sigaction sa_resize = {
        .sa_handler = sigresize,
    };
    if (sigaction(SIGTERM, &sa_resize, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa_ded = {
        .sa_handler = sigdie,
    };
    if (sigaction(SIGTERM, &sa_ded, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGINT, &sa_ded, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (backupterm()) {
        exit(EXIT_FAILURE);
    }

    if (setupterm(rows)) {
        exit(EXIT_FAILURE);
    }

    atexit(resetterm);

    uint8_t update = 1;
    size_t selection = 0,
           pos = 0,
           dcount = 0;

    while (1) {
        if (update) {
            update = 0;
        }
        if (redraw) {
            redraw = 0;
        }
    }

    exit(EXIT_SUCCESS);
}
