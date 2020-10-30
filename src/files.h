#ifndef CFM_FILES_H_
#define CFM_FILES_H_

#include <stdbool.h>
#include <stddef.h>

#include "types.h"

/*
 * Deletes the file or directory (recursively) at 'path'.
 * Returns 0 on success.
 */
extern int cfm_delete(const char* path);

/*
 * Checks whether or not a file exists.
 * Returns 1 if the file exists, 0 if not, and -1 on error.
 */
extern int cfm_file_exists(const char* path);

/*
 * Returns the basename of a given path.
 */
extern char* cfm_basename(const char* path);

/*
 * Gets the working directory the same way as getcwd(), but checks $PWD first.
 * If $PWD is valid, it will be stored in buf; otherwise getcwd() will be
 * called. This fixes the case where the pwd has a symlink in it, where
 * getcwd() would resolve the symlink which is undesirable.
 */
extern void cfm_getcwd(char* buf, size_t size);

/*
 * Copies a file or directory. Returns 0 on success and -1 on failure.
 */
extern int cfm_cp(const char* src, const char* dst);

/*
 * Lists the entries in the directory specified by 'path', populating 'list'.
 *
 * 'listsize' is an output used to store the total size of the list, which
 * should not be changed outside of this function.
 * 'rcount' is the actual number of elements in the list (may be less than or
 * equal to 'listsize')
 * 'hidden' determines whether or not hidden files (.dotfiles) should be listed
 *
 * Returns 0 on success, else returns. See opendir(3) for errno values set by
 * this function.
 */
extern int cfm_listdir(const char* path, struct cfm_listelem** list, size_t* listsize,
        size_t* rcount, bool hidden);

#endif /* CFM_FILES_H_ */
