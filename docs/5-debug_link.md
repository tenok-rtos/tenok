Real-time Visualization with rtplot and debug-link
==================================================

### 1. Build rtplot example 

Download the example files using:

```
./scripts/download-examples.sh
```

Enable debug-link task in `user/task/task.mk`

```make
# Uncomment the line
SRC += $(PROJ_ROOT)/user/tasks/debug_task.c
```

Rebuild the source code:

```
make all
```

Install rtplot tool to the system:

```
cd tools/rtplot/
make install
```

### 2. Run rtplot example with QEMU

First, execute the following command to launch the Tenok with QEMU:

```
make qemu
```

The output should be similar to the following:

```
char device redirected to /dev/pts/X (label serial1)
char device redirected to /dev/pts/Y (label serial2)
[    0.001000000] Tenok RTOS (built time: 11:14:41 Nov  8 2023)
[    0.001000000] Machine model: stm32f407
[    0.006000000] chardev serial0: console
[    0.008000000] chardev serial1: mavlink
[    0.008000000] chardev serial2: debug-link
[    0.008000000] blkdev rom: romfs storage
type `help' for help
user@stm32f407:/$
```

According to the output, serial2 (debug-link) is redirceted to `/dev/pts/Y`.

Now open another terminal and execute the following command:

```
cd tools/rtplot/
make rtplot
```

After the rtplot window shows up, select `/dev/pts/Y` port, `test` message then click the connect button.
The user should expect to see some real-time plotting on the screen.

## 3. Define debug-link message

Tenok uses a metalanguage to define messages for debug visualization, this message prorocol is called the Debug-Link.
All message files should named as `*.msg` and by default they should be placed in the `msg/` directory.

To define a variable, the user can use the following syntax:

```c
type variable "description"
```

For array, the syntax is:

```c
type array[size] "description"
```

For example, in `test.msg` (provided by the `./scripts/download-examples.sh`), the messages are defined as:

```c
uint32_t time     "boot time"
float    accel[3] "acceleration"
float    q[4]     "quaternion"
```

The supported types are:

* bool
* uint8_t
* int8_t
* uint16_t
* int16_t
* uint32_t
* int32_t
* uint64_t
* int64_t
* float
* double

The third description field of the message definition is optional, but it is recommended to not neglect it as
it will show up in the real-time plotting graph.

When user execute `make all` to build the code, the Makefile target calls the `msggen` tool to generate C code and YAML files.
The user should pack their on-board data with the generated C functions and send out the message to their computer via I/Os like USART, USB, etc.
To visualize the messages, the user can then use another built-in tool called `rtplot`, which reads the message definitions from the generated YAML files.

The following code illustrates a basic C code implementation for sending message:

```c
...
#include "tenok_test_msg.h"

...
void debug_link_task(void)
{
    setprogname("debug link");

    int debug_link_fd = open("/dev/serial2", O_RDWR);
...
    tenok_msg_test_t msg = {
        .time = 0,
        .accel = {1.0f, 2.0f, 3.0f},
        .q = {1.0f, 0.0f, 0.0f, 0.0f}
    };

    uint8_t buf[100];

    while(1) {
        size_t size = pack_tenok_test_msg(&msg, buf);
        write(debug_link_fd, buf, size);
...
        usleep(10000); //100Hz (10ms)
    }
}

HOOK_USER_TASK(debug_link_task, 3, 1024);

```

The full  example code can be found in `user/task/debug/debug_task.c`.
