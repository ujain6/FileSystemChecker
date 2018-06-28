all:
	gcc xv6_fsck.c -o xv6_fsck -Wall -std=gnu99 

clean:
	rm xv6_fsck
