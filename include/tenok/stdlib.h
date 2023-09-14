#ifndef __STDLIB_H__
#define __STDLIB_H__

#define exit(status) _exit(status)
void _exit(int status);

void *malloc(size_t size);
void free(void *ptr);

#endif
