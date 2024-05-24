include ../../makefiles/config.mk

ROMFS := ./romfs.bin
ROMFS_OBJ := ./romfs.o

CFLAGS :=
CFLAGS += -g -Wall

CFLAGS += -I../../ \
	  -I../../include/common

SRC := ./msggen.c

all:
	gcc $(CFLAGS) -o msggen $(SRC)

gdbauto:
	cgdb --args ./msggen

clean:
	rm -rf msggen

.PHONY: all gdbauto clean
