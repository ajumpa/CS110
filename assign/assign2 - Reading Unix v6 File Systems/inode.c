#include <stdio.h>
#include <assert.h>

#include "inode.h"
#include "diskimg.h"

#define INDIR_ADDR 7
#define INODES_PER_BLOCK DISKIMG_SECTOR_SIZE/sizeof(struct inode) 

int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp) {
  struct inode i_nodes [INODES_PER_BLOCK];
  if (diskimg_readsector(fs->dfd, INODE_START_SECTOR + inumber/INODES_PER_BLOCK, &i_nodes) == -1)
  {
    fprintf(stderr, "Error reading inode data\n");
    return -1;
  }
  inp = &i_nodes[inumber];
  printf("%d :\n", inode_getsize(inp));

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