#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "userconfig.h"

static const char* editor = NULL;
static const char* shell = NULL;
static const char* opener = NULL;
static const char* tmpdir = NULL;

static const char* getconfig(const char* envvar, const char* fallback,
        const char* fromconfig, const char* defaultval);

void cfm_initconfig(void) {
    editor = getconfig("CFM_EDITOR", "EDITOR", EDITOR, NULL);
    shell = getconfig("CFM_SHELL", "SHELL", SHELL, NULL);
    opener = getconfig("CFM_OPENER", "OPENER", OPENER, NULL);
    tmpdir = getconfig("CFM_TMP", NULL, TMP_DIR, "/tmp/cfmtmp");
}

const char* cfm_geteditor(void) {
    return editor;
}

const char* cfm_getshell(void) {
    return shell;
}

const char* cfm_getopener(void) {
    return opener;
}

const char* cfm_gettmpdir(void) {
    return tmpdir;
}

/*
 * Get a configuration option.
 */
static const char* getconfig(const char* envvar, const char* fallback,
        const char* fromconfig, const char* defaultval) {
    if (fromconfig) {
        return fromconfig;
    } else {
        const char* res = getenv(envvar);
        if (!res && fallback) {
            res = getenv(fallback);
        }
        if (!res && defaultval) {
            return defaultval;
        }
        return res;
    }
}
