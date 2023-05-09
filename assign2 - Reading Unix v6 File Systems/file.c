#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "file.h"
#include "inode.h"
#include "diskimg.h"


/**
 * Fetches the specified file block from the specified inode.
 * Returns the number of valid bytes in the block, -1 on error.
 */
int file_getblock(struct unixfilesystem *fs, int inumber, int blockNum, void *buf) {
  struct inode in;
  if (inode_iget(fs, inumber, &in) < 0) {
    fprintf(stderr,"Can't read inode %d \n", inumber);
    return -1;
  }

  assert ( in.i_mode & IALLOC );

  int size = inode_getsize(&in);
  int n_read;
  int sector_num = inode_indexlookup(fs, &in, blockNum);
  int remainder_bytes = size%DISKIMG_SECTOR_SIZE;
  int last_block = size/DISKIMG_SECTOR_SIZE;
  int n_bytes = (blockNum == last_block && remainder_bytes) ? remainder_bytes : DISKIMG_SECTOR_SIZE;

  if (lseek(fs->dfd, sector_num * DISKIMG_SECTOR_SIZE, SEEK_SET) == (off_t) -1) return -1;  
  n_read = read(fs->dfd, buf, n_bytes);
  
  //printf("Read %d bytes from sector %d\n", n_read, sector_num);

  return n_read;
}
