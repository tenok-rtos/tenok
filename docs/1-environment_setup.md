Development Tools Setup
=======================

Build requirement: `GNU/Linux x86-64` and `python3`

Tested with `Ubuntu 22.04`.

### 1. Prerequisites:

```
sudo add-apt-repository ppa:deadsnakes/ppa # Python 3.8 required by arm-none-eabi-gdb
sudo apt update
sudo apt install git cgdb curl python3.8 build-essential automake* autoconf* \
                 libsdl2-dev libpixman-1-dev libusb-1.0.0-dev libjaylink-dev libncursesw5 \
                 ninja-build flex bison gcc-multilib
```

### 2. OpenOCD:

```
git clone https://github.com/openocd-org/openocd.git
cd openocd
./bootstrap
./configure --prefix=/usr/local --enable-jlink --enable-amtjtagaccel --enable-buspirate \
            --enable-stlink --disable-libftdi
make -j$(nproc)
sudo make install
```

### 3. ARM GNU toolchain 12.3:

Download the pre-built ARM GNU toolchain:

```
wget https://developer.arm.com/-/media/Files/downloads/gnu/12.3.rel1/binrel/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi.tar.xz
tar xf arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi.tar.xz
rm arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi.tar.xz
```

Append the following instruction in the `~/.bashrc`:

```
PATH=$PATH:${YOUR_PATH}/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi/bin
```

Note that `Python 3.8` is a hard requirement for running version `12.3` of `arm-none-eabi-gdb`.

### 4. QEMU:

```
git clone https://github.com/qemu/qemu.git
cd qemu
git submodule update --init --recursive
mkdir build
cd build
../configure
make -j $(nproc)
sudo make install
```

### 5. Restart the terminal
