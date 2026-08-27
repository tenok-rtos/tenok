/**
 * @file
 */
#ifndef __DRIVERS_FB_H__
#define __DRIVERS_FB_H__

#include <stddef.h>

/* What a board tells the frame buffer driver about its display */
struct fb_info {
    const char *name;
    void *base;  /* Where the controller reads the pixels from */
    size_t size; /* How much memory that is, every page together */
    unsigned width;
    unsigned height;
    unsigned bpp;
    unsigned stride; /* How many bytes one row takes */
    unsigned pages;  /* How many pictures the memory holds */
};

void fb_register(struct fb_info *info);

#endif
