#include <errno.h>
#include <string.h>
#include "stm32f4xx.h"
#include "semaphore.h"
#include "uart.h"
#include "fs.h"
#include "mqueue.h"
#include "syscall.h"
#include "kernel.h"

ssize_t serial2_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t serial2_write(struct file *filp, const char *buf, size_t size, loff_t offset);

static struct file_operations serial2_file_ops = {
    .read = serial2_read,
    .write = serial2_write
};

void uart1_init(uint32_t baudrate)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10,
        .GPIO_Mode = GPIO_Mode_AF,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_UP
    };
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    USART_InitTypeDef USART_InitStruct = {
        .USART_BaudRate = baudrate,
        .USART_Mode = USART_Mode_Rx | USART_Mode_Tx,
        .USART_WordLength = USART_WordLength_8b,
        .USART_StopBits = USART_StopBits_1,
        .USART_Parity = USART_Parity_No
    };
    USART_Init(USART1, &USART_InitStruct);
    USART_Cmd(USART1, ENABLE);
    USART_ClearFlag(USART1, USART_FLAG_TC);
}

void serial2_init(void)
{
    /* initialize serial2 as character device */
    register_chrdev("serial2", &serial2_file_ops);

    /* initialize uart1 */
    uart1_init(115200);
}

ssize_t serial2_read(struct file *filp, char *buf, size_t size, loff_t offset)
{
    return uart_getc(USART1);
}

ssize_t serial2_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
    return uart_puts(USART1, buf, size);
}

HOOK_DRIVER(serial2_init);
