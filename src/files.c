#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#include "files.h"
#include "hashtable.h"
#include "types.h"
#include "util.h"

#define LIST_ALLOC_SIZE 64

static cfm_hashtable file_table;

static int deldir(const char* dir);
static int rmFiles(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb);
static int cpfile_inner(const char* src, const char* dst);

int cfm_delete(const char* path) {
    struct stat fst;
    if (0 != lstat(path, &fst)) {
        return -1;
    }

    if (S_ISDIR(fst.st_mode)) {
        return deldir(path);
    } else {
        return unlink(path);
    }
}

int cfm_file_exists(const char* path) {
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

char* cfm_basename(const char* path) {
    char* r = strrchr(path, '/');
    if (r) {
        r++;
    }
    return r;
}

int cfm_parentdir(char* path) {
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

void cfm_getcwd(char* buf, size_t size) {
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

int cfm_cp(const char* src, const char* dst) {
    int s = cpfile_inner(src, dst);
    cfm_reset_hashtable(&file_table);
    return s;
}

int cfm_listdir(const char* path, struct cfm_listelem** list, size_t* listsize,
        size_t* rcount, bool hidden) {
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
        qsort(*list, count, sizeof(**list), cfm_elemcmp);
    } else {
        return -1;
    }

    *rcount = count;
    return 0;
}

static int deldir(const char* dir) {
    return nftw(dir, rmFiles, 512, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
}

static int rmFiles(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb) {
    (void)sbuf; (void)type; (void)ftwb;
    return remove(pathname);
}

static int cpfile_inner(const char* src, const char* dst) {
    char* b = cfm_basename(src);
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
    if (cfm_is_hashed(&file_table, &srcstat)) {
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
        cfm_hash_file(&file_table, &newst);

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
        cfm_hash_file(&file_table, &newst);

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
