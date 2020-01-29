#ifndef CFM_CONFIG_H
#define CFM_CONFIG_H

/* 
 * Uncomment an option to enable it.
 * Comment to disable.
 * Each is accompanied with an explanation.
 */

/* EDITOR:
 * If not set, cfm will attempt to use the
 * $EDITOR environment variable when opening
 * files.
 *
 * Default: $CFM_EDITOR then $EDITOR
 * Value: string
 */
//#define EDITOR "/bin/vi"

/* SHELL:
 * If not set, cfm will attempt to use the $SHELL
 * environment variable to open shells in a given directory.
 * Else, the specified shell will be invoked.
 *
 * Default: $CFM_SHELL then $SHELL
 * Value: string
 */
//#define SHELL "/bin/bash"

/* OPENER:
 * If not set, cfm will attempt to use the $OPENER
 * environment variable to open files with an opener.
 * Otherwise, it will use the specified opener when
 * you use the 'o' key.
 *
 * Default: $CFM_OPENER then $OPENER
 * Value: string
 */
//#define OPENER "xdg-open"

/* ENTER_OPENS:
 * If not set, using enter will be the same as using
 * l or right arrow on normal files, aka they will be
 * opened with EDITOR. Otherwise, pressing enter will open
 * a file with OPENER. No-op if OPENER not defined or NULL.
 *
 * Default: 0
 * Value: boolean (1 or 0)
 */
//#define ENTER_OPENS 0

/* POINTER:
 * If not set, cfm will use "->" in front of the selected
 * item. You can set a different string here, such as ">".
 *
 * Default: "->"
 * Value: string
 */
//#define POINTER "->"

/* BOLD_POINTER:
 * If set, cfm will make the pointer bold for files
 * who appear bold, such as directories and executables.

 * Default: 1
 * Value: boolean (1 or 0)
 */
//#define BOLD_POINTER 1

/* INVERT_SELECTION:
 * If set, cfm will reverse the foreground/background colors
 * of the currently selected line.
 *
 * Default: 1
 * Value: boolean (1 or 0)
 */
//#define INVERT_SELECTION 1

/* INDENT_SELECTION:
 * If not set, all lines will be indented enough to make room
 * for the pointer, regardless of being selected or not.
 * If set, only the selected line will be indented.
 *
 * Default: 1
 * Value: boolean (1 or 0)
 */
//#define INDENT_SELECTION 0

/* MARK_SYMBOL:
 * If not set, marked items will be prefixed with a '!' symbol.
 * Else, can be a single character which will be placed at the
 * start of the line for any non-selected, marked item.
 *
 * Default: '!'
 * Value: char ('c')
 */
//#define MARK_SYMBOL '!'

/* VIEW_COUNT:
 * If not set, cfm will allow for two views by default.
 * Else, it will use this number of views. The value
 * of VIEW_COUNT must be within the range 1-5.
 *
 * Default: 2
 * Value: integer (1-5)
 */
//#define VIEW_COUNT 2

#endif
