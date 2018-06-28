

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include "include/types.h"

#define ROOTNO 1
#define BSIZE 512

#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Special device

#define NDIRECT 12
#define NINDIRECT BSIZE/sizeof(uint)
#define DIRSIZ 14


// File system super block
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
};

struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};


struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i)     ((i) / IPB + 2)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block containing bit for block b
#define BBLOCK(b, ninodes) (b/BPB + (ninodes)/IPB + 3)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14


#define NDIRECT 12




int main(int argc, char * argv[]) {

  int fd = open(argv[1], O_RDONLY);
  if(fd == -1) {

    fprintf(stderr, "image not found.\n");
    exit(1);

  }

  int rc;
  struct stat sbuf;
  rc = fstat(fd, &sbuf);
  assert(rc == 0);

  void *img_ptr;
  img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);


  assert(img_ptr != MAP_FAILED);


  struct superblock *sb;
  sb = (struct superblock *) (img_ptr + BSIZE);

  struct dinode *dip = (struct dinode *) (img_ptr + 2*BSIZE);
  dip = (struct dinode *) (img_ptr + 2*BSIZE);
  for(int i = 0; i < sb->ninodes ; i++) {
    if(dip->type != 0){
      printf("Inode %d\n", i);

    }
    dip++;
  }


  dip = (struct dinode *) (img_ptr + 2*BSIZE);
  int numEntries = BSIZE/sizeof(struct dirent);
  printf("number of inodes %d\n", sb->ninodes);
  dip++;
  struct dirent *cacheDir;
  for(int i = 1; i < sb->ninodes ; i++) {

    if(dip->type == T_DIR){

      printf("Inside directory %d\n", i);


      //Skip . and ..

      for(int j = 0; j < NDIRECT; j++){
        if(dip->addrs[j] == 0){
          continue;
        }

        printf("---Inside data block%d----\n", dip->addrs[j]);


        cacheDir = (struct dirent*)(img_ptr + dip->addrs[j]*BSIZE);



        for(int k = 0; k < numEntries; k++){

          if(cacheDir->inum <= 0 || cacheDir->inum > sb->ninodes){
            cacheDir++;
            continue;
          }

          //Else, increment the number of times the inode is referenced
          uint inodeNum = cacheDir->inum;
          struct dinode *search = (struct dinode*)(img_ptr + 2*BSIZE + sizeof(struct dinode)*inodeNum);
          uint type = search->type;

          if(type == T_DIR){
            printf("Entry %d: Directory: %s Inode number %d\n", k, cacheDir->name, cacheDir->inum);
          }
          if(type == T_FILE){
            printf("Entry %d: File: %s Inode number %d\n", k, cacheDir->name, cacheDir->inum);
          }
          if(type == 0){

            printf("Entry %d: Empty Inode, Inode number: %d\n", k, cacheDir->inum);
          }

          cacheDir++;
        }



      }

      if(dip->addrs[NDIRECT] != 0){
        printf("----Indirect address %d---\n", dip->addrs[NDIRECT]);
       uint *x = (uint *)(img_ptr + dip->addrs[NDIRECT]*BSIZE);

        for(int k = 0; k < NINDIRECT; k++){
          uint address = x[k];

          if(address == 0){
            continue;
          }
          printf("---Inside data block%d----\n", address);
          cacheDir = (struct dirent*)(img_ptr + address*BSIZE);

          for(int k = 0; k < numEntries; k++){

            if(cacheDir->inum <= 0 || cacheDir->inum > sb->ninodes){
              cacheDir++;
              continue;
            }

              uint inodeNum = cacheDir->inum;
              struct dinode *search = (struct dinode*)(img_ptr + 2*BSIZE + sizeof(struct dinode)*inodeNum);
              uint type = search->type;

              if(type == T_DIR){
                printf("Entry %d: Directory: %s Inode number %d\n", k, cacheDir->name, cacheDir->inum);
              }
              if(type == T_FILE){
                printf("Entry %d: File: %s Inode number %d\n", k, cacheDir->name, cacheDir->inum);
              }
              if(type == 0){

                printf("Entry %d: Empty Inode, Inode number: %d\n", k, cacheDir->inum);
              }

            //Else, increment the number of times the inode is referenced

            cacheDir++;
          }
        }

      }

    }




  dip++;


}



return 0;
}
