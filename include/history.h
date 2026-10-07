#ifndef HISTORY_H
#define HISTORY_H

/*
 * Shellforge command history support.
 *
 * The actual history storage is provided by GNU Readline.
 * This header exposes the Shellforge-specific history interface.
 */

void print_history(void);

#endif /* HISTORY_H */
