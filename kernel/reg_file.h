#ifndef __REG_FILE_H__
#define __REG_FILE_H__

#include "fs.h"

struct reg_file {
    int pos;
    struct inode* file_inode;
    struct file file;
};

int reg_file_init(struct file **files, struct inode *file_inode, struct memory_pool *mem_pool);

#endif

