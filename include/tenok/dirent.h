#ifndef __DIRENT_H__
#define __DIRENT_H__

#include "kconfig.h"

/* return type of the opendir() syscall */
typedef struct dirstream {
    struct inode  *inode_dir;   //directory inode
    struct list   *dentry_list; //list pointer of the dentry to return
} DIR;

/* return type of the readdir() syscall */
struct dirent {
    char     d_name[FILE_NAME_LEN_MAX]; //file name
    uint32_t d_ino;  //the inode of the file
    uint8_t  d_type; //file type
};

int opendir(const char *name, DIR *dir);
int readdir(DIR *dirp, struct dirent *dirent);

#endif
