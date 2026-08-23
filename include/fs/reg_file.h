/**
 * @file
 */
#ifndef __REG_FILE_H__
#define __REG_FILE_H__

#include <fs/fs.h>

struct reg_file {
    off_t pos;
    struct file file;
};

int reg_file_init(struct file **files,
                  struct inode *file_inode,
                  struct reg_file *reg_file);
ssize_t reg_file_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t reg_file_write(struct file *filp,
                       const char *buf,
                       size_t size,
                       off_t offset);
void reg_file_rewind(struct file *filp);
void reg_file_seek_end(struct file *filp);
void reg_file_truncate(struct file *filp);

#endif
