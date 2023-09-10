#ifndef __REG_FILE_H__
#define __REG_FILE_H__

#include "fs.h"
#include "mpool.h"

struct reg_file {
    int pos;
    struct inode* f_inode;
    struct file file;
};

int reg_file_init(struct file **files, struct inode *file_inode, struct memory_pool *mem_pool);
ssize_t reg_file_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t reg_file_write(struct file *filp, const char *buf, size_t size, off_t offset);

#endif

