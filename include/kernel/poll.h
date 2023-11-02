/**
 * @file
 */
#ifndef __KERNEL_POLL_H__
#define __KERNEL_POLL_H__

#include <fs/fs.h>

void poll_notify(struct file *notify_file);

#endif
