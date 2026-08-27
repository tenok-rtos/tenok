/**
 * @file
 *
 * Tenok has no MMU, these are named and nothing implements them
 */
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

/* Asking for the mapping to be put where the caller says. The address it gives
 * back is where the contents already are, so this is named and nothing acts
 * on it, the same way the requested address itself is
 */
#define MAP_FIXED 0x10

#define MAP_FAILED ((void *) -1)

/**
 * @brief  Reach the contents of a file where they already are. Tenok has no
 *         address space to map anything into, so only a device that lives in
 *         memory of its own can answer this, and the address it gives back is
 *         where it already is.
 * @param  addr: Where the caller would like it, which Tenok cannot honour.
 * @param  length: How much of it to reach.
 * @param  prot: What the caller means to do with it.
 * @param  flags: How the mapping is to be shared.
 * @param  fd: The file to reach.
 * @param  off: Where in the file to start.
 * @retval void *: Where the contents are, and MAP_FAILED on error with the
 *         reason left in errno.
 */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t off);

/**
 * @brief  Give back what mmap() handed out. Nothing was mapped, so nothing is
 *         given back.
 * @param  addr: What mmap() gave.
 * @param  length: How much of it.
 * @retval int: Always 0.
 */
int munmap(void *addr, size_t length);
int munmap(void *addr, size_t length);

#endif
