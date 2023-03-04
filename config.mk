PROJECT=tenok
ELF=$(PROJECT).elf
BIN=$(PROJECT).bin

CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
GDB=arm-none-eabi-gdb
SIZE=arm-none-eabi-size
QEMU=qemu-system-arm
CPPCHECK=cppcheck
