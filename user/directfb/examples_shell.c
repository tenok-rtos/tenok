/**
 * @file
 *
 * The shell commands that run the DirectFB2 examples. They differ only in the
 * name they are called by and the name the build gave the main() of each, so
 * one of them is written here and the rest are made from it.
 */
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "modules.h"
#include "shell.h"

/* An example draws until it is told to stop, and nothing here tells it to, so
 * it runs in a thread of its own and the shell goes on being a shell. Under
 * every thread that has work to do, and over the idle thread
 */
#define EXAMPLE_PRIORITY 1

/* DirectFB2 uses more than the smallest stack Tenok recommends for a single
 * line of its own logging
 */
#define EXAMPLE_STACK_SIZE 8192

static int example_start(const char *name,
                         void *(*task)(void *),
                         pthread_t *thread,
                         bool *running)
{
    char str[PRINT_SIZE_MAX];

    if (*running) {
        snprintf(str, sizeof(str), "%s is already drawing\n\r", name);
        shell_puts(str);
        return 0;
    }

    directfb_register_modules();

    pthread_attr_t attr;
    struct sched_param param = {
        .sched_priority = EXAMPLE_PRIORITY,
    };

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, EXAMPLE_STACK_SIZE);
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int retval = pthread_create(thread, &attr, task, NULL);
    if (retval) {
        snprintf(str, sizeof(str), "%s: %s\n\r", name, strerror(retval));
        shell_puts(str);
        return -1;
    }

    *running = true;

    snprintf(str, sizeof(str), "%s is drawing in thread %d\n\r", name,
             (int) *thread);
    shell_puts(str);

    return 0;
}

/* The build renames the main() of an example, and this is what calls it */
#define DFB_EXAMPLE(command, entry)                                       \
    int entry(int argc, char *argv[]);                                    \
                                                                          \
    static pthread_t command##_thread;                                    \
    static bool command##_running;                                        \
                                                                          \
    static void *command##_task(void *arg)                                \
    {                                                                     \
        char *argv[] = {#command, NULL};                                  \
                                                                          \
        entry(1, argv);                                                   \
        command##_running = false;                                        \
                                                                          \
        return NULL;                                                      \
    }                                                                     \
                                                                          \
    static int command(int argc, char *argv[])                            \
    {                                                                     \
        return example_start(#command, command##_task, &command##_thread, \
                             &command##_running);                         \
    }                                                                     \
                                                                          \
    HOOK_SHELL_CMD(#command, command)

DFB_EXAMPLE(gears, dfb_gears_main);
DFB_EXAMPLE(window, dfb_window_main);
DFB_EXAMPLE(fire, dfb_fire_main);
