/**
 * File: subprocess.cc
 * -------------------
 * Presents the implementation of the subprocess routine.
 */

#include "subprocess.h"
using namespace std;

/**
 * Type: subprocess_t
 * ------------------
 * Bundles information related to the child process created
 * by the subprocess function below.
 * 
 *  pid: the id of the child process created by a call to subprocess
 *  supplyfd: the descriptor where one pipes text to the child's stdin (or kNotInUse if child hasn't rewired its stdin)
 *  ingestfd: the descriptor where the text a child pushes to stdout shows up (or kNotInUse if child hasn't rewired its stdout)
 *
 */

/**
 * Function: subprocess
 * --------------------
 * Creates a new process running the executable identified via argv[0].
 *
 *   argv: the NULL-terminated argument vector that should be passed to the new process's main function
 *   supplyChildInput: true if the parent process would like to pipe content to the new process's stdin, false otherwise
 *   ingestChildOutput: true if the parent would like the child's stdout to be pushed to the parent, false otheriwse
 */
subprocess_t subprocess(char *argv[], bool supplyChildInput, bool ingestChildOutput) throw (SubprocessException) {
  subprocess_t sp = {NULL, kNotInUse, kNotInUse};
  int pipe_1[2];
  int pipe_2[2];

  if ( pipe2(pipe_1, O_CLOEXEC) == -1 )
    throw SubprocessException("Error with pipe2()");
  if ( pipe2(pipe_2, O_CLOEXEC) == -1 )
    throw SubprocessException("Error with pipe2()");

  pid_t pid = fork();

  if ( pid == -1 )
    throw SubprocessException("Error with fork()");
  
  if ( pid == 0 )
  {
    close(pipe_2[0]);
    close(pipe_1[1]);

    if (supplyChildInput)
      dup2(pipe_1[0], STDIN_FILENO);
    else
      close(pipe_1[0]);
    if (ingestChildOutput)
      dup2(pipe_2[1], STDOUT_FILENO);
    else
      close(pipe_2[1]);
     
    execvp( argv[0], argv );
    throw SubprocessException("Execution passed execvp()");
  }
  
  sp.pid = pid;
  close(pipe_1[0]);
  close(pipe_2[1]);

  if (ingestChildOutput)
    sp.ingestfd = pipe_2[0];
  else
    close(pipe_2[0]);
  if (supplyChildInput)
    sp.supplyfd = pipe_1[1];
  else
    close(pipe_1[0]);

  return sp;
}
