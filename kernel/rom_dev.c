#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "file.h"
#include "syscall.h"
#include "uart.h"

extern char _section_rom_start;
extern char _section_rom_end;

long rom_dev_llseek(struct file *filp, long offset, int whence);
ssize_t rom_dev_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t rom_dev_write(struct file *filp, const char *buf, size_t size, loff_t offset);

char *rom_data_ptr = NULL;

static struct file_operations rom_dev_ops = {
	.llseek = rom_dev_llseek,
	.read = rom_dev_read,
	.write = rom_dev_write
};

void mount_rom(void)
{
	char rom_buf[100] = {0};
	int fd = open("/dev/rom", 0, 0);

	if(fd < 0) {
		return;
	}

	read(fd, rom_buf, 5);
	uart3_puts(rom_buf);
}

int rom_dev_init(void)
{
	register_blkdev("rom", &rom_dev_ops);

	char *rom_data_ptr = &_section_rom_start;
}

ssize_t rom_dev_read(struct file *filp, char *buf, size_t size, loff_t offset)
{
	if((rom_data_ptr + size) > &_section_rom_end) {
		return EFAULT;
	}

	memcpy(buf, rom_data_ptr, sizeof(char) * size);
	rom_data_ptr += size;

	return size;
}

ssize_t rom_dev_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
	return 0;
}

long rom_dev_llseek(struct file *filp, long offset, int whence)
{
	char *new_pos;

	switch(whence) {
	case SEEK_SET:
		new_pos = &_section_rom_start + offset;
		break;
	case SEEK_END:
		new_pos = &_section_rom_end + offset;
		break;
	case SEEK_CUR:
		new_pos = rom_data_ptr + offset;
		break;
	default:
		return -1;
	}

	if((new_pos >= &_section_rom_start) && (new_pos <= &_section_rom_end)) {
		rom_data_ptr = new_pos;

		long offset_from_start = new_pos - &_section_rom_start;
		return offset_from_start;
	} else {
		return -1;
	}
}
