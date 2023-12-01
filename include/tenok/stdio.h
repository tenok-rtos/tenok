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

#define fopen _fopen
#define fclose _fclose
#define fread _fread
#define fwrite _fwrite
#define fseek _fseek
#define fileno _fileno

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
 * @retval int: The read number on success and nonzero error number on error.
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
 * @retval int: The write number on success and nonzero error number on error.
 */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/**
 * @brief  Set the file position indicator for the stream pointed to by stream
 * @param  stream: The file stream to provide.
 * @param  offset: The new offset to the position specified by whence.
 * @param  whence: The start position of the new offset.
 * @retval int: The new file position on success and nonzero error number on
 *         error.
 */
int fseek(FILE *stream, long offset, int whence);

/**
 * @brief  Examine the argument stream and returns the integer file descriptor
 *         used to implement this stream
 * @param  stream: The file stream to provide.
 * @retval int: The file descriptor integer.
 */
int fileno(FILE *stream);

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

#endif
