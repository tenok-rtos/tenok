#ifndef __REG_FILE_H__
#define __REG_FILE_H__

#include "file.h"

struct reg_file {
	int pos;
	struct inode* file_inode;
	struct file file;
};

int reg_file_init(struct inode *file_inode, struct file **files, struct memory_pool *mem_pool);

#endif

