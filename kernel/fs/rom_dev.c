#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <fs/fs.h>
#include <kernel/printk.h>

extern char _rom_start;
extern char _rom_end;

int rom_dev_open(struct inode *inode, struct file *file)
{
    return 0;
}

ssize_t rom_dev_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    size_t rom_size = (size_t) (&_rom_end - &_rom_start);

    if (offset < 0)
        return -EINVAL;

    /* Past the end is the end of the file, and a read that reaches it is
     * shortened rather than refused: a reader of the whole device has to be
     * able to stop.
     */
    if ((size_t) offset >= rom_size)
        return 0;

    if (size > (rom_size - (size_t) offset))
        size = rom_size - (size_t) offset;

    memcpy(buf, &_rom_start + offset, size);

    return size;
}

ssize_t rom_dev_write(struct file *filp,
                      const char *buf,
                      size_t size,
                      off_t offset)
{
    return 0; /* ROM does not support write operation */
}

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
