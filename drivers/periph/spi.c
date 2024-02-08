#include <stdbool.h>

#include <fs/fs.h>
#include <printk.h>

#include "stm32f4xx.h"

int spi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
int spi_open(struct inode *inode, struct file *file);

static struct file_operations spi_file_ops = {
    .ioctl = spi_ioctl,
    .open = spi_open,
};

int spi_open(struct inode *inode, struct file *file)
{
    return 0;
}

int spi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return 0;
}

/* SPI1
 * CS:   GPIO A4
 * SCK:  GPIO A5
 * MISO: GPIO A6
 * MOSI: GPIO A7
 */
void spi1_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_SPI1);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7,
        .GPIO_Mode = GPIO_Mode_AF,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_UP,
    };
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    SPI_InitTypeDef SPI_InitStruct = {
        .SPI_Direction = SPI_Direction_2Lines_FullDuplex,
        .SPI_Mode = SPI_Mode_Master,
        .SPI_DataSize = SPI_DataSize_8b,
        .SPI_CPOL = SPI_CPOL_High,
        .SPI_CPHA = SPI_CPHA_2Edge,
        .SPI_NSS = SPI_NSS_Soft,
        .SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4,
        .SPI_FirstBit = SPI_FirstBit_MSB,
        .SPI_CRCPolynomial = 7,
    };
    SPI_Init(SPI1, &SPI_InitStruct);

    SPI_Cmd(SPI1, ENABLE);

    register_chrdev("spi", &spi_file_ops);
    printk("spi1: full duplex mode");
}

uint8_t spi1_read_write(uint8_t data)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_FLAG_TXE) == RESET)
        ;
    SPI_I2S_SendData(SPI1, data);

    while (SPI_I2S_GetFlagStatus(SPI1, SPI_FLAG_RXNE) == RESET)
        ;
    return SPI_I2S_ReceiveData(SPI1);
}

void spi1_chip_select(void)
{
    GPIO_ResetBits(GPIOA, GPIO_Pin_4);
}

void spi1_chip_deselect(void)
{
    GPIO_SetBits(GPIOA, GPIO_Pin_4);
}
