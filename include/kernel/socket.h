/**
 * @file
 * A socket of the stack, as a file of this system.
 */
#ifndef __KERNEL_SOCKET_H__
#define __KERNEL_SOCKET_H__

#include <stdbool.h>

#include <fs/fs.h>

bool file_is_socket(struct file *filp);

/* Wrap a socket of the stack in a file. The file holds no descriptor of its
 * own until one is handed out for it
 */
struct file *socket_file_alloc(int lwip_fd);

/* Which socket of the stack the file stands for, or -1 when it stands for
 * something else
 */
int socket_file_lwip_fd(struct file *filp);

/* Counted the way a pipe is: the socket is closed when the last descriptor
 * naming it is given up
 */
void socket_file_take(struct file *filp);
bool socket_file_give_up(struct file *filp);

#endif
