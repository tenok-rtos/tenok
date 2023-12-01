/**
 * @file
 */
#ifndef __MOUNT_H__
#define __MOUNT_H__

/**
 * @brief  Attach the filesystem specified by source to the location specified
           by the pathname in target
 * @param  source: The path name of the device to mount on the system.
 * @param  target: The path name to mount the file system.
 * @retval int: 0 on success and nonzero error number on error.
 */
int mount(const char *source, const char *target);

#endif
