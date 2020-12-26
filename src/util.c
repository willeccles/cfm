#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "term.h"
#include "types.h"
#include "userconfig.h"
#include "util.h"

int cfm_strcmp(const char *s1, const char *s2) {
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

int cfm_elemcmp(const void* a, const void* b) {
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

void execcmd(const char* path, const char* cmd, const char* arg) {
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

int readfname(char* out, const char* initialstr) {
  const char* editor = cfm_geteditor();
  if (editor) {
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

char* homesubstwd(char* wd, char* home, size_t homelen) {
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
