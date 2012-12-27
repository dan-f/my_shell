#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "run_shell.h"
#include "util.h"

/* Max. length of input */
#define MAXLEN 1024

/* Daniel Friedman
 * Fall 2012
 *
 * My implementation of a shell. Command-line input is grabbed with readline,
 * and parsed by handle_line. handle_line parses input into 'command chunks',
 * which each represent a single command (with arguments) in the pipeline.
 * Piping is then handled in handle_line. Commands are executed in start_prog,
 * which tests for syntax errors, calls run_child, and waits on the child if
 * necessary.
 *
 */

/* The pid of the currently running child
 * If there is no child, child_pid == -127
 */
pid_t child_pid = -127;

/* Main loop for input -- prompts the user for a command.
 *   If the command is "exit", the program is exited,
 *   otherwise the line is sent to handle_line() which makes
 *   calls to tokenize and attempt to run the appropriate command.
 *
 *   If handle_line() returns non-zero, the user tries again.
 */
int main(int argc, char *argv[])
{
    /* Set up our signal handler - from Love p. 288 */
    /* handles SIGTSTP (Ctrl-z)                     */
    if (signal(SIGTSTP, sigtstp_handler) == SIG_ERR) {
        fprintf(stderr, "Can't handle SIGINT\n");
        exit(EXIT_FAILURE);
    }

    int status;         /* Exit status of child */
    int ret;            /* Return value of waitpid */
    char hostname[128]; /* Host names on OCCS aren't nearly 128 chars long */
    char prompt[MAXLEN];
    if (gethostname (hostname, 128) < 0)
        strcpy(hostname, "oberlin-cs");
    snprintf(prompt, MAXLEN, "[%s @ %s] ", getenv("USER"), hostname);

    char *line;

    for (;;) {
        /* Wait on background processes */
        ret = waitpid(-1, &status, WNOHANG);
        if (ret < 0 && errno != ECHILD) /* It's OK to have no child process */
            perror("run_shell: main");
        else if (ret > 0)
            printf("Child %d exited with status %d\n", ret, status);

        /* Display prompt and get next command */
        if ((line = readline(prompt))) {
            add_history(line); // Add the line to our history in readline
            if (!strcmp(line, "\0")) // check for empty string with readline
                continue;
            else if (!strcmp(line, "exit"))
                break;
            if (handle_line(line))
                fprintf(stderr, "run_shell: syntax error\n");
        } else {
            if (errno && (errno != ECHILD)) {
                if (errno == EINTR) {
                    printf("POOPFUCKSHITS!\n");
                }
                perror("run_shell: main");
                exit(EXIT_FAILURE);
            } else
                /* EOF */
                break;
        }
        free(line);
    }
    return 0;
}

/* Attemtps to run a line of code from main()
 *
 * Parameters:
 *   line, character pointer to the user's input, terminated with a newline and
 *   end of string character.
 * Output:
 *   Returns 0 on success, 1 if syntax error occurred
 * Error handling:
 *   Returns 1 in case of syntax error.
 *   If open() returns -1, an error is printed. If the error is ENOENT (file not
 *   found), then the program is exited. Otherwise 1 is returned (syntax error).
 *   If pipe() returns -1, an error is printed, and the program is exited.
 *   If start_prog returns 1, a syntax error has occurred, and we return 1 to
 *   main()
 */
int handle_line(char *line)
{
    /* Find out how many 'command chunks' we have for our data structure;
     * Command chunks are sections of input separated by pipes
     */
    int nchunks = 1;
    int i;
    for (i = 0; i < strlen(line); i++) {
        if (line[i] == '|' && (i != 0 && i != strlen(line) - 1))
            nchunks++;
        else if (line[i] == '|' && (i == 0 || i == strlen(line) - 1))
            /* Syntax error */
            return 1;
    }

    /**************************************
     * Construct array of command structs *
     **************************************/

    struct command *commands = malloc(nchunks * sizeof(struct command));
    int cur_chunk;
    for (cur_chunk = 0; cur_chunk < nchunks; cur_chunk++) {
        char *chunk;  /* Buffer of chars for the current chunk */
        if (cur_chunk == 0)
            chunk = strtok(line, "|");
        else
            chunk = strtok(NULL, "|");

        /* Count the number of tokens in chunk */
        char *ptr = chunk;
        char last = ' ';
        int toks = 1;
        while (*ptr) {
            if (isspace(*ptr) && !isspace(last))
                toks++;
            last = *ptr;
            ptr++;
        }
        if (isspace(last))
            toks--;

        /* Initialize our command struct for the current chunk */
        commands[cur_chunk].argv = malloc((toks + 1) * sizeof(char *));
        commands[cur_chunk].argc = toks;

        /* Tokenize the chunk */
        tokenize(chunk, commands[cur_chunk].argv, commands[cur_chunk].argc);
    }

    /******************************
     * Run, and pipe if necessary *
     ******************************/

    if (nchunks == 1) {
        /* Run only command */
        int ret = start_prog(0, 1, commands[0].argv[0], commands[0].argc, commands[0].argv, 0, 1);
        free_commands(commands, nchunks);
        free(commands);
        return ret;
    }

    int p1[2];   /* Pipe for parent */
    int p2[2];   /* Pipe for child  */

    if (pipe(p1))
        perror("run_shell: handle_line");

    /* Get started from stdin */
    if (start_prog(0, nchunks, commands[0].argv[0], commands[0].argc, commands[0].argv, 0, p1[1])) {
	free_commands(commands, nchunks);
	free(commands);
        return 1;
    }
    close_pipe(p1[1]);

    for (i = 1; i < nchunks - 1; i++) {
        /* Read from parent's pipe, write to child's */
        /* Close the pipe we read from, and the one we write to */
        if (i % 2) {
            if (pipe(p2))
                perror("run_shell: handle_line");
            if (start_prog(i, nchunks, commands[i].argv[0], commands[i].argc, commands[i].argv, p1[0], p2[1])) {
		free_commands(commands, nchunks);
		free(commands);
                return 1;
	    }
            close_pipe(p1[0]);
            close_pipe(p2[1]);
        } else {
            if (pipe(p1))
                perror("run_shell: handle_line");
            if (start_prog(i, nchunks, commands[i].argv[0], commands[i].argc, commands[i].argv, p2[0], p1[1])) {
		free_commands(commands, nchunks);
		free(commands);
                return 1;
	    }
            close_pipe(p2[0]);
            close_pipe(p1[1]);
        }
    }

    /* Finish on stdout */
    if (i % 2) {
        if (start_prog(i, nchunks, commands[i].argv[0], commands[i].argc, commands[i].argv, p1[0], 1)) {
	    free_commands(commands, nchunks);
	    free(commands);
            return 1;
	}
        close_pipe(p1[0]);
    } else {
        if (start_prog(i, nchunks, commands[i].argv[0], commands[i].argc, commands[i].argv, p2[0], 1)) {
	    free_commands(commands, nchunks);
	    free(commands);
            return 1;
	}
        close_pipe(p2[0]);
    }

    /* Free every command struct in commands */
    free_commands(commands, nchunks);
    free(commands);

    return 0;
}

/* Calls run_child, and waits on child if necessary
 *
 * Parameters:
 *   pipeno, number of command in pipe sequence (starting from 0)
 *   progname, name of program to run
 *   argv, array of arguments. First is program name, last is NULL
 *   fd_in, file descriptor for child's stdin
 *   fd_out, file descriptor for child's stdout
 * Input:
 * Output:
 *   0 on success
 *   1 if syntax error
 * Error handling:
 *   Prints out a message if the progname is not found on the path (run_child()
 *   returns -1).
 *   If waitpid returns with an error (-1), a message is printed, and the program is
 *   exited.
 */
int start_prog(int pipeno, int numpipes, char *progname, int argc, char *argv[], int fd_in, int fd_out)
{
    /* Check for syntax errors */
    if (argc == 0)
        return 1;

    /* Find out if there's File I/O */
    int flags;                    /* Flags for open() */ 
    int mode = S_IRUSR | S_IWUSR; /* Mode for open() */
    int i;
    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], ">") || !strcmp(argv[i], "<")) {
            if (numpipes > 1) {
                /* Check syntax error -- redirection where innappropriate */
                if (!strcmp(argv[i], ">") && pipeno + 1 < numpipes)
                    return 1;
                if (!strcmp(argv[i], "<") && pipeno != 0)
                    return 1;
            }
            /* Check syntax error -- no file given */
            if (i >= argc)
                return 1;

            flags = (!strcmp(argv[i], ">")) ? O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY;
            int fd_tmp = open(argv[i+1], flags, mode);
            if (fd_tmp < 0) {
                perror("run_shell: start_prog");
                if (errno != ENOENT) /* Consider ENOENT (file not found) a syntax error */
                    exit(EXIT_FAILURE);
                return 1;
            }
            fd_in = (!strcmp(argv[i], "<")) ? fd_tmp : fd_in;
            fd_out = (!strcmp(argv[i], ">")) ? fd_tmp : fd_out;
            argv[i] = NULL;
        }
    }

    /* Run in background if requested */
    if (!strcmp(argv[argc-1], "&")) {
        argv[argc-1] = NULL;
        if ((child_pid = run_child(progname, argv, fd_in, fd_out, 2)) < 0) {
            /* error */
            perror("Command not found on the path");
            child_pid = -127;
        }
    } else {
        child_pid = run_child(progname, argv, fd_in, fd_out, 2);
        if (child_pid < 0) {
            /* run_child returned error */
            perror("Command not found on the path");
        } else {
            int status;
            if (waitpid(child_pid, &status, 0) < 0) {
                /* Error -- check if control-c was sent */
                if (errno == EINTR) {
                    printf("Exiting process %d\n", child_pid);
                    return 0;
                }
                perror("run_shell: handle_line");
                exit(EXIT_FAILURE);
            }
            child_pid = -127;
        }
    }

    return 0;
}

/* Closes a file descriptor.
 * Input:
 *   file descriptor
 * Error Handling:
 *   If EINTR is returned by close, tries until success or worse error
 *   If EBADF or EIO are returned by close, perror is called and the program is
 *   terminated
 */
void close_pipe(int fd)
{
    int ret;
    while ((ret = close(fd))) {
        if (ret == EBADF || ret == EIO) {
            perror("run_shell: close_pipe");
            exit(EXIT_FAILURE);
        }
    }
}

/* Frees all command structs in an array
 * Input:
 *   Command struct array to free.
 *   Length of the array.
 */
void free_commands(struct command *cmds, int len)
{
    int i;
    for (i = 0; i < len; i++) {
	free(cmds[i].argv);
    }
}

/* Signal handler for SIGTSTP
 */
static void sigtstp_handler(int signo)
{
    if (child_pid > 0) {
        kill(child_pid, signo);
        child_pid = -127;
    }
}
