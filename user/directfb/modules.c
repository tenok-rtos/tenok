/**
 * @file
 *
 * DirectFB2 is built without constructors, since Tenok runs nothing before a
 * program starts. What a constructor would have registered is registered here
 * instead, once, however many programs go on to use it.
 */
#define DFB_FONT_PROVIDER dgiff
#define DFB_IMAGE_PROVIDER dfiff
#define DFB_VIDEO_PROVIDER dfvff

#include <directfb.h>
#include <stdbool.h>

#include "modules.h"

DirectFBCoreSystemInitProtoype(nuttxfb);
DirectFBWindowManagerInitProtoype(default);
DirectFBFontProviderInitProtoype(DGIFF);
DirectFBImageProviderInitProtoype(DFIFF);
DirectFBVideoProviderInitProtoype(DFVFF);

/* The OpenGL implementation is only there when a program that draws through it
 * is in the build, and it is the build that says so
 */
#ifdef DFB_OPENGL_IMPLEMENTATION
DirectFBGLInitProtoype(PGL);
#endif

void directfb_register_modules(void)
{
    static bool done;

    if (done)
        return;

    /* Reading the configuration is what brings DirectFB2 far enough up for
     * anything to be registered with it or set on it. A program that calls
     * this again on its own finds it already done.
     */
    DirectFBInit(NULL, NULL);

    DirectFBCoreSystemInit(nuttxfb);
    DirectFBWindowManagerInit(default);
    DirectFBFontProviderInit(DGIFF);
    DirectFBImageProviderInit(DFIFF);
    DirectFBVideoProviderInit(DFVFF);
#ifdef DFB_OPENGL_IMPLEMENTATION
    DirectFBGLInit(PGL);
#endif

    /* A thread of Tenok is given the smallest stack the system recommends
     * unless it asks for more, and DirectFB2 uses more than that for a single
     * line of its own logging
     */
    DirectFBSetOption("thread-stacksize", "8192");

    done = true;
}
