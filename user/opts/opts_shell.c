/**
 * @file
 *
 * The shell command that runs the Open POSIX Test Suite. A test of the suite
 * is written as a program of its own, and the build renames the entry of each
 * so that they can all be called from here instead.
 */
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include <stdlib.h>

#include "shell.h"

#include "opts_tests.h"

/* What a test of the suite answers with */
#define PTS_PASS 0
#define PTS_FAIL 1
#define PTS_UNRESOLVED 2
#define PTS_UNSUPPORTED 4
#define PTS_UNTESTED 5

/* A test of the suite says how it went by calling exit(), which would end the
 * task it runs in. The build points that name here instead, and the answer is
 * carried back to the loop below the way a return would have carried it
 */
static jmp_buf finished;
static int armed;

void opts_exit(int status)
{
    if (armed)
        longjmp(finished, status ? status : -1);

    /* Nothing is running a test, so this is the exit it says it is */
    exit(status);
}

static const char *verdict(int result)
{
    switch (result) {
    case PTS_PASS:
        return "PASS";
    case PTS_FAIL:
        return "FAIL";
    case PTS_UNRESOLVED:
        return "UNRESOLVED";
    case PTS_UNSUPPORTED:
        return "UNSUPPORTED";
    case PTS_UNTESTED:
        return "UNTESTED";
    default:
        return "?";
    }
}

int opts(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX];
    const char *wanted = argc > 1 ? argv[1] : NULL;
    int counted[6] = {0};
    int ran = 0;

    for (size_t i = 0; i < sizeof(opts_tests) / sizeof(opts_tests[0]); i++) {
        const struct opts_test *test = &opts_tests[i];

        if (wanted && strcmp(wanted, test->interface) != 0)
            continue;

        /* Said before the test runs, so that a test that does not come back
         * says which one it was
         */
        snprintf(str, sizeof(str), "%-24s %-12s ", test->interface,
                 test->name);
        shell_puts(str);

        char *args[] = {(char *) test->name, NULL};
        int result;

        armed = 1;
        result = setjmp(finished);
        if (result == 0)
            result = test->run(1, args);
        armed = 0;

        /* A test that passed said so by exiting with zero, which setjmp()
         * cannot tell apart from not having jumped at all
         */
        if (result == -1)
            result = PTS_PASS;

        if (result >= 0 && result < 6)
            counted[result]++;
        ran++;

        snprintf(str, sizeof(str), "%s\n\r", verdict(result));
        shell_puts(str);
    }

    if (!ran) {
        shell_puts("no test of that interface\n\r");
        return -1;
    }

    snprintf(str, sizeof(str),
             "%d passed, %d failed, %d unresolved, %d unsupported\n\r",
             counted[PTS_PASS], counted[PTS_FAIL], counted[PTS_UNRESOLVED],
             counted[PTS_UNSUPPORTED]);
    shell_puts(str);

    return counted[PTS_FAIL] ? -1 : 0;
}

HOOK_SHELL_CMD("opts", opts);
