#ifndef CFM_UTIL_H_
#define CFM_UTIL_H_

#include <stddef.h>

/*
 * A more natural version of strcmp.
 */
extern int cfm_strcmp(const char* s1, const char* s2);

/*
 * A function to compare list elements with qsort().
 */
extern int cfm_elemcmp(const void* a, const void* b);

/*
 * Executes a command.
 */
extern void execcmd(const char* path, const char* cmd, const char* arg);

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
extern int readfname(char* out, const char* initialstr);

extern char* homesubstwd(char* wd, char* home, size_t homelen);

#endif /* CFM_UTIL_H_ */
