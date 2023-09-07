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

int reg_file_init(struct file **files, struct inode *file_inode, struct memory_pool *mem_pool)
{
    /* create a new regular file */
    struct reg_file *reg_file = memory_pool_alloc(mem_pool, sizeof(struct reg_file));
    reg_file->pos         = 0;
    reg_file->f_inode  = file_inode;
    reg_file->file.f_op   = &reg_file_ops;

    files[file_inode->i_fd] = &reg_file->file;
    files[file_inode->i_fd]->f_inode = file_inode;

    return 0;
}

ssize_t reg_file_read(struct file *filp, char *buf, size_t size, loff_t offset /* not used */)
{
    struct reg_file *reg_file = container_of(filp, struct reg_file, file);

    /* get the inode of the regular file */
    struct inode *inode = reg_file->f_inode;

    /* get the driver file of the storage device */
    struct file *driver_file = mount_points[inode->i_rdev].dev_file;

    uint32_t blk_head_size = sizeof(struct block_header);
    uint32_t blk_free_size = FS_BLK_SIZE - sizeof(struct block_header);

    size_t remained_size = size;
    size_t read_size = 0;

    while(1) {
        /* calculate the block index corresponding to the current file read position */
        int blk_i = reg_file->pos / blk_free_size;

        /* get the start and end address of the block */
        uint32_t blk_start_addr = fs_get_block_addr(inode, blk_i);
        uint32_t blk_end_addr = blk_start_addr + FS_BLK_SIZE;

        /* calculate the block offset of the current read position */
        uint8_t blk_pos = reg_file->pos % blk_free_size;

        /* calculate the read address */
        uint32_t read_addr = blk_start_addr + blk_head_size + blk_pos;

        /* calculate the max size to read from the current block */
        uint32_t max_read_size = blk_free_size - blk_pos;
        if(remained_size > max_read_size) {
            read_size = max_read_size;
        } else {
            read_size = remained_size;
        }

        /* read data */
        int buf_pos = size - remained_size;
        int retval = driver_file->f_op->read(NULL, (uint8_t *)&buf[buf_pos], read_size, read_addr);

        /* read failure */
        if(retval < 0)
            break;

        /* update the remained size */
        remained_size -= read_size;

        /* update the file read position */
        reg_file->pos += read_size;

        /* no more data to read */
        if(remained_size == 0)
            break;
    }

    return size - remained_size;
}

ssize_t reg_file_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
    return 0; //regular file write is currently not implemented, import the file with mkromfs instead.
}

long reg_file_llseek(struct file *filp, long offset, int whence)
{
    struct reg_file *reg_file = container_of(filp, struct reg_file, file);

    /* get the inode of the regular file */
    struct inode *inode = reg_file->f_inode;

    char new_pos;

    switch(whence) {
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

    /* check if the new position is valid or not */
    if((new_pos >= 0) && (new_pos < inode->i_size)) {
        reg_file->pos = new_pos;

        return new_pos;
    } else {
        return -1;
    }
}
