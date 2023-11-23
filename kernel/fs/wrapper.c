#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <fs/fs.h>
#include <fs/reg_file.h>
#include <kernel/kernel.h>

#include "kconfig.h"

#define MAX_READ_SIZE 100
#define MAX_WRITE_SIZE 100

FILE *_fopen(const char *pathname, const char *mode)
{
    /* Open the file with the system call */
    int fd = open(pathname, 0);

    /* Failed to open the file */
    if (fd < 0)
        return NULL;

    /* Allocate new file stream */
    FILE *stream = malloc(sizeof(FILE));
    __FILE *_stream = (__FILE *) stream;

    /* Failed to allocate file stream */
    if (!_stream) {
        close(fd);
        return NULL;
    }

    _stream->fd = fd;

    return stream;
}

int _fclose(FILE *stream)
{
    __FILE *_stream = (__FILE *) stream;
    int retval = close(_stream->fd);
    free(_stream);
    return retval;
}

size_t _fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    __FILE *_stream = (__FILE *) stream;

    size_t nbytes = size * nmemb;
    int times = (nbytes - 1) / MAX_READ_SIZE + 1;
    int total = 0;

    int retval;
    for (int i = 0; i < times; i++) {
        int rsize = (nbytes >= MAX_READ_SIZE) ? MAX_READ_SIZE : nbytes;
        retval = read(_stream->fd, (char *) ((uintptr_t) ptr + total), rsize);

        total += rsize;
        nbytes -= rsize;

        if (retval != rsize) {
            break; /* EOF or read error */
        }
    }

    return total;
}

size_t _fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    __FILE *_stream = (__FILE *) stream;

    size_t nbytes = size * nmemb;
    int times = (nbytes - 1) / MAX_WRITE_SIZE + 1;
    int total = 0;

    int retval;
    for (int i = 0; i < times; i++) {
        /* Write file */
        int wsize = (nbytes >= MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : nbytes;
        retval = write(_stream->fd, (char *) ((uintptr_t) ptr + total), wsize);

        total += wsize;
        nbytes -= wsize;

        if (retval != wsize) {
            break; /* EOF or write error */
        }
    }

    return total;
}

int _fseek(FILE *stream, long offset, int whence)
{
    __FILE *_stream = (__FILE *) stream;
    return lseek(_stream->fd, offset, whence);
}

int _fileno(FILE *stream)
{
    __FILE *_stream = (__FILE *) stream;
    return _stream->fd;
}
