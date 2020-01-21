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
 * Value: string
 */
//#define EDITOR "/usr/local/bin/nvim"

/* SHELL:
 * If not set, cfm will attempt to use the $SHELL
 * environment variable to open shells in a given directory.
 * Else, the specified shell will be invoked.
 *
 * Value: string
 */
//#define SHELL "/bin/bash"

/* USE_ITALICS:
 * If not set (or 0), bold will be used in place of italics.
 * Otherwise, italics will be used.
 *
 * Value: boolean (0 or 1)
 */
//#define USE_ITALICS 1

/* POINTER:
 * If not set, cfm will use "->" in front of the selected
 * item. You can set a different string here, such as ">".
 *
 * Value: string
 */
//#define POINTER "->"

#endif
