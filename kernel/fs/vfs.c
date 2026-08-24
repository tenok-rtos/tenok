#include <errno.h>
#include <stdbool.h>

#include <fs/fs.h>
#include <fs/vfs.h>
#include <kernel/errno.h>
#include <kernel/pipe.h>
#include <kernel/sched.h>

extern struct file *files[FILE_RESERVED_NUM + FILE_MAX];

/* Read the reply of a file system request from the anonymous pipe of the
 * calling thread
 */
static int vfs_read_reply(int tid, void *reply, size_t size)
{
    int fifo_retval;

    while (1) {
        fifo_retval =
            fifo_read(files[THREAD_PIPE_FD(tid)], (char *) reply, size, 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return fifo_retval;
}

int vfs_mount(int tid, const char *source, const char *target)
{
    /* Send mount request to the file system daemon */
    request_mount(tid, source, target);

    /* Read mount result from the file system daemon */
    int fifo_retval, mnt_result;
    while (1) {
        fifo_retval = fifo_read(files[THREAD_PIPE_FD(tid)],
                                (char *) &mnt_result, sizeof(mnt_result), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return mnt_result;
}

int vfs_open_file(int tid, const char *pathname)
{
    /* Send file open request to the file system daemon */
    request_open_file(tid, pathname);

    /* Read the file index from the file system daemon */
    int file_idx;
    int fifo_retval;
    while (1) {
        fifo_retval = fifo_read(files[THREAD_PIPE_FD(tid)], (char *) &file_idx,
                                sizeof(file_idx), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    /* Pass the error number back to the caller */
    return file_idx;
}

int vfs_create_file(int tid, const char *pathname, mode_t mode)
{
    /* Send file create request to the file system daemon */
    request_create_file(tid, pathname, mode);

    /* Read file index from the file system daemon  */
    int file_idx;
    int fifo_retval;
    while (1) {
        fifo_retval = fifo_read(files[THREAD_PIPE_FD(tid)], (char *) &file_idx,
                                sizeof(file_idx), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return file_idx;
}

int vfs_open_dir(int tid, const char *pathname, DIR *dirp)
{
    /* Send directory open request to the file system daemon */
    request_open_directory(tid, pathname);

    /* Read the directory inode from the file system daemon */
    struct inode *inode_dir;
    int fifo_retval;
    while (1) {
        fifo_retval = fifo_read(files[THREAD_PIPE_FD(tid)], (char *) &inode_dir,
                                sizeof(inode_dir), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    /* Return directory information */
    if (!inode_dir) {
        dirp->inode_dir = NULL;
        dirp->dentry_list = NULL;
        return -ENOENT;
    }

    dirp->inode_dir = inode_dir;
    dirp->dentry_list = inode_dir->i_dentry.next;
    return 0;
}

int vfs_readdir(DIR *dirp, struct dirent *dirent)
{
    return fs_read_dir(dirp, dirent);
}

char *vfs_getcwd(int tid, char *buf, size_t size)
{
    /* Send getcwd request to the file system daemon */
    request_getcwd(tid, buf, size);

    /* Read getcwd result from the file system daemon */
    char *path;
    int fifo_retval;
    while (1) {
        fifo_retval = fifo_read(files[THREAD_PIPE_FD(tid)], (char *) &path,
                                sizeof(path), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return path;
}

int vfs_chdir(int tid, const char *path)
{
    /* Send chdir request to the file system daemon */
    request_chdir(tid, path);

    /* Read chdir result from the file system daemon */
    int chdir_result;
    int fifo_retval;
    while (1) {
        fifo_retval =
            fifo_read(files[THREAD_PIPE_FD(tid)], (char *) &chdir_result,
                      sizeof(chdir_result), 0);

        if (fifo_retval != -ERESTARTSYS)
            break;

        schedule();
    }

    return chdir_result;
}

int vfs_mkdir(int tid, const char *pathname, mode_t mode)
{
    /* Send mkdir request to the file system daemon */
    request_mkdir(tid, pathname, mode);

    /* Read mkdir result from the file system daemon */
    int result;
    vfs_read_reply(tid, &result, sizeof(result));

    return result;
}

int vfs_remove(int tid, const char *pathname, bool rm_dir)
{
    /* Send unlink/rmdir request to the file system daemon */
    request_remove(tid, pathname, rm_dir);

    /* Read the result from the file system daemon */
    int result;
    vfs_read_reply(tid, &result, sizeof(result));

    return result;
}

int vfs_chmod(int tid, const char *pathname, mode_t mode)
{
    /* Send the change mode request to the file system daemon */
    request_chmod(tid, pathname, mode);

    /* Read the result from the file system daemon */
    int result;
    vfs_read_reply(tid, &result, sizeof(result));

    return result;
}

int vfs_stat(int tid, const char *pathname, struct stat *statbuf)
{
    /* Send stat request to the file system daemon */
    request_stat(tid, pathname, statbuf);

    /* Read stat result from the file system daemon */
    int result;
    vfs_read_reply(tid, &result, sizeof(result));

    return result;
}

int vfs_rename(int tid, const char *oldpath, const char *newpath)
{
    /* Send rename request to the file system daemon */
    request_rename(tid, oldpath, newpath);

    /* Read rename result from the file system daemon */
    int result;
    vfs_read_reply(tid, &result, sizeof(result));

    return result;
}
