#ifndef __REG_FILE_H__
#define __REG_FILE_H__

struct reg_file {
	int pos;

	struct file *driver_file;
	struct file file;
};

int reg_file_init(int fd, struct file **files, struct file *driver_file, struct memory_pool *mem_pool);

#endif

