#ifndef __SERIAL_H__
#define __SERIAL_H__

int serial_init(char *port_name, int baudrate);
void serial_puts(int serial_fd, char *s, size_t size);
char serial_getc(int serial_fd);

#endif
