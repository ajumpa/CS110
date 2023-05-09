#include <stdio.h>
#include <assert.h>

#include "inode.h"
#include "diskimg.h"
#include <string.h>

#define INDIR_ADDR 7
#define INODES_PER_BLOCK 16
#define NUM_BLOCKS_PER_BLOCK 256


/**
 * Fetches the specified inode from the filesystem. 
 * Returns 0 on success, -1 on error.  
 */
int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp) {
  struct inode i_nodes[INODES_PER_BLOCK];
  int sector_offset = ((inumber-1)/INODES_PER_BLOCK);
  int node_offset = (inumber-1)%INODES_PER_BLOCK;

  if (diskimg_readsector(fs->dfd, INODE_START_SECTOR + sector_offset, i_nodes) < 0)
    return -1;
  
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
  
  // MORE THAN LIKELY OFF BY ONE ERRORS IN INDIRECT AND DOUBLY INDIRECT SECTIONS
  if ( !(large) )
  {
    return inp->i_addr[blockNum];
  }
  else if ( blockNum <= INDIR_ADDR*NUM_BLOCKS_PER_BLOCK )
  {
    int direct_block_offset = blockNum/NUM_BLOCKS_PER_BLOCK;
    int indirect_block_offset = blockNum%NUM_BLOCKS_PER_BLOCK;
    int direct_block = inp->i_addr[direct_block_offset];
    uint16_t indirect_blocks[NUM_BLOCKS_PER_BLOCK];

    if (diskimg_readsector(fs->dfd, direct_block, &indirect_blocks) < 0)
      return -1;
    return indirect_blocks[indirect_block_offset];
  }
  else if ( blockNum > INDIR_ADDR*NUM_BLOCKS_PER_BLOCK )
  {
    int direct_block_offset = INDIR_ADDR;
    blockNum -= INDIR_ADDR*NUM_BLOCKS_PER_BLOCK;
    int indirect_block_offset = blockNum/NUM_BLOCKS_PER_BLOCK;
    int double_indirect_block_offset = blockNum%NUM_BLOCKS_PER_BLOCK;
    
    uint16_t indirect_blocks[NUM_BLOCKS_PER_BLOCK];
    uint16_t double_indirect_blocks[NUM_BLOCKS_PER_BLOCK];

    int direct_block = inp->i_addr[direct_block_offset];
    if (diskimg_readsector(fs->dfd, direct_block, &indirect_blocks) < 0)
      return -1;
    
    uint16_t next_block = indirect_blocks[indirect_block_offset];

    if (diskimg_readsector(fs->dfd, next_block, &double_indirect_blocks) < 0)
      return -1;

    return double_indirect_blocks[double_indirect_block_offset];
  }

  return 0;
}

/**
 * Computes the size in bytes of the file identified by the given inode
 */
int inode_getsize(struct inode *inp) {
  return ((inp->i_size0 << 16) | inp->i_size1); 
}
