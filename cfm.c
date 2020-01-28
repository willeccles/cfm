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
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

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
# define POINTER "->"
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

#ifndef MARK_SYMBOL
# define MARK_SYMBOL '+'
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
    bool marked;
};

#define E_DIR(t) ((t)==ELEM_DIR || (t)==ELEM_DIRLINK)

static struct termios old_term;
static atomic_bool redraw = false;
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

/*
 * Creates a child process.
 */
static void execcmd(const char* path, const char* cmd, const char* arg) {
    pid_t pid = fork();
    if (pid < 0) {
        return;
    }
    
    resetterm();

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
static size_t listdir(const char* path, struct listelem** list, size_t* listsize, bool hidden) {
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

            (*list)[count].marked = false;

            if (0 != fstatat(dfd, dir->d_name, &st, AT_SYMLINK_NOFOLLOW)) {
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                (*list)[count].type = ELEM_DIR;
            } else if (S_ISLNK(st.st_mode)) {
                if (0 == fstatat(dfd, dir->d_name, &st, 0)) {
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
static void drawentry(struct listelem* e, bool selected) {
    printf("\033[2K"); // clear line
    
    if (e->marked) {
        printf("\033[35;1m");
    } else {
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
                printf("\033[37m");
                break;
        }
    }

#if defined INVERT_SELECTION && INVERT_SELECTION
    if (selected) {
        printf("\033[7m");
    }
#endif
    
#if defined INDENT_SELECTION && INDENT_SELECTION
    if (selected) {
        printf("%s", POINTER);
    }
#else
    printf("%-*s", pointerwidth, selected ? POINTER : "");
#endif

#if defined INVERT_SELECTION && INVERT_SELECTION
    if (selected) {
        printf(" %s%-*s", e->name, cols, E_DIR(e->type) ? "/" : "");
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

    printf("\033[m\r"); // cursor to column 1
}

/*
 * Draws the status line at the bottom of the screen.
 */
static void drawstatusline(struct listelem* l, size_t n, size_t s, size_t m) {
    // TODO remove u and s escapes
    printf("\033[s" // save location of cursor
        "\033[%d;H" // go to the bottom row
        "\033[2K" // clear the row
        "\033[37;7;1m", // inverse + bold
        rows);

    int p1 = 0, p2 = 0;
    printf(" %zu/%zu" // position
        "%n", // chars printed
        n ? s+1 : n,
        n,
        &p1);
    if (m) {
        printf(" (%zu marked)%n", m, &p2);
    }
    // print the type of the file
    printf("%*s \r", cols-p1-p2-1, elemtypestrings[l->type]);
    printf("\033[m\033[u"); // move cursor back and reset formatting
}

/*
 * Draws the whole screen (redraw).
 * Use sparingly.
 */
static void drawscreen(char* wd, struct listelem* l, size_t n, size_t s, size_t o, size_t m) {
    // go to the top and print the info bar
    int p;
    printf("\033[2J" // clear
        "\033[H" // top left
        "\033[37;7;1m"); // style
    printf(" %s%n", // print working directory
        wd, &p);
    printf("%-*s", (int)(cols - p), (wd[1] == '\0') ? "" : "/");

    printf("\033[m"); // reset formatting

    for (size_t i = s - o; i < n && (int)(i - (s - o)) < rows - 2; i++) {
        printf("\r\n");
        drawentry(&(l[i]), (bool)(i == s));
    }

    drawstatusline(&(l[s]), n, s, m);
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
    redraw = true;
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

    bool update = true;
    bool showhidden = false;
    size_t selection = 0,
           pos = 0,
           dcount = 0,
           marks = 0;
    size_t newdcount = 0;

    int k, pk;
    while (1) {
        if (update) {
            update = false;
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
                while (selection >= newdcount) {
                    if (selection) {
                        selection--;
                        if (pos) {
                            pos--;
                        }
                    }
                }
            }
            dcount = newdcount;
            redraw = true;
        }

        if (redraw) {
            redraw = false;
            if (termsize()) {
                exit(EXIT_FAILURE);
            }
            drawscreen(wd, list, dcount, selection, pos, marks);
            printf("\033[%zu;1H", pos+2);
            fflush(stdout);
        }

        k = getkey();
        switch(k) {
            case 'h':
                if (parentdir(wd)) {
                    pos = 0;
                    selection = 0;
                    update = true;
                }
                break;
            case '\033':
            case 'q':
                exit(EXIT_SUCCESS);
                break;
            case '.':
                showhidden = !showhidden;
                selection = 0;
                pos = 0;
                update = true;
                break;
            case 'r':
                update = true;
                break;
            case 's':
                if (shell != NULL) {
                    execcmd(wd, shell, NULL);
                    update = true;
                }
                break;
        }

        if (!dcount) {
            pk = k;
            fflush(stdout);
            continue;
        }

        switch (k) {
            case 'j':
                if (selection < dcount - 1) {
                    drawentry(&(list[selection]), false);
                    selection++;
                    printf("\n");
                    drawentry(&(list[selection]), true);
                    drawstatusline(&(list[selection]), dcount, selection, marks);
                    if (pos < rows - 3) {
                        pos++;
                    }
                }
                break;
            case 'k':
                if (selection > 0) {
                    drawentry(&(list[selection]), false);
                    selection--;
                    if (pos > 0) {
                        pos--;
                        printf("\r\033[A");
                    } else {
                        printf("\r\033[L");
                    }
                    drawentry(&(list[selection]), true);
                    drawstatusline(&(list[selection]), dcount, selection, marks);
                }
                break;
            case 'g':
                if (pk != 'g') {
                    break;
                }
                pos = 0;
                selection = 0;
                redraw = true;
                pk = 0;
                break;
            case 'G':
                selection = dcount - 1;
                if (dcount > rows - 2) {
                    pos = rows - 3;
                } else {
                    pos = selection;
                }
                redraw = true;
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
                    update = true;
                } else {
                    execcmd(wd, editor, list[selection].name);
                    update = true;
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
                    update = true;
                    break;
                } else if (opener != NULL) {
                    if (editor != NULL) {
                        execcmd(wd, opener, list[selection].name);
                        update = true;
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
                    update = true;
                }
                pk = 0;
                break;
            case 'D':
                if (!marks) {
                    break;
                }
                {
                    char tmpbuf[PATH_MAX];
                    for (int i = 0; i < dcount; i++) {
                        if (list[i].marked) {
                            snprintf(tmpbuf, PATH_MAX, "%s/%s", wd, list[i].name);
                            remove(tmpbuf);
                        }
                    }
                    update = true;
                }
                break;
            case 'e':
                if (editor != NULL) {
                    execcmd(wd, editor, list[selection].name);
                    update = true;
                }
                break;
            case 'm':
                list[selection].marked = !(list[selection].marked);
                if (list[selection].marked) {
                    marks++;
                } else {
                    marks--;
                }
                drawstatusline(&(list[selection]), dcount, selection, marks);
                drawentry(&(list[selection]), true);
                break;
        }

        pk = k;
        fflush(stdout);
    }

    exit(EXIT_SUCCESS);
}
