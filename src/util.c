#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "util.h"

static int cfm_strcmp(const char *s1, const char *s2) {
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

static int cfm_elemcmp(const void* a, const void* b) {
    const struct cfm_listelem* x = a;
    const struct cfm_listelem* y = b;

    if ((x->type == ELEM_DIR || x->type == ELEM_DIRLINK) &&
            (y->type != ELEM_DIR && y->type != ELEM_DIRLINK)) {
        return -1;
    }

    if ((y->type == ELEM_DIR || y->type == ELEM_DIRLINK) &&
            (x->type != ELEM_DIR && x->type != ELEM_DIRLINK)) {
        return 1;
    }

    return cfm_strcmp(x->name, y->name);
}
