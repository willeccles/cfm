#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "input.h"
#include "term.h"

static int getkey(void) {
    char c[6];

    if (!isinteractive()) {
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
