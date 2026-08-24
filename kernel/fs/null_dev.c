#include <stddef.h>
#include <sys/types.h>

#include <fs/fs.h>

int null_dev_open(struct inode *inode, struct file *file)
{
    return 0;
}

ssize_t null_dev_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    /* Reading the null device reports the end of the file at once. Returning
     * the requested size instead hands the caller a buffer that was never
     * written, and one that never runs out.
     */
    return 0;
}

ssize_t null_dev_write(struct file *filp,
                       const char *buf,
                       size_t size,
                       off_t offset)
{
    return size;
}

static struct file_operations null_dev_ops = {
    .read = null_dev_read,
    .write = null_dev_write,
    .open = null_dev_open,
};

void null_dev_init(void)
{
    register_chrdev("null", &null_dev_ops);
}
