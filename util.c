#include <sys/resource.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "util.h"

#define RC_CHECK(s) if(!(s)) run_child_error();

void run_child_error();

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
int tokenize(char *buffer, char *argv[], int maxargs)
{ int n=0; /* Number of elements used in argv */

  while(*buffer && isspace(*buffer))
    /* Skip initial whitespace */
    buffer++;

  /* Invariant: The buffer is empty or the first byte is not space */
  while(*buffer && n<maxargs)
  { argv[n++]=buffer; /* Add the token to argv */

    while(*buffer && !isspace(*buffer))
      /* Skip token */
      buffer++;

    while(*buffer && isspace(*buffer))
      /* Mark end of token */
      *(buffer++)=0;
  }
  
  argv[n]=NULL; /* Mark the end of the array with NULL */
  return n;
}

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
pid_t run_child(char *progname, char *argv[], int child_stdin, int child_stdout, int child_stderr)
{ pid_t child;
  struct rlimit lim;
  int max;

  if((child=fork()))
  { /* error or parent */
    return child;
  }

  /******************************
   * Child                      *
   *                            *
   * 1. Set up file descriptors *
   ******************************/
  
  /* First, duplicate the provided file descriptors to 0, 1, 2 */
  if(child_stdout==STDIN_FILENO)
  { /* Move stdout out of the way of stdin */
    child_stdout=dup(child_stdout);
    RC_CHECK(child_stdout>=0);
  }

  while(child_stderr==STDIN_FILENO || child_stderr==STDOUT_FILENO)
  { /* Move stderr out of the way of stdin/stdout */
    child_stderr=dup(child_stderr);
    RC_CHECK(child_stderr>=0);
  }

  child_stdin=dup2(child_stdin,STDIN_FILENO);  /* Force stdin onto 0 */
  RC_CHECK(child_stdin==STDIN_FILENO);
  child_stdout=dup2(child_stdout,STDOUT_FILENO);  /* Force stdout onto 1 */
  RC_CHECK(child_stdout==STDOUT_FILENO);
  child_stderr=dup2(child_stderr,STDERR_FILENO);  /* Force stderr onto 2 */
  RC_CHECK(child_stderr==STDERR_FILENO);

  RC_CHECK(getrlimit(RLIMIT_NOFILE,&lim)>=0); /* Get max fileno */
  max=(int)lim.rlim_cur;

  /* Now, ask the kernel to close all other open files when we exec */
  for(int fd=STDERR_FILENO+1; fd<max; fd++)
  { int code=fcntl(fd,F_GETFD);

    if(!code&FD_CLOEXEC) /* if the file is not open, FD_CLOEXEC looks set */
    { if(fcntl(fd,F_SETFD,code|FD_CLOEXEC)<0)
        perror("run_child"); /* Report errors, but proceed */
    }
  }

  /**************
   * 2. Execute *
   **************/

  execvp(progname,argv);
  
  /*******************************
   * 3. Only gets here on error! *
   *******************************/

  run_child_error();
  exit(1); /* Never happens, but make compiler happy */
}

/* Run perror and exit */
void run_child_error()
{ perror("run_child");

  if(errno&255)
    exit(errno); /* Normally reasonable */
  else
    exit(1); /* Always return someting negative */
}
