include ../../config.mk

ROMFS := ./romfs.bin
ROMFS_OBJ := ./romfs.o

CFLAGS :=
CFLAGS += -g
CFLAGS += -m32 #to make the pointer be 4 bytes instead of 8 bytes
#CFLAGS += -Wpadded

CFLAGS += -I ../../ \
          -I ../../include

SRC := ../../kernel/list.c \
       ./mkromfs.c

MKROMFS_FLAGS := -v #verbose option

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
