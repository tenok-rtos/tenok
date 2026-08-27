/**
 * The frame buffer the display controller scans out of.
 *
 * The panel is wired to the controller and the memory it reads is there
 * before anything opens the device, so the driver answers what the layout is
 * and hands the memory over. Nothing is copied and nothing is mapped: a
 * program reaches the pixels where the controller already reads them.
 */
#include <errno.h>
#include <fcntl.h>
#include <nuttx/video/fb.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#include <fs/fs.h>
#include <kernel/printk.h>

#include <drivers/fb.h>

static struct fb_info *fb;

static int fb_dev_open(struct inode *inode, struct file *file)
{
    return 0;
}

/* The memory is where it is, so the offset is the only thing to honour and
 * a length past the end is refused rather than silently shortened
 */
static void *fb_dev_mmap(struct file *filp, size_t length, off_t offset)
{
    if (!fb)
        return (void *) -ENODEV;

    if (offset < 0 || (size_t) offset > fb->size ||
        length > fb->size - (size_t) offset)
        return (void *) -EINVAL;

    return (char *) fb->base + offset;
}


static int fb_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    if (!fb)
        return -ENODEV;

    switch (cmd) {
    case FBIOGET_VIDEOINFO: {
        struct fb_videoinfo_s *info = (struct fb_videoinfo_s *) arg;

        info->fmt = FB_FMT_RGB16_565;
        info->xres = fb->width;
        info->yres = fb->height;
        info->nplanes = 1;
        return 0;
    }

    case FBIOGET_PLANEINFO: {
        struct fb_planeinfo_s *info = (struct fb_planeinfo_s *) arg;

        memset(info, 0, sizeof(*info));

        info->fbmem = fb->base;
        info->fblen = fb->size;
        info->stride = fb->stride;
        info->bpp = fb->bpp;
        info->xres_virtual = fb->width;
        info->yres_virtual = fb->height * fb->pages;
        return 0;
    }

    default:
        return -ENOTTY;
    }
}

static struct file_operations fb_file_ops = {
    .open = fb_dev_open,
    .ioctl = fb_dev_ioctl,
    .mmap = fb_dev_mmap,
};

void fb_register(struct fb_info *info)
{
    fb = info;

    register_chrdev("fb0", &fb_file_ops);

    printk("fb0: %ux%u %u bpp at %p", info->width, info->height, info->bpp,
           info->base);
}
