#include <tenok.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>

#include <kernel/task.h>

#include "uart.h"
#include "tenok_test_msg.h"

#define INC 1
#define DEC 0

void debug_link_task(void)
{
    setprogname("debug link");

    int debug_link_fd = open("/dev/serial2", O_RDWR);

    int dir = INC;

    tenok_msg_test_t msg = {
        .time = 0,
        .accel = {1.0f, 2.0f, 3.0f},
        .q = {1.0f, 0.0f, 0.0f, 0.0f}
    };

    uint8_t buf[100];

    while(1) {
        size_t size = pack_tenok_test_msg(&msg, buf);
        write(debug_link_fd, buf, size);

        if(dir == INC) {
            msg.time += 10;
            if(msg.time == 1000) {
                dir = DEC;
            }
        } else {
            msg.time -= 10;
            if(msg.time == 0) {
                dir = INC;
            }
        }

        usleep(10000); //100Hz (10ms)
    }
}

HOOK_USER_TASK(debug_link_task, 3, 1024);
