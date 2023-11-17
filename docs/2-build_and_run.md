Build and Run the Tenok
=======================

### 1. Source code compilation:

The following instructions demonstrate the procedures of building Tenok with example ROM files:

```
git clone https://github.com/shengwen-tw/tenok.git
cd tenok
git submodule update --init --recursive # MAVLink library
./scripts/download-examples.sh # Example ROM files
make
 ```
 
### 2. Run Tenok with QEMU

Type the following command to emulate Tenok with QEMU:

```
make qemu
```

To exit the emulator, use the key combination of <Ctrl-A X> (i.e., press Ctrl-A first, leave the key, then press X.)
 
To remote debugging with gdb, type:

```
make gdbauto
```

### 3. Run Tenok with real hardware

Type the following command to upload firmware binary to your board:

```
make flash
```

To remote debugging using gdb, start OpenOCD as the remote server first:

```
make openocd
```

Now, type the following command to start debugging with gdb:

```
make gdbauto
```

### 4. Example programs

Additionally, the user can uncomment the following lines in `tenok/user/tasks/tasks.mk` to test
the example programs located under `tenok/user/tasks/examples/`:

```make
...
#SRC += $(PROJ_ROOT)/user/tasks/debug_task.c
...
#SRC += $(PROJ_ROOT)/user/tasks/examples/fifo-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/mqueue-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/mutex-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/priority-inversion.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/signal-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/timer-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/poll-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/pthread-ex.c
```
