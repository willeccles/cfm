#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

// include user config
#include "config.h"

#ifdef __GNUC__
#define UNUSED(x) UNUSED_##x __attribute__((unused))
#else
#define UNUSED(x) UNUSED_##x
#endif

#define K_ESC '\033'
#define ESC_UP 'A'
#define ESC_DOWN 'B'
#define ESC_LEFT 'D'
#define ESC_RIGHT 'C'

#define LIST_ALLOC_SIZE 64

#if defined(USE_ITALICS) && USE_ITALICS
#define EMPHASIS "\033[3m"
#else
#define EMPHASIS "\033[1m"
#endif /* USE_ITALICS */

#if !defined(POINTER) || !POINTER
#define POINTER "->"
#endif /* POINTER */

enum elemtype {
    ELEM_DIR,
    ELEM_LINK,
    ELEM_DIRLINK,
    ELEM_EXEC,
    ELEM_FILE,
};

struct listelem {
    enum elemtype type;
    char name[NAME_MAX];
    uint8_t selected;
};

static struct termios old_term;
static uint8_t redraw = 0;
static int pointerwidth = 2;

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
 * Creates a child process.
 */
static void execcmd(const char* path, const char* cmd, const char* arg, int r) {
    pid_t pid = fork();
    if (pid < 0) {
        return;
    }

    if (pid == 0) {
        if (chdir(path) < 0) {
            _exit(EXIT_FAILURE);
        }
        execlp(cmd, cmd, arg, NULL);
        _exit(EXIT_FAILURE);
    } else {
        int s;
        do {
            waitpid(pid, &s, WUNTRACED);
        } while (!WIFEXITED(s) && !WIFSIGNALED(s));
    }

    setupterm(r);
}

/*
 * Reads a directory into the list, returning the number of items in the dir.
 */
static size_t listdir(const char* path, struct listelem** list, size_t* listsize, uint8_t hidden) {
    DIR* d;
    struct dirent* dir;
    d = opendir(path);
    size_t count = 0;
    struct stat st;
    int dfd = dirfd(d);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_name[0] == '.' && (dir->d_name[1] == '\0' || (dir->d_name[1] == '.' && dir->d_name[2] == '\0'))) {
                continue;
            }

            if (!hidden && dir->d_name[0] == '.') {
                continue;
            }

            if (count == *listsize) {
                *listsize += LIST_ALLOC_SIZE;
                *list = realloc(*list, *listsize * sizeof(**list));
                if (*list == NULL) {
                    perror("realloc");
                    exit(EXIT_FAILURE);
                }
            }

            strncpy((*list)[count].name, dir->d_name, NAME_MAX);

            (*list)[count].selected = 0;

            if (0 != fstatat(dfd, dir->d_name, &st, AT_SYMLINK_NOFOLLOW)) {
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                (*list)[count].type = ELEM_DIR;
            } else if (S_ISLNK(st.st_mode)) {
                if (0 != fstatat(dfd, dir->d_name, &st, 0)) {
                    if (S_ISDIR(st.st_mode)) {
                        (*list)[count].type = ELEM_DIRLINK;
                    } else {
                        (*list)[count].type = ELEM_LINK;
                    }
                } else {
                    (*list)[count].type = ELEM_LINK;
                }
            } else {
                if (st.st_mode & S_IXUSR) {
                    (*list)[count].type = ELEM_EXEC;
                } else {
                    (*list)[count].type = ELEM_FILE;
                }
            }

            count++;
        }

        closedir(d);
        // TODO sort the list
    }

    return count;
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
        default:
            return -1;
    }
}

/*
 * Draws one element to the screen.
 */
static void drawentry(struct listelem* e) {
    if (e->selected) {
        printf("%s ", POINTER);
    } else {
        for (int i = 0; i < pointerwidth; i++) {
            printf(" ");
        }
    }

    switch (e->type) {
        case ELEM_EXEC:
            printf("\033[33m");
            break;
        case ELEM_DIR:
            printf("\033[32m");
            break;
        case ELEM_LINK:
        case ELEM_DIRLINK:
            printf("\033[36m");
            break;
        case ELEM_FILE:
        default:
            printf("\033[m");
            break;
    }

    printf("%s\033[m", e->name);

    printf("\033[G"); // cursor to column 1
}

/*
 * Draws the whole screen (redraw).
 * Use sparingly.
 */
static void drawscreen(char* wd, struct listelem* l, size_t n, size_t s, size_t o, int r, int c) {
    char fmt[256];
    // use a format str to align the path and such
    snprintf(fmt, 256,
        "\033[2J" // clear
        "\033[%d;H" // go to the bottom row
        EMPHASIS "\033[7m%%-%ds" // path + count
        "\033[m",
        r, c);
    char status[256];
    snprintf(status, 256, "%zu in %s", n, wd);

    printf(fmt, status);

    printf("\033[H"); // move to top left

    for (size_t i = o; i < n && i - o < r - 2; i++) {
        drawentry(&(l[i]));
        printf("\n\r");
    }

    fflush(stdout);
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

    pointerwidth = strlen(POINTER) + 1;

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

    size_t listsize = LIST_ALLOC_SIZE;
    struct listelem* list = malloc(LIST_ALLOC_SIZE * sizeof(struct listelem));
    if (!list) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    uint8_t update = 1;
    size_t selection = 0,
           pos = 0,
           dcount = 0;

    int k;
    while (1) {
        if (update) {
            update = 0;
            dcount = listdir(wd, &list, &listsize, 0);
            selection = 0;
            list[selection].selected = 1;
            redraw = 1;
        }
        if (redraw) {
            redraw = 0;
            drawscreen(wd, list, dcount, selection, pos, rows, cols);
            printf("\033[%zu;1H", selection+1);
        }

        k = getkey();
        switch (k) {
            case 'j':
                if (selection < dcount - 1) {
                    list[selection].selected = 0;
                    drawentry(&(list[selection]));
                    selection++;
                    list[selection].selected = 1;
                    printf("\033[E");
                    drawentry(&(list[selection]));
                    fflush(stdout);
                }
                break;
            case 'k':
                if (selection > 0) {
                    list[selection].selected = 0;
                    drawentry(&(list[selection]));
                    selection--;
                    list[selection].selected = 1;
                    printf("\033[L");
                    drawentry(&(list[selection]));
                    fflush(stdout);
                }
                break;
        }
    }

    exit(EXIT_SUCCESS);
}
