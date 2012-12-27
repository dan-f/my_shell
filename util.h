#ifndef UTIL_H
#define UTIL_H

#include <sys/types.h>

/* Tokenize a buffer of data.
 * The buffer is split into tokens by whitespace. The pointers to the tokens
 * are returned in an array that is provided by the caller. The pointers all
 * point into the buffer, so the pointers have the same scope as the buffer.
 *
 * Parameters:
 *   buffer, a pointer to the buffer, terminated by a 0 byte.
 *   argv, output array of pointers to each token.
 *   maxargs, the maximum number of arguments argv can hold.
 * Output:
 *   Returns the number of arguments found in the buffer.
 *   The argv array is modified to point to each token.
 *   The end of the used portion of argv is set to NULL.
 */
int tokenize(char *buffer, char *argv[], int maxargs);

/* Spawn a child process.
 * Parameters:
 *   progname, name of program to run
 *   argv, array of arguments.
 *     First element should be the name of the program to run.
 *     Last element must be NULL.
 *   child_stdin, file descriptor to be provided to child as stdin
 *   child_stdout, file descriptor to be provided to child as stdout
 *   child_stderr, file descriptor to be provided to child as stderr
 * Output:
 *   Returns the PID of the child or -1 if fork() returned an error.
 * Error handling:
 *   If fork() returns an error, -1 is returned.
 *   For errors which happen in the child process, an error message is
 *   printed to stderr (which could be the stderr of the parent or of the child)
 *   and the child exits with a non-zero return value.
 */
pid_t run_child(char *progname, char *argv[], int child_stdin, int child_stdout, int child_stderr);

#endif /* UTIL_H */
