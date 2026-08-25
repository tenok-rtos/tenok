/**
 * @file
 */
#ifndef __STDIO_H__
#define __STDIO_H__

#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/reent.h>
#include <sys/types.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define EOF (-1)

#define fopen _fopen
#define fclose _fclose
#define fread _fread
#define fwrite _fwrite
#define fseek _fseek
#define ftell _ftell
#define rewind _rewind
#define fileno _fileno
#define freopen _freopen
#define fseeko _fseeko
#define ftello _ftello
#define fputs _fputs
#define fputc _fputc
#define putchar _putchar
#define puts _puts
#define fgetc _fgetc
#define getchar _getchar
#define fgets _fgets
#define feof _feof
#define ferror _ferror
#define clearerr _clearerr
#define fflush _fflush
#define setvbuf _setvbuf
#define setbuf _setbuf

/* The two the standard allows to be macros over the calls above */
#define putc(c, stream) fputc(c, stream)
#define getc(stream) fgetc(stream)

/* A stream of Tenok carries no lock, so these are the locked half under
 * another name. They name the implementations, which a caller cannot redefine
 */
#define getc_unlocked(stream) _fgetc(stream)
#define getchar_unlocked() _getchar()
#define putc_unlocked(c, stream) _fputc(c, stream)
#define putchar_unlocked(c) _putchar(c)
#define fputc_unlocked(c, stream) _fputc(c, stream)
#define fgetc_unlocked(stream) _fgetc(stream)
#define fgets_unlocked(s, size, stream) _fgets(s, size, stream)
#define fputs_unlocked(s, stream) _fputs(s, stream)
#define fread_unlocked(p, size, n, stream) _fread(p, size, n, stream)
#define fwrite_unlocked(p, size, n, stream) _fwrite(p, size, n, stream)
#define feof_unlocked(stream) _feof(stream)
#define ferror_unlocked(stream) _ferror(stream)
#define clearerr_unlocked(stream) _clearerr(stream)
#define fflush_unlocked(stream) _fflush(stream)
#define fileno_unlocked(stream) _fileno(stream)

#define flockfile(stream) ((void) (stream))
#define funlockfile(stream) ((void) (stream))
#define ftrylockfile(stream) (0)

#define __SIZEOF_FILE sizeof(__FILE)

typedef union {
    char __size[__SIZEOF_FILE];
    uint32_t __align;
} FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/**
 * @brief  Open the file whose name is the string pointed to by pathname and
 *         associate a stream with it
 * @param  pathname: The pathname of the file to open.
 * @param  mode: Not used.
 * @retval FILE *: File stream object.
 */
FILE *fopen(const char *pathname, const char *mode);

/**
 * @brief  Put a stream on a descriptor that is already open
 * @param  fd: The file descriptor to provide.
 * @param  mode: Not used, the descriptor carries how it was opened.
 * @retval FILE *: The stream on success and a null pointer on error, with
 *         the reason left in errno.
 */
FILE *fdopen(int fd, const char *mode);

/**
 * @brief  Point a stream that is already open at another file, which is how
 *         a program replaces one of the standard streams
 * @param  pathname: The pathname of the file to open.
 * @param  mode: The mode to open it with.
 * @param  stream: The stream to point at it.
 * @retval FILE *: The stream on success and a null pointer on error, with
 *         the reason left in errno.
 */
FILE *freopen(const char *pathname, const char *mode, FILE *stream);

/**
 * @brief  Reposition a stream, with the offset given as an off_t
 * @param  stream: The file stream to provide.
 * @param  offset: The new offset from the position whence names.
 * @param  whence: SEEK_SET, SEEK_CUR or SEEK_END.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int fseeko(FILE *stream, off_t offset, int whence);

/**
 * @brief  Report where a stream is, as an off_t
 * @param  stream: The file stream to provide.
 * @retval off_t: The position on success and -1 on error, with the reason
 *         left in errno.
 */
off_t ftello(FILE *stream);

/**
 * @brief  Close the given file stream
 * @param  stream: The file stream to provide.
 * @retval int: 0 on success and nonzero error number on error.
 */
int fclose(FILE *stream);

/**
 * @brief  Read nmemb items of data, each size bytes long, from the stream
 *         pointed to by stream, storing them at the location given by ptr
 * @param  ptr: The memory space for storing the read data.
 * @param  size: The number of nmemb bytes to read.
 * @param  nmemb: The bytes number to read at once.
 * @param  stream: The file stream to provide.
 * @retval size_t: The number of the items that were read, which is smaller
 *         than nmemb if the end of the file is reached or an error occurs.
 */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

/**
 * @brief  Write nmemb items of data, each size bytes long, to the stream
 *         pointed to by stream, obtaining them  from  the  location given by
 *         ptr
 * @param  ptr: The data to write to the file.
 * @param  size: The number of nmemb bytes to write.
 * @param  nmemb: The byte number to write at once.
 * @param  stream: The file stream to provide.
 * @retval size_t: The number of the items that were written, which is
 *         smaller than nmemb if an error occurs.
 */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/**
 * @brief  Set the file position indicator for the stream pointed to by stream
 * @param  stream: The file stream to provide.
 * @param  offset: The new offset to the position specified by whence.
 * @param  whence: The start position of the new offset.
 * @retval int: 0 on success and -1 on error.
 */
int fseek(FILE *stream, long offset, int whence);

/**
 * @brief  Obtain the current value of the file position indicator
 * @param  stream: The file stream to provide.
 * @retval long: The current file position on success and -1 on error.
 */
long ftell(FILE *stream);

/**
 * @brief  Set the file position indicator to the beginning of the file
 * @param  stream: The file stream to provide.
 * @retval None
 */
void rewind(FILE *stream);

/**
 * @brief  Examine the argument stream and returns the integer file descriptor
 *         used to implement this stream
 * @param  stream: The file stream to provide.
 * @retval int: The file descriptor integer.
 */
int fileno(FILE *stream);

/**
 * @brief  Change the name or the location of a file
 * @param  oldpath: The pathname of the file to rename.
 * @param  newpath: The new pathname of the file.
 * @retval int: 0 on success and nonzero error number on error.
 */
int rename(const char *oldpath, const char *newpath);

/**
 * @brief  Delete a file or an empty directory
 * @param  pathname: The pathname of the file to remove.
 * @retval int: 0 on success and nonzero error number on error.
 */
int remove(const char *pathname);

/**
 * @brief  Format and print data to the standard output
 * @param  format: Format string.
 * @param  ...: Print arguments.
 * @retval Upon successful return, the function returns the number of
 *         characters printed; otherwie a negative value is returned.
 */
int printf(const char *format, ...);

/**
 * @brief  Format and print data to a file
 * @param  stream: The pointer to the file stream.
 * @param  format: Format string.
 * @param  ...: Print arguments.
 * @retval Upon successful return, the function returns the number of
 *         characters printed; otherwie a negative value is returned.
 */
int fprintf(FILE *stream, const char *format, ...);

/**
 * @brief  Format and print data to a file
 * @param  str: The pointer to the print buffer.
 * @param  format: Format string.
 * @param  ...: Print arguments.
 * @retval Upon successful return, the function returns the number of
 *         characters printed; otherwie a negative value is returned.
 */
int dprintf(int fd, const char *format, ...);

/**
 * @brief  Format and print data to a buffer
 * @param  str: The pointer to the print buffer.
 * @param  format: Format string.
 * @param  ...: Print arguments.
 * @retval Upon successful return, the function returns the number of
 *         characters printed; otherwie a negative value is returned.
 */
int sprintf(char *str, const char *format, ...);

/**
 * @brief  Format and print data to a buffer with limited buffer size
 * @param  str: The pointer to the print buffer.
 * @param  size: The size of the buffer.
 * @param  format: Format string.
 * @param  ...: Print arguments.
 * @retval Upon successful return, the function returns the number of
 *         characters printed; otherwie a negative value is returned.
 */
int snprintf(char *str, size_t size, const char *format, ...);

/**
 * @brief  Format and print data to the standard output with argument list
 * @param  format: Format string.
 * @param  ap: Argument list.
 * @retval Upon successful return, the function returns the number of
 *         characters printed; otherwie a negative value is returned.
 */
int vprintf(const char *format, va_list ap);

/**
 * @brief  Format and print data to a file with argument list
 * @param  stream: The pointer to the file stream.
 * @param  format: Format string.
 * @param  ap: Argument list.
 * @retval Upon successful return, the function returns the number of
 *         characters printed; otherwie a negative value is returned.
 */
int vfprintf(FILE *stream, const char *format, va_list ap);

/**
 * @brief  Format and print data to a file with argument list
 * @param: fd: File descriptor number.
 * @param  format: Format string.
 * @param  ap: Argument list.
 * @retval Upon successful return, the function returns the number of
 *         characters printed; otherwie a negative value is returned.
 */
int vdprintf(int fd, const char *format, va_list ap);

/**
 * @brief  Format and print data to a buffer with argument list
 * @param  str: The pointer to the print buffer.
 * @param  size: The size of the buffer.
 * @param  format: Format string.
 * @param  ap: Argument list.
 * @retval Upon successful return, the function returns the number of
 *         characters printed; otherwie a negative value is returned.
 */
int vsprintf(char *str, const char *format, va_list ap);

/**
 * @brief  Format and print data to a buffer with limited size and argument
 *         list
 * @param  str: The pointer to the print buffer.
 * @param  size: The size of the buffer.
 * @param  format: Format string.
 * @param  ap: Argument list.
 * @retval Upon successful return, the function returns the number of
 *         characters printed; otherwie a negative value is returned.
 */
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

/**
 * @brief  Write a string to the stream, without its terminating null byte
 * @param  s: The string to write.
 * @param  stream: The file stream to provide.
 * @retval int: A nonnegative number on success and EOF on error.
 */
int fputs(const char *s, FILE *stream);

/**
 * @brief  Write a character to the stream
 * @param  c: The character to write.
 * @param  stream: The file stream to provide.
 * @retval int: The character written on success and EOF on error.
 */
int fputc(int c, FILE *stream);

/**
 * @brief  Write a character to the standard output
 * @param  c: The character to write.
 * @retval int: The character written on success and EOF on error.
 */
int putchar(int c);

/**
 * @brief  Write a string and a newline to the standard output
 * @param  s: The string to write.
 * @retval int: A nonnegative number on success and EOF on error.
 */
int puts(const char *s);

/**
 * @brief  Read a character from the stream
 * @param  stream: The file stream to provide.
 * @retval int: The character read, and EOF at the end of the file or on error.
 */
int fgetc(FILE *stream);

/**
 * @brief  Read a character from the standard input
 * @retval int: The character read, and EOF at the end of the file or on error.
 */
int getchar(void);

/**
 * @brief  Read a line from the stream, the newline included, and terminate it
 * @param  s: The buffer to read into.
 * @param  size: The size of the buffer, the null byte included.
 * @param  stream: The file stream to provide.
 * @retval char *: The buffer on success and a null pointer at the end of the
 *         file or on error.
 */
char *fgets(char *s, int size, FILE *stream);

/**
 * @brief  Tell whether the end of the stream has been reached
 * @param  stream: The file stream to provide.
 * @retval int: Nonzero once the end of the file was reached.
 */
int feof(FILE *stream);

/**
 * @brief  Tell whether the stream has failed
 * @param  stream: The file stream to provide.
 * @retval int: Nonzero once a call on the stream has failed.
 */
int ferror(FILE *stream);

/**
 * @brief  Clear the end of file and the error state of the stream
 * @param  stream: The file stream to provide.
 */
void clearerr(FILE *stream);

/**
 * @brief  Hand over what the stream is holding. Tenok does not buffer, so
 *         there is never anything to hand over
 * @param  stream: The file stream to provide.
 * @retval int: 0 on success and EOF on error.
 */
int fflush(FILE *stream);

/* How a stream may be asked to buffer. Tenok writes every stream straight
 * through, which is the last of the three
 */
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

/* The size a program is invited to hand setvbuf(), which Tenok does not use */
#define BUFSIZ 1024

/**
 * @brief  Ask the stream to buffer what is written to it. Tenok reaches the
 *         device on the call that writes, so the only buffering it can be
 *         given is the one it already has.
 * @param  stream: The file stream to provide.
 * @param  buf: The buffer to use, which Tenok does not take.
 * @param  mode: One of _IOFBF, _IOLBF and _IONBF.
 * @param  size: The size of the buffer.
 * @retval int: 0 when _IONBF was asked for and nonzero otherwise, with errno
 *         set to EINVAL.
 */
int setvbuf(FILE *stream, char *buf, int mode, size_t size);

/**
 * @brief  Ask the stream to buffer what is written to it, the way setvbuf()
 *         does, without reporting whether it could be done.
 * @param  stream: The file stream to provide.
 * @param  buf: The buffer to use, or NULL to ask for no buffering.
 */
void setbuf(FILE *stream, char *buf);

#endif
