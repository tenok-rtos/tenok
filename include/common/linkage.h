/**
 * @file
 */
#ifndef __LINKAGE_H__
#define __LINKAGE_H__

#define ENTRY(name) \
    .global name;   \
    name:

#define END(name) .size name, .- name /* Calculate the section size */

#define ENDPROC(name)       \
    .type name, % function; \
    END(name)

#endif
