#include <stdio.h>
#include <assert.h>

#include "inode.h"
#include "diskimg.h"
#include <string.h>

#define INDIR_ADDR 7
#define INODES_PER_BLOCK 16

/**
 * Fetches the specified inode from the filesystem. 
 * Returns 0 on success, -1 on error.  
 */
int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp) {
  struct inode i_nodes[INODES_PER_BLOCK];
  int sector_offset = ((inumber-1)/INODES_PER_BLOCK);
  int node_offset = (inumber-1)%INODES_PER_BLOCK;

  if (diskimg_readsector(fs->dfd, INODE_START_SECTOR + sector_offset, i_nodes) < 0)
  {
    return -1;
  }
  
  memcpy(inp, &i_nodes[node_offset], sizeof(struct inode) );

  return inumber;
}

/**
 * Given an index of a file block, retrieves the file's actual block number
 * of from the given inode.
 *
 * Returns the disk block number on success, -1 on error.  
 */
int inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int blockNum) {
  
  int sect_num = 0;
  int size = inode_getsize(inp);
  int large = (inp->i_mode & ILARG);
  

  // Direct addressing
  if ( !(large) )
  {
    sect_num = inp->i_addr[blockNum];
  }

  else
  {

  }


  return sect_num;
}

/**
 * Computes the size in bytes of the file identified by the given inode
 */
int inode_getsize(struct inode *inp) {
  return ((inp->i_size0 << 16) | inp->i_size1); 
}
