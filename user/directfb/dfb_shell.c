/**
 * @file
 *
 * The shell command that draws through DirectFB2 onto the panel of the board
 */
#include <directfb.h>
#include <stdio.h>

#include "modules.h"
#include "shell.h"

static void say(const char *what, DFBResult ret)
{
    char str[PRINT_SIZE_MAX];

    snprintf(str, sizeof(str), "%-22s %s\n\r", what,
             ret == DFB_OK ? "ok" : DirectFBErrorString(ret));
    shell_puts(str);
}

int dfb(int argc, char *argv[])
{
    IDirectFB *dfb = NULL;
    IDirectFBSurface *primary = NULL;
    DFBSurfaceDescription desc;
    DFBResult ret;
    int width = 0, height = 0;
    char str[PRINT_SIZE_MAX];

    directfb_register_modules();

    ret = DirectFBInit(NULL, NULL);
    say("DirectFBInit", ret);
    if (ret != DFB_OK)
        return -1;

    ret = DirectFBCreate(&dfb);
    say("DirectFBCreate", ret);
    if (ret != DFB_OK)
        return -1;

    ret = dfb->SetCooperativeLevel(dfb, DFSCL_FULLSCREEN);
    say("SetCooperativeLevel", ret);

    desc.flags = DSDESC_CAPS;
    desc.caps = DSCAPS_PRIMARY;

    ret = dfb->CreateSurface(dfb, &desc, &primary);
    say("CreateSurface", ret);
    if (ret != DFB_OK)
        goto out;

    primary->GetSize(primary, &width, &height);
    snprintf(str, sizeof(str), "surface               %dx%d\n\r", width,
             height);
    shell_puts(str);

    /* Something that could not have got there by accident: a blue field with
     * a red bar across the middle and a white frame around the edge
     */
    primary->SetColor(primary, 0x00, 0x00, 0xa0, 0xff);
    primary->FillRectangle(primary, 0, 0, width, height);

    primary->SetColor(primary, 0xe0, 0x20, 0x20, 0xff);
    primary->FillRectangle(primary, 0, height / 2 - 20, width, 40);

    primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff);
    primary->DrawRectangle(primary, 4, 4, width - 8, height - 8);
    primary->DrawLine(primary, 0, 0, width - 1, height - 1);
    primary->DrawLine(primary, width - 1, 0, 0, height - 1);

    ret = primary->Flip(primary, NULL, DSFLIP_NONE);
    say("Flip", ret);

    primary->Release(primary);

out:
    dfb->Release(dfb);
    shell_puts("done\n\r");

    return 0;
}

HOOK_SHELL_CMD("dfb", dfb);
