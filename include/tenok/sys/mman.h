/* Tenok has no MMU, these are named and nothing implements them */
#ifndef _TENOK_SYS_MMAN_H
#define _TENOK_SYS_MMAN_H

#include <sys/types.h>

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define PROT_NONE 0x0

#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *) -1)

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t off);
int munmap(void *addr, size_t length);

#endif
