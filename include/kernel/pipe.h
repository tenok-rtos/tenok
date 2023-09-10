#ifndef __PIPE_H__
#define __PIPE_H__

#include <stdio.h>

#include <kernel/ringbuf.h>

typedef struct ringbuf pipe_t;

pipe_t *generic_pipe_create(size_t nmem, size_t size);

ssize_t generic_pipe_read_isr(pipe_t *pipe, char *buf, size_t size);
ssize_t generic_pipe_write_isr(pipe_t *pipe, const char *buf, size_t size);

ssize_t generic_pipe_read(pipe_t *pipe, char *buf, size_t size);
ssize_t generic_pipe_write(pipe_t *pipe, const char *buf, size_t size);

#endif
