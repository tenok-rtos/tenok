#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "file.h"
#include "mpool.h"
#include "syscall.h"
#include "reg_file.h"

long reg_file_llseek(struct file *filp, long offset, int whence);
ssize_t reg_file_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t reg_file_write(struct file *filp, const char *buf, size_t size, loff_t offset);

static struct file_operations reg_file_ops = {
	.llseek = reg_file_llseek,
	.read = reg_file_read,
	.write = reg_file_write
};

int reg_file_init(struct inode *file_inode, struct file **files, struct memory_pool *mem_pool)
{
	/* create new regular file */
	struct reg_file *reg_file = memory_pool_alloc(mem_pool, sizeof(struct reg_file));
	reg_file->pos         = 0;
	reg_file->file_inode  = file_inode;
	reg_file->file.f_op   = &reg_file_ops;

	files[file_inode->i_fd] = &reg_file->file;
	files[file_inode->i_fd]->file_inode = file_inode;

	return 0;
}

ssize_t reg_file_read(struct file *filp, char *buf, size_t size, loff_t offset)
{
	struct reg_file *reg_file = container_of(filp, struct reg_file, file);

	uint8_t *read_addr = reg_file->file_inode->i_data + offset + reg_file->pos;
	int retval = fs_read(reg_file->file_inode, read_addr, (uint8_t *)buf, size);

	if(retval >= 0)
		reg_file->pos += size;

	return size;
}

ssize_t reg_file_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
	struct reg_file *reg_file = container_of(filp, struct reg_file, file);

	uint8_t *write_addr = reg_file->file_inode->i_data + offset + reg_file->pos;
	int retval = fs_write(reg_file->file_inode, write_addr, (uint8_t *)buf, size);

	if(retval >= 0)
		reg_file->pos += size;

	return retval;
}

long reg_file_llseek(struct file *filp, long offset, int whence)
{
	struct reg_file *reg_file = container_of(filp, struct reg_file, file);

	char new_pos;

	switch(whence) {
	case SEEK_SET:
		new_pos = offset;
		break;
	case SEEK_END:
		new_pos = ROOTFS_BLK_SIZE + offset;
		break;
	case SEEK_CUR:
		new_pos = reg_file->pos + offset;
		break;
	default:
		return -1;
	}

	if((new_pos >= 0) && (new_pos <= ROOTFS_BLK_SIZE)) {
		reg_file->pos = new_pos;

		return new_pos;
	} else {
		return -1;
	}
}
