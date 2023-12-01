Development Tools Setup
=======================

## Linux

Recommended distribution: `Ubuntu 22.04`

### 1. Prerequisites:

```
sudo add-apt-repository ppa:deadsnakes/ppa # Python 3.8 required by arm-none-eabi-gdb
sudo apt update
sudo apt install git cgdb curl python3.8 stlink-tools build-essential automake* autoconf* \
                 libsdl2-dev libpixman-1-dev libusb-1.0-0-dev libjaylink-dev libncursesw5 \
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
sudo systemctl restart udev
```

### 3. QEMU:

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

### 4. ARM GNU toolchain 12.3 (x86-64):

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

Execute the following command or restart the terminal:

```
source ~/.bashrc
```

---

## macOS

> Caveat: rtplot and Gazebo are not yet fully tested on macOS

### 1. Prerequisites:

```
brew install git cgdb curl wget python@3.8 texinfo automake pkg-config \
             libtool libusb pixman glib ncurses ninja flex bison
```

### 2. OpenOCD:

Install `libjaylink`:

```
https://repo.or.cz/libjaylink.git
git clone https://gitlab.zapb.de/libjaylink/libjaylink.git
cd libjaylink
./autogen.sh
./configure
make
make install
```

Install `OpenOCD`:

```
git clone https://github.com/openocd-org/openocd.git
cd openocd
./bootstrap
./configure --prefix=/usr/local --enable-jlink --enable-buspirate \
            --enable-stlink --disable-libftdi
make -j$(sysctl -n hw.ncpu)
make install
```

### 3. QEMU:

```
git clone https://github.com/qemu/qemu.git
cd qemu
git submodule update --init --recursive
mkdir build
cd build
../configure
make -j$(sysctl -n hw.ncpu)
make install
```

### 4. ARM GNU toolchain 12.3:

**macOS (AArch64):**

Download the pre-built ARM GNU toolchain:

```
wget https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-darwin-arm64-arm-none-eabi.tar.xz
tar xf arm-gnu-toolchain-13.2.rel1-darwin-arm64-arm-none-eabi.tar.xz
```
Append the following instruction in the `~/.zshrc`:

```
PATH=$PATH:${PATH}/arm-gnu-toolchain-13.2.Rel1-darwin-arm64-arm-none-eabi/bin
```

Execute the following command or restart the terminal:

```
source ~/.zshrc
```

**macOS (x86-64):**

Download the pre-built ARM GNU toolchain:

```
wget https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-darwin-x86_64-arm-none-eabi.tar.xz
tar xf arm-gnu-toolchain-13.2.rel1-darwin-x86_64-arm-none-eabi.tar.xz
```

Append the following instruction in the `~/.zshrc`:

```
PATH=$PATH:${PATH}/arm-gnu-toolchain-13.2.Rel1-darwin-x86_64-arm-none-eabi/bin
```

Execute the following command or restart the terminal:

```
source ~/.zshrc
```
