/**
 * @file
 */
#ifndef __DIRENT_H__
#define __DIRENT_H__

#include <stdint.h>

#include "kconfig.h"

/* return type of the opendir() syscall */
typedef struct dirstream {
    struct inode *inode_dir;       /* directory inode */
    struct list_head *dentry_list; /* list pointer of the dentry to return */
} DIR;

/* return type of the readdir() syscall */
struct dirent {
    char d_name[FILE_NAME_LEN_MAX]; /* file name */
    uint32_t d_ino;                 /* the inode of the file */
    uint8_t d_type;                 /* file type */
};

/**
 * @brief  Open a directory stream corresponding to the directory name,
           and returns a pointer to  the  directory  stream
 * @param  name: The file path to the directory.
 * @param  dir: The directory stream to return.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int opendir(const char *name, DIR *dir);

/**
 * @brief  Return a pointer to a dirent structure representing the next
           directory entry in the directory stream pointed to by dirp
 * @param  dirp: The directory stream to provide.
 * @param  dirent: The structure of file entries under the directory to
                   return.
 * @retval int: 0 on sucess and nonzero error number on error.
 */
int readdir(DIR *dirp, struct dirent *dirent);

#endif
