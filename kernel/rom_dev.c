#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "fs.h"
#include "syscall.h"
#include "uart.h"

extern char _section_rom_start;
extern char _section_rom_end;

ssize_t rom_dev_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t rom_dev_write(struct file *filp, const char *buf, size_t size, loff_t offset);

static struct file_operations rom_dev_ops = {
    .read = rom_dev_read,
    .write = rom_dev_write
};

int rom_dev_init(void)
{
    register_blkdev("rom", &rom_dev_ops);
}

ssize_t rom_dev_read(struct file *filp, char *buf, size_t size, loff_t offset)
{
    char *read_addr = &_section_rom_start + offset;

    if((uint32_t)(read_addr + size) > (uint32_t)&_section_rom_end) {
        return -EFAULT;
    }

    memcpy(buf, read_addr, sizeof(char) * size);

    return size;
}

ssize_t rom_dev_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
    return 0; //rom does not support write operation
}
