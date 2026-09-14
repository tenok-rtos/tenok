/**
 * @file
 * A socket, as a file.
 *
 * The stack beside this system numbers its sockets in a space of its own, and
 * a program does not want a second kind of descriptor: it wants to read one,
 * write one, hand one to poll(), move one onto another with dup2(). So a
 * socket of the stack is wrapped in a file of this system, given a descriptor
 * like anything else, and everything that already works on a descriptor works
 * on it without knowing what is behind it.
 *
 * Like a pipe, a socket belongs to nothing but the descriptors that name it,
 * so it is counted the same way and closed when the last of them goes.
 */

#include <errno.h>
#include <stddef.h>

#include <fs/fs.h>
#include <kernel/kernel.h>
#include <mm/mm.h>

#include "kconfig.h"

#ifdef CONFIG_NET

/* Nothing here calls the stack.
 *
 * Everything the stack does it does by taking a lock or waiting on a queue,
 * and both of those are system calls -- a supervisor instruction, which from
 * inside the handling of one is a fault and not a call. So the kernel keeps
 * only the book: which descriptor names which socket, and how many of them
 * still do. The calling is left to the thread that asked, where a system call
 * is a call.
 */
struct socket_file {
    struct file file;
    int lwip_fd;
    int refs;
};

/* A socket answers none of these through the file system: what reads and
 * writes one goes to the stack directly. The table exists so that a file can
 * be told to be a socket by which table it carries
 */
static struct file_operations socket_file_ops = {0};

bool file_is_socket(struct file *filp)
{
    return filp && filp->f_op == &socket_file_ops;
}

struct file *socket_file_alloc(int lwip_fd)
{
    struct socket_file *sock = kmalloc(sizeof(struct socket_file));

    if (!sock)
        return NULL;

    sock->file.f_op = &socket_file_ops;
    sock->file.f_inode = NULL;
    sock->file.f_events = 0;
    sock->file.f_flags = 0;
    sock->file.f_pos = 0;
    sock->lwip_fd = lwip_fd;
    sock->refs = 0;

    return &sock->file;
}

/* Which socket of the stack a file stands for, or nothing when the file is
 * not one
 */
int socket_file_lwip_fd(struct file *filp)
{
    if (!file_is_socket(filp))
        return -1;

    return ((struct socket_file *) filp)->lwip_fd;
}

void socket_file_take(struct file *filp)
{
    if (file_is_socket(filp))
        ((struct socket_file *) filp)->refs++;
}

/* Answers whether that was the last descriptor naming the socket, so that
 * the caller can close it where closing is allowed
 */
bool socket_file_give_up(struct file *filp)
{
    if (!file_is_socket(filp))
        return false;

    struct socket_file *sock = (struct socket_file *) filp;

    if (--sock->refs > 0)
        return false;

    kfree(sock);

    return true;
}

#endif /* CONFIG_NET */
