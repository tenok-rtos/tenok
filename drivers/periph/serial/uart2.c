#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <fs/fs.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/kfifo.h>
#include <kernel/preempt.h>
#include <kernel/printk.h>

#include "stm32f4xx_conf.h"
#include "uart.h"

#define UART2_RX_BUF_SIZE 100

#define UART2_ISR_PRIORITY 14

uart_dev_t uart2 = {
    .rx_fifo = NULL,
    .rx_wait_size = 0,
};

int uart2_open(struct inode *inode, struct file *file)
{
    return 0;
}

ssize_t uart2_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    mutex_lock(&uart2.rx_mtx);

    preempt_disable();
    uart2.rx_wait_size = size;
    wait_event(uart2.rx_wait_list, kfifo_len(uart2.rx_fifo) >= size);
    preempt_enable();

    kfifo_out(uart2.rx_fifo, buf, size);

    mutex_unlock(&uart2.rx_mtx);

    return size;
}

ssize_t uart2_write(struct file *filp,
                    const char *buf,
                    size_t size,
                    off_t offset)
{
    return uart_puts(USART2, buf, size);
}

static struct file_operations uart2_file_ops = {
    .read = uart2_read,
    .write = uart2_write,
    .open = uart2_open,
};

static void __uart2_init(uint32_t baudrate)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource6, GPIO_AF_USART2);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6,
        .GPIO_Mode = GPIO_Mode_AF,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_UP,
    };
    GPIO_Init(GPIOD, &GPIO_InitStruct);

    USART_InitTypeDef USART_InitStruct = {
        .USART_BaudRate = baudrate,
        .USART_Mode = USART_Mode_Tx | USART_Mode_Rx,
        .USART_WordLength = USART_WordLength_8b,
        .USART_StopBits = USART_StopBits_2,
        .USART_Parity = USART_Parity_Even,
        .USART_HardwareFlowControl = USART_HardwareFlowControl_None,
    };
    USART_Init(USART2, &USART_InitStruct);
    USART_Cmd(USART2, ENABLE);

    NVIC_InitTypeDef NVIC_InitStruct = {
        .NVIC_IRQChannel = USART2_IRQn,
        .NVIC_IRQChannelPreemptionPriority = UART2_ISR_PRIORITY,
        .NVIC_IRQChannelSubPriority = 0,
        .NVIC_IRQChannelCmd = ENABLE,
    };
    NVIC_Init(&NVIC_InitStruct);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
}

void uart2_init(char *dev_name, char *description)
{
    /* Register UART2 to the file system */
    register_chrdev(dev_name, &uart2_file_ops);

    /* Create wait queues for synchronization */
    init_waitqueue_head(&uart2.tx_wait_list);
    init_waitqueue_head(&uart2.rx_wait_list);

    /* Create kfifo for UART2 rx */
    uart2.rx_fifo = kfifo_alloc(sizeof(uint8_t), UART2_RX_BUF_SIZE);

    /* Initialize UART2 */
    __uart2_init(115200);

    mutex_init(&uart2.tx_mtx);
    mutex_init(&uart2.rx_mtx);

    printk("chardev %s: %s", dev_name, description);
}

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET) {
        uint8_t c = USART_ReceiveData(USART2);
        kfifo_put(uart2.rx_fifo, &c);

        if (uart2.rx_wait_size &&
            kfifo_len(uart2.rx_fifo) >= uart2.rx_wait_size) {
            uart2.rx_wait_size = 0;
            wake_up(&uart2.rx_wait_list);
        }
    }
}
