#ifndef __DIRENT_H__
#define __DIRENT_H__

#include "fs.h"

int opendir(const char *name, DIR *dir);
int readdir(DIR *dirp, struct dirent *dirent);

#endif
