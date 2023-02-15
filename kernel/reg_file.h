#ifndef __REG_FILE_H__
#define __REG_FILE_H__

struct reg_file {
	int pos;

	struct file *driver_file;
	struct file file;
};

#endif
