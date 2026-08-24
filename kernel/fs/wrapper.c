#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reent.h>
#include <unistd.h>

#define MAX_READ_SIZE 100
#define MAX_WRITE_SIZE 100

/* Convert a fopen() mode string into the flags of the open() system call */
static int fopen_flags(const char *mode)
{
    /* The characters that carry no meaning for Tenok are ignored */
    bool update = (strchr(mode, '+') != NULL);

    switch (mode[0]) {
    case 'r':
        return update ? O_RDWR : O_RDONLY;
    case 'w':
        return (update ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
    case 'a':
        return (update ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
    default:
        return -1;
    }
}

FILE *_fopen(const char *pathname, const char *mode)
{
    /* An empty mode string is accepted and means read only */
    int flags = (!mode || mode[0] == '\0') ? O_RDONLY : fopen_flags(mode);
    if (flags < 0)
        return NULL;

    /* Open the file with the system call */
    int fd = open(pathname, flags);

    /* Failed to open the file. The system call reports the reason as a
     * negative error number and does not touch errno, which is where a
     * caller of the standard library looks for it.
     */
    if (fd < 0) {
        errno = -fd;
        return NULL;
    }

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

    /* Unlike lseek(), fseek() reports success rather than the new position */
    return (lseek(_stream->fd, offset, whence) < 0) ? -1 : 0;
}

long _ftell(FILE *stream)
{
    __FILE *_stream = (__FILE *) stream;
    return lseek(_stream->fd, 0, SEEK_CUR);
}

void _rewind(FILE *stream)
{
    __FILE *_stream = (__FILE *) stream;
    lseek(_stream->fd, 0, SEEK_SET);
}

int _fileno(FILE *stream)
{
    __FILE *_stream = (__FILE *) stream;
    return _stream->fd;
}
