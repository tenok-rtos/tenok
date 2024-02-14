#ifndef __SPI_H__
#define __SPI_H__

#include <stdint.h>

void spi1_init(void);

uint8_t spi1_w8r8(uint8_t data);
void spi1_set_chipselect(bool chipselect);

#endif
