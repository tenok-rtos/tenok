/**
 * The memory an applet leaves behind.
 *
 * BusyBox is written for a system where leaving an applet hands its memory
 * back to the kernel, and it leaves memory behind accordingly. Every block it
 * allocates is therefore chained in a header of its own and stamped with the
 * applet that asked for it, so that returning from one takes back what it did
 * not free. The memory itself comes from Tenok's heap.
 *
 * No allocation may outlive the run that made it, which holds: the only path
 * that puts one somewhere long lived is the putenv() of ash, and that one is
 * reachable only for NOEXEC applets after a fork.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tenok.h>

#include "../../include/busybox/applet.h"
#include "internal.h"

struct block {
    size_t size;
    struct block *prev;
    struct block *next;
    /* Which applet was running when this was allocated */
    unsigned generation;
    char data[0];
};

#define BLOCK_HEADER sizeof(struct block)

static struct block *block_list;
static unsigned generation;

void *applet_malloc(size_t size)
{
    struct block *block = malloc(size + BLOCK_HEADER);

    if (!block) {
        errno = ENOMEM;
        return 0;
    }

    block->size = size;
    block->generation = generation;
    block->prev = NULL;
    block->next = block_list;

    if (block_list)
        block_list->prev = block;

    block_list = block;

    return (char *) block + BLOCK_HEADER;
}

char *applet_strdup(const char *s)
{
    size_t size = strlen(s) + 1;
    char *copy = applet_malloc(size);

    if (!copy)
        return NULL;

    memcpy(copy, s, size);

    return copy;
}

char *applet_strndup(const char *s, size_t n)
{
    size_t len = strnlen(s, n);
    char *copy = applet_malloc(len + 1);

    if (!copy)
        return NULL;

    memcpy(copy, s, len);
    copy[len] = '\0';

    return copy;
}

void applet_free(void *ptr)
{
    if (!ptr)
        return;

    struct block *block = (struct block *) ((char *) ptr - BLOCK_HEADER);

    if (block->prev)
        block->prev->next = block->next;
    else
        block_list = block->next;

    if (block->next)
        block->next->prev = block->prev;

    /* applet_run() releases whatever a run left behind, which is only correct
     * while nothing outlives the run that allocated it. Nothing enforces
     * that, so the memory is filled with a value that is neither a plausible
     * pointer nor a plausible string: reading it again faults or prints
     * visibly wrong, instead of quietly returning what used to be there.
     */
    memset(block->data, 0xde, block->size);

    free(block);
}

void *applet_calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr = applet_malloc(total);

    if (ptr)
        memset(ptr, 0, total);

    return ptr;
}

void *applet_realloc(void *ptr, size_t size)
{
    if (!ptr)
        return applet_malloc(size);

    if (size == 0) {
        applet_free(ptr);
        return 0;
    }

    /* Resizing the block resizes its header along with its contents, so the
     * chain survives the move and a block that grows in place costs nothing
     */
    struct block *block = (struct block *) ((char *) ptr - BLOCK_HEADER);
    struct block *moved = realloc(block, size + BLOCK_HEADER);

    if (!moved) {
        errno = ENOMEM;
        return 0;
    }

    moved->size = size;

    /* The block may have moved, so whatever points at it has to be told */
    if (moved->prev)
        moved->prev->next = moved;
    else
        block_list = moved;

    if (moved->next)
        moved->next->prev = moved;

    return (char *) moved + BLOCK_HEADER;
}

int vasprintf(char **strp, const char *format, va_list ap)
{
    va_list copy;
    va_copy(copy, ap);
    int len = vsnprintf(NULL, 0, format, copy);
    va_end(copy);

    if (len < 0)
        return -1;

    /* Handed over to BusyBox, whose free() is applet_free() */
    *strp = applet_malloc(len + 1);
    if (!*strp)
        return -1;

    return vsnprintf(*strp, len + 1, format, ap);
}

/* Releases what a run of BusyBox left behind */
void memory_release_all(void)
{
    while (block_list)
        applet_free((char *) block_list + BLOCK_HEADER);
}

void memory_enter(unsigned depth)
{
    generation = depth;
}

/* Whatever the applet allocated and did not free goes with it. A NOFORK
 * applet returns nothing but a status, so nothing it allocated can still be
 * wanted; freed memory is poisoned, so a violation is loud.
 */
void memory_leave(unsigned depth)
{
    struct block *block = block_list;

    while (block) {
        struct block *next = block->next;

        if (depth > 0 && block->generation >= depth)
            applet_free((char *) block + BLOCK_HEADER);

        block = next;
    }

    generation = (depth > 0) ? depth - 1 : 0;
}
