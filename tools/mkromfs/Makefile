include ../../makefiles/config.mk

ROMFS := ./romfs.bin
ROMFS_OBJ := ./romfs.o

CFLAGS :=
CFLAGS += -g -Wall

CFLAGS += -I../../ \
          -I../../include/common

SRC := ./mkromfs.c

MKROMFS_FLAGS := -v # Verbose option

all: $(ROMFS_OBJ)

$(ROMFS): $(SRC)
	gcc $(CFLAGS) -o mkromfs $(SRC)
	./mkromfs $(MKROMFS_FLAGS) 

$(ROMFS_OBJ): $(ROMFS)
	@echo "OBJCPY" $@ $<
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm --prefix-sections '.rom' $(ROMFS) $(ROMFS_OBJ)

gdbauto:
	cgdb --args ./mkromfs

clean:
	rm -rf mkromfs
	rm -rf $(ROMFS)
	rm -rf $(ROMFS_OBJ)

.PHONY: gdbauto clean
