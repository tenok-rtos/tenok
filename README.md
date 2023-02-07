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
cd ${YOUR_PATH}
git clone git://git.qemu.org/qemu.git
cd qemu
git submodule init
git submodule update --recursive
mkdir build
cd build
../configure
make -j $(nproc)
```

vim ~/.bashrc

```
PATH=$PATH:${YOUR_PATH}/qemu/build
```
## Build and Run

compilation

 ```
 git clone https://github.com/shengwen-tw/neo-rtenv.git
 cd neo-rtenv
 make
 ```
 
 run emulation
 
 ```
 make qemu
 ```
 
 upload binary
 
 ```
 make flash
 ```
