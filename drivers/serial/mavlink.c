#include <errno.h>
#include <string.h>
#include "stm32f4xx.h"
#include "semaphore.h"
#include "uart.h"
#include "fs.h"
#include "mqueue.h"
#include "syscall.h"
#include "kernel.h"
#include "pipe.h"

#define UART2_RX_BUF_SIZE 100

ssize_t serial1_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t serial1_write(struct file *filp, const char *buf, size_t size, loff_t offset);

pipe_t *uart2_rx_pipe;

static struct file_operations serial1_file_ops = {
    .read = serial1_read,
    .write = serial1_write
};

void uart2_init(uint32_t baudrate)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource6, GPIO_AF_USART2);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6,
        .GPIO_Mode = GPIO_Mode_AF,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_UP
    };
    GPIO_Init(GPIOD, &GPIO_InitStruct);

    USART_InitTypeDef USART_InitStruct = {
        .USART_BaudRate = baudrate,
        .USART_Mode = USART_Mode_Tx | USART_Mode_Rx,
        .USART_WordLength = USART_WordLength_8b,
        .USART_StopBits = USART_StopBits_2,
        .USART_Parity = USART_Parity_Even,
        .USART_HardwareFlowControl = USART_HardwareFlowControl_None
    };
    USART_Init(USART2, &USART_InitStruct);
    USART_Cmd(USART2, ENABLE);

    NVIC_InitTypeDef NVIC_InitStruct = {
        .NVIC_IRQChannel = USART2_IRQn,
        .NVIC_IRQChannelPreemptionPriority = KERNEL_INT_PRI + 1,
        .NVIC_IRQChannelSubPriority = 0,
        .NVIC_IRQChannelCmd = ENABLE
    };
    NVIC_Init(&NVIC_InitStruct);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
}

void serial1_init(void)
{
    /* initialize serial0 as character device */
    register_chrdev("serial1", &serial1_file_ops);

    /* create pipe for reception */
    uart2_rx_pipe = generic_pipe_create(sizeof(uint8_t), UART2_RX_BUF_SIZE);

    /* initialize uart3 */
    uart2_init(115200);
}

ssize_t serial1_read(struct file *filp, char *buf, size_t size, loff_t offset)
{
    generic_pipe_read(uart2_rx_pipe, (char *)buf, size, offset);
    return size;
}

ssize_t serial1_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
    return uart_puts(USART2, buf, size);
}

void USART2_IRQHandler(void)
{
    if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET) {
        uint8_t c = USART_ReceiveData(USART2);
        generic_pipe_write(uart2_rx_pipe, (char *)&c, sizeof(char), 0);
    }
}

HOOK_DRIVER(serial1_init);
