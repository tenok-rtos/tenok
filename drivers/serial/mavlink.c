#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <fs/fs.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/kfifo.h>
#include <kernel/printk.h>

#include "stm32f4xx_conf.h"
#include "uart.h"

#define UART2_RX_BUF_SIZE 100

#define UART2_ISR_PRIORITY 14

ssize_t serial1_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t serial1_write(struct file *filp,
                      const char *buf,
                      size_t size,
                      off_t offset);
int serial1_open(struct inode *inode, struct file *file);

void USART2_IRQHandler(void);

uart_dev_t uart2 = {
    .rx_fifo = NULL,
    .rx_wait_size = 0,
    .tx_state = UART_TX_IDLE,
};

static struct file_operations serial1_file_ops = {
    .read = serial1_read,
    .write = serial1_write,
    .open = serial1_open,
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

void serial1_init(void)
{
    /* Register serial1 to the file system */
    register_chrdev("serial1", &serial1_file_ops);

    /* Create wait queues for synchronization */
    INIT_LIST_HEAD(&uart2.tx_wait_list);
    INIT_LIST_HEAD(&uart2.rx_wait_list);

    /* Create kfifo for UART2 rx */
    uart2.rx_fifo = kfifo_alloc(sizeof(uint8_t), UART2_RX_BUF_SIZE);

    /* Initialize UART2 */
    uart2_init(115200);

    printk("chardev serial1: mavlink");
}

int serial1_open(struct inode *inode, struct file *file)
{
    return 0;
}

ssize_t serial1_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    if (kfifo_len(uart2.rx_fifo) >= size) {
        kfifo_out(uart2.rx_fifo, buf, size);
        return size;
    } else {
        CURRENT_THREAD_INFO(curr_thread);
        prepare_to_wait(&uart2.rx_wait_list, curr_thread, THREAD_WAIT);
        uart2.rx_wait_size = size;
        return -ERESTARTSYS;
    }
}

ssize_t serial1_write(struct file *filp,
                      const char *buf,
                      size_t size,
                      off_t offset)
{
    return uart_puts(USART2, buf, size);
}

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET) {
        uint8_t c = USART_ReceiveData(USART2);
        kfifo_put(uart2.rx_fifo, &c);

        if (uart2.rx_wait_size &&
            kfifo_len(uart2.rx_fifo) >= uart2.rx_wait_size) {
            wake_up(&uart2.rx_wait_list);
            uart2.rx_wait_size = 0;
        }
    }
}
