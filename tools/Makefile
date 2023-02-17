-include ../config.mk

ROMFS=./romfs.bin
ROMFS_OBJ=./romfs.o

CFLAGS=
CFLAGS+=-I../kernel

SRC=../kernel/list.c \
	 ./mkromfs.c

all: $(ROMFS_OBJ)

$(ROMFS): $(SRC)
	gcc $(CFLAGS) -o mkromfs $(SRC)
	./mkromfs

$(ROMFS_OBJ): $(ROMFS)
	@echo "OBJCPY" $@ $<
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm --prefix-sections '.rom' $(ROMFS) $(ROMFS_OBJ)

clean:
	rm -rf $(ROMFS)
	rm -rf $(ROMFS_OBJ)

.PHONY: clean
