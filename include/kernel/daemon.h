/**
 * @file
 */
#ifndef __DAEMON_H__
#define __DAEMON_H__

/* clang-format off */
#define DAEMON_LIST \
    DECLARE_DAEMON(SOFTIRQD), \
    DECLARE_DAEMON(FILESYSD)
/* clang-format on */

#define DECLARE_DAEMON(x) x
enum {
    DAEMON_LIST,
    DAEMON_CNT,
};
#undef DECLARE_DAEMON

void set_daemon_id(int daemon);
uint16_t get_daemon_id(int daemon);

#endif
