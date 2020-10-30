#ifndef CFM_USERCONFIG_H_
#define CFM_USERCONFIG_H_

#include "../config.h"

#ifndef POINTER
# define POINTER "->"
#endif

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

#ifndef EDITOR
# define EDITOR NULL
#endif

#ifndef OPENER
# define OPENER NULL
#endif

#ifndef SHELL
# define SHELL NULL
#endif

#ifndef TMP_DIR
# define TMP_DIR NULL
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

/*
 * Initialize configuration options. Call before other functions here.
 */
extern void cfm_initconfig(void);

/*
 * Get editor.
 */
extern const char* cfm_geteditor(void);

/*
 * Get shell.
 */
extern const char* cfm_getshell(void);

/*
 * Get the opener program.
 */
extern const char* cfm_getopener(void);

/*
 * Get the temporary directory.
 */
extern const char* cfm_gettmpdir(void);

#endif /* CFM_USERCONFIG_H_ */
