## neo-rtenv

An experimental real-time operating system inspired by [rtenv](https://github.com/embedded2014/rtenv) and [rtenv-plus](https://github.com/embedded2014/rtenv-plus).

## Features

* POSIX APIs
* Spinlock
* Mutex
* Semaphore
* FIFO (Named pipe)
* Message queue
* Shell
* A simple ROM file system

## Supported Platforms

* [STM32F4Discovery](https://www.st.com/en/evaluation-tools/stm32f4discovery.html)
* QEMU Emulation of [netduinoplus2](https://freshman1997.slack.com/?redir=%2Fmessages%2Fgrand-tour-project%2F) (STM32F405RGT6)

## Development toolchain

1. Prerequisites:

```
sudo apt install build-essential git zlib1g-dev libsdl1.2-dev automake* autoconf* \
         libtool libpixman-1-dev lib32gcc1 lib32ncurses5 libc6:i386 libncurses5:i386 \
         libstdc++6:i386 libusb-1.0.0-dev ninja-build
```

2. OpenOCD:

```
git clone git://git.code.sf.net/p/openocd/code openocd
cd openocd
./bootstrap
./configure --prefix=/usr/local  --enable-jlink --enable-amtjtagaccel --enable-buspirate \
            --enable-stlink   --disable-libftdi
echo -e "all:\ninstall:" > doc/Makefile
make -j4
sudo make install
```

3. ARM GCC toolchain 9:

```
wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/9-2019q4/gcc-arm-none-eabi-9-2019-q4-major-x86_64-linux.tar.bz2
tar jxf ./gcc-arm-none-eabi-9-2019-q4-major-x86_64-linux.tar.bz2
rm gcc-arm-none-eabi-9-2019-q4-major-x86_64-linux.tar.bz2
```

4. QEMU:

```
git clone git://git.qemu.org/qemu.git
cd qemu
git submodule init
git submodule update --recursive
mkdir build
cd build
../configure
make -j $(nproc)
```

5. Edit `~/.bashrc` and append the following instructions:

```
PATH=$PATH:${YOUR_PATH}/qemu/build
PATH=$PATH:${YOUR_PATH}/gcc-arm-none-eabi-9-2019-q4-major/bin
```

6. Restart the terminal:

## Build and Run

Build:

```
git clone https://github.com/shengwen-tw/neo-rtenv.git
cd neo-rtenv
make
 ```
 
Run emulation:
 
```
make qemu
```
 
Upload binary:
 
```
make flash
```

Remote debugging with gdb:

```
make openocd #start the gdb server if you have real hardware
make gdbauto
```
