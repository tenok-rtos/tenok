#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fs/fs.h>
#include <fs/reg_file.h>
#include <kernel/kernel.h>

#include "kconfig.h"

#define MAX_READ_SIZE  100
#define MAX_WRITE_SIZE 100

extern struct file *files[TASK_CNT_MAX + FILE_CNT_MAX];

int _fopen(const char *pathname, const char *mode, FILE *stream)
{
    /* open the file with the system call */
    int fd = open(pathname, 0);

    /* failed to open the file */
    if(fd < 0)
        return -1;

    /* check if the file is a regular file */
    struct stat stat;
    fstat(fd, &stat);

    /* not a regular file */
    if(stat.st_mode != S_IFREG)
        return -1;

    pthread_mutex_init(&stream->lock, NULL);
    stream->fd = fd;

    return 0;
}

int _fclose(FILE *stream)
{
    return close(stream->fd);
}

size_t _fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    /* start of the critical section */
    pthread_mutex_lock(&stream->lock);

    size_t nbytes = size * nmemb;
    int times = (nbytes - 1) / MAX_READ_SIZE + 1;
    int total = 0;

    int retval;
    for(int i = 0; i < times; i++) {
        int rsize = (nbytes >= MAX_READ_SIZE) ? MAX_READ_SIZE : nbytes;
        retval = read(stream->fd, (char *)((uintptr_t)ptr + total), rsize);

        total += rsize;
        nbytes -= rsize;

        if(retval != rsize) {
            break; //EOF or read error
        }
    }

    /* end of the critical section */
    pthread_mutex_unlock(&stream->lock);

    return total;
}

size_t _fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    /* start of the critical section */
    pthread_mutex_lock(&stream->lock);

    size_t nbytes = size * nmemb;
    int times = (nbytes - 1) / MAX_WRITE_SIZE + 1;
    int total = 0;

    int retval;
    for(int i = 0; i < times; i++) {
        /* write file */
        int wsize = (nbytes >= MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : nbytes;
        retval = write(stream->fd, (char *)((uintptr_t)ptr + total), wsize);

        total += wsize;
        nbytes -= wsize;

        if(retval != wsize) {
            break; //EOF or write error
        }
    }

    /* end of the critical section */
    pthread_mutex_unlock(&stream->lock);

    return total;
}

int _fseek(FILE *stream, long offset, int whence)
{
    return lseek(stream->fd, offset, whence);
}

int _fileno(FILE *stream)
{
    return stream->fd;
}
