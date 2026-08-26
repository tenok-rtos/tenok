/**
 * @file
 */
#ifndef _TENOK_MNTENT_H
#define _TENOK_MNTENT_H

#include <stdio.h>

struct mntent {
    char *mnt_fsname;
    char *mnt_dir;
    char *mnt_type;
    char *mnt_opts;
    int mnt_freq;
    int mnt_passno;
};

FILE *setmntent(const char *file, const char *mode);
struct mntent *getmntent(FILE *stream);
int endmntent(FILE *stream);

#endif
