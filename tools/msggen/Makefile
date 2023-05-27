include ../../config.mk

ROMFS=./romfs.bin
ROMFS_OBJ=./romfs.o

CFLAGS=
CFLAGS+=-g

CFLAGS+=-I../ \
	-I../../kernel

all:
	gcc -o msggen msggen.c

gdbauto:
	cgdb --args ./msggen

clean:
	rm -rf msggen

.PHONY: all gdbauto clean
