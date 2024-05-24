include ../../makefiles/config.mk

ELF = type_size

CFLAGS :=
CFLAGS += -g -m32

CFLAGS += -I../../ \
          -I../../include \
          -I../../include/common \
          -I../../include/kernel \
          -I../../include/tenok \
          -I../../include/tenok/sys

SRC := ./main.c

$(ELF): $(SRC)
	gcc $(CFLAGS) -o $(ELF) $(SRC)

clean:
	rm -rf $(ELF) 

.PHONY: clean
