Build and Run the Tenok
=======================

## 1. Build:

The following instructions demonstrate the procedures of building Tenok with example ROM files:

```
git clone https://github.com/shengwen-tw/tenok.git
cd tenok
git submodule update --init --recursive # MAVLink library
./scripts/download-examples.sh # Example ROM files
make
 ```
 
## 2. Run with Emulator (QEMU)

Type the following command to emulate Tenok with QEMU:

```
make qemu
```

To exit the emulator, use the key combination of <ctrl-a x> (i.e., press ctrl-a first, leave the key, then press x.)
 
To remote debugging with gdb, type:

```
make gdbauto
```

## 3. Run with Real Hardware

Type the following command to upload firmware binary to your board:

```
make flash
```

To remote debugging with gdb, start OpenOCD as remote server first:

```
make openocd
```

Then, you can start debugging with gdb:

```
make gdbauto
```
