Build and Run the Tenok
=======================

Build:

```
git clone https://github.com/shengwen-tw/tenok.git
cd tenok
git submodule update --init --recursive
./scripts/download-examples.sh
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
make openocd # start the gdb server if you have real hardware
make gdbauto
```
