#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "fs.h"
#include "mpool.h"
#include "syscall.h"
#include "reg_file.h"
#include "kconfig.h"

long reg_file_llseek(struct file *filp, long offset, int whence);
ssize_t reg_file_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t reg_file_write(struct file *filp, const char *buf, size_t size, loff_t offset);

extern struct mount mount_points[MOUNT_CNT_MAX + 1];

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

	/* get file inode */
	struct inode *inode = reg_file->file_inode;

	/* get storage device file */
	struct file *driver_file = mount_points[inode->i_rdev].dev_file;

	/* calculate the read address */
	uint32_t read_addr = (uint32_t)inode->i_data + offset + reg_file->pos;

	/* read data */
	int retval = driver_file->f_op->read(NULL, (uint8_t *)buf, size, read_addr);

	if(retval >= 0)
		reg_file->pos += size;

	return size;
}

ssize_t reg_file_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
	struct reg_file *reg_file = container_of(filp, struct reg_file, file);

	/* get file inode */
	struct inode *inode = reg_file->file_inode;

	/* get storage device file */
	struct file *driver_file = mount_points[inode->i_rdev].dev_file;

	/* calculate the read address */
	uint32_t write_addr = (uint32_t)inode->i_data + offset + reg_file->pos;

	/* write data */
	int retval = driver_file->f_op->write(NULL, (uint8_t *)buf, size, write_addr);

	if(retval >= 0) {
		reg_file->pos += size;
		reg_file->file.file_inode->i_size += size;
	}

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
