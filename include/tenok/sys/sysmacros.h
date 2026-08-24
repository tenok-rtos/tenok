#ifndef _TENOK_SYS_SYSMACROS_H
#define _TENOK_SYS_SYSMACROS_H

#define major(dev) ((int) (((dev) >> 8) & 0xff))
#define minor(dev) ((int) ((dev) &0xff))
#define makedev(ma, mi) ((((ma) &0xff) << 8) | ((mi) &0xff))

#endif
