
#include "pathname.h"
#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#define DIR_MAX_LEN 14

/**
 * Returns the inode number associated with the specified pathname.  This need only
 * handle absolute paths.  Returns a negative number (-1 is fine) if an error is 
 * encountered.
 */
int pathname_lookup(struct unixfilesystem *fs, const char *pathname) {

  int inumber = 1;
  char *path_tok;
  char *pathname_mod = strdup(pathname);
  while( (path_tok = strsep(&pathname_mod, "/")) )
  {
    if (!strcmp(path_tok, "\0")) continue;
    struct direntv6 dirEnt;
    int i = directory_findname(fs, path_tok, inumber, &dirEnt);
    if ( i < 0 ) return -1;
    inumber = dirEnt.d_inumber;
  }
  free(pathname_mod);

  return inumber;
}
