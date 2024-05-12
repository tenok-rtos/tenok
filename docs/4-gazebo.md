Run Tenok with Gazebo Simulator
===============================

First, install the Gazebo simulator:

> Check Gazebo's official [tutorial](https://classic.gazebosim.org/tutorials?tut=install_ubuntu).

Next, build PX4 SITL to provide MAVLink translation plugin for Gazebo:

```
git clone https://github.com/tenok-rtos/gazebo.git
cd gazebo
git submodule update --init --recursive
cd PX4-Autopilot
make px4_sitl_default gazebo
```

Additionally, build `gazebo_bridge` for forwarding messages between `Tenok` (serial) and Gazebo (TCP/IP):

```
cd tenok/tools/gazebo_bridge
make
```

After finishing the installation, launch Gazeobo simulator:

```
./run_gazebo.sh
```

By the same time, launch `Tenok` with QEMU:

```
make qemu # Or use real hardware alternatively
```

The user should expect to see similar output as following:

```
char device redirected to /dev/pts/X (label serial1)
char device redirected to /dev/pts/Y (label serial2)
[    0.001000000] Tenok RTOS (built time: 11:14:41 Nov  8 2023)
[    0.001000000] Machine model: stm32f407
[    0.006000000] chardev console: shell (alias: serial0)
[    0.008000000] chardev mavlink: mavlink (alias: serial1)
[    0.008000000] chardev dbglink: debug-link (alias: serial2)
[    0.008000000] blkdev rom: romfs storage
type `help' for help
user@stm32f407:/$
```

Next, launch Gazeobo bridge with the serial path given by QEMU:

```
cd tenok/tools/gazebo_bridge
make
./gazebo_bridge -i 127.0.0.1 -p 4560 -s /dev/pts/X -b 115200
```

Finally, the simulation should start. Note that `115200` is the default baudate of the MAVLink port set by `Tenok`.
