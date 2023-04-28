#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define DIR_ENTRY_SIZE 14
#define NUM_INODE_ADDR_ENTRIES 8

/**
 * Looks up the specified name (name) in the specified directory (dirinumber).  
 * If found, return the directory entry in space addressed by dirEnt.  Returns 0 
 * on success and something negative on failure. 
 */
int directory_findname(struct unixfilesystem *fs, const char *name, int dirinumber, struct direntv6 *dirEnt) {
  
  struct inode dir;
  if (inode_iget(fs, dirinumber, &dir) < 0)
  {
    fprintf(stderr, "Failed to find inode %d in directory_findname()\n", dirinumber);
    return -1;
  }

  assert ( dir.i_mode & IFDIR );
  

  // Could extend to handle double indirect addressing, but im tired 
  if ( dir.i_mode & ILARG )
  {
    uint16_t entry_blocks[DISKIMG_SECTOR_SIZE/sizeof(uint16_t)];
    for (int i = 0; i < NUM_INODE_ADDR_ENTRIES; i++)
    {

      int block_num = dir.i_addr[i];
      if (diskimg_readsector(fs->dfd, block_num, &entry_blocks) < 0)
        return -1;

      for (uint32_t i = 0; i < DISKIMG_SECTOR_SIZE/sizeof(uint16_t); i++)
      {
        struct direntv6 entry_names[DISKIMG_SECTOR_SIZE/sizeof(struct direntv6)];

        if (diskimg_readsector(fs->dfd, entry_blocks[i], &entry_names) < 0)
          return -1;

        for (uint32_t i = 0; i < DISKIMG_SECTOR_SIZE/sizeof(struct direntv6); i++)
        {
          if ( !strncmp( name, entry_names[i].d_name, DIR_ENTRY_SIZE) )
          {
            memcpy(dirEnt, &entry_names[i], sizeof(struct direntv6) );
            return 0;
          }
        }
      }
    }
  }
  else
  {
    struct direntv6 entry_names[DISKIMG_SECTOR_SIZE/sizeof(struct direntv6)];
    for (int i = 0; i < NUM_INODE_ADDR_ENTRIES; i++)
    {
      int block_num = dir.i_addr[i];
      if (diskimg_readsector(fs->dfd, block_num, &entry_names) < 0)
        return -1;
       
      for (uint32_t i = 0; i < DISKIMG_SECTOR_SIZE/sizeof(struct direntv6); i++)
      {
        if ( !strncmp( name, entry_names[i].d_name, DIR_ENTRY_SIZE) )
        {
          memcpy(dirEnt, &entry_names[i], sizeof(struct direntv6) );
          return 0;
        }
      }
    }
  }
  return -1;
}
