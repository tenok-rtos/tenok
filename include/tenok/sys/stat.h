/**
 * @file
 */
#ifndef __STAT_H__
#define __STAT_H__

#include <sys/types.h>

#define S_IFIFO 0 /* FIFO */
#define S_IFCHR 1 /* Character device */
#define S_IFBLK 2 /* Block device */
#define S_IFREG 3 /* Regular file */
#define S_IFDIR 4 /* Directory */

/* Return type of the fstat() syscall */
struct stat {
    uint8_t st_mode;    /* File type */
    uint32_t st_ino;    /* inode number */
    uint32_t st_rdev;   /* Device number */
    uint32_t st_size;   /* Total size in bytes */
    uint32_t st_blocks; /* Number of the blocks used by the file */
};

/**
 * @brief  Return information about a file, in the buffer pointed to by statbuf
 * @param  fd: The file descriptor to provide.
 * @retval int: 0 on success and nonzero error number on error.
 */
int fstat(int fd, struct stat *statbuf);

/**
 * @brief  Create a filesystem node (file, device special file, or named pipe)
 *         named pathname, with attributes specified by mode and dev
 * @param  pathname: The pathname to create the new file.
 * @param  mode: Not used.
 * @param  dev: The file type to provide (S_IFIFO, S_IFCHR, ...).
 * @retval int: 0 on success and nonzero error number on error.
 */
int mknod(const char *pathname, mode_t mode, dev_t dev);

/**
 * @brief  Makes a FIFO special file with name pathname.
 * @param  pathname: The path name to create the new fifo file.
 * @param  mode: Not used.
 * @retval int: 0 on success and nonzero error number on error.
 */
int mkfifo(const char *pathname, mode_t mode);

#endif
