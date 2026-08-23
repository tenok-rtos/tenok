#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/reent.h>
#include <unistd.h>

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

    if (size == 0 || nmemb == 0)
        return 0;

    size_t remained = size * nmemb;
    size_t total = 0;

    while (remained > 0) {
        size_t rsize = (remained > MAX_READ_SIZE) ? MAX_READ_SIZE : remained;
        ssize_t retval = read(_stream->fd, (char *) ptr + total, rsize);

        /* End of the file or read error */
        if (retval <= 0)
            break;

        total += retval;
        remained -= retval;

        /* A short read means the end of the file is reached */
        if ((size_t) retval < rsize)
            break;
    }

    /* The function returns the number of the items that were read */
    return total / size;
}

size_t _fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    __FILE *_stream = (__FILE *) stream;

    if (size == 0 || nmemb == 0)
        return 0;

    size_t remained = size * nmemb;
    size_t total = 0;

    while (remained > 0) {
        size_t wsize = (remained > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : remained;
        ssize_t retval = write(_stream->fd, (char *) ptr + total, wsize);

        /* Write error */
        if (retval <= 0)
            break;

        total += retval;
        remained -= retval;

        /* A short write means there is no space left */
        if ((size_t) retval < wsize)
            break;
    }

    /* The function returns the number of the items that were written */
    return total / size;
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
