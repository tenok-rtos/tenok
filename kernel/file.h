#ifndef __FILE_H__
#define __FILE_H__

#include "spinlock.h"

#define FILE rtenv__FILE

#define fopen  __fopen
#define fread  __fread
#define fwrite __fwrite
#define fseek  __fseek
#define fileno __fileno

typedef struct {
    spinlock_t lock;
    int fd;
} rtenv__FILE;

int fopen(const char *pathname, const char *mode, FILE *file);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fstream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fstream);
int fseek(FILE *stream, long offset, int whence);
int fileno(FILE *stream);

#endif
