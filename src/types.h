#ifndef CFM_TYPES_H_
#define CFM_TYPES_H_

#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/types.h>

/*
 * Types of files.
 */
enum cfm_elemtype {
    ELEM_DIR,
    ELEM_LINK,
    ELEM_DIRLINK,
    ELEM_EXEC,
    ELEM_FILE,
};

/*
 * Strings representing the types of files.
 */
static const char* elemtypestrings[] = {
    [ELEM_DIR]     = "dir",
    [ELEM_LINK]    = "@file",
    [ELEM_DIRLINK] = "@dir",
    [ELEM_EXEC]    = "exec",
    [ELEM_FILE]    = "file",
};

/*
 * Element in a file listing./
 */
struct cfm_listelem {
    enum cfm_elemtype type;
    // TODO should probably replace NAME_MAX with something more reliable
    char name[NAME_MAX + 1];
    bool marked;
};

/*
 * Macro to determine whether or not a file is a directory OR a link to a
 * directory (argument is 'enum cfm_elemtype').
 */
#define E_DIR(t) ((t) == ELEM_DIR || (t) == ELEM_DIRLINK)

/* TODO: create a generic linked list implementation to use for the following types */

/*
 * Represents a deleted file to be used in a linked list of deleted files.
 *
 * TODO: move to another file
 */
struct cfm_deletedfile {
    char* original;
    int id;
    bool mass;
    int massid;
    struct cfm_deletedfile* prev;
};

/*
 * Represents a saved position for a view (i.e. when coming out of a
 * directory). Can be chained.
 *
 * TODO: move to another file
 */
struct cfm_savedpos {
    size_t pos, sel; // position and selection
    struct cfm_savedpos* prev;
};

/*
 * Represents a file in the hash table.
 *
 * TODO: move to another file
 */
struct cfm_hashedfile {
    ino_t ino;
    dev_t dev;
    struct cfm_hashedfile* next;
    bool isdir;
};

#endif /* CFM_TYPES_H_ */
