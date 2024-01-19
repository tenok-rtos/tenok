#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <common/list.h>
#include <fs/fs.h>
#include <fs/reg_file.h>
#include <kernel/interrupt.h>

#include "kconfig.h"

off_t reg_file_lseek(struct file *filp, off_t offset, int whence);
ssize_t reg_file_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t reg_file_write(struct file *filp,
                       const char *buf,
                       size_t size,
                       off_t offset);
int reg_file_open(struct inode *inode, struct file *file);

extern struct mount mount_points[MOUNT_MAX + 1];

static struct file_operations reg_file_ops = {
    .lseek = reg_file_lseek,
    .read = reg_file_read,
    .write = reg_file_write,
    .open = reg_file_open,
};

int reg_file_init(struct file **files,
                  struct inode *file_inode,
                  struct reg_file *reg_file)
{
    /* Initialize the data pointer */
    reg_file->pos = 0;

    /* Register regular file on the file table */
    memset(&reg_file->file, 0, sizeof(reg_file->file));
    reg_file->file.f_inode = file_inode;
    reg_file->file.f_op = &reg_file_ops;
    files[file_inode->i_fd] = &reg_file->file;

    return 0;
}

int reg_file_open(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t __reg_file_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    struct reg_file *reg_file = container_of(filp, struct reg_file, file);

    /* get the inode of the regular file */
    struct inode *inode = reg_file->file.f_inode;

    /* Get the driver file of the storage device */
    struct file *driver_file = mount_points[inode->i_rdev].dev_file;

    uint32_t blk_head_size = sizeof(struct block_header);
    uint32_t blk_free_size = FS_BLK_SIZE - sizeof(struct block_header);

    size_t remained_size = size;
    size_t read_size = 0;

    while (remained_size) {
        /* Calculate the block index corresponding to the current file read
         * position */
        int blk_i = reg_file->pos / blk_free_size;

        /* Get the start address of the block */
        uint32_t blk_start_addr = fs_get_block_addr(inode, blk_i);

        /* Calculate the block offset of the current read position */
        uint8_t blk_pos = reg_file->pos % blk_free_size;

        /* Calculate the read address */
        uint32_t read_addr = blk_start_addr + blk_head_size + blk_pos;

        /* Calculate the max size to read from the current block */
        uint32_t max_read_size = blk_free_size - blk_pos;
        if (remained_size > max_read_size) {
            read_size = max_read_size;
        } else {
            read_size = remained_size;
        }

        /* Read data */
        int buf_pos = size - remained_size;
        int retval = driver_file->f_op->read(NULL, (char *) &buf[buf_pos],
                                             read_size, read_addr);

        /* Read failure */
        if (retval < 0)
            break;

        /* Update the remained size */
        remained_size -= read_size;

        /* Update the file read position */
        reg_file->pos += read_size;
    }

    return size - remained_size;
}

ssize_t reg_file_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    preempt_disable();
    ssize_t retval = __reg_file_read(filp, buf, size, offset);
    preempt_enable();

    return retval;
}

static ssize_t __reg_file_write(struct file *filp,
                         const char *buf,
                         size_t size,
                         off_t offset)
{
    struct reg_file *reg_file = container_of(filp, struct reg_file, file);

    /* Get the inode of the regular file */
    struct inode *inode = reg_file->file.f_inode;

    if (inode->i_rdev != RDEV_ROOTFS) {
        return 0;
    }

    /* Get the driver file of the storage device */
    struct file *driver_file = mount_points[inode->i_rdev].dev_file;

    uint32_t blk_head_size = sizeof(struct block_header);
    uint32_t blk_free_size = FS_BLK_SIZE - sizeof(struct block_header);

    size_t remained_size = size;
    size_t write_size = 0;

    while (remained_size) {
        /* Calculate the block index corresponding to the current file read
         * position */
        int blk_i = reg_file->pos / blk_free_size;

        /* Get the start address of the block */
        uint32_t blk_start_addr = (blk_i >= filp->f_inode->i_blocks)
                                      ? fs_file_append_block(inode)
                                      : fs_get_block_addr(inode, blk_i);

        /* Check if the block address is valid */
        if (blk_start_addr == (uint32_t) NULL) {
            break;
        }

        /* Calculate the block offset of the current read position */
        uint8_t blk_pos = reg_file->pos % blk_free_size;

        /* Calculate the write address */
        uint32_t write_addr = blk_start_addr + blk_head_size + blk_pos;

        /* Calculate the max size to read from the current block */
        uint32_t max_write_size = blk_free_size - blk_pos;
        if (remained_size > max_write_size) {
            write_size = max_write_size;
        } else {
            write_size = remained_size;
        }

        /* Write data */
        int buf_pos = size - remained_size;
        int retval = driver_file->f_op->write(NULL, (char *) &buf[buf_pos],
                                              write_size, write_addr);

        /* Write failure */
        if (retval < 0)
            break;

        /* Update the remained size */
        remained_size -= write_size;

        /* Update the file read position */
        reg_file->pos += write_size;
    }

    /* Update the file size */
    filp->f_inode->i_size += size;

    return size - remained_size;
}

ssize_t reg_file_write(struct file *filp,
                       const char *buf,
                       size_t size,
                       off_t offset)
{
    preempt_disable();
    ssize_t retval = __reg_file_write(filp, buf, size, offset);
    preempt_enable();

    return retval;
}

static off_t __reg_file_lseek(struct file *filp, off_t offset, int whence)
{
    struct reg_file *reg_file = container_of(filp, struct reg_file, file);

    /* Get the inode of the regular file */
    struct inode *inode = reg_file->file.f_inode;

    char new_pos;

    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_END:
        new_pos = inode->i_size + offset;
        break;
    case SEEK_CUR:
        new_pos = reg_file->pos + offset;
        break;
    default:
        return -1;
    }

    /* Check if the new position is valid or not */
    if ((new_pos >= 0) && (new_pos < inode->i_size)) {
        reg_file->pos = new_pos;
        return new_pos;
    } else {
        return -1;
    }
}

off_t reg_file_lseek(struct file *filp, off_t offset, int whence)
{
    preempt_disable();
    off_t retval = __reg_file_lseek(filp, offset, whence);
    preempt_enable();

    return retval;
}
