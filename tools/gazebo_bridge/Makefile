LDFLAGS=-lpthread

CFLAGS=-I./

SRC=main.c \
	serial.c

all: $(SRC)
	gcc $(CFLAGS) $(LDFLAGS) -o gazebo_bridge $^

clean:
	rm -rf gazebo_bridge

.PHONY: all clean
