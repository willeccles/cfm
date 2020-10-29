#ifndef CFM_UTIL_H_
#define CFM_UTIL_H_

/*
 * A more natural version of strcmp.
 */
static int cfm_strcmp(const char* s1, const char* s2);

/*
 * A function to compare list elements with qsort().
 */
static int cfm_elemcmp(const void* a, const void* b);

#endif /* CFM_UTIL_H_ */
