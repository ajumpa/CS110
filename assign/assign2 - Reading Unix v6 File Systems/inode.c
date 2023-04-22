#include <stdio.h>
#include <assert.h>

#include "inode.h"
#include "diskimg.h"

#define INDIR_ADDR 7

/**
 */
int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp) {
  return 0;
}


/**
 */
int inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int blockNum) {
  return 0;
}


/**
 * Computes the size in bytes of the file identified by the given inode
 */
int inode_getsize(struct inode *inp) {
  return ((inp->i_size0 << 16) | inp->i_size1); 
}
