An experimental real-time operating system

## Features

1. spinlock
2. mutex
3. semaphore
4. fifo (named pipe)
5. message queue

## Prerequisites

### QEMU

```
sudo apt install ninja-build
git clone git://git.qemu.org/qemu.git
cd qemu
git submodule init
git submodule update --recursive
mkdir build
cd build
../configure
make -j $(nproc)
```
