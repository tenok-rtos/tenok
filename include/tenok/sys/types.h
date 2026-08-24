/**
 * @file
 */
#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>

typedef int ssize_t;
typedef int mode_t;
typedef int dev_t;
typedef uint32_t ino_t;
typedef uint16_t nlink_t;
typedef uint16_t uid_t;
typedef uint16_t gid_t;
typedef long int blksize_t;
typedef long int blkcnt_t;
typedef int pid_t;
typedef int clockid_t;
typedef int timer_t;
typedef int64_t time_t;
typedef long int off_t;
typedef unsigned long useconds_t;
typedef unsigned long clock_t;

/* Tenok has no sockets. The type is named so that a program that mentions one
 * compiles
 */
typedef unsigned int socklen_t;

/* The older spellings, which BSD derived headers still use */
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef unsigned short u_short;
typedef unsigned char u_char;

#endif
