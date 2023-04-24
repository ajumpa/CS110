#include <stdio.h>
#include <assert.h>

#include "inode.h"
#include "diskimg.h"
#include <string.h>

#define INDIR_ADDR 7
#define INODES_PER_BLOCK 16

int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp) {
  struct inode i_nodes[INODES_PER_BLOCK];
  if (diskimg_readsector(fs->dfd, INODE_START_SECTOR + ((inumber-1)/INODES_PER_BLOCK), &i_nodes) == -1)
  {
    fprintf(stderr, "Error reading inode data\n");
    return -1;
  }
  memcpy(inp, &i_nodes[(inumber-1)%INODES_PER_BLOCK], sizeof(struct inode) );
  return 0;
}

int inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int blockNum) {
  return 0;
}

/**
 * Computes the size in bytes of the file identified by the given inode
 */
int inode_getsize(struct inode *inp) {
  return ((inp->i_size0 << 16) | inp->i_size1); 
}
