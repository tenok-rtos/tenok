#ifndef __UTIL_H__
#define __UTIL_H__

#define container_of(ptr, type, member) \
        ((type *)((void *)ptr - offsetof(type, member)))

typedef int  ssize_t;
typedef int  _mode_t;
typedef int  _dev_t;
typedef long loff_t;

#endif
