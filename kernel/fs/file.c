#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "fs.h"
#include "syscall.h"
#include "reg_file.h"
#include "kconfig.h"
#include "file.h"

extern struct file *files[TASK_CNT_MAX + FILE_CNT_MAX];

/*
 * when accessing regular files, the user should consider using fread/fwrite
 * instead of read/write since the latter are executed in the kernel space,
 * which has the potential of affecting the real-time performance, whereas
 * the former is implemented to run in the user space, which is safer to use.
 */

int __fopen(const char *pathname, const char *mode, FILE *fstream)
{
    /* open the file with the system call */
    int fd = open(pathname, 0, 0);

    /* failed to open the file */
    if(fd < 0)
        return -1;

    /* check if the file is a regular file */
    struct stat stat;
    fstat(fd, &stat);

    /* not a regular file */
    if(stat.st_mode != S_IFREG)
        return -1;

    fstream->lock = 0;
    fstream->fd = fd;

    return 0;
}

size_t __fread(void *ptr, size_t size, size_t nmemb, FILE *fstream)
{
    /* start of the critical section */
    spin_lock(&fstream->lock);

    struct file *filp = files[fstream->fd];
    int retval = reg_file_read(filp, ptr, size * nmemb, 0);

    /* end of the critical section */
    spin_unlock(&fstream->lock);

    return retval;
}

size_t __fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fstream)
{
    /* start of the critical section */
    spin_lock(&fstream->lock);

    struct file *filp = files[fstream->fd];
    int retval = reg_file_write(filp, ptr, size * nmemb, 0);

    /* end of the critical section */
    spin_unlock(&fstream->lock);

    return retval;
}

int __fseek(FILE *stream, long offset, int whence)
{
    lseek(stream->fd, offset, whence);
}

int __fileno(FILE *stream)
{
    return stream->fd;
}
