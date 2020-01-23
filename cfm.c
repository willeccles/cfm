/* Copyright (c) Will Eccles <will@eccles.dev>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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
#include <stdatomic.h>

// include user config
#include "config.h"

#ifdef __GNUC__
# define UNUSED(x) UNUSED_##x __attribute__((unused))
#else
# define UNUSED(x) UNUSED_##x
#endif

#define K_ESC '\033'
#define ESC_UP 'A'
#define ESC_DOWN 'B'
#define ESC_LEFT 'D'
#define ESC_RIGHT 'C'

#define LIST_ALLOC_SIZE 64

#ifndef POINTER
# define POINTER "-> "
#endif /* POINTER */

#ifdef OPENER
# if defined ENTER_OPENS && ENTER_OPENS
#  define ENTER_OPEN
# else
#  undef ENTER_OPEN
# endif
#else
# undef ENTER_OPENS
# undef ENTER_OPEN
#endif

enum elemtype {
    ELEM_DIR,
    ELEM_LINK,
    ELEM_DIRLINK,
    ELEM_EXEC,
    ELEM_FILE,
};

static const char* elemtypestrings[] = {
    "dir",
    "@file",
    "@dir",
    "exec",
    "file",
};

struct listelem {
    enum elemtype type;
    char name[NAME_MAX];
    uint8_t selected;
};

#define E_DIR(t) ((t)==ELEM_DIR || (t)==ELEM_DIRLINK)

static struct termios old_term;
static atomic_bool redraw = 0;
static int rows, cols;
static int pointerwidth = 2;

/*
 * Comparison function for list elements for qsort.
 */
static int elemcmp(const void* a, const void* b) {
    const struct listelem* x = a;
    const struct listelem* y = b;

    if ((x->type == ELEM_DIR || x->type == ELEM_DIRLINK) &&
        (y->type != ELEM_DIR && y->type != ELEM_DIRLINK)) {
        return -1;
    }

    if ((y->type == ELEM_DIR || y->type == ELEM_DIRLINK) &&
        (x->type != ELEM_DIR && x->type != ELEM_DIRLINK)) {
        return 1;
    }

    return strcmp(x->name, y->name);
}

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
    return SHELL;
#else
    const char* res = getenv("SHELL");
    if (!res) {
        return NULL;
    }
    return res;
#endif /* SHELL */
}

/*
 * Get the opener program.
 */
static const char* getopener() {
#ifdef OPENER
    return OPENER;
#else
    return NULL;
#endif
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
static int termsize() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0) {
        perror("ioctl");
        return 1;
    }

    rows = ws.ws_row;
    cols = ws.ws_col;

    return 0;
}

/*
 * Sets up the terminal for TUI.
 * Return 0 on success.
 */
static int setupterm() {
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
        rows-1);

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
static void execcmd(const char* path, const char* cmd, const char* arg) {
    pid_t pid = fork();
    if (pid < 0) {
        return;
    }
    
    resetterm();
    fflush(stdout);

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

    setupterm();
    fflush(stdout);
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
        qsort(*list, count, sizeof(**list), elemcmp);
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
    printf("\033[2K"); // clear line
    
    switch (e->type) {
        case ELEM_EXEC:
            printf("\033[33;1m");
            break;
        case ELEM_DIR:
            printf("\033[32;1m");
            break;
        case ELEM_DIRLINK:
            printf("\033[36;1m");
            break;
        case ELEM_LINK:
            printf("\033[36m");
            break;
        case ELEM_FILE:
        default:
            printf("\033[m");
            break;
    }

#if defined INVERT_SELECTION && INVERT_SELECTION
    if (e->selected) {
        printf("\033[7m");
    }
#endif
    
    printf("%-*s", pointerwidth, e->selected ? POINTER : "");

#if defined INVERT_SELECTION && INVERT_SELECTION
    if (e->selected) {
        printf("%s%-*s", e->name, cols, E_DIR(e->type) ? "/" : "");
    } else {
        printf("%s", e->name);
        if (E_DIR(e->type)) {
            printf("/");
        }
    }
#else
    printf("%s", e->name);
    if (E_DIR(e->type)) {
        printf("/");
    }
#endif

    printf("\033[m\r"); // cursor to column 1
}

/*
 * Draws the status line at the bottom of the screen.
 */
static void drawstatusline(struct listelem* l, size_t n, size_t s) {
    printf("\033[s" // save location of cursor
        "\033[%d;H" // go to the bottom row
        "\033[2K" // clear the row
        "\033[7;1m", // inverse + bold
        rows);

    int p = 0;
    printf(" %zu/%zu" // position
        "%n", // chars printed
        n ? s+1 : n,
        n,
        &p);
    // print the type of the file
    printf("%*s ", cols-p-1, elemtypestrings[l->type]);
    printf("\033[u\033[m"); // move cursor back and reset formatting
}

/*
 * Draws the whole screen (redraw).
 * Use sparingly.
 */
static void drawscreen(char* wd, struct listelem* l, size_t n, size_t s, size_t o) {
    // go to the top and print the info bar
    printf("\033[2J" // clear
        "\033[H" // top left
        "\033[7;3m %-*s ", // print working directory
        cols-2, wd);

    printf("\033[m"); // reset formatting

    for (size_t i = s - o; i < n && (int)(i - (s - o)) < rows - 2; i++) {
        printf("\r\n");
        drawentry(&(l[i]));
    }

    drawstatusline(&(l[s]), n, s);
}

/*
 * Writes back the parent directory of a path.
 * Returns 1 if the path was changed, 0 if not (i.e. if
 * the path was "/").
 */
static int parentdir(char* path) {
    char* last = strrchr(path, '/');
    if (last == path && path[1] == '\0') {
        return 0;
    }

    if (last == path) {
        path[1] = '\0';
    } else {
        *last = '\0';
    }

    return 1;
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
    redraw = 1;
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
        if (NULL == realpath(argv[1], wd)) {
            exit(EXIT_FAILURE);
        }
    }

    pointerwidth = strlen(POINTER);

    const char* editor = geteditor();
    const char* shell = getshell();
    const char* opener = getopener();

    if (termsize()) {
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

    if (setupterm()) {
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
    uint8_t showhidden = 0;
    size_t selection = 0,
           pos = 0,
           dcount = 0;
    size_t newdcount = 0;

    int k, pk;
    while (1) {
        if (update) {
            update = 0;
            list[selection].selected = 0;
            newdcount = listdir(wd, &list, &listsize, showhidden);
            if (!newdcount) {
                pos = 0;
                selection = 0;
            } else {
                // lock to bottom if deleted file at top
                if (newdcount < dcount) {
                    if (pos == 0 && selection > 0) {
                        if (dcount - selection == rows - 2) {
                            selection--;
                        }
                    }
                }
                if (selection >= newdcount) {
                    selection = newdcount - 1;
                }
                list[selection].selected = 1;
            }
            dcount = newdcount;
            redraw = 1;
        }

        if (redraw) {
            redraw = 0;
            if (termsize()) {
                exit(EXIT_FAILURE);
            }
            drawscreen(wd, list, dcount, selection, pos);
            printf("\033[%zu;1H", pos+2);
            fflush(stdout);
        }

        k = getkey();
        switch(k) {
            case 'h':
                if (parentdir(wd)) {
                    pos = 0;
                    selection = 0;
                    update = 1;
                }
                break;
            case '\033':
            case 'q':
                exit(EXIT_SUCCESS);
                break;
            case '.':
                showhidden = !showhidden;
                list[selection].selected = 0;
                selection = 0;
                pos = 0;
                update = 1;
                break;
            case 'r':
                update = 1;
                break;
            case 's':
                if (shell != NULL) {
                    execcmd(wd, shell, NULL);
                    update = 1;
                }
                break;
        }

        if (!dcount) {
            pk = k;
            continue;
        }

        switch (k) {
            case 'j':
                if (selection < dcount - 1) {
                    list[selection].selected = 0;
                    drawentry(&(list[selection]));
                    selection++;
                    printf("\r\n");
                    list[selection].selected = 1;
                    drawentry(&(list[selection]));
                    drawstatusline(&(list[selection]), dcount, selection);
                    if (pos < rows - 3) {
                        pos++;
                    }
                    fflush(stdout);
                }
                break;
            case 'k':
                if (selection > 0) {
                    list[selection].selected = 0;
                    drawentry(&(list[selection]));
                    selection--;
                    list[selection].selected = 1;
                    if (pos > 0) {
                        pos--;
                        printf("\r\033[A");
                    } else {
                        printf("\r\033[L");
                    }
                    drawentry(&(list[selection]));
                    drawstatusline(&(list[selection]), dcount, selection);
                    fflush(stdout);
                }
                break;
            case 'g':
                if (pk != 'g') {
                    break;
                }
                list[selection].selected = 0;
                pos = 0;
                selection = 0;
                list[selection].selected = 1;
                redraw = 1;
                pk = 0;
                break;
            case 'G':
                list[selection].selected = 0;
                selection = dcount - 1;
                list[selection].selected = 1;
                if (dcount > rows - 2) {
                    pos = rows - 3;
                } else {
                    pos = selection;
                }
                redraw = 1;
                break;
#ifndef ENTER_OPEN
            case '\n':
#endif
            case 'l':
                if (list[selection].type == ELEM_DIR
                    || list[selection].type == ELEM_DIRLINK) {
                    if (wd[1] != '\0') {
                        strcat(wd, "/");
                    }
                    strncat(wd, list[selection].name, PATH_MAX - strlen(wd) - 2);
                    selection = 0;
                    pos = 0;
                    update = 1;
                } else {
                    execcmd(wd, editor, list[selection].name);
                    update = 1;
                }
                break;
#ifdef ENTER_OPEN
            case '\n':
#endif
#ifdef OPENER
            case 'o':
                if (list[selection].type == ELEM_DIR
                    || list[selection].type == ELEM_DIRLINK) {
                    if (wd[1] != '\0') {
                        strcat(wd, "/");
                    }
                    strncat(wd, list[selection].name, PATH_MAX - strlen(wd) - 2);
                    selection = 0;
                    pos = 0;
                    update = 1;
                    break;
                } else if (opener != NULL) {
                    if (editor != NULL) {
                        execcmd(wd, opener, list[selection].name);
                        update = 1;
                    }
                }
                break;
#endif
            case 'd':
                if (pk != 'd') {
                    break;
                }
                {
                    char tmpbuf[PATH_MAX];
                    snprintf(tmpbuf, PATH_MAX, "%s/%s", wd, list[selection].name);
                    remove(tmpbuf);
                    update = 1;
                }
                pk = 0;
                break;
            case 'e':
                if (editor != NULL) {
                    execcmd(wd, editor, list[selection].name);
                    update = 1;
                }
                break;
        }

        pk = k;
    }

    exit(EXIT_SUCCESS);
}
