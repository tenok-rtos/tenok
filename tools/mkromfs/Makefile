include ../../makefiles/config.mk

ROMFS := ./romfs.bin
ROMFS_OBJ := ./romfs.o

CFLAGS :=
CFLAGS += -g -Wall

CFLAGS += -I../../ \
          -I../../include \
          -I../../include/common

SRC := ./mkromfs.c

# The image has to be built again when its contents change, not only when the
# tool that packs them does. HOST_INPUT_DIR of mkromfs.c names the directory.
ROM_SRC := $(shell find ../../rom -type f 2>/dev/null)

MKROMFS_FLAGS := -v # Verbose option

all: $(ROMFS_OBJ)

$(ROMFS): $(SRC) $(ROM_SRC)
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
