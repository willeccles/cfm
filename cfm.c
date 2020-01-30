/* Copyright (c) Will Eccles <will@eccles.dev>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
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

#ifndef BOLD_POINTER
# define BOLD_POINTER 1
#endif

#ifndef INVERT_SELECTION
# define INVERT_SELECTION 1
#endif

#ifndef INDENT_SELECTION
# define INDENT_SELECTION 1
#endif

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
# define MARK_SYMBOL '!'
#endif

#ifdef VIEW_COUNT
# if VIEW_COUNT > 5
#  undef VIEW_COUNT
#  define VIEW_COUNT 5
# elif VIEW_COUNT <= 0
#  undef VIEW_COUNT
#  define VIEW_COUNT 1
# endif
#else
# define VIEW_COUNT 2
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

static int del_id = 0;

struct deletedfile {
    char* original;
    int id;
    bool mass;
    struct deletedfile* prev;
};

static struct termios old_term;
static atomic_bool redraw = false;
static int rows, cols;
static int pointerwidth = 2;
static char editor[PATH_MAX];
static char opener[PATH_MAX];
static char shell[PATH_MAX];
static char trashdir[PATH_MAX];

static int rmFiles(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb) {
    return remove(pathname) ? -1 : 0;
}

/*
 * Deletes a directory, even if it contains files.
 */
static int deldir(const char* dir) {
    return nftw(dir, rmFiles, 10, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
}

static struct deletedfile* newdeleted(bool mass) {
    struct deletedfile* d = malloc(sizeof(struct deletedfile));
    if (!d) {
        return NULL;
    }

    d->original = malloc(NAME_MAX);
    if (!d->original) {
        free(d);
        return NULL;
    }

    d->id = del_id++;
    d->mass = mass;
    d->prev = NULL;

    return d;
}

static struct deletedfile* freedeleted(struct deletedfile* f) {
    struct deletedfile* d = f->prev;
    free(f->original);
    free(f);
    return d;
}

static int strnatcmp(const char *s1, const char *s2) {
    for (;;) {
        if (*s2 == '\0') {
            return *s1 != '\0';
        }

        if (*s1 == '\0') {
            return 1;
        }

        if (!(isdigit(*s1) && isdigit(*s2))) {
            if (toupper(*s1) != toupper(*s2)) {
                return toupper(*s1) - toupper(*s2);
            }
            ++s1;
            ++s2;
        } else {
            char *lim1;
            char *lim2;
            unsigned long n1 = strtoul(s1, &lim1, 10);
            unsigned long n2 = strtoul(s2, &lim2, 10);
            if (n1 > n2) {
                return 1;
            } else if (n1 < n2) {
                return -1;
            }
            s1 = lim1;
            s2 = lim2;
        }
    }
}

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

    return strnatcmp(x->name, y->name);
}

/* 
 * Get the user's home dir.
 */
static char* gethome(char* out, size_t len) {
    const char* home = getenv("HOME");
    if (!home) {
        return NULL;
    } else {
        strncpy(out, home, len);
        return out;
    }
}

/*
 * Get editor.
 */
static void geteditor() {
#ifdef EDITOR
    if (NULL == realpath(EDITOR, editor)) {
        editor[0] = '\0';
    }
#else
    const char* res = getenv("CFM_EDITOR");
    if (!res) {
        res = getenv("EDITOR");
        if (!res) {
            editor[0] = '\0';
        } else {
            if (!realpath(res, editor)) {
                editor[0] = '\0';
            }
        }
    } else {
        if (!realpath(res, editor)) {
            editor[0] = '\0';
        }
    }
#endif /* EDITOR */
}

/*
 * Get shell.
 */
static void getshell() {
#ifdef SHELL
    if (NULL == realpath(SHELL, shell)) {
        shell[0] = '\0';
    }
#else
    const char* res = getenv("CFM_SHELL");
    if (!res) {
        res = getenv("SHELL");
        if (!res) {
            shell[0] = '\0';
        } else {
            if (!realpath(res, shell)) {
                shell[0] = '\0';
            }
        }
    } else {
        if (!realpath(res, shell)) {
            shell[0] = '\0';
        }
    }
#endif /* SHELL */
}

/*
 * Get the opener program.
 */
static void getopener() {
#ifdef OPENER
    if (NULL == realpath(OPENER, opener)) {
        shell[0] = '\0';
    }
#else
    const char* res = getenv("CFM_OPENER");
    if (!res) {
        res = getenv("OPENER");
        if (!res) {
            opener[0] = '\0';
        } else {
            if (!realpath(res, opener)) {
                opener[0] = '\0';
            }
        }
    } else {
        if (!realpath(res, opener)) {
            opener[0] = '\0';
        }
    }
#endif
}

/*
 * Get the trash directory.
 */
static void maketrashdir() {
    if (gethome(trashdir, PATH_MAX - strlen("/.cfmtrash"))) {
        strncat(trashdir, "/.cfmtrash", PATH_MAX - strlen(trashdir));
        if (mkdir(trashdir, 0751)) {
            if (errno == EEXIST) {
                return;
            }
            trashdir[0] = '\0';
        }
    } else {
        trashdir[0] = '\0';
    }
}

static void emptytrash() {
    if (trashdir[0]) {
        if (0 != deldir(trashdir)) {
            perror("unlink: ~/.cfmtrash");
        }
    }
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
 * This will return 0 on success.
 * On failure, 'opendir' will have set 'errno'.
 */
static int listdir(const char* path, struct listelem** list, size_t* listsize, size_t* rcount, bool hidden) {
    DIR* d;
    struct dirent* dir;
    d = opendir(path);
    size_t count = 0;
    struct stat st;
    if (d) {
        int dfd = dirfd(d);
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
    } else {
        return -1;
    }

    *rcount = count;
    return 0;
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

#if INVERT_SELECTION
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

    printf("\r\033[m"); // cursor to column 1
}

/*
 * Draws the status line at the bottom of the screen.
 */
static void drawstatusline(struct listelem* l, size_t n, size_t s, size_t m, size_t p) {
    printf("\033[%d;H" // go to the bottom row
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
    printf("\033[m\033[%zu;H", p+2); // move cursor back and reset formatting
}

/*
 * Draws the statusline with an error message in it.
 */
static void drawstatuslineerror(const char* prefix, const char* error, size_t p) {
    printf("\033[%d;H"
        "\033[2K"
        "\033[31;7;1m",
        rows);
    int n;
    printf(" %s: %n", prefix, &n);
    printf("%-*s\r", cols-n-1, error);
    printf("\033[m\033[%zu;H", p+2);
}

/*
 * Draws the whole screen (redraw).
 * Use sparingly.
 */
static void drawscreen(char* wd, struct listelem* l, size_t n, size_t s, size_t o, size_t m, int v) {
    // go to the top and print the info bar
    int p;
    printf("\033[2J" // clear
        "\033[H" // top left
        "\033[37;7;1m"); // style
#if VIEW_COUNT > 1
    printf(" %d: %s%n", // working directory + view
        v+1, wd, &p);
#else
    (void)i;
    printf(" %s%n", // print working directory
        wd, &p);
#endif
    printf("%-*s", (int)(cols - p), (wd[1] == '\0') ? "" : "/");

    printf("\033[m"); // reset formatting

    for (size_t i = s - o; i < n && (int)(i - (s - o)) < rows - 2; i++) {
        printf("\r\n");
        drawentry(&(l[i]), (bool)(i == s));
    }

    drawstatusline(&(l[s]), n, s, m, o);
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

    geteditor();
    getshell();
    getopener();
    maketrashdir();

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

    if (trashdir[0]) {
        atexit(emptytrash);
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
    size_t newdcount = 0;
    size_t dcount = 0;

    struct deletedfile* delstack = NULL;

    struct view {
        char* wd;
        const char* eprefix;
        const char* emsg;
        bool errorshown;
        size_t selection;
        size_t pos;
        size_t lastsel;
        size_t lastpos;
        size_t marks;
    } views[VIEW_COUNT];

    for (int i = 0; i < VIEW_COUNT; i++) {
        views[i] = (struct view){
            .wd = NULL,
            .errorshown = false,
            .selection = 0,
            .pos = 0,
            .lastsel = 0,
            .lastpos = 0,
            .marks = 0,
        };
    }

    for (int i = 0; i < VIEW_COUNT; i++) {
        views[i].wd = malloc(PATH_MAX);
        if (!views[i].wd) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        strcpy(views[i].wd, wd);
    }

    free(wd);

    // selected view
    int _view = 0;
    struct view* view = views;

    if (!trashdir[0]) {
        view->errorshown = true;
        view->eprefix = "Warning";
        view->emsg = "Trash dir not available";
    }

    int k, pk, status;
    char tmpbuf[PATH_MAX];
    while (1) {
        if (update) {
            update = false;
            status = listdir(view->wd, &list, &listsize, &newdcount, showhidden);
            if (0 != status) {
                parentdir(view->wd);
                view->errorshown = true;
                view->eprefix = "Error";
                view->emsg = strerror(errno);
                view->selection = view->lastsel;
                view->pos = view->lastpos;
                redraw = true;
                continue;
            }
            if (!newdcount) {
                view->pos = 0;
                view->selection = 0;
            } else {
                // lock to bottom if deleted file at top
                if (newdcount < dcount) {
                    if (view->pos == 0 && view->selection > 0) {
                        if (dcount - view->selection == rows - 2) {
                            view->selection--;
                        }
                    }
                }
                while (view->selection >= newdcount) {
                    if (view->selection) {
                        view->selection--;
                        if (view->pos) {
                            view->pos--;
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
            drawscreen(view->wd, list, dcount, view->selection, view->pos, view->marks, _view);
            if (view->errorshown) {
                drawstatuslineerror(view->eprefix, view->emsg, view->pos);
            }
            printf("\033[%zu;1H", view->pos+2);
            fflush(stdout);
        }

        k = getkey();
        switch(k) {
            case 'h':
                if (parentdir(view->wd)) {
                    view->errorshown = false;
                    view->pos = 0;
                    view->selection = 0;
                    update = true;
                }
                break;
            case '\033':
            case 'q':
                exit(EXIT_SUCCESS);
                break;
            case '.':
                showhidden = !showhidden;
                view->selection = 0;
                view->pos = 0;
                update = true;
                break;
            case 'r':
                update = true;
                break;
            case 's':
                if (shell[0]) {
                    execcmd(view->wd, shell, NULL);
                    update = true;
                }
                break;
#if INSTANCE_COUNT > 1
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
                if (k - '1' < VIEW_COUNT) {
                    _view = k - '1';
                    view = &(views[_view]);
                    update = true;
                }
                break;
            case '`':
                _view--;
                if (_view < 0) {
                    _view = VIEW_COUNT - 1;
                }
                view = &(views[_view]);
                update = true;
                break;
            case '\t':
                _view = (_view + 1) % VIEW_COUNT;
                view = &(views[_view]);
                update = true;
                break;
#endif
            case 'u':
                if (trashdir[0] && delstack != NULL) {
                    do {
                        snprintf(tmpbuf, PATH_MAX, "%s/%d", trashdir, delstack->id);
                        if (0 != rename(tmpbuf, delstack->original)) {
                            view->eprefix = "Error undoing";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                        } else {
                            delstack = freedeleted(delstack);
                        }
                    } while (delstack && delstack->mass);
                    update = 1;
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
                if (view->selection < dcount - 1) {
                    view->errorshown = false;
                    drawentry(&(list[view->selection]), false);
                    view->selection++;
                    printf("\n");
                    drawentry(&(list[view->selection]), true);
                    if (view->pos < rows - 3) {
                        view->pos++;
                    }
                    drawstatusline(&(list[view->selection]), dcount, view->selection, view->marks, view->pos);
                }
                break;
            case 'k':
                if (view->selection > 0) {
                    view->errorshown = false;
                    drawentry(&(list[view->selection]), false);
                    view->selection--;
                    if (view->pos > 0) {
                        view->pos--;
                        printf("\r\033[A");
                    } else {
                        printf("\r\033[L");
                    }
                    drawentry(&(list[view->selection]), true);
                    drawstatusline(&(list[view->selection]), dcount, view->selection, view->marks, view->pos);
                }
                break;
            case 'g':
                if (pk != 'g') {
                    break;
                }
                view->errorshown = false;
                view->pos = 0;
                view->selection = 0;
                redraw = true;
                pk = 0;
                break;
            case 'G':
                view->selection = dcount - 1;
                if (dcount > rows - 2) {
                    view->pos = rows - 3;
                } else {
                    view->pos = view->selection;
                }
                view->errorshown = false;
                redraw = true;
                break;
#ifndef ENTER_OPEN
            case '\n':
#endif
            case 'l':
                if (E_DIR(list[view->selection].type)) {
                    if (view->wd[1] != '\0') {
                        strcat(view->wd, "/");
                    }
                    strncat(view->wd, list[view->selection].name, PATH_MAX - strlen(view->wd) - 2);
                    view->lastsel = view->selection;
                    view->lastpos = view->pos;
                    view->selection = 0;
                    view->pos = 0;
                    update = true;
                } else {
                    if (editor[0]) {
                        execcmd(view->wd, editor, list[view->selection].name);
                        update = true;
                    }
                }
                view->errorshown = false;
                break;
#ifdef ENTER_OPEN
            case '\n':
#endif
            case 'o':
                if (E_DIR(list[view->selection].type)) {
                    if (view->wd[1] != '\0') {
                        strcat(view->wd, "/");
                    }
                    strncat(view->wd, list[view->selection].name, PATH_MAX - strlen(view->wd) - 2);
                    view->lastsel = view->selection;
                    view->lastpos = view->pos;
                    view->selection = 0;
                    view->pos = 0;
                    update = true;
                    break;
                } else if (opener[0]) {
                    if (opener[0]) {
                        execcmd(view->wd, opener, list[view->selection].name);
                        update = true;
                    }
                }
                view->errorshown = false;
                break;
            case 'd':
                if (pk != 'd') {
                    break;
                }
                if (trashdir[0]) {
                    if (NULL == delstack) {
                        delstack = newdeleted(false);
                    } else {
                        struct deletedfile* d = newdeleted(false);
                        d->prev = delstack;
                        delstack = d;
                    }

                    snprintf(delstack->original, PATH_MAX, "%s/%s", view->wd, list[view->selection].name);
                    
                    snprintf(tmpbuf, PATH_MAX, "%s/%d", trashdir, delstack->id);
                    if (0 != rename(delstack->original, tmpbuf)) {
                        view->eprefix = "Error deleting";
                        view->emsg = strerror(errno);
                        view->errorshown = true;
                        delstack = freedeleted(delstack);
                    } else {
                        if (list[view->selection].marked) {
                            view->marks--;
                        }
                    }
                } else {
                    snprintf(tmpbuf, PATH_MAX, "%s/%s", view->wd, list[view->selection].name);
                    if (E_DIR(list[view->selection].type)) {
                        if (0 != deldir(list[view->selection].name)) {
                            view->eprefix = "Error deleting";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                        }
                    } else {
                        if (0 != remove(tmpbuf)) {
                            view->eprefix = "Error deleting";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                        } else {
                            if (list[view->selection].marked) {
                                view->marks--;
                            }
                        }
                    }
                }
                update = true;
                pk = 0;
                break;
            case 'D':
                if (!view->marks) {
                    break;
                }
                for (int i = 0; i < dcount; i++) {
                    if (list[i].marked) {
                        if (trashdir[0]) {
                            if (NULL == delstack) {
                                delstack = newdeleted(true);
                            } else {
                                struct deletedfile* d = newdeleted(true);
                                d->prev = delstack;
                                delstack = d;
                            }

                            snprintf(delstack->original, PATH_MAX, "%s/%s", view->wd, list[i].name);

                            snprintf(tmpbuf, PATH_MAX, "%s/%d", trashdir, delstack->id);
                            if (0 != rename(delstack->original, tmpbuf)) {
                                view->eprefix = "Error deleting";
                                view->emsg = strerror(errno);
                                view->errorshown = true;
                                delstack = freedeleted(delstack);
                            } else {
                                view->marks--;
                            }
                        } else {
                            snprintf(tmpbuf, PATH_MAX, "%s/%s", view->wd, list[i].name);
                            if (E_DIR(list[view->selection].type)) {
                                if (0 != deldir(list[view->selection].name)) {
                                    view->eprefix = "Error deleting";
                                    view->emsg = strerror(errno);
                                    view->errorshown = true;
                                }
                            } else {
                                if (0 != remove(tmpbuf)) {
                                    view->eprefix = "Error deleting";
                                    view->emsg = strerror(errno);
                                    view->errorshown = true;
                                } else {
                                    view->marks--;
                                }
                            }
                        }
                    }
                }
                update = true;
                break;
            case 'e':
                if (editor[0]) {
                    execcmd(view->wd, editor, list[view->selection].name);
                    update = true;
                }
                break;
            case 'm':
                list[view->selection].marked = !(list[view->selection].marked);
                if (list[view->selection].marked) {
                    view->marks++;
                } else {
                    view->marks--;
                }
                drawstatusline(&(list[view->selection]), dcount, view->selection, view->marks, view->pos);
                drawentry(&(list[view->selection]), true);
                break;
        }

        pk = k;
        fflush(stdout);
    }

    exit(EXIT_SUCCESS);
}
