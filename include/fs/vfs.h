/**
 * @file
 */
#ifndef __FS_VFS_H__
#define __FS_VFS_H__

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

int vfs_mount(int tid, const char *source, const char *target);
int vfs_open_file(int tid, const char *pathname);
int vfs_create_file(int tid, const char *pathname, uint8_t file_type);
int vfs_open_dir(int tid, const char *pathname, DIR *dirp);
int vfs_readdir(DIR *dirp, struct dirent *dirent);
char *vfs_getcwd(int tid, char *buf, size_t size);
int vfs_chdir(int tid, const char *path);
int vfs_mkdir(int tid, const char *pathname);
int vfs_remove(int tid, const char *pathname, bool rm_dir);
int vfs_stat(int tid, const char *pathname, struct stat *statbuf);

#endif
