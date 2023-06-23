#include <stddef.h>
#include "uart.h"
#include "syscall.h"
#include "tenok_test_msg.h"

#define INC 1
#define DEC 0

void debug_link_task(void)
{
    set_program_name("debug link");
    setpriority(0, getpid(), 3);

    int dir = INC;

    tenok_msg_test_t msg = {
        .time = 0,
        .accel = {1.0f, 2.0f, 3.0f},
        .q = {1.0f, 0.0f, 0.0f, 0.0f}
    };

    uint8_t buf[100];

    while(1) {
        size_t size = pack_tenok_test_msg(&msg, buf);
        uart_puts(USART1, buf, size);

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

        sleep(10); //100Hz
    }
}
