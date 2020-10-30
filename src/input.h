#ifndef CFM_INPUT_H_
#define CFM_INPUT_H_

#define K_ESC '\033'
#define ESC_UP 'A'
#define ESC_DOWN 'B'
#define ESC_LEFT 'D'
#define ESC_RIGHT 'C'
#define K_ALT(k) ((int)(k) | (int)0xFFFFFF00)

// arbitrary values for keys
#define KEY_PGUP 'K'
#define KEY_PGDN 'J'

/*
 * Get a key. Wraps getchar() and returns hjkl instead of arrow keys.
 * Also, returns
 */
static int getkey(void);

#endif
