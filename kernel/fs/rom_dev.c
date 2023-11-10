#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <fs/fs.h>
#include <kernel/printk.h>

#include "uart.h"

extern char _rom_start;
extern char _rom_end;

ssize_t rom_dev_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t rom_dev_write(struct file *filp,
                      const char *buf,
                      size_t size,
                      off_t offset);
int rom_dev_open(struct inode *inode, struct file *file);

static struct file_operations rom_dev_ops = {
    .read = rom_dev_read,
    .write = rom_dev_write,
    .open = rom_dev_open,
};

void rom_dev_init(void)
{
    register_blkdev("rom", &rom_dev_ops);
    printk("blkdev rom: romfs storage");
}

int rom_dev_open(struct inode *inode, struct file *file)
{
    return 0;
}

ssize_t rom_dev_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    char *read_addr = &_rom_start + offset;

    if ((uint32_t) (read_addr + size) > (uint32_t) &_rom_end)
        return -EFAULT;

    memcpy(buf, read_addr, sizeof(char) * size);

    return size;
}

ssize_t rom_dev_write(struct file *filp,
                      const char *buf,
                      size_t size,
                      off_t offset)
{
    return 0;  // rom does not support write operation
}
