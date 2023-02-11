-include ../config.mk

ROMFS=./romfs.bin
ROMFS_OBJ=./romfs.o

all: $(ROMFS_OBJ)

$(ROMFS): ./mkromfs.c
	gcc -o mkromfs $<
	./mkromfs

$(ROMFS_OBJ): $(ROMFS)
	@echo "OBJCPY" $@ $<
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm --prefix-sections '.rom' $(ROMFS) $(ROMFS_OBJ)

clean:
	rm -rf $(ROMFS)
	rm -rf $(ROMFS_OBJ)

.PHONY: clean
