LDFLAGS=-lpthread

CFLAGS=-I./

SRC=main.c \
	serial.c

all: $(SRC)
	gcc $(CFLAGS) $(LDFLAGS) -o gazebo_bridge $^

connect:
	./gazebo_bridge -i 127.0.0.1 -p 4560 -s /dev/pts/5 -b 115200

clean:
	rm -rf gazebo_bridge

.PHONY: all connect clean
