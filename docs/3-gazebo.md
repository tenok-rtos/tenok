Run Tenok with Gazebo
=====================

1. Install Gazebo simulator:

> Check Gazebo's official [tutorial](https://classic.gazebosim.org/tutorials?tut=install_ubuntu).

2. Build PX4 SITL to provide MAVLink translation plugin for Gazebo:

```
git clone https://github.com/Tenok-RTOS/gazebo.git
cd gazebo
git submodule update --init --recursive
cd PX4-Autopilot
make px4_sitl_default gazebo
```

3. Launch Gazeobo:

```
./run_gazebo.sh
```

3. Launch Tenok:

```
make qemu # or alternatively use real hardware
```

6. Build Gazeobo bridge:

```
cd tenok/tools/gazebo_bridge
make
```

8. Execute Gazeobo bridge:

> Check MAVLink serial port path from QEMU (e.g., /dev/pts/X) then execute:

```
cd tenok/tools/gazebo_bridge
make
./gazebo_bridge -i 127.0.0.1 -p 4560 -s ${MAVLINK_SERIAL_PATH} -b 115200
```

> Note that 115200 is the default baudate of the MAVLink serial port by the Tenok.
