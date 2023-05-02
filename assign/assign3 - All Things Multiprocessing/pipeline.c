/**
 * File: pipeline.c
 * ----------------
 * Presents the implementation of the pipeline routine.
 */

#include "pipeline.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

void pipeline(char *argv1[], char *argv2[], pid_t pids[]) {
  int fds[2];
  pipe(fds);
  pids[0] = fork();
  if ( pids[0] == 0 )
  {
    close(fds[0]);
    dup2(fds[1], STDOUT_FILENO);
    execvp( argv1[0], argv1);  
    fprintf(stderr, "Child process should not reach execution here\n");
  }
  pids[1] = fork();
  if ( pids[1] == 0 )
  {
    close(fds[1]); 
    dup2(fds[0], STDIN_FILENO);
    execvp( argv2[0], argv2);  
    fprintf(stderr, "Child process should not reach execution here\n");
  }
  close(fds[1]);
  close(fds[0]);
}
