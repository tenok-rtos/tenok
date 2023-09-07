#ifndef __FILE_H__
#define __FILE_H__

#include "spinlock.h"

#define FILE tenok__FILE

#define fopen  __fopen
#define fclose __fclose
#define fread  __fread
#define fwrite __fwrite
#define fseek  __fseek
#define fileno __fileno

typedef struct {
    spinlock_t lock;
    int fd;
} tenok__FILE;

int fopen(const char *pathname, const char *mode, FILE *file);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
int fileno(FILE *stream);

#endif
