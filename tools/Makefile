OBJCOPY=arm-none-eabi-objcopy
ROMFS=../romfs.bin
ROMFS_OBJ=../romfs.o

all:
	gcc -o mkromfs mkromfs.c
	./mkromfs
	$(OBJCOPY) -I binary -O elf32-littlearm -B arm --prefix-sections '.rom' $(ROMFS) $(ROMFS_OBJ)

.PHONY: all
