#include <errno.h>
#include <fcntl.h>
#include <fs/fs.h>
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
    int fd = open(pathname, flags, FS_DEFAULT_FILE_MODE);

    /* Failed to open the file. open() has already left the reason in errno */
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

/* The descriptor is already open, the stream only has to name it */
FILE *fdopen(int fd, const char *mode)
{
    FILE *stream = malloc(sizeof(FILE));

    if (!stream) {
        errno = ENOMEM;
        return NULL;
    }

    ((__FILE *) stream)->fd = fd;
    ((__FILE *) stream)->eof = 0;
    ((__FILE *) stream)->err = 0;

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
        if (retval <= 0) {
            if (retval == 0)
                _stream->eof = 1;
            else
                _stream->err = 1;
            break;
        }

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
        if (retval <= 0) {
            _stream->err = 1;
            break;
        }

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

/* Newlib ships these, but its FILE is not Tenok's */
int _fputs(const char *s, FILE *stream)
{
    __FILE *_stream = (__FILE *) stream;
    size_t len = strlen(s);

    if (write(_stream->fd, s, len) != (ssize_t) len) {
        _stream->err = 1;
        return EOF;
    }

    return len;
}

int _fputc(int c, FILE *stream)
{
    __FILE *_stream = (__FILE *) stream;
    char ch = (char) c;

    if (write(_stream->fd, &ch, 1) != 1) {
        _stream->err = 1;
        return EOF;
    }

    return (unsigned char) c;
}

int _putchar(int c)
{
    return _fputc(c, stdout);
}

int _puts(const char *s)
{
    if (_fputs(s, stdout) == EOF)
        return EOF;

    return _fputc('\n', stdout);
}

int _fgetc(FILE *stream)
{
    __FILE *_stream = (__FILE *) stream;
    unsigned char c;

    ssize_t retval = read(_stream->fd, &c, 1);

    if (retval == 0) {
        _stream->eof = 1;
        return EOF;
    }

    if (retval < 0) {
        _stream->err = 1;
        return EOF;
    }

    return c;
}

int _getchar(void)
{
    return _fgetc(stdin);
}

char *_fgets(char *s, int size, FILE *stream)
{
    int len = 0;

    if (size <= 0)
        return NULL;

    while (len < size - 1) {
        int c = _fgetc(stream);

        if (c == EOF)
            break;

        s[len++] = (char) c;

        if (c == '\n')
            break;
    }

    /* A line of no characters at all is the end of the file */
    if (len == 0)
        return NULL;

    s[len] = '\0';

    return s;
}

int _feof(FILE *stream)
{
    return ((__FILE *) stream)->eof;
}

int _ferror(FILE *stream)
{
    return ((__FILE *) stream)->err;
}

void _clearerr(FILE *stream)
{
    __FILE *_stream = (__FILE *) stream;

    _stream->eof = 0;
    _stream->err = 0;
}

/* Tenok writes through to the file, there is nothing held back */
int _fflush(FILE *stream)
{
    return 0;
}
