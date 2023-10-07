#include <stddef.h>

#include <kernel/kernel.h>

int main(void)
{
    /* start the os */
    sched_start();

    return 0;
}
