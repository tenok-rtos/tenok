#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "fs.h"
#include "syscall.h"
#include "reg_file.h"
#include "kconfig.h"
#include "file.h"

#define MAX_READ_SIZE  100
#define MAX_WRITE_SIZE 100

extern struct file *files[TASK_CNT_MAX + FILE_CNT_MAX];

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

    size_t nbytes = size * nmemb;
    int times = (nbytes - 1) / MAX_READ_SIZE + 1;
    int total = 0;

    int retval;
    for(int i = 0; i < times; i++) {
        int rsize = (nbytes >= MAX_READ_SIZE) ? MAX_READ_SIZE : nbytes;

        /* read file */
        if(read(fstream->fd, (char *)((uintptr_t)ptr + total), rsize) != rsize) {
            break; //read error
        }

        total += rsize;
        nbytes -= rsize;
    }

    /* end of the critical section */
    spin_unlock(&fstream->lock);

    return total;
}

size_t __fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fstream)
{
    /* start of the critical section */
    spin_lock(&fstream->lock);

    size_t nbytes = size * nmemb;
    int times = (nbytes - 1) / MAX_WRITE_SIZE + 1;
    int total = 0;

    int retval;
    for(int i = 0; i < times; i++) {
        int wsize = (nbytes >= MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : nbytes;

        /* write file */
        if(write(fstream->fd, (char *)((uintptr_t)ptr + total), wsize) != wsize) {
            break; //write error
        }

        total += wsize;
        nbytes -= wsize;
    }

    /* end of the critical section */
    spin_unlock(&fstream->lock);

    return total;
}

int __fseek(FILE *stream, long offset, int whence)
{
    lseek(stream->fd, offset, whence);
}

int __fileno(FILE *stream)
{
    return stream->fd;
}
