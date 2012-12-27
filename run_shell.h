/* Daniel Friedman
 * Fall 2012
 *
 * run_shell.h
 *
 * Contains function prototypes and struct definition for run_shell.c
 */
#ifndef _RUN_SHELL_H_
#define _RUN_SHELL_H_

/* Represents a command (that may be part of a pipe sequence) */
struct command {
    char **argv;
    int argc;
};

/* Get rid of compile warnings */
int gethostname (char *name, size_t len);
int kill (pid_t pid, int signo);

int handle_line (char *line);
int start_prog (int pipeno, int numpipes, char *progname, int argc, char *argv[], int fd_in, int fd_out );
void close_pipe (int fd);
void free_commands (struct command *cmds, int len);
static void sigtstp_handler (int signo);

#endif /* _RUN_SHELL_H_ */
