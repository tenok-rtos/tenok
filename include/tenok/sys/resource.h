/**
 * @file
 */
#ifndef __RESOURCE_H__
#define __RESOURCE_H__

/* Resources a task can ask the limit of, with the numbers Linux gives them */
#define RLIMIT_CPU 0
#define RLIMIT_FSIZE 1
#define RLIMIT_DATA 2
#define RLIMIT_STACK 3
#define RLIMIT_CORE 4
#define RLIMIT_RSS 5
#define RLIMIT_NPROC 6
#define RLIMIT_NOFILE 7
#define RLIMIT_MEMLOCK 8
#define RLIMIT_AS 9
#define RLIMIT_LOCKS 10
#define RLIMIT_SIGPENDING 11
#define RLIMIT_MSGQUEUE 12
#define RLIMIT_NICE 13
#define RLIMIT_RTPRIO 14

/* A resource Tenok does not count has no limit of its own */
#define RLIM_INFINITY (~0ULL)

typedef unsigned long long rlim_t;

struct rlimit {
    rlim_t rlim_cur; /* The limit in effect */
    rlim_t rlim_max; /* The most it can be raised to */
};

/**
 * @brief  Read the limit of a resource. The limits of Tenok are properties of
 *         the build, so the two fields always read back the same
 * @param  resource: One of the RLIMIT_ values.
 * @param  rlim: The buffer for returning the limit.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int getrlimit(int resource, struct rlimit *rlim);

/**
 * @brief  Replace the limit of a resource. Every limit of Tenok is fixed at
 *         the build, so only a request that asks for no more than what is
 *         already in effect can be granted
 * @param  resource: One of the RLIMIT_ values.
 * @param  rlim: The limit to apply.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int setrlimit(int resource, const struct rlimit *rlim);

#endif
