/**
 * @file
 */
#ifndef __LINKAGE_H__
#define __LINKAGE_H__

#define ENTRY(name) \
    .global name;   \
    name:

#define END(name) .size name, .- name  // calculate the size of the section

#define ENDPROC(name)       \
    .type name, % function; \
    END(name)

#endif
