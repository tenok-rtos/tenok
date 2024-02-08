#ifndef __SPI_H__
#define __SPI_H__

#include <stdint.h>

void spi1_init(void);

uint8_t spi1_read_write(uint8_t data);
void spi1_chip_select(void);
void spi1_chip_deselect(void);

#endif
