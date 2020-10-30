#ifndef CFM_TERM_H_
#define CFM_TERM_H_

#include <stdbool.h>

/*
 * Backup terminal state before changing configuration.
 */
extern int backupterm(void);

/*
 * Get the size of the terminal.
 */
extern int termsize(int* rows, int* cols);

/*
 * Setup the terminal for TUI use.
 */
extern int setupterm(void);

/*
 * Reset the terminal to settings backed up in backupterm().
 */
extern void resetterm(void);

/*
 * Is this session interactive?
 */
extern bool isinteractive(void);

/*
 * Get terminal rows.
 */
extern int getrows();

/*
 * Get terminal columns.
 */
extern int getcols();

#endif
