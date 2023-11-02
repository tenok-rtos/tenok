# Tenok

[Tenok](https://github.com/shengwen-tw/tenok) is a real-time operating system (RTOS) for Robotics and Internet of Things (IoT) inspired by [rtenv+](https://github.com/embedded2014/rtenv-plus) and [Piko/RT](https://github.com/PikoRT/pikoRT).

The Amis people are an indigenous tribe that originated in Taiwan, and the term "tenok" in their language means "kernel."

## Key Features

* POSIX compliant RTOS
* Linux-like design: Wait queue, tasklet, kfifo, and printk
* Task and Thread (Task resembles UNIX process as a group of threads)
* Synchronization: Mutex (supports priority inheritance), Semaphore, and Spinlock
* Inter-Process Communication (IPC): FIFO (Named pipe), Message Queue, and Signals
* Kernel-space memory allocation: Buddy system and Slab allocator
* User-space memory allocation: Memory pool and First-Fit Free List
* Software timer
* Built-in Shell with command completion and history saving
* Root and ROM file systems
* Real-time plotting and customizable debug messaging with metalanguage
* Integrated with MAVLink communication protocol
* Software-in-the-loop (SIL) simulation with Gazebo simulator

## Tools

* **mkromfs**: Import files into firmware binary with `Tenok`'s romfs format
* **msggen**: Convert user-defined metalanguage messages into C codes and YAML files
* **rtplot**: For on-board data real-time plotting, where the message definitions are loaded from the auto-generated YAML files
* **gazebo_bridge**: Message forwarding between `Tenok` (serial) and Gazebo simulator (TCP/IP)

## Getting Started

* [Developement Tools Setup](./docs/1-environment_setup.md)
* [Build and Run the Tenok](./docs/2-build_and_run.md)
* [Run Tenok with Gazebo Simulator](./docs/3-gazebo.md)
* [Interact with Tenok Shell](./docs/4-shell.md)
* [Supported Platforms](./docs/5-platforms.md)

## Resources 

* [Full API List](https://tenok-rtos.github.io/globals_func.html)
* [Doxygen Page](https://tenok-rtos.github.io/index.html)
* [Presentation at COSCUP 2023](https://drive.google.com/file/d/1p8YJVPVwFAEknMXPbXzjj0y0p5qcqT2T/view?fbclid=IwAR1kYbiMB8bbCdlgW6ffHRBong7hNtJ8uCeVU4Qi5HvZ3G3srwhKPasPLEg)

## License

`Tenok` is released under the BSD 2-Clause License, for detailed information please read [LICENSE](https://github.com/shengwen-tw/neo-rtenv/blob/master/LICENSE).

## Related Projects
1. [rtenv](https://github.com/embedded2014/rtenv) / [rtenv+](https://github.com/embedded2014/rtenv-plus)
2. [mini-arm-os](https://github.com/jserv/mini-arm-os)
3. [Piko/RT](https://github.com/PikoRT/pikoRT)
4. [linenoise](https://github.com/antirez/linenoise)
