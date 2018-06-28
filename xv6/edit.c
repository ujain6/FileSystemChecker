#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdlib.h>
#include<string.h>
#include <stdbool.h>


#define ROOTINO 1
#define BSIZE 512
#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))

//Free node denoted by type 0
#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Special device
struct superblock {

  uint size;   //Size of file syatem image (blocks)
  uint nblocks;
  uint ninodes;

};

struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};



// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))
// Block containing inode i
#define IBLOCK(i)     ((i) / IPB + 2)
// Bitmap bits per block
#define BPB           (BSIZE*8)
// Block containing bit for block b
#define BBLOCK(b, ninodes) (b/BPB + (ninodes)/IPB + 3)
// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

ushort
xshort(ushort x)
{
  ushort y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

int main(int argc, char *argv[]){


  int fd = open(argv[1], O_RDWR);
  uint numEntries = BSIZE/sizeof(struct dirent);

  if(fd == -1){
    fprintf(stderr, "%s\n", "image not found." );
    exit(1);
  }

  int rc;
  struct stat sbuf;
  rc = fstat(fd, &sbuf);
  assert(rc == 0);

  //use mmap()
  //Points to the starting of the file system, pointing to block 0
  void *img_ptr = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  assert(img_ptr != MAP_FAILED);

  struct superblock *sb;
  //Block 0 + BSIZE, 512 bytes, moves us to the superblock
  sb = (struct superblock*)(img_ptr + BSIZE);

  //printf("%d %d %d\n", sb->size, sb->nblocks, sb->ninodes);

  /*******MISMATCH EDITS ****************/
  //uint dataCache[(sb->nblocks) * (NDIRECT + 1)];
  // struct dinode *dip = (struct dinode*)(img_ptr + 2*BSIZE + sizeof(struct dinode)*7);
  //
  // struct dirent *entry = (struct dirent*)(img_ptr + dip->addrs[0]*BSIZE);
  //
  // entry++;
  //
  // //Change parent to directory 10
  // entry->inum = 10;
/*******MISMATCH EDITS ****************/

 /*******LOST AND FOUND EDITS ****************/
  // struct dirent *cacheDir;
  //  // At the root directory
  //  uint freeInum = 0;
  //  dip++;
  // //Find an unused inode number
  // for(int i = 1; i < sb->ninodes; i++){
  //   if(dip->type == 0){
  //     freeInum = i;
  //     break;
  //   }
  //   dip++;
  // }
  //
  // dip = (struct dinode*)(img_ptr + 2*BSIZE);
  // dip++;
  // cacheDir = (struct dirent*)(img_ptr + 29*BSIZE);
  // cacheDir++;
  //
  // cacheDir++;
  //
  // for(int i = 0 ; i < numEntries; i++){
  //
  //   if(cacheDir->inum >0 && cacheDir->inum < sb->ninodes){
  //     printf("name %s\n", cacheDir->name);
  //   }
  //   else{
  //     cacheDir->inum = xshort(freeInum);
  //     strcpy(cacheDir->name, "lost_found");
  //     break;
  //   }
  //
  //
  //
  //   cacheDir++;
  //
  // }

  // dip = (struct dinode*)(img_ptr + 2*BSIZE);
  //
  // for(int i = 0; i < sb->ninodes; i++){
  //   if(i == 65){
  //     dip->type = T_DIR;
  //     dip->nlink  = 1;
  //
  //   }
  //   dip++;
  // }

   /*******LOST AND FOUND EDITS ****************/


   //
  struct dinode *dip = (struct dinode*)(img_ptr + 2*BSIZE + sizeof(struct dinode)*7);
   struct dirent *entry = (struct dirent*)(img_ptr + dip->addrs[0]*BSIZE);
entry++;
entry->inum = 10;

dip = (struct dinode*)(img_ptr + 2*BSIZE + sizeof(struct dinode)*10);
entry = (struct dirent*)(img_ptr + dip->addrs[0]*BSIZE);
entry++;
entry->inum = 7;
   //Go to directory 7




  return 0;
}
