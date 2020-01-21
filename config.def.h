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
//#define EDITOR "/usr/local/nvim"

/* SHELL:
 * If not set, cfm will attempt to use the $SHELL
 * environment variable to open shells in a given directory.
 * Else, the specified shell will be invoked.
 *
 * Value: string
 */
//#define SHELL "/bin/bash"

/* TRASH:
 * If not set, cfm will permanently remove deleted files.
 * Else, cfm will move them to the specified directory.
 * Directory must exist.
 * 
 * Value: string
 */
//#define TRASH "/home/cactus/.cfmtrash"

#endif
