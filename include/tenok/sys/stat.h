/**
 * @file
 */
#ifndef __STAT_H__
#define __STAT_H__

#include <sys/types.h>
#include <time.h>

/* File types, with the values POSIX gives them */
#define S_IFMT 0170000   /* Mask for the file type */
#define S_IFIFO 0010000  /* FIFO */
#define S_IFCHR 0020000  /* Character device */
#define S_IFDIR 0040000  /* Directory */
#define S_IFBLK 0060000  /* Block device */
#define S_IFREG 0100000  /* Regular file */
#define S_IFLNK 0120000  /* Symbolic link */
#define S_IFSOCK 0140000 /* Socket */

#define S_ISFIFO(m) (((m) &S_IFMT) == S_IFIFO)
#define S_ISCHR(m) (((m) &S_IFMT) == S_IFCHR)
#define S_ISDIR(m) (((m) &S_IFMT) == S_IFDIR)
#define S_ISBLK(m) (((m) &S_IFMT) == S_IFBLK)
#define S_ISREG(m) (((m) &S_IFMT) == S_IFREG)
#define S_ISLNK(m) (((m) &S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) &S_IFMT) == S_IFSOCK)

/* Permission bits. Tenok stores them and checks none of them */
#define S_ISUID 0004000
#define S_ISGID 0002000
#define S_ISVTX 0001000
#define S_IRWXU 0000700
#define S_IRUSR 0000400
#define S_IWUSR 0000200
#define S_IXUSR 0000100
#define S_IRWXG 0000070
#define S_IRGRP 0000040
#define S_IWGRP 0000020
#define S_IXGRP 0000010
#define S_IRWXO 0000007
#define S_IROTH 0000004
#define S_IWOTH 0000002
#define S_IXOTH 0000001

/* Return type of the stat() and fstat() syscalls */
struct stat {
    dev_t st_dev;            /* Device the file lives on */
    ino_t st_ino;            /* inode number */
    mode_t st_mode;          /* File type and permission bits */
    nlink_t st_nlink;        /* Number of hard links */
    uid_t st_uid;            /* Owner of the file */
    gid_t st_gid;            /* Group of the file */
    dev_t st_rdev;           /* Device number, when the file is a device */
    off_t st_size;           /* Total size in bytes */
    blksize_t st_blksize;    /* Block size of the file system */
    blkcnt_t st_blocks;      /* Number of the blocks used by the file */
    struct timespec st_atim; /* Time of the last access */
    struct timespec st_mtim; /* Time of the last modification */
    struct timespec st_ctim; /* Time of the last status change */
};

/* Tenok keeps the modification time, the other two read back as zero */
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

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
 * @param  mode: The file type and the permission bits of the new file.
 * @param  dev: The device number, when the file is a device. Not used.
 * @retval int: 0 on success and nonzero error number on error.
 */
int mknod(const char *pathname, mode_t mode, dev_t dev);

/**
 * @brief  Makes a FIFO special file with name pathname.
 * @param  pathname: The path name to create the new fifo file.
 * @param  mode: The permission bits of the new file.
 * @retval int: 0 on success and nonzero error number on error.
 */
int mkfifo(const char *pathname, mode_t mode);

/**
 * @brief  Create a directory named pathname
 * @param  pathname: The pathname of the directory to create.
 * @param  mode: The permission bits of the new directory.
 * @retval int: 0 on success and nonzero error number on error.
 */
int mkdir(const char *pathname, mode_t mode);

/**
 * @brief  Set the permission bits withheld from the files the task creates
 * @param  mask: The bits to withhold from now on.
 * @retval mode_t: The mask that was in effect before the call.
 */
mode_t umask(mode_t mask);

/* Given to utime() in place of a time, asking for the present one */
#define UTIME_TO_NOW ((uint32_t) -1)

/* What utimensat() of POSIX is given in place of a time */
#define UTIME_NOW ((1l << 30) - 1l)
#define UTIME_OMIT ((1l << 30) - 2l)

/**
 * @brief  Replace the time the contents of a file were last written
 * @param  pathname: The pathname of the file.
 * @param  mtime: Seconds since the epoch, or UTIME_TO_NOW for the present.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int utime(const char *pathname, uint32_t mtime);

/**
 * @brief  Replace the permission bits of a file
 * @param  pathname: The pathname of the file.
 * @param  mode: The permission bits to give the file.
 * @retval int: 0 on success and -1 on error, with the reason in errno.
 */
int chmod(const char *pathname, mode_t mode);

/**
 * @brief  Return information about the file specified by the pathname, in
 *         the buffer pointed to by statbuf
 * @param  pathname: The pathname of the file.
 * @param  statbuf: The buffer for returning the file information.
 * @retval int: 0 on success and nonzero error number on error.
 */
int stat(const char *pathname, struct stat *statbuf);

/**
 * @brief  Return information about the file specified by the pathname, the
 *         way stat() does. Tenok gives a file one name, so there is no link
 *         for the call to stop at and the two report the same thing.
 * @param  pathname: The pathname of the file.
 * @param  statbuf: The buffer for returning the file information.
 * @retval int: 0 on success and nonzero error number on error.
 */
int lstat(const char *pathname, struct stat *statbuf);

/**
 * @brief  Stamp the file a descriptor is open on with the times given. Tenok
 *         keeps no way back from a descriptor to the file it was opened on,
 *         which is what the call would need.
 * @param  fd: The descriptor.
 * @param  times: The times to stamp it with.
 * @retval int: -1 with errno set to ENOSYS.
 */
int futimens(int fd, const struct timespec times[2]);

#endif
