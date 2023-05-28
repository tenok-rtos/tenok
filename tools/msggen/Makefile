include ../../config.mk

ROMFS=./romfs.bin
ROMFS_OBJ=./romfs.o

CFLAGS=
CFLAGS+=-g

CFLAGS+=-I../../ \
	-I../../kernel

SRC=../../kernel/list.c \
	./msggen.c

all:
	gcc $(CFLAGS) -o msggen $(SRC)

gdbauto:
	cgdb --args ./msggen

clean:
	rm -rf msggen

.PHONY: all gdbauto clean
