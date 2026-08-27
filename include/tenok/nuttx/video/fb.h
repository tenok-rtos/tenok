/**
 * @file
 *
 * What a program asks a frame buffer about, in the shape NuttX gave it.
 *
 * Tenok answers this beside the shape of Linux because it is the smaller of
 * the two: two questions instead of eight, and no virtual terminal to switch
 * away from. DirectFB2 reaches a NuttX display through these.
 */
#ifndef _TENOK_NUTTX_VIDEO_FB_H
#define _TENOK_NUTTX_VIDEO_FB_H

#include <stddef.h>
#include <stdint.h>

/* What ioctl() is given to ask each question. NuttX numbers its frame buffer
 * commands from a base of its own and lays the number beside it
 */
#define _FBIOCBASE 0x2800
#define _FBIOC(nr) (_FBIOCBASE | (nr))

#define FBIOGET_VIDEOINFO _FBIOC(0x0001)
#define FBIOGET_PLANEINFO _FBIOC(0x0002)

/* What the pixels stand for. Only the one Tenok has a display for is named */
#define FB_FMT_RGB16_565 11

typedef uint16_t fb_coord_t;

/* What the display is */
struct fb_videoinfo_s {
    uint8_t fmt;     /* One of FB_FMT_ */
    fb_coord_t xres; /* How many pixel columns are shown */
    fb_coord_t yres; /* How many pixel rows */
    uint8_t nplanes; /* How many colour planes there are */
};

/* Where the pixels of one plane are */
struct fb_planeinfo_s {
    void *fbmem;           /* Where the memory starts */
    size_t fblen;          /* How much of it there is */
    fb_coord_t stride;     /* How many bytes one row takes */
    uint8_t display;       /* Which display it belongs to */
    uint8_t bpp;           /* How many bits one pixel takes */
    uint32_t xres_virtual; /* What there is to show from */
    uint32_t yres_virtual;
    uint32_t xoffset; /* Which part of it is shown */
    uint32_t yoffset;
};

#endif
