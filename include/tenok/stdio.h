#ifndef __STDIO_H__
#define __STDIO_H__

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>

#include <kernel/spinlock.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct {
    spinlock_t lock;
    int fd;
} FILE;

int fopen(const char *pathname, const char *mode, FILE *file);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
int fileno(FILE *stream);

/* the following functions link to the glibc */
int sprintf (char *str, const char *format, ...);
int snprintf (char *str, size_t size, const char *format, ...);
int vsprintf (char *str, const char *format, va_list);
int vsnprintf(char *str, size_t,  const char *format, va_list);

#endif
