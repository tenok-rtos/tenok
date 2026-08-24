/**
 * @file
 */
#ifndef __DIRENT_H__
#define __DIRENT_H__

#include <stdint.h>
#include <sys/limits.h>
#include <sys/stat.h>

#include "kconfig.h"

/* File types of d_type, which is the file type of the mode shifted down by
 * twelve bits
 */
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12

#define IFTODT(mode) (((mode) &S_IFMT) >> 12)
#define DTTOIF(dirtype) ((dirtype) << 12)

/* Return type of the readdir() */
struct dirent {
    char d_name[NAME_MAX]; /* File name */
    uint32_t d_ino;        /* The inode of the file */
    uint8_t d_type;        /* File type, one of the DT_ values above */
};

/* Return type of the opendir(). The entry readdir() answers with lives here */
typedef struct dirstream {
    struct inode *inode_dir;       /* Directory inode */
    struct list_head *dentry_list; /* List pointer of the dentry to return */
    struct dirent entry;           /* Storage of the entry readdir() returns */
} DIR;

/**
 * @brief  Open a directory stream corresponding to the directory name,
 *         and return a pointer to the directory stream
 * @param  name: The file path to the directory.
 * @retval DIR *: The directory stream on success and a null pointer on
 *         error, with the reason left in errno.
 */
DIR *opendir(const char *name);

/**
 * @brief  Return the next entry of the directory stream. The entry belongs
 *         to the stream and is overwritten by the next call on it.
 * @param  dirp: The directory stream to read.
 * @retval struct dirent *: The entry on success, and a null pointer at the
 *         end of the directory or on error, which are told apart by errno.
 */
struct dirent *readdir(DIR *dirp);

/**
 * @brief  Close a directory stream and release what it holds
 * @param  dirp: The directory stream to close.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int closedir(DIR *dirp);

#endif
