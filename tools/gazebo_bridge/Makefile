CFLAGS=-I./

SRC=main.c \
	serial.c

all: $(SRC)
	gcc $(CFLAGS) -o gazebo_bridge $^

clean:
	rm -rf gazebo_bridge

.PHONY: all clean
