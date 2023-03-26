## Tenok
An experimental real-time operating system inspired by [rtenv](https://github.com/embedded2014/rtenv) and [rtenv-plus](https://github.com/embedded2014/rtenv-plus).

The Amis people are an indigenous tribe that originated in Taiwan, and
the term "`tenok`" in their language means "`kernel`."

## Features

* POSIX style
* Spinlock
* Mutex
* Semaphore
* FIFO (named pipe)
* Message queue
* Built-in shell interface
* Simple rootfs and romfs

## Supported Platforms

### ARM Cortex-M4F

* [STM32F4DISCOVERY](https://www.st.com/en/evaluation-tools/stm32f4discovery.html) (STM32F407VG)
  - Select by enabling `include platform/stm32f4disc.mk` in the Makefile
  - Serial0 (UART3): PC10 (TX), PC11 (RX)

* [32F429IDISCOVERY](https://www.st.com/en/evaluation-tools/32f429idiscovery.html) (STM32F429ZI)
  - Select by enabling `include platform/stm32f429disc.mk` in the Makefile
  - Serial0 (UART3): PC10 (TX), PC11 (RX)
* QEMU Emulation of [netduinoplus2](https://qemu.readthedocs.io/en/latest/system/arm/stm32.html) (STM32F405RGT6)
  - Select by enabling `include platform/qemu.mk` in the Makefile

## Shell Keys

* `Backspace`, `Delete`: Delete a single character

* `Home`, `Ctrl+A`: Move the cursor to the leftmost

* `End`, `Ctrl+E`: Move the cursor to the rightmost

* `Ctrl+U`: Delete a whole line

* `Left Arrow`, `Ctrl+B`: Move the cursor one to left

* `Right Arrow`, `Ctrl+F`: Move the cursor one to right

* `Up Arrow`, `Down Arrow`: Display previous typings

* `Tab`: Command completion

## Development Tools

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
./configure --prefix=/usr/local --enable-jlink --enable-amtjtagaccel --enable-buspirate \
            --enable-stlink --disable-libftdi
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

6. Restart the terminal

## Build and Run

Build:

```
git clone https://github.com/shengwen-tw/tenok.git
cd tenok
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

## License

Tenok is released under the BSD 2-Clause License, for detailed information please read [LICENSE](https://github.com/shengwen-tw/neo-rtenv/blob/master/LICENSE).
