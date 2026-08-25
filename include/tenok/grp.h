/**
 * @file
 */
#ifndef __GRP_H__
#define __GRP_H__

#include <sys/types.h>

/* Tenok has one group, which is root */
struct group {
    char *gr_name;   /* The name, always "root" */
    char *gr_passwd; /* Tenok keeps no password, always empty */
    gid_t gr_gid;    /* Always 0 */
    char **gr_mem;   /* The members, always empty */
};

/**
 * @brief  Look a group up by its number
 * @param  gid: The group to look up.
 * @retval struct group *: The record when the group exists, and a null
 *         pointer when it does not.
 */
struct group *getgrgid(gid_t gid);

/**
 * @brief  Look a group up by its name
 * @param  name: The group to look up.
 * @retval struct group *: The record when the group exists, and a null
 *         pointer when it does not.
 */
struct group *getgrnam(const char *name);

#endif
