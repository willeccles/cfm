/* vim: set ts=2 et sw=2 tw=80 fo=tjcroqln1 : */
/* vim: set cino=h1,l1,g1,t0,i4,+4,(0,w1,W4,E-s : */

/* Copyright (c) Will Eccles <will@eccles.dev>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifdef __APPLE__
# define _DARWIN_C_SOURCE
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
# define __BSD_VISIBLE 1
#endif

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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// include user config
#include "config.h"

#ifdef __GNUC__
# define UNUSED(x) UNUSED_##x __attribute__((unused))
#else
# define UNUSED(x) UNUSED_##x
#endif

#if 0
#define K_ESC '\033'
#define ESC_UP 'A'
#define ESC_DOWN 'B'
#define ESC_LEFT 'D'
#define ESC_RIGHT 'C'
#define K_ALT(k) ((int)(k) | (int)0xFFFFFF00)

// arbitrary values for keys
#define KEY_PGUP 'K'
#define KEY_PGDN 'J'
#endif

#if 0
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

#ifndef INVERT_FULL_SELECTION
# define INVERT_FULL_SELECTION 1
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
# define MARK_SYMBOL '^'
#endif

#ifndef ALLOW_SPACES
# define ALLOW_SPACES 1
#endif

#ifndef ABBREVIATE_HOME
# define ABBREVIATE_HOME 1
#endif

#ifdef VIEW_COUNT
# if VIEW_COUNT > 10
#  undef VIEW_COUNT
#  define VIEW_COUNT 10
# elif VIEW_COUNT <= 0
#  undef VIEW_COUNT
#  define VIEW_COUNT 1
# endif
#else
# define VIEW_COUNT 2
#endif
#endif

#define COPYFLAGS (COPYFILE_ALL | COPYFILE_EXCL | COPYFILE_NOFOLLOW | COPYFILE_RECURSIVE)

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
    char name[NAME_MAX+1];
    bool marked;
};

#define E_DIR(t) ((t)==ELEM_DIR || (t)==ELEM_DIRLINK)

static int del_id = 0;
static int mdel_id = 0;

struct deletedfile {
    char* original;
    int id;
    bool mass;
    int massid;
    struct deletedfile* prev;
};

struct savedpos {
    size_t pos, sel;
    struct savedpos* prev;
};

static struct termios old_term;
static atomic_bool redraw = false;
static atomic_bool resize = false;
static int rows, cols;
static int pointerwidth = 2;
static char editor[PATH_MAX+1];
static char opener[PATH_MAX+1];
static char shell[PATH_MAX+1];
static char tmpdir[PATH_MAX+1];
static char cdonclosefile[PATH_MAX+1];

static atomic_bool interactive = true;

/*
 * Hash table for files created by the copy function.
 * The source for this was heavily inspired by libbb since I am too
 * lazy to make my own entirely from scratch.
 */
#define HASH_SIZE 311U // prime number
#define hashfn(ino) ((unsigned)(ino) % HASH_SIZE)

struct hashed_file {
    ino_t ino;
    dev_t dev;
    struct hashed_file* next;
    bool isdir;
};

#if 0
static struct hashed_file** file_table;
#endif

/*
 * Returns whether or not a file is in the hash table.
 */
#if 0
static bool is_file_hashed(const struct stat* st) {
    if (!file_table) {
        return false;
    }
    struct hashed_file* elem = file_table[hashfn(st->st_ino)];
    while (elem != NULL) {
        if (elem->ino == st->st_ino
                && elem->dev == st->st_dev
                && elem->isdir == !!S_ISDIR(st->st_mode)) {
            return true;
        }
        elem = elem->next;
    }
    return false;
}
#endif

/*
 * Hashes a file and adds it to the hash table.
 */
#if 0
static void add_file_hash(const struct stat* st) {
    struct hashed_file* elem = malloc(sizeof(struct hashed_file));
    elem->ino = st->st_ino;
    elem->dev = st->st_dev;
    elem->isdir = !!S_ISDIR(st->st_mode);
    if (!file_table) {
        file_table = calloc(HASH_SIZE, sizeof(*file_table));
    }
    int i = hashfn(st->st_ino);
    elem->next = file_table[i];
    file_table[i] = elem;
}
#endif

/*
 * Cleans up the file table.
 */
#if 0
static void reset_file_table(void) {
    if (!file_table) {
        return;
    }

    struct hashed_file* elem;
    struct hashed_file* next;
    for (size_t i = 0; i < HASH_SIZE; i++) {
        elem = file_table[i];
        while (elem != NULL) {
            next = elem->next;
            free(elem);
            elem = next;
        }
    }
    free(file_table);
    file_table = NULL;
}
#endif

/*
 * Unlinks a file. Used for deldir.
 */
#if 0
static int rmFiles(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb) {
    (void)sbuf; (void)type; (void)ftwb;
    return remove(pathname);
}
#endif

/*
 * Deletes a directory, even if it contains files.
 */
#if 0
static int deldir(const char* dir) {
    return nftw(dir, rmFiles, 512, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
}
#endif

/*
 * If the path pointed to by f is a file, unlinks the file.
 * Else, deletes the directory.
 *
 * Returns 0 on success.
 */
#if 0
static int del(const char* f) {
    struct stat fst;
    if (0 != lstat(f, &fst)) {
        return -1;
    }

    if (S_ISDIR(fst.st_mode)) {
        return deldir(f);
    } else {
        return unlink(f);
    }
}
#endif

/*
 * Stats a file and returns 1 if it exists or 0 if
 * it doesn't exist. It returns -1 if there was an error.
 */
#if 0
static int exists(const char* path) {
    struct stat fst;
    if (0 != lstat(path, &fst)) {
        if (errno == ENOENT) {
            return 0;
        } else {
            return -1;
        }
    } else {
        return 1;
    }
}
#endif

/*
 * Get the base name of a file.
 * This is the same thing as running `basename x/y/z` at
 * the command line.
 */
#if 0
static char* basename(const char* path) {
    char* r = strrchr(path, '/');
    if (r) {
        r++;
    }
    return r;
}
#endif

/*
 * Gets the working directory the same way as getcwd,
 * but checks $PWD first. If it is valid, then it will be
 * stored in buf, else getcwd will be called. This fixes
 * paths with one or more symlinks in them.
 *
 * This is a *little bit* identical to the GNU extension,
 * get_current_dir_name().
 */
#if 0
static void getrealcwd(char* buf, size_t size) {
    char* pwd;
    pwd = getenv("PWD");
    struct stat dotstat, pwdstat;
    if (pwd != NULL
            && stat(".", &dotstat) == 0
            && stat(pwd, &pwdstat) == 0
            && pwdstat.st_dev == dotstat.st_dev
            && pwdstat.st_ino == dotstat.st_ino) {
        strncpy(buf, pwd, size);
    } else {
        (void)getcwd(buf, size);
    }
}
#endif

/*
 * This is the internal portion. Use the cpfile function instead.
 * Copy file or directory recursively.
 * Returns 0 on success, -1 on error.
 */
#if 0
static int cpfile_inner(const char* src, const char* dst) {
    char* b = basename(src);
    if (b && (0 == strcmp(b, ".") || 0 == strcmp(b, ".."))) {
        return 0;
    }

    struct stat srcstat, dststat;
    bool dstexist = false;
    int s = 0;

    if (lstat(src, &srcstat) < 0) {
        // couldn't stat source
        return -1;
    }

    // if we already created this file we don't want to create it again
    if (is_file_hashed(&srcstat)) {
        return 0;
    }

    if (lstat(dst, &dststat) < 0) {
        // couldn't stat target
        if (errno != ENOENT) {
            return -1;
        }
    } else {
        if (srcstat.st_dev == dststat.st_dev && srcstat.st_ino == dststat.st_ino) {
            // same file
            return -1;
        }
        dstexist = true;
    }

    if (dstexist) {
        return -1;
    }

    if (S_ISDIR(srcstat.st_mode)) {
        mode_t smask = umask(0);
        mode_t mode = srcstat.st_mode & ~smask;
        mode |= S_IRWXU;
        if (mkdir(dst, mode) < 0) {
            umask(smask);
            return -1;
        }
        umask(smask);

        struct stat newst;
        if (lstat(dst, &newst) < 0) {
            return -1;
        }
        add_file_hash(&newst);

        DIR* d = opendir(src);
        if (d == NULL) {
            s = -1;
            goto preserve;
        }

        struct dirent* de;
        while ((de = readdir(d)) != NULL) {
            char *ns = malloc(PATH_MAX);
            if (ns == NULL) {
                s = -1;
            }

            char *nd = malloc(PATH_MAX);
            if (nd == NULL) {
                free(ns);
                s = -1;
            }

            snprintf(ns, PATH_MAX, "%s/%s", src, de->d_name);
            snprintf(nd, PATH_MAX, "%s/%s", dst, de->d_name);
            if (cpfile_inner(ns, nd) != 0) {
                s = -1;
            }

            if (ns) free(ns);
            if (nd) free(nd);
        }

        closedir(d);

        chmod(dst, srcstat.st_mode & ~smask);
        goto preserve;
    }

    if (S_ISREG(srcstat.st_mode)) {
        int sfd, dfd;
        mode_t nmode;

        if (S_ISLNK(srcstat.st_mode)) {
            goto notreg;
        }

        sfd = open(src, O_RDONLY);
        if (sfd == -1) {
            return -1;
        }

        nmode = srcstat.st_mode;
        if (!S_ISREG(srcstat.st_mode)) {
            nmode = 0666;
        }

        dfd = open(dst, O_WRONLY|O_CREAT|O_EXCL, nmode);
        if (dfd == -1) {
            close(sfd);
            return -1;
        }

        struct stat newst;
        if (fstat(dfd, &newst) < 0) {
            close(dfd);
            return -1;
        }
        add_file_hash(&newst);

        // copy the file
        char cbuf[4096] = {0};
        while (1) {
            ssize_t r, w;
            r = read(sfd, cbuf, 4096);
            if (!r) {
                break;
            }

            if (r < 0) {
                s = -1;
                break;
            }

            w = write(dfd, cbuf, r);
            if (w < r) {
                s = -1;
                break;
            }
        }

        if (close(dfd) < 0) {
            return -1;
        }
        close(sfd);

        if (!S_ISREG(srcstat.st_mode)) {
            return s;
        }

        goto preserve;
    }

notreg:
    {
        // source is not a regular file (it's a symlink or special)
        if (S_ISLNK(srcstat.st_mode)) {
            char lbuf[PATH_MAX+1] = {0};
            ssize_t ls = readlink(src, lbuf, PATH_MAX);
            if (ls == -1) {
                return -1;
            }
            lbuf[ls] = '\0';

            int r = symlink(lbuf, dst);
            if (r != 0) {
                return -1;
            }

            // can't preserve stuff for symlinks
            return 0;
        } else if (S_ISBLK(srcstat.st_mode) || S_ISCHR(srcstat.st_mode)
                || S_ISSOCK(srcstat.st_mode) || S_ISFIFO(srcstat.st_mode)) {
            if (mknod(dst, srcstat.st_mode, srcstat.st_rdev) < 0) {
                return -1;
            }
        } else {
            return -1;
        }
    }

preserve:
    {
        // preserve mode, owner, attributes, etc. here
        struct timeval t[2];
        t[1].tv_sec = t[0].tv_sec = srcstat.st_mtime;
        t[1].tv_usec = t[0].tv_usec = 0;

        // we will fail silently if any of these don't work
        utimes(dst, t);
        (void)chown(dst, srcstat.st_uid, srcstat.st_gid);
        chmod(dst, srcstat.st_mode);

        return s;
    }
}
#endif

/*
 * Copies a file or directory. Returns 0 on success and -1 on failure.
 */
#if 0
static int cpfile(const char* src, const char* dst) {
    int s = cpfile_inner(src, dst);
    reset_file_table();
    return s;
}
#endif

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
    if (d->mass) {
        d->massid = mdel_id;
    }
    d->prev = NULL;

    return d;
}

static struct deletedfile* freedeleted(struct deletedfile* f) {
    struct deletedfile* d = f->prev;
    free(f->original);
    free(f);
    return d;
}

#if 0
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
#endif

/*
 * Comparison function for list elements for qsort.
 */
#if 0
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
#endif

/*
 * Get editor.
 */
#if 0
static void geteditor(void) {
#ifdef EDITOR
    strncpy(editor, EDITOR, PATH_MAX);
#else
    const char* res = getenv("CFM_EDITOR");
    if (!res) {
        res = getenv("EDITOR");
        if (!res) {
            editor[0] = '\0';
        } else {
            strncpy(editor, res, PATH_MAX);
        }
    } else {
        strncpy(editor, res, PATH_MAX);
    }
#endif /* EDITOR */
}
#endif

/*
 * Get shell.
 */
#if 0
static void getshell(void) {
#ifdef SHELL
    strncpy(shell, SHELL, PATH_MAX);
#else
    const char* res = getenv("CFM_SHELL");
    if (!res) {
        res = getenv("SHELL");
        if (!res) {
            shell[0] = '\0';
        } else {
            strncpy(shell, res, PATH_MAX);
        }
    } else {
        strncpy(shell, res, PATH_MAX);
    }
#endif /* SHELL */
}
#endif

/*
 * Get the opener program.
 */
#if 0
static void getopener(void) {
#ifdef OPENER
    strncpy(opener, OPENER, PATH_MAX);
#else
    const char* res = getenv("CFM_OPENER");
    if (!res) {
        res = getenv("OPENER");
        if (!res) {
            opener[0] = '\0';
        } else {
            strncpy(opener, res, PATH_MAX);
        }
    } else {
        strncpy(opener, res, PATH_MAX);
    }
#endif
}
#endif

/*
 * Get the tmp directory.
 */
static void maketmpdir(void) {
#ifdef TMP_DIR
    strncpy(tmpdir, TMP_DIR, PATH_MAX);
#else
    const char* res = getenv("CFM_TMP");
    if (res) {
        strncpy(tmpdir, res, PATH_MAX);
    } else {
        strncpy(tmpdir, "/tmp/cfmtmp", PATH_MAX);
    }
#endif
    if (mkdir(tmpdir, 0751)) {
        if (errno == EEXIST) {
            return;
        } else {
            tmpdir[0] = '\0';
        }
    }
}

static void rmtmp(void) {
    if (tmpdir[0]) {
        if (0 != deldir(tmpdir)) {
            perror("rmtmp: deldir");
        }
    }
}

/*
 * Write current working directory to CD_ON_CLOSE file.
 * Creates the file if it doesn't exist; does not report errors.
 */
static void cdonclose(const char* wd) {
    // since we assume that rmpwdfile() has already been called before this,
    // cdonclosefile should have a valid path already if it's set
    if (*cdonclosefile) {
        FILE* outfile = fopen(cdonclosefile, "w");
        if (outfile) {
            fprintf(outfile, "%s\n", wd);
            fclose(outfile);
        }
    }
}

/*
 * Remove existing pwd file (CD_ON_CLOSE).
 * Does not report errors.
 */
static void rmpwdfile(void) {
#ifdef CD_ON_CLOSE
    if (NULL == realpath(CD_ON_CLOSE, cdonclosefile)) {
        *cdonclosefile = 0;
    }
#else
    const char* cdocf = getenv("CFM_CD_ON_CLOSE");
    if (cdocf) {
        strncpy(cdonclosefile, cdocf, PATH_MAX);
    }
#endif
    if (*cdonclosefile) {
        del(cdonclosefile);
    }
}

/*
 * Save the default terminal settings.
 * Returns 0 on success.
 */
#if 0
static int backupterm(void) {
    if (!interactive) return 0;
    if (tcgetattr(STDIN_FILENO, &old_term) < 0) {
        perror("tcgetattr");
        return 1;
    }
    return 0;
}
#endif

/*
 * Get the size of the terminal.
 * Returns 0 on success.
 */
#if 0
static int termsize(void) {
    if (!interactive) return 0;
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0) {
        perror("ioctl");
        return 1;
    }

    rows = ws.ws_row;
    cols = ws.ws_col;

    return 0;
}
#endif

/*
 * Sets up the terminal for TUI.
 * Return 0 on success.
 */
#if 0
static int setupterm(void) {
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
            rows-1);

    return 0;
}
#endif

/*
 * Resets the terminal to how it was before we ruined it.
 */
#if 0
static void resetterm(void) {
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
#endif

/*
 * Creates a child process.
 */
#if 0
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
#endif

/*
 * Reads a directory into the list, returning the number of items in the dir.
 * This will return 0 on success.
 * On failure, 'opendir' will have set 'errno'.
 */
#if 0
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
#endif

/*
 * Get a filename from the user and store it in 'out'.
 * out must point to a buffer capable of containing
 * at least NAME_MAX bytes.
 *
 * initialstr is the text to write into the file before
 * the user edits it.
 *
 * Returns 0 on success, else:
 *   -1 = no editor
 *   -2 = other error (check errno)
 *   -3 = invalid filename entered
 *
 * If an error occurs but the data in 'out'
 * is still usable, it will be there. Else, out will
 * be empty.
 */
#if 0
static int readfname(char* out, const char* initialstr) {
    if (editor[0]) {
        char template[] = "/tmp/cfmtmp.XXXXXXXXXX";
        int fd;
        if (-1 == (fd = mkstemp(template))) {
            return -2;
        }

        int rval = 0;

        if (initialstr) {
            if (-1 == write(fd, initialstr, strlen(initialstr))) {
                rval = -2;
            } else {
                if (-1 == lseek(fd, 0, SEEK_SET)) {
                    rval = -2;
                }
            }
        }

        if (rval == 0) {
            execcmd("/tmp/", editor, template);

            ssize_t c;
            memset(out, 0, NAME_MAX);
            if (-1 == (c = read(fd, out, NAME_MAX - 1))) {
                rval = -2;
                out[0] = '\0';
            } else {
                if (out[0] == '\0') {
                    rval = -3;
                }
                char* nl = strchr(out, '\n');
                if (nl != NULL) {
                    *nl = '\0';
                }

                rval = 0;

                // validate the string
                // only allow POSIX portable paths and spaces if enabled
                // which is to say A-Za-z0-9._-
                for (char* x = out; *x; x++) {
                    if (!(isalnum(*x) || *x == '.' || *x == '_' || *x == '-'
#if ALLOW_SPACES
                                || *x == ' '
#endif
                         )) {
                        rval = -3;
                        out[0] = '\0';
                        break;
                    }
                }
            }
        }

        unlink(template);
        close(fd);
        return rval;
    } else {
        return -1;
    }
}
#endif

/*
 * Get a key. Wraps getchar() and returns hjkl instead of arrow keys.
 * Also, returns
 */
#if 0
static int getkey(void) {
    char c[6];

    if (!interactive) {
        ssize_t n = read(STDIN_FILENO, c, 1);
        if (n == 0) {
            return 'q';
        }
        return *c;
    }

    ssize_t n = read(STDIN_FILENO, c, 6);
    if (n <= 0) {
        return -1;
    }

    if (n == 2 && c[0] == '\033' && isalpha(c[1])) {
        return K_ALT(c[1]);
    }

    if (n < 3) {
        return c[0];
    }

    if (n == 3) {
        switch (c[2]) {
            case ESC_UP:
                return 'k';
            case ESC_DOWN:
                return 'j';
            case ESC_RIGHT:
                return 'l';
            case ESC_LEFT:
                return 'h';
        }
    } else if (n == 4) {
        if (!strncmp(c+2, "5~", 2)) {
            return KEY_PGUP;
        }
        if (!strncmp(c+2, "6~", 2)) {
            return KEY_PGDN;
        }
    } else if (n == 6) {
        // shift-up
        if (!strncmp(c+2, "1;2A", 4)) {
            return KEY_PGUP;
        }
        // shift-down
        if (!strncmp(c+2, "1;2B", 4)) {
            return KEY_PGDN;
        }
    }

    return -1;
}
#endif

/*
 * Draws one element to the screen.
 */
#if 0
static void drawentry(struct listelem* e, bool selected) {
    if (!interactive) return;
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
        printf(" %s%-*s", e->name, cols, E_DIR(e->type) ? "/" : "");
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
#endif

/*
 * Draws the status line at the bottom of the screen.
 */
#if 0
static void drawstatusline(struct listelem* l, size_t n, size_t s, size_t m, size_t p) {
    if (!interactive) return;
    printf("\033[%d;H" // go to the bottom row
            //"\033[2K" // clear the row
            "\033[37;7;1m", // inverse + bold
            rows);

    int count;
    if (!m) {
        count = printf(" %zu/%zu", n ? s+1 : n, n);
    } else {
        count = printf(" %zu/%zu (%zu marked)", n ? s+1 : n, n, m);
    }
    // print the type of the file
    printf("%*s \r", cols-count-1, elemtypestrings[l->type]);
    printf("\033[m\n\033[%zu;H", p+2); // move cursor back and reset formatting
}
#endif

/*
 * Draws the statusline with an error message in it.
 */
#if 0
static void drawstatuslineerror(const char* prefix, const char* error, size_t p) {
    if (!interactive) {
        // instead print to stderr
        fprintf(stderr, "%s: %s\n", prefix, error);
        exit(EXIT_FAILURE);
        return;
    }

    printf("\033[%d;H"
            //"\033[2K"
            "\033[31;7;1m",
            rows);
    int count = printf(" %s: ", prefix);
    printf("%-*s \r", cols-count-1, error);
    printf("\033[m\033[%zu;H", p+2);
}
#endif

/*
 * Draws the whole screen (redraw).
 * Use sparingly.
 */
#if 0
static void drawscreen(char* wd, struct listelem* l, size_t n, size_t s, size_t o, size_t m, int v) {
    if (!interactive) return;

    // clear the screen except for the top and bottom lines
    // this gets rid of the flashing when redrawing
    for (int i = 2; i < rows; i++) {
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

    printf("%-*s", (int)(cols - count), (wd[1] == '\0') ? "" : "/");

    printf("\033[m"); // reset formatting

    for (size_t i = s - o; i < n && (int)(i - (s - o)) < rows - 2; i++) {
        printf("\r\n");
        drawentry(&(l[i]), (bool)(i == s));
    }

    drawstatusline(&(l[s]), n, s, m, o);
}
#endif

/*
 * Writes back the parent directory of a path.
 * Returns 1 if the path was changed, 0 if not (i.e. if
 * the path was "/").
 */
#if 0
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
#endif

/*
 * Returns a pointer to a string with the working directory after replacing
 * a leading $HOME with ~, or the original wd if none was found.
 */
#if 0
static char* homesubstwd(char* wd, char* home, size_t homelen) {
#if ABBREVIATE_HOME
    static char subbedpwd[PATH_MAX+1] = {0};
    if (wd && !strncmp(wd, home, homelen)) {
        snprintf(subbedpwd, PATH_MAX+1, "~%s", wd + homelen);
        return subbedpwd;
    }
#else
    (void)home;
    (void)homelen;
#endif
    return wd;
}
#endif

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
    resize = true;
    redraw = true;
}

/*
 * Signal handler for SIGTSTP, which is ^Z from the shell.
 */
static void sigtstp(int UNUSED(sig)) {
    resetterm();
    kill(getpid(), SIGSTOP);
}

/*
 * Signal handler for SIGCONT which is used when using fg in the shell.
 */
static void sigcont(int UNUSED(sig)) {
    backupterm();
    setupterm();
    resize = true;
    redraw = true;
}

int main(int argc, char** argv) {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        interactive = false;
    }

    char* wd = malloc(PATH_MAX);
    memset(wd, 0, PATH_MAX);

    if (argc == 1) {
        getrealcwd(wd, PATH_MAX);
    } else {
        if (NULL == realpath(argv[1], wd)) {
            exit(EXIT_FAILURE);
        }
    }

    pointerwidth = strlen(POINTER);

    geteditor();
    getshell();
    getopener();
    maketmpdir();
    rmpwdfile();

    char* userhome = getenv("HOME");
    size_t homelen = strlen(userhome);

    if (termsize()) {
        exit(EXIT_FAILURE);
    }

    struct sigaction sa_resize = {
        .sa_handler = sigresize,
    };
    if (sigaction(SIGWINCH, &sa_resize, NULL) < 0) {
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

    struct sigaction sa_tstp = {
        .sa_handler = sigtstp,
    };
    if (sigaction(SIGTSTP, &sa_tstp, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa_cont = {
        .sa_handler = sigcont,
    };
    if (sigaction(SIGCONT, &sa_cont, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (backupterm()) {
        exit(EXIT_FAILURE);
    }

    if (setupterm()) {
        exit(EXIT_FAILURE);
    }

    if (tmpdir[0]) {
        atexit(rmtmp);
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
        size_t marks;
        struct savedpos* backstack;
    } views[VIEW_COUNT];

    for (int i = 0; i < VIEW_COUNT; i++) {
        views[i] = (struct view){ NULL, NULL, NULL, false, 0, 0, 0, NULL };
    }

    for (int i = 0; i < VIEW_COUNT; i++) {
        views[i].wd = malloc(PATH_MAX+1);
        if (!views[i].wd) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        strncpy(views[i].wd, wd, PATH_MAX);
    }

    free(wd);

    // selected view
    int _view = 0;
    struct view* view = views;

    if (!tmpdir[0]) {
        view->errorshown = true;
        view->eprefix = "Warning";
        view->emsg = "Trash dir not available";
    }

    int k = -1, pk = -1, status;
    char tmpbuf[PATH_MAX+1] = {0};
    char tmpbuf2[PATH_MAX+1] = {0};
    char tmpnam[NAME_MAX+1] = {0};
    char lastname[NAME_MAX+1] = {0};
    char yankbuf[PATH_MAX+1] = {0};
    char cutbuf[NAME_MAX+1] = {0};
    bool hasyanked = false;
    bool hascut = false;
    int cutid = -1;
    while (1&&1) {
        if (update) {
            update = false;
            status = listdir(view->wd, &list, &listsize, &newdcount, showhidden);
            if (0 != status) {
                parentdir(view->wd);
                view->errorshown = true;
                view->eprefix = "Error";
                view->emsg = strerror(errno);
                if (view->backstack) {
                    view->pos = view->backstack->pos;
                    view->selection = view->backstack->sel;
                    struct savedpos* s = view->backstack;
                    view->backstack = s->prev;
                    free(s);
                }
                update = true;
                continue;
            }
            if (!newdcount) {
                view->pos = 0;
                view->selection = 0;
            } else {
                // lock to bottom if deleted file at top
                if (newdcount < dcount) {
                    if (view->pos == 0 && view->selection > 0) {
                        if (dcount - view->selection == (size_t)rows - 2) {
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
                if (view->pos == 0 && view->selection == 0 && lastname[0]) {
                    for (size_t i = 0; i < newdcount; i++) {
                        if (0 == strcmp(lastname, list[i].name)) {
                            view->selection = i;
                            view->pos = (i > (size_t)rows - 2) ? (size_t)rows/2 : i;
                            break;
                        }
                    }
                    lastname[0] = 0;
                }
            }
            dcount = newdcount;
            redraw = true;
        }

        if (redraw && interactive) {
            redraw = false;
            // only get the current terminal size if we resized
            if (resize) {
                if (termsize()) {
                    exit(EXIT_FAILURE);
                }

                // set new scroll region
                printf("\033[2;%dr", rows-1);

                // if our current item is outside the bounds of the screen, we need to move it up
                if (view->pos >= (size_t)rows - 2) {
                    view->pos = rows - 3;
                }

                // TODO: if the bottom of the screen is visible (or would become visible),
                // we should pin it to the bottom and now allow blank space to show up at
                // the bottom of the screen
                // this may require us to store the old rows value before calling termsize()

                resize = false;
            }
            drawscreen(homesubstwd(view->wd, userhome, homelen), list, dcount, view->selection, view->pos, view->marks, _view);
            if (view->errorshown) {
                drawstatuslineerror(view->eprefix, view->emsg, view->pos);
            }
            printf("\033[%zu;1H", view->pos+2);
            fflush(stdout);
        }

        k = getkey();
        switch(k) {
            case 'h':
#if 0
                {
                    char* bn = basename(view->wd);
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
                }
#endif
                break;
            case '\033':
                if (view->marks > 0) {
                    view->marks = 0;
                    update = true;
                    break;
                } // fallthrough
            case 'q':
                exit(EXIT_SUCCESS);
                break;
            case 'Q':
                cdonclose(view->wd);
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
            case 'S':
                if (shell[0]) {
                    execcmd(view->wd, shell, NULL);
                    update = true;
                }
                break;
#if VIEW_COUNT > 1
            case '0':
                k = '9' + 1;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
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
                if (tmpdir[0] && delstack != NULL) {
                    int did = 0;
                    do {
                        did = delstack->massid;
                        snprintf(tmpbuf, PATH_MAX, "%s/%d", tmpdir, delstack->id);
                        if (0 != cpfile(tmpbuf, delstack->original)) {
                            view->eprefix = "Error undoing";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                        } else {
                            // fail silently here
                            unlink(tmpbuf);
                            delstack = freedeleted(delstack);
                        }
                    } while (delstack && delstack->mass && delstack->massid == did);
                    update = 1;
                }
                break;
            case 'T':
                status = readfname(tmpnam, "new file name");
                switch (status) {
                    case -1:
                        view->eprefix = "Error";
                        view->emsg = "No editor available";
                        view->errorshown = true;
                        break;
                    case -2:
                        if (tmpnam[0] == '\0') {
                            view->eprefix = "Error";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                        } else {
                            view->eprefix = "Warning";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                            status = 0;
                        }
                        break;
                    case -3:
                        view->eprefix = "Error";
                        view->emsg = "Invalid file name";
                        view->errorshown = true;
                        break;
                }

                if (status == 0) {
                    snprintf(tmpbuf, PATH_MAX, "%s/%s", view->wd, tmpnam);
                    // use fopen instead of testing with 'exists' here because
                    // this way the file will be created if the file doesn't
                    // exist
                    FILE* f = fopen(tmpbuf, "wx");
                    if (!f) {
                        if (errno == EEXIST) {
                            view->eprefix = "Error";
                            view->emsg = "File already exists";
                            view->errorshown = true;
                        } else {
                            view->eprefix = "Error";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                        }
                    } else {
                        fclose(f);
                        strncpy(lastname, tmpnam, NAME_MAX);
                        view->pos = 0;
                        view->selection = 0;
                    }
                }
                update = true;
                break;
            case 'M':
                status = readfname(tmpnam, "new directory name");
                switch (status) {
                    case -1:
                        view->eprefix = "Error";
                        view->emsg = "No editor available";
                        view->errorshown = true;
                        break;
                    case -2:
                        if (tmpnam[0] == '\0') {
                            view->eprefix = "Error";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                        } else {
                            view->eprefix = "Warning";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                            status = 0;
                        }
                        break;
                    case -3:
                        view->eprefix = "Error";
                        view->emsg = "Invalid directory name";
                        view->errorshown = true;
                        break;
                }
                snprintf(tmpbuf, PATH_MAX, "%s/%s", view->wd, tmpnam);
                if (-1 == mkdir(tmpbuf, 0751)) {
                    if (errno == EEXIST) {
                        view->eprefix = "Error";
                        view->emsg = "Directory already exists";
                        view->errorshown = true;
                    } else {
                        view->eprefix = "Error";
                        view->emsg = strerror(errno);
                        view->errorshown = true;
                    }
                }
                update = true;
                break;
            case 'p':
                if (hasyanked) {
                    strncpy(tmpbuf, yankbuf, PATH_MAX);
                    snprintf(tmpbuf2, PATH_MAX, "%s/%s", view->wd, basename(yankbuf));
                } else if (hascut) {
                    snprintf(tmpbuf, PATH_MAX, "%s/%d", tmpdir, cutid);
                    snprintf(tmpbuf2, PATH_MAX, "%s/%s", view->wd, cutbuf);
                }
                bool didpaste = true;
                do {
                    if (!didpaste) {
                        if (hasyanked) {
                            status = readfname(tmpnam, basename(yankbuf));
                        } else if (hascut) {
                            status = readfname(tmpnam, cutbuf);
                        }
                        switch (status) {
                            case -1:
                                view->eprefix = "Error";
                                view->emsg = "No editor available";
                                view->errorshown = true;
                                break;
                            case -2:
                                if (tmpnam[0] == '\0') {
                                    view->eprefix = "Error";
                                    view->emsg = strerror(errno);
                                    view->errorshown = true;
                                } else {
                                    view->eprefix = "Warning";
                                    view->emsg = strerror(errno);
                                    view->errorshown = true;
                                    status = 0;
                                }
                                break;
                            case -3:
                                view->eprefix = "Error";
                                view->emsg = "Invalid file name";
                                view->errorshown = true;
                                break;
                        }

                        if (status == 0) {
                            snprintf(tmpbuf2, PATH_MAX, "%s/%s", view->wd, tmpnam);
                        } else {
                            goto outofloop;
                        }
                    }
                    int s = exists(tmpbuf2);
                    if (s == 0) {
                        if (0 != cpfile(tmpbuf, tmpbuf2)) {
                            view->eprefix = "Error";
                            view->emsg = "Could not copy files";
                            view->errorshown = true;
                        }
                        didpaste = true;
                    } else if (s == -1) {
                        view->eprefix = "Error";
                        view->emsg = "Could not stat target file";
                        view->errorshown = true;
                        goto outofloop;
                    } else if (s == 1) {
                        didpaste = false;
                    }
                } while (!didpaste);
outofloop:
                update = true;
                break;
        }

        if (!dcount) {
            pk = k;
            fflush(stdout);
            continue;
        }

        switch (k) {
            case 'j':
#if 0
                if (view->selection < dcount - 1) {
                    view->errorshown = false;
                    drawentry(&(list[view->selection]), false);
                    view->selection++;
                    printf("\n");
                    drawentry(&(list[view->selection]), true);
                    if (view->pos < (size_t)rows - 3) {
                        view->pos++;
                    }
                    drawstatusline(&(list[view->selection]), dcount, view->selection, view->marks, view->pos);
                }
#endif
                break;
            case 'k':
#if 0
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
#endif
                break;
            case KEY_PGDN:
#if 0
                // don't do anything if we are too low to page down
                if ((size_t)rows - 2 + view->selection - view->pos < dcount) {
                    // 1. move the view down so the last item is now the top item
                    // 2. select that one
                    // 3. if we are within view of the bottom
                    view->selection += (size_t)rows - 2 - view->pos - 1;
                    view->pos = 0;
                    view->errorshown = false;
                    redraw = true;
                }
#endif
                break;
            case KEY_PGUP:
#if 0
                // do nothing if we are in the top "page"
                if (view->pos != view->selection) {
                    // 1. move view up so that the top item is now the last item
                    // 2. select that one
                    // 3. if we are within view of the top, don't go up
                    if ((size_t)rows - 2 > view->selection - view->pos) {
                        view->selection = rows - 3;
                    } else {
                        view->selection -= view->pos;
                    }
                    view->pos = rows - 3;
                    view->errorshown = false;
                    redraw = true;
                }
#endif
                break;
            case 'g':
                if (pk != 'g') {
                    break;
                }
#if 0
                view->errorshown = false;
                if (view->pos != view->selection) {
                    view->pos = 0;
                    view->selection = 0;
                    redraw = true;
                } else {
                    drawentry(&(list[view->selection]), false);
                    view->pos = 0;
                    view->selection = 0;
                    printf("\033[%zu;1H", view->pos+2);
                    drawentry(&(list[view->selection]), true);
                    drawstatusline(&(list[view->selection]), dcount, view->selection, view->marks, view->pos);
                }
#endif
                break;
            case 'G':
#if 0
                view->selection = dcount - 1;
                if (dcount > (size_t)rows - 2) {
                    view->pos = rows - 3;
                } else {
                    view->pos = view->selection;
                }
                view->errorshown = false;
                redraw = true;
#endif
                break;
#ifndef ENTER_OPEN
            case '\n':
#endif
            case 'l':
#if 0
                if (E_DIR(list[view->selection].type)) {
                    struct savedpos* sp = malloc(sizeof(struct savedpos));
                    sp->pos = view->pos;
                    sp->sel = view->selection;
                    sp->prev = view->backstack;
                    view->backstack = sp;
                    if (view->wd[1] != '\0') {
                        strcat(view->wd, "/");
                    }
                    strncat(view->wd, list[view->selection].name, PATH_MAX - strlen(view->wd) - 2);
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
#endif
                break;
#ifdef ENTER_OPEN
            case '\n':
#endif
            case 'o':
                if (E_DIR(list[view->selection].type)) {
                    struct savedpos* sp = malloc(sizeof(struct savedpos));
                    sp->pos = view->pos;
                    sp->sel = view->selection;
                    sp->prev = view->backstack;
                    view->backstack = sp;
                    if (view->wd[1] != '\0') {
                        strcat(view->wd, "/");
                    }
                    strncat(view->wd, list[view->selection].name, PATH_MAX - strlen(view->wd) - 2);
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
            case K_ALT('d'):
            case 'd':
                if (pk != k) {
                    break;
                }
                if (interactive && k == 'd' && tmpdir[0]) {
                    if (NULL == delstack) {
                        delstack = newdeleted(false);
                    } else {
                        struct deletedfile* d = newdeleted(false);
                        d->prev = delstack;
                        delstack = d;
                    }

                    snprintf(delstack->original, PATH_MAX, "%s/%s", view->wd, list[view->selection].name);

                    snprintf(tmpbuf, PATH_MAX, "%s/%d", tmpdir, delstack->id);
                    if (0 != cpfile(delstack->original, tmpbuf)) {
                        view->eprefix = "Error deleting";
                        view->emsg = strerror(errno);
                        view->errorshown = true;
                        delstack = freedeleted(delstack);
                    } else {
                        if (0 != del(delstack->original)) {
                            view->eprefix = "Error deleting";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                            delstack = freedeleted(delstack);
                        } else {
                            if (list[view->selection].marked) {
                                view->marks--;
                            }
                        }
                    }
                } else {
                    snprintf(tmpbuf, PATH_MAX, "%s/%s", view->wd, list[view->selection].name);
                    if (0 != del(tmpbuf)) {
                        view->eprefix = "Error deleting";
                        view->emsg = strerror(errno);
                        view->errorshown = true;
                    } else {
                        if (list[view->selection].marked) {
                            view->marks--;
                        }
                    }
                }
                update = true;
                break;
            case 'D':
                if (!view->marks) {
                    break;
                }
                for (size_t i = 0; i < dcount; i++) {
                    if (list[i].marked) {
                        if (tmpdir[0]) {
                            if (NULL == delstack) {
                                delstack = newdeleted(true);
                            } else {
                                struct deletedfile* d = newdeleted(true);
                                d->prev = delstack;
                                delstack = d;
                            }

                            snprintf(delstack->original, PATH_MAX, "%s/%s", view->wd, list[i].name);

                            snprintf(tmpbuf, PATH_MAX, "%s/%d", tmpdir, delstack->id);
                            if (0 != cpfile(delstack->original, tmpbuf)) {
                                view->eprefix = "Error copying for deletion";
                                view->emsg = strerror(errno);
                                view->errorshown = true;
                                delstack = freedeleted(delstack);
                            } else {
                                if (0 != del(delstack->original)) {
                                    view->eprefix = "Error deleting";
                                    view->emsg = strerror(errno);
                                    view->errorshown = true;
                                    delstack = freedeleted(delstack);
                                } else {
                                    view->marks--;
                                }
                            }
                        } else {
                            snprintf(tmpbuf, PATH_MAX, "%s/%s", view->wd, list[i].name);
                            if (0 != del(tmpbuf)) {
                                view->eprefix = "Error deleting";
                                view->emsg = strerror(errno);
                                view->errorshown = true;
                            } else {
                                view->marks--;
                            }
                        }
                    }
                }
                if (tmpdir[0]) {
                    mdel_id++;
                }
                update = true;
                break;
            case 'e':
                if (editor[0]) {
                    execcmd(view->wd, editor, list[view->selection].name);
                    update = true;
                }
                break;
            case ' ':
            case 'm':
#if 0
                list[view->selection].marked = !(list[view->selection].marked);
                if (list[view->selection].marked) {
                    view->marks++;
                } else {
                    view->marks--;
                }
                drawstatusline(&(list[view->selection]), dcount, view->selection, view->marks, view->pos);
                drawentry(&(list[view->selection]), true);
#endif
                break;
            case 'R':
                status = readfname(tmpnam, list[view->selection].name);
                switch (status) {
                    case -1:
                        view->eprefix = "Error";
                        view->emsg = "No editor available";
                        view->errorshown = true;
                        break;
                    case -2:
                        if (tmpnam[0] == '\0') {
                            view->eprefix = "Error";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                        } else {
                            view->eprefix = "Warning";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                            status = 0;
                        }
                        break;
                    case -3:
                        view->eprefix = "Error";
                        view->emsg = "Invalid file name";
                        view->errorshown = true;
                        break;
                }

                if (status == 0) {
                    snprintf(tmpbuf, PATH_MAX, "%s/%s", view->wd, tmpnam);
                    int s = exists(tmpbuf);
                    if (s == 1) {
                        view->eprefix = "Error";
                        view->emsg = "Target file already exists";
                        view->errorshown = true;
                    } else if (s == 0) {
                        // the target file does not exist
                        snprintf(tmpbuf2, PATH_MAX, "%s/%s", view->wd, list[view->selection].name);
                        if (-1 == rename(tmpbuf2, tmpbuf)) {
                            view->eprefix = "Error";
                            view->emsg = strerror(errno);
                            view->errorshown = true;
                        } else {
                            // go find the new file and select it
                            strncpy(lastname, tmpnam, NAME_MAX);
                            view->pos = 0;
                            view->selection = 0;
                        }
                    } else if (s == -1) {
                        view->eprefix = "Error";
                        view->emsg = strerror(errno);
                        view->errorshown = true;
                    }
                }
                update = true;
                break;
            case 'y':
                if (pk != 'y') {
                    break;
                }
                snprintf(yankbuf, PATH_MAX, "%s/%s", view->wd, list[view->selection].name);
                hasyanked = true;
                break;
            case 'X':
                if (tmpbuf[0]) {
                    snprintf(tmpbuf, PATH_MAX, "%s/%s", view->wd, list[view->selection].name);
                    snprintf(cutbuf, NAME_MAX, "%s", list[view->selection].name);
                    snprintf(tmpbuf2, PATH_MAX, "%s/%d", tmpdir, (cutid = del_id++));
                    if (0 != cpfile(tmpbuf, tmpbuf2)) {
                        view->eprefix = "Error";
                        view->emsg = strerror(errno);
                        view->errorshown = true;
                    } else {
                        if (0 != del(tmpbuf)) {
                            view->eprefix = "Error";
                            view->emsg = "Couldn't delete original file";
                            view->errorshown = true;
                        } else {
                            if (list[view->selection].marked) {
                                view->marks--;
                            }
                        }
                        hasyanked = false;
                        hascut = true;
                    }
                } else {
                    view->eprefix = "Error";
                    view->emsg = "No tmp dir, cannot cut!";
                    view->errorshown = true;
                }
                update = true;
                break;
            case '~':
                if (userhome) {
                    strncpy(view->wd, userhome, PATH_MAX);
                    view->pos = 0;
                    view->selection = 0;
                    update = true;
                }
                break;
            case '/':
                view->wd[0] = '/';
                view->wd[1] = '\0';
                view->pos = 0;
                view->selection = 0;
                update = true;
                break;
        }

        if (pk != k) {
            pk = k;
        } else {
            pk = 0;
        }
        fflush(stdout);
    }

    exit(EXIT_SUCCESS);
}
