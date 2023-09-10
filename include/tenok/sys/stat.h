#ifndef __STAT_H__
#define __STAT_H__

#include "fs.h"

#define S_IFIFO 0 //fifo
#define S_IFCHR 1 //char device
#define S_IFBLK 2 //block device
#define S_IFREG 3 //regular file
#define S_IFDIR 4 //directory

/* return type of the fstat() syscall */
struct stat {
    uint8_t  st_mode;   //file type
    uint32_t st_ino;    //inode number
    uint32_t st_rdev;   //device number
    uint32_t st_size;   //total size in bytes
    uint32_t st_blocks; //number of the blocks used by the file
};

int fstat(int fd, struct stat *statbuf);
int mknod(const char *pathname, mode_t mode, dev_t dev);
int mkfifo(const char *pathname, mode_t mode);

#endif
