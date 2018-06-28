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


//File structure
// |--BOOT--| |--SUPER--| |------INODES------| |--BITMAP--| |----DATA BLOCKS---|

typedef unsigned char uchar;

//Block 0 is unused
//Block 1 is the superbock
//Inodes start at block 2;

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

struct Node{
  uint data;
  short type;
  short nLink;
  struct Node *next;
};

//Insert the inode number in the beginning of the linked list
void addToList(struct Node** head_ref, uint inum, short type){
  struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
  newNode->data = inum;
  newNode->type = type;
  newNode->nLink = 1; //When an inode is referenced for the first time
  newNode->next = *head_ref;
  *head_ref = newNode;

}

//Check if a certain inode number is present in our linked list or not
bool checkList(struct Node *head_ref, uint inum){


  struct Node *current = head_ref;
  while(current != NULL){

    if(current->data == inum){

      return true;
    }
    current = current->next;
  }
  //printf("Checklist couldn't find %d\n", inum);
  return false;
}

//Function to return the number of links of a file
int numLinks(struct Node *head_ref, uint inum){

  if(head_ref == NULL){
    return -1;
  }

  struct Node *current = head_ref;
  while(current != NULL){
    if(current->data == inum){

      return current->nLink;
    }
    current = current->next;
  }
  return -1;
}


//Increments the number of times a certain inode number is referenced in the
//directories of our file system
//If for a directory, number of links become greater than one, then it raises
//an ERROR
void increment(struct Node *head_ref, uint inum, int i){

  struct Node *current = head_ref;
  while(current != NULL){
    if(current->data == inum){
      current->nLink++;

      //printf("Inode %d is being incremented by %d\n", inum, i);

      break;
    }
    current = current->next;
  }
}

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))
// Block containing inode i
#define IBLOCK(i)     ((i) / IPB + 2)
#define BPB           (BSIZE*8)
// Block containing bit for block b
#define BBLOCK(b, ninodes) (b/BPB + (ninodes)/IPB + 3)
// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};


bool seenBefore(uint address, uint cache[], int n){

  for(int i = 0; i <= n; i++){
    if(cache[i] == address){
      return true;
    }
  }
  return false;
}


int main(int argc, char *argv[]){
  //Raise an error if no file system image is provided



  bool flag = false;
  if(argc == 3){
    if(strcmp(argv[1], "-r") != 0){
      fprintf(stderr, "%s\n", "Usage: xv6_fsck <file_system_image>." );
      exit(1);
    }
    flag = true;
  }


  if(argc < 2 || argc > 3){
    fprintf(stderr, "%s\n", "Usage: xv6_fsck <file_system_image>." );
    exit(1);
  }
  int fd;

  if(!flag){
    fd = open(argv[1], O_RDONLY);
  }
  else if(flag){
    fd = open(argv[2], O_RDWR);
  }

  // int fd = open(argv[1], O_RDONLY);
  // printf("file descriptor %d", fd);
//If the file system does not exist, exit with the appropriate error message

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

  void *img_ptr;
  if(!flag){
    img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  }
  else if(flag){
    img_ptr = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  }



  assert(img_ptr != MAP_FAILED);

  struct superblock *sb;
  //Block 0 + BSIZE, 512 bytes, moves us to the superblock
  sb = (struct superblock*)(img_ptr + BSIZE);

  //printf("%d %d %d\n", sb->size, sb->nblocks, sb->ninodes);

  //uint dataCache[(sb->nblocks) * (NDIRECT + 1)];
  struct dinode *dip = (struct dinode*)(img_ptr + 2*BSIZE);

  uint numEntries = BSIZE/(sizeof(struct dirent));
  char *bitmap = (char *) ((char*)img_ptr + ((sb->ninodes / IPB) + 3)*BSIZE);

  //Points to the first empty location in our address cache
  int initialIndex = 0;
  //Dip points to the first inode block
  dip = (struct dinode*)(img_ptr + 2*BSIZE);
  uint dataCache[sb->nblocks];

  uint lowerLimit = sb->size - sb->nblocks;

  /***********/
  /***********/
  //MAIN INODE LOOP
  //Each inode is either unallocated or one of the valid types (T_FILE, T_DIR, T_DEV). ERROR: bad inode.
  for(int i = 0; i < sb->ninodes; i++){
    //printf("-------We're at inode number %d and it's type is %d---------\n", i, dip->type);
    //If it is a free inode, move onto the next inode
    if(dip->type == 0){
      //Dip ++ to move onto the next inode
      dip++;
      continue;
    }
    /**** TEST - 1 *****/ //passing//
    else if(dip->type > T_DEV || dip->type < 0){
      // printf("type of directory %d\n", dip->type);
      // printf("happened at inode # %d\n", i);
      fprintf(stderr, "%s\n", "ERROR: bad inode.");
      exit(1);
    }

    //Else if the inode is allocated and is a valid type, perform other checks
    //ALPHA ELSE CONDITION
    else{

		if(i == 1 && dip->type != T_DIR) {
			fprintf(stderr, "ERROR: root directory does not exist.\n");
			exit(1);

		}

		if(i == 1 && dip->type == T_DIR) {

			struct dirent *dir = (struct dirent *)( img_ptr + dip->addrs[0]*BSIZE);

			if(strcmp(dir->name, ".") != 0) {

				fprintf(stderr, "ERROR: directory not properly formatted.\n");
				exit(1);

			}
			// check if the .. directory exists
			dir++;

			if(strcmp(dir->name, "..") != 0 || dir->inum != 1) {

				fprintf(stderr, "ERROR: root directory does not exist.\n");
				exit(1);

			}

		 } else if(dip->type  == T_DIR) {


			// check if the . exists
			struct dirent *dir = (struct dirent *)(img_ptr + dip->addrs[0]*BSIZE);
			if(strcmp(dir->name, ".") != 0 || dir->inum != i) {

			      fprintf(stderr, "ERROR: directory not properly formatted.\n");
						exit(1);
			}

			// check if the .. entry exists
			dir++;
			if(strcmp(dir->name, "..") != 0) {

			    fprintf(stderr, "ERROR: directory not properly formatted.\n");
				  exit(1);
			}
		}

     //Go through all the direct addresses of the inode
      for(int j = 0; j < NDIRECT; j++){

        if(dip->addrs[j] == 0){
          continue;
        }

        if(dip->addrs[j] > sb->size || dip->addrs[j] < lowerLimit){
          fprintf(stderr, "%s\n", "ERROR: bad direct address in inode.");
          exit(1);
        }

        if(!seenBefore(dip->addrs[j], dataCache, initialIndex)){

          //Delete this if anything breaks
          dataCache[initialIndex++] = dip->addrs[j];
        }
        else if(seenBefore(dip->addrs[j], dataCache, initialIndex)){

          fprintf(stderr, "%s\n", "ERROR: direct address used more than once.");
          exit(1);
        }

      }


    //Take care of the indirect addresses
    if(dip->addrs[NDIRECT] != 0){

      //First check if the indirect address is a valid address
      if(dip->addrs[NDIRECT] > sb->size || dip->addrs[NDIRECT] < lowerLimit){
        fprintf(stderr, "%s\n", "ERROR: bad indirect address in inode.");
        exit(1);
      }
      //If the indirect address has been seen before, raise an error
      else if(seenBefore(dip->addrs[NDIRECT], dataCache, initialIndex)){
        fprintf(stderr, "%s\n", "ERROR: indirect address used more than once.");
      }
      //Now if the indirect block is fine, we need to go into the direct addresses
      //inside the data block pointed to by this indirect data block
      else if(seenBefore(dip->addrs[NDIRECT], dataCache, initialIndex) == false){

        //Add the indirect address to our data cache
        dataCache[initialIndex++] = dip->addrs[NDIRECT];

        uint *x = (uint *)(img_ptr + dip->addrs[NDIRECT]*BSIZE);

        for(int k = 0; k < NINDIRECT; k++){
          //Check if the address is a valid one
          uint address = x[k];

          if(address != 0){
            //printf("address is %d\n", address);


            if(address > sb->size || address < lowerLimit){
              fprintf(stderr, "%s\n", "ERROR: bad indirect address in inode.");
              exit(1);
            }

            //Check if the address is present in our array
            if(!seenBefore(address, dataCache, initialIndex)){

              dataCache[initialIndex++] = address;

            }
            else{
              fprintf(stderr, "%s\n", "ERROR: direct address used more than once.");
              exit(1);
            }
          }
        }

      }

    }

  }

  dip++;
}//End of the inode traversal for loop



uint nByte;
uint bit_num;
uchar realByte;
uchar value;

uint data_block;
uint vdInodes[sb->ninodes*(NDIRECT+1)];

uint direct = 0;
uint indirect = 0;
uint vidInodes[100000];
uint *dblock;
uint *indirect_addr;

/***** Takes care of TEST -3 and TEST - 4 *****/
dip = (struct dinode *) (img_ptr + 2*BSIZE);
//Take care of the bitmaps
/***************TEST - 5  HERE ****************************************/
dip = (struct dinode*)(img_ptr + 2*BSIZE);


for(int i = 0; i < sb->ninodes; i++){
  if(dip->type < 4 && dip->type > 0) {

    for(int j = 0; j < NDIRECT; j++){

      if(dip->addrs[j] == 0){
        continue;
      }
      //We get the bit offset to know which bit we have to operand with
      bit_num = dip->addrs[j]%8;
      //get the block offset
      nByte = dip->addrs[j]/8;
      char *bitmap = ((uchar*)img_ptr + BBLOCK(dip->addrs[j], sb->ninodes)*BSIZE);
      realByte = (uchar)*(bitmap + nByte);

      if(((realByte >> bit_num) & 1) == 0){
        fprintf(stderr ,"ERROR: address used by inode but marked free in bitmap.\n");
        exit(1);
      }
    }
    //If indirect address is valid
    if(dip->addrs[NDIRECT] != 0){
      uint *x = (uint *)(img_ptr + dip->addrs[NDIRECT]*BSIZE);

      for(int k = 0; k < NINDIRECT; k++){
        uint address = x[k];

        if(address != 0){

          //Do the same thing as above, but this time for direct addresses
          //inside the indirect block
          nByte = address/8;
          bit_num = address%8;
          char *bitmap = ((uchar*)img_ptr + BBLOCK(address, sb->ninodes)*BSIZE);
          realByte = (uchar)*(bitmap + nByte);

          //And it with 1 to get the value in the bitmap

          if(((realByte >> bit_num) & 1) != 1){
            fprintf(stderr ,"ERROR: address used by inode but marked free in bitmap.\n");
            exit(1);
          }
        }
      }
    }
  }
  dip++;
}

/***************TEST - 6 STARTS HERE ****************************************/

dip = (struct dinode *) (img_ptr + 2*BSIZE);
for(int i = 0 ; i < sb->ninodes; i++) {
  if(dip->type > 0 && dip->type < 4) {
    for(int j = 0; j < NDIRECT+1; j++) {

      if(dip->addrs[j] != 0) {
        vdInodes[direct++] = dip->addrs[j];
      }
    }
    if(dip->addrs[NDIRECT] != 0) {

      uint *x = (uint*)(img_ptr + dip->addrs[NDIRECT]*BSIZE);
      for(uint  m = 0; m < NINDIRECT; m++)  {
        uint address = x[m];
        if(address != 0) {
          vidInodes[indirect++] = address;
        }
      }
    }
  }
  dip++;
}

for(int i = 0 ; i < BSIZE ; i++) {

  realByte = (uchar)*((uchar*)img_ptr + (IBLOCK(sb->ninodes)+1)*BSIZE + i);
  for(int j = 0; j < 8 ; j++) {

    bool flag = false;
    if(((realByte >> j) & (1)))  {
      int x = i*8 + j;
      data_block = x;
      if(data_block <= (IBLOCK(sb->ninodes)+1)){
        continue;
      }

      for(int s = 0 ; s < direct; s++) {
        if(vdInodes[s] == data_block) {
          flag = true;
          break;
        }
      }
      if(flag != true) {
        for(int s = 0; s < indirect; s++) {
          if(vidInodes[s] == data_block) {
            flag = true;
            break;
          }
        }
      }
      if(flag != true) {
        fprintf(stderr,"ERROR: bitmap marks block in use but it is not in use.\n");
        exit(1);
      }
    }
  }
}

/***************TEST - 6 ENDS HERE ****************************************/


// Creating a linked list representation of our file system, will be used to
// pass tests 9, 10, 11, 12
dip = (struct dinode*)(img_ptr + 2*BSIZE);
struct dirent *cacheDir;
struct dinode *searchInode;
dip++; //To start from the root directory
struct Node* head;
for(int i = 1; i < sb->ninodes; i++){

  if(dip->type == 0){
    dip++;
    continue;
  }
  //It is a
  if(dip->type > 0 && dip->type < 4){


    if(dip->type == T_DIR){

      //Go through all the direct addresses of the directory
      for(int j = 0; j < NDIRECT; j++){
        //printf("Inside address %d\n", j);
        //Get the directory entry
        if(dip->addrs[j] == 0){
          continue;
        }

        int loops = numEntries;
        cacheDir = (struct dirent*)(img_ptr + dip->addrs[j]*BSIZE);
        //Ignore the . and .. entry

        if(strcmp(cacheDir->name, ".") == 0){
          loops -= 1;
          cacheDir++;
        }

        if(strcmp(cacheDir->name, "..") == 0){
          loops -= 1;
          cacheDir++;
        }


        //Now store all the inode numbers inside these sequence of directory
        //entries

        for(int k = 0; k < loops; k++){
          //printf("inum is %d\n", cacheDir->inum);
          //If the dir entry points to null, we have reached the limit of
          //directory entries, therefore, we terminate out of the loop
          //and go to the next data block and look for directory entries
          if(cacheDir->inum <= 0 || cacheDir->inum > sb->ninodes){
            cacheDir++;
            continue;
          }

          /*****Add the inode to the linked list *******/
          //Get the inode number
          uint inum = cacheDir->inum;
          searchInode = (struct dinode*)(img_ptr + 2*BSIZE + sizeof(struct dinode)*inum);
          if(checkList(head, inum) == false){

            addToList(&head, inum, searchInode->type);
          }
          else if(checkList(head, inum) == true){

            //printf("Found inode #%d with type %d, it ref count is %d\n", inum, searchInode->type, numLinks(head, inum));
            increment(head, inum, i);
          }
          /****Added the inode to our linked list******/

          //move onto the next directory entry
          cacheDir++;

        }//Went through all the directory entries inside a data block

      }//For loop to go through all the direct addresses

      //Go through the indirect block
      if(dip->addrs[NDIRECT] != 0){

        //Go through all the direct addresses of the directory
        uint *x = (uint *)(img_ptr + dip->addrs[NDIRECT]*BSIZE);

        for(int j = 0; j < NINDIRECT; j++){
          //Get the direct address inside the indirect block
          uint address = x[j];
          if(address == 0){
            continue;
          }

          int loops = numEntries;
          cacheDir = (struct dirent*)(img_ptr + address*BSIZE);
          //printf("HERE? for address %d\n", j);
          //Ignore the . and .. entry

          if(strcmp(cacheDir->name, ".") == 0){
          loops -= 1;
          cacheDir++;
        }

        if(strcmp(cacheDir->name, "..") == 0){
          loops -= 1;
          cacheDir++;
        }



          //Now store all the inode numbers inside these sequence of directory
          //entries

          for(int k = 0; k < loops; k++){

            //If the dir entry points to null, we have reached the limit of
            //directory entries, therefore, we terminate out of the loop
            //and go to the next data block and look for directory entries
            if(cacheDir->inum <= 0 || cacheDir->inum > sb->ninodes){
              //flag2 = true;
              cacheDir++;
              continue;
            }

            /*****Add the inode to the linked list *******/
            //Get the inode number
            uint inum2 = cacheDir->inum;
            //printf("Inode number %d in block %d", inum, IBLOCK(i));
            searchInode = (struct dinode*)(img_ptr + 2*BSIZE + sizeof(struct dinode)*inum2);
            //printf("BEFORE CHECKLIST?\n");
            if(checkList(head, inum2) == false){

                addToList(&head, inum2, searchInode->type);
            }

            else if(checkList(head, inum2) == true){



              increment(head, inum2, i);
            }
            /****Added the inode to our linked list******/
            //printf("AFTER CHECK LIST--------\n");
            //move onto the next directory entry
            cacheDir++;


          }//Went through all the directory entries inside a data block
          //printf("Number of directory entries inside address %d is %d\n", j, count);
        }//For loop to go through all the direct addresses
      }//IF CONDITION TO HANDLE THE INDIRECT BLOCK


    }//If condition that only does the work when the inode is a directory


  }//all shit happens inside this if conditon that checks if the inode is a
  //valid type (type < 4 && > 0)

  //Move onto the next inode
  dip++;

}//End main code

dip = (struct dinode*)(img_ptr + 2*BSIZE);
struct Node* current = head;

//Handle TEST  - 9, 11 , 12
dip = (struct dinode*)(img_ptr + 2*BSIZE);
//Skip unused and root inodes
dip++;
dip++;
int lostAndFound = 0;
for(int i = 2; i < sb->ninodes; i++){

  if(dip->type == 0){
    dip++;
    continue;
  }

  //Now that the inode is marked use, check the linked list to see if that inode
  //has been referred to in at least one directory

  //If that inode number is not found in the linked list, that means, it has
  //not been referred to any directory
  if(checkList(head, i) == false){

    //If it is in repair mode, we will put the inode in a directory
    if(argc == 3 && (strcmp(argv[1], "-r") == 0)){

      //Now, the lost and found directory is under the root directory
      struct dinode *rootInode = (struct dinode*)(img_ptr + 2*BSIZE);
      rootInode++; //Go to the root inode
      uint address = rootInode->addrs[0];
      struct dirent *rootCache = (struct dirent*)(img_ptr + address*BSIZE);

      uint lfinum;
      for(int z = 0; z < numEntries; z++){

        if(strcmp(rootCache->name, "lost_found") == 0){
            //Get the inode number of the lost and found directory
            lfinum = rootCache->inum;
            //printf("Lost and found inode %d\n", lfinum);
            break;
        }

          rootCache++;
      }

      //After we have the inode of the lost and found directory
      struct dinode *lfinode = (struct dinode*)(img_ptr + 2*BSIZE + sizeof(struct dinode)*lfinum);

      //Now we add this lost inode to one of the empty directory entries inside the
      //lost and found directory
      struct dirent *lfcacheDir = (struct dirent*)(img_ptr + lfinode->addrs[0]*BSIZE);
      //Ignore . and ..
      lfcacheDir++;
      lfcacheDir++;


      //Now look for an entry so that it is empty
      for(int z = 0; z < numEntries - 2; z++){


        if(lfcacheDir->inum == 0){
            //Add the lost inode's inode number to this entry
            lfcacheDir->inum = i;
            //Give a name to this inode

            sprintf(lfcacheDir->name, "lostAndFound%d", lostAndFound);
            lostAndFound++; //Increment this number

            //Now if the lost inode is a directory, we set its parent to the lost
            //and found directory
            if(dip->type == T_DIR){
              //Go to its directory entry
              struct dirent* repair = (struct dirent*)(img_ptr + dip->addrs[0]*BSIZE);
              //Skip the .. entry
              repair++;

              //Assigned a parent to the lost directory
              repair->inum = lfinum;
            }


            //break out of the loop when you have repaired
            break;
        }

        lfcacheDir++;
      }

    }
    //If not in repair mode, exit the program
    else{

      fprintf(stderr, "%s\n", "ERROR: inode marked use but not found in a directory.");
      exit(1);
    }

  }
  //For inodes of type files, their nLinks should match the number of times
  //they were referred to in our directory
  //TEST - 11
  else if(checkList(head, i) == true ){
    int actualRefs = numLinks(head, i);
    if(dip->type == T_FILE){

      if(dip->nlink != actualRefs){
        //printf("Reference count for %d is bad, it is%d, but should be %d\n", i, dip->nlink, actualRefs);
        fprintf(stderr, "%s\n", "ERROR: bad reference count for file.");
        exit(1);
      }
    }
    else if(dip->type == T_DIR){

      if(actualRefs > 1){
        //printf("Reference count for %d is bad, it is %d, but should be %d\n", i, dip->nlink, actualRefs);
        fprintf(stderr, "%s\n", "ERROR: directory appears more than once in file system.");
        exit(1);
      }

    }
  }

  //Move onto the next node
  dip++;
}
/**************TEST 9, 11, 12 ********************/

//Handle test 10
current = head;

while(current != NULL){

  //TEST - 10
  //Inum of the inode that is marked in the directory
  uint inum = current->data;

  //Go to the inode that is associated with this inode number
  searchInode = (struct dinode*)(img_ptr + 2*BSIZE + inum*sizeof(struct dinode));

  if(searchInode->type == 0){
    //printf("The inode that is marked but found free in image is %d\n", inum);
    fprintf(stderr, "%s\n", "ERROR: inode referred to in directory but marked free.");
    exit(1);
  }

  current = current->next;
}

//Parent directory mismatch test
 dip = (struct dinode*)(img_ptr + 2*BSIZE);
 dip++;
struct dirent *entry;
uint parent;
 for(int i = 1; i < sb->ninodes; i++){

   if(dip->type == 0){
     dip++;
     continue;
   }

   if(dip->type == T_DIR){
     //printf("inside directory %d\n", i);
     entry = (struct dirent*)(img_ptr + dip->addrs[0]*BSIZE);
     bool mismatch = true;
     //Get the parent inode number
     entry++; //To skip "." entry

     //Now, we're at the ".." entry
     parent = entry->inum;
     //printf("Parent for directory %d is %d\n", i, parent);
     //Check for root directory
     if(i == 1 && parent == 1){
       mismatch = false;
     }

     if(i != 1){
       //Now, we've got the parent inode's number, we'll check if the parent inode
       //points back to this node
       struct dinode* parentInode = (img_ptr + 2*BSIZE + sizeof(struct dinode)*parent);

       //printf("inode %d type %d\n", parent, parentInode->type);
       //Now go through all the directory entries of this parent inode
       struct dirent* parentEntry;

       //First look at all the direct addresses entries
       for(int j = 0; j < NDIRECT; j++){

         if(parentInode->addrs[j] == 0){
           continue;
         }

         parentEntry = (struct dirent*)(img_ptr + parentInode->addrs[j]*BSIZE);
         //Skip the . and .. entry
         parentEntry++;
         parentEntry++;

         for(int k = 0; k < numEntries; k++){

           if(parentEntry->inum <= 0 || parentEntry->inum > sb->ninodes){
             parentEntry++;
             continue;
           }

           //Now if one of the directory's parent's entries point to the directory
           //There is no mismatch
           if(parentEntry->inum == i){
             mismatch = false;
             //Break out of the loop
             break;
           }

           parentEntry++;
         }

         //if no mismatch, no need to look into further direct addresses
         if(mismatch == false){
           break;
         }
       }//END OF checking for direct addresses


       //If there we didn't find the directory in the direct addresses of the
       //parent directory, look further
       if(dip->addrs[NDIRECT] != 0 && mismatch == true){

         uint *x = (uint *)(img_ptr + dip->addrs[NDIRECT]*BSIZE);
         uint address;

         for(int k = 0; k < NINDIRECT; k++){

           address = x[k];

           if(address == 0){
             continue;
           }

           parentEntry = (struct dirent*)(img_ptr + address*BSIZE);
           //Skip the . and .. entry
           parentEntry++;
           parentEntry++;

           for(int p = 0; p < numEntries; p++){

             if(parentEntry->inum <= 0 || parentEntry->inum > sb->ninodes){
               parentEntry++;
               continue;
             }

             //Now if one of the directory's parent's entries point to the directory
             //There is no mismatch
             if(parentEntry->inum == i){
               mismatch = false;
               //Break out of the loop
               break;
             }

             parentEntry++;
           }

           //if no mismatch, no need to look into further direct addresses
           if(mismatch == false){
             break;
           }

         }

       }
     }


     //After checking all entries of the parentEntry
     if(mismatch == true){
       //printf("parent mismatch for %d\n", i);
       fprintf(stderr, "%s\n", "ERROR: parent directory mismatch.");
       exit(1);
     }

   }//For each directory

   dip++;
 }


//Inaccessible directory exists
uint history[sb->ninodes];
int index = 0;
struct dinode *parentInode;
dip = (struct dinode*)(img_ptr + 2*BSIZE);
dip++;

for(int i = 1; i < sb->ninodes; i++){

  if(dip->type != T_DIR){
    dip++;
    continue;
  }
  //Now for a directory, we keep going up in the directory tree, as soon as we see
  //an address that we've seen before, we change the flag
  printf("Checking for directory %d------------\n", i);
  entry = (struct dirent*)(img_ptr + dip->addrs[0]*BSIZE);
  entry++; //Skip the "." entry

  printf("parent is %d\n", parent);
  uint parentInum = entry->inum;
  bool cycle = false;

  //If the parent is root directory, then move onto the next directory
  if(parentInum == 1){
    continue;
  }

  while(1){

       //Update the parent
       if(parent == 1){
         break;
       }

      //Add the parent to the history array
      if(!seenBefore(parentInum, history, index)){
        printf("Adding %d as parent to history\n", parent);
        history[index++] = parent;

        //Update the parent, go to the directory entry of the parent
        parentInode = (struct dinode*)(img_ptr + 2*BSIZE + sizeof(struct dinode)*parentInum);
        entry = (struct dirent*)(img_ptr + parentInode->addrs[0]*BSIZE);
        entry++;

        parentInum = entry->inum;
        printf("Parent of parent is %d\n", parentInum);
      }
      else{
        fprintf(stderr, "%s\n", "ERROR: inaccessible directory exists.");
        exit(1);
      }
  }//Loop to keep going up the directory tree

}

return 0;
}
