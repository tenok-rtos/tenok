#include <stddef.h>
#include "file.h"

extern char _section_rom_start;
extern char _section_rom_end;

long rom_dev_llseek(struct file *filp, long offset, int whence);
ssize_t rom_dev_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t rom_dev_write(struct file *filp, const char *buf, size_t size, loff_t offset);

static struct file_operations rom_dev_ops = {
	.llseek = rom_dev_llseek,
	.read = rom_dev_read,
	.write = rom_dev_write
};

int rom_dev_init(void)
{
	char *start_addr = &_section_rom_start;
}

ssize_t rom_dev_read(struct file *filp, char *buf, size_t size, loff_t offset)
{
}

ssize_t rom_dev_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
}

long rom_dev_llseek(struct file *filp, long offset, int whence)
{
}
