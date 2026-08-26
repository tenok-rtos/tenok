/**
 * @file
 */
#ifndef __FS_H__
#define __FS_H__

#include <dirent.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <common/list.h>
#include <kernel/time.h>
#include <kernel/wait.h>

#include "kconfig.h"

#define RDEV_ROOTFS 0

/* Mode of a new file, the one a Linux system gives under a umask of 022 */
#define FS_DEFAULT_FILE_MODE 0644
#define FS_DEFAULT_DIR_MODE 0755

/* Withheld from the mode a task asks for, which is what turns the 0666 of
 * an ordinary creation into the 0644 of an ordinary file
 */
#define FS_DEFAULT_UMASK 022

/* Every thread has a pipe the file system daemon answers it through. It is
 * reached by the thread that owns it and by the daemon, never by a program,
 * so it carries no descriptor number
 */
extern struct file *thread_pipe[THREAD_MAX];

/* +---------------------------+
 * |     File table layout     |
 * +-----------+---------------+
 * |     0     |     stdin     |
 * +-----------+---------------+
 * |     1     |     stdout    |
 * +-----------+---------------+
 * |     2     |     stderr    |
 * +-----------+---------------+
 * |     3     |     File 1    |
 * +-----------+---------------+
 * |    ...    |      ...      |
 * +-----------+---------------+
 * |   M + 2   |     File M    |
 * +-----------+---------------+
 *
 * M = OPEN_MAX
 */

typedef void (*drv_init_func_t)(void);

enum {
    FS_CREATE_FILE = 1,
    FS_OPEN_FILE = 2,
    FS_OPEN_DIR = 3,
    FS_MOUNT = 4,
    FS_GET_CWD = 5,
    FS_CHANGE_DIR = 6,
    FS_MAKE_DIR = 7,
    FS_REMOVE = 8,
    FS_STAT = 9,
    FS_RENAME = 10,
    FS_CHANGE_MODE = 11,
    FS_CHANGE_TIME = 12,
} FS_SERVER_CMDS;

struct super_block {
    bool s_rd_only;       /* Read-only flag */
    uint32_t s_blk_cnt;   /* number of the used blocks */
    uint32_t s_inode_cnt; /* number of the used inodes */
    uint64_t s_sb_addr;   /* Start address of the super block */
    uint64_t s_ino_addr;  /* Start address of the inode table */
    uint64_t s_blk_addr;  /* Start address of the blocks region */
};

/* Block header will be placed at the top of every blocks of regular files */
struct block_header {
    /* Virtual address of the next block */
    uint32_t b_next;
    uint32_t reserved;
};

struct mount {
    struct file *dev_file;        /* Driver file of the mounted device */
    struct super_block super_blk; /* Super block of the mounted device */
};

/* index node */
struct inode {
    /* File type and permission bits: S_IFREG | 0644, and so on */
    uint16_t i_mode;
    /* The device on which the file is mounted */
    uint8_t i_rdev;
    /* The mounted file is loaded into the rootfs or not */
    bool i_sync;
    /* inode number */
    uint32_t i_ino;
    /* inode number of the parent directory */
    uint32_t i_parent;
    /* File descriptor number */
    uint32_t i_fd;
    /* File size (bytes) */
    uint32_t i_size;
    /* block_numbers = file_size / (block_size - block_header_size) */
    uint32_t i_blocks;
    /* Virtual address for accessing the storage */
    uint32_t i_data;
    /* List head of the dentry table */
    struct list_head i_dentry;
    /* Seconds since the epoch, last written. Thirty two of them reach 2106 */
    uint32_t i_mtime;
    uint32_t reserved[2];
};

/* Directory entry */
struct dentry {
    /* File name */
    char d_name[NAME_MAX];
    /* The inode of the file */
    uint32_t d_inode;
    /* The inode of the parent directory */
    uint32_t d_parent;
    /* List head of the dentry */
    struct list_head d_list;
    uint32_t reserved[2];
};

struct file {
    struct inode *f_inode;
    struct file_operations *f_op;
    uint32_t f_events;
    int f_flags;
    /* Where the next read or write starts. A regular file keeps its own
     * position in struct reg_file and ignores this one; it is what gives a
     * device file, /dev/rom for instance, a position of its own.
     */
    off_t f_pos;
    struct list_head list;
};

struct file_operations {
    off_t (*lseek)(struct file *filp, off_t offset, int whence);
    ssize_t (*read)(struct file *filp, char *buf, size_t size, off_t offset);
    ssize_t (*write)(struct file *filp,
                     const char *buf,
                     size_t size,
                     off_t offset);
    int (*ioctl)(struct file *, unsigned int cmd, unsigned long arg);
    int (*open)(struct inode *inode, struct file *file);
    /* Where the file already lives in memory. Tenok has no address space to
     * map anything into, so a device that answers this is one whose contents
     * a program can reach where they already are
     */
    void *(*mmap)(struct file *filp, size_t length, off_t offset);
};

struct fdtable {
    /* The flags the file was opened with */
    int flags;
    /* The flags of the descriptor itself, which is FD_CLOEXEC and nothing
     * else. They are kept apart from the ones above because FD_CLOEXEC and
     * O_WRONLY are both one, and a descriptor would otherwise report the
     * access mode as the close on exec bit and lose it when the bit is set.
     */
    int fd_flags;
    struct file *file;
};

void rootfs_init(void);

void link_stdin_dev(char *path);
void link_stdout_dev(char *path);
void link_stderr_dev(char *path);

int register_chrdev(char *name, struct file_operations *fops);
int register_blkdev(char *name, struct file_operations *fops);

bool file_is_opened(struct file *filp);

int fs_read_dir(DIR *dirp, struct dirent *dirent);
uint32_t fs_get_block_addr(struct inode *inode, int blk_index);
uint32_t fs_file_append_block(struct inode *inode);

/* What an inode stamps itself with. The clock reads zero until it is set */
static inline uint32_t fs_now(void)
{
    struct timespec tp;

    get_sys_time(&tp);

    return (uint32_t) tp.tv_sec;
}

/* Fill in a stat buffer from an inode. Tenok tracks no ownership and no link
 * count, so those fields read back as zero
 */
static inline void fs_fill_stat(struct stat *statbuf, const struct inode *inode)
{
    memset(statbuf, 0, sizeof(*statbuf));

    statbuf->st_dev = inode->i_rdev;
    statbuf->st_ino = inode->i_ino;
    statbuf->st_mode = inode->i_mode;
    statbuf->st_nlink = 1;
    statbuf->st_rdev = inode->i_rdev;
    statbuf->st_size = inode->i_size;
    statbuf->st_blksize = FS_BLK_SIZE;
    statbuf->st_blocks = inode->i_blocks;
    statbuf->st_mtim.tv_sec = inode->i_mtime;
}

void request_create_file(int thread_id, const char *path, mode_t mode);
void request_mkdir(int thread_id, const char *path, mode_t mode);
void request_chmod(int thread_id, const char *path, mode_t mode);
void request_utime(int thread_id, const char *path, uint32_t mtime);
void request_remove(int thread_id, const char *path, bool rm_dir);
void request_stat(int thread_id, const char *path, struct stat *statbuf);
void request_rename(int thread_id, const char *oldpath, const char *newpath);
void request_open_file(int thread_id, const char *path);
void request_open_directory(int reply_fd, const char *path);
void request_mount(int thread_id, const char *source, const char *target);
void request_getcwd(int thread_id, char *buf, size_t len);
void request_chdir(int thread_id, const char *path);

void filesysd(void);

void fs_print_inode_bitmap(void);
void fs_print_block_bitmap(void);

#endif
