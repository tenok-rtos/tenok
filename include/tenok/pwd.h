/**
 * @file
 */
#ifndef __PWD_H__
#define __PWD_H__

#include <sys/types.h>

/* Tenok has one user, which is root */
struct passwd {
    char *pw_name;   /* The name, always "root" */
    char *pw_passwd; /* Tenok keeps no password, always empty */
    uid_t pw_uid;    /* Always 0 */
    gid_t pw_gid;    /* Always 0 */
    char *pw_gecos;  /* The description, always empty */
    char *pw_dir;    /* The home directory, always "/" */
    char *pw_shell;  /* The shell, always empty */
};

/**
 * @brief  Look a user up by its number
 * @param  uid: The user to look up.
 * @retval struct passwd *: The record when the user exists, and a null
 *         pointer when it does not.
 */
struct passwd *getpwuid(uid_t uid);

/**
 * @brief  Look a user up by its name
 * @param  name: The user to look up.
 * @retval struct passwd *: The record when the user exists, and a null
 *         pointer when it does not.
 */
struct passwd *getpwnam(const char *name);

#endif
