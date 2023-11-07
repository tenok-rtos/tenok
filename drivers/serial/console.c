#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include <fs/fs.h>
#include <kernel/pipe.h>
#include <kernel/wait.h>
#include <kernel/errno.h>
#include <kernel/mutex.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/softirq.h>
#include <kernel/interrupt.h>

#include "uart.h"
#include "stm32f4xx.h"

#define UART1_RX_BUF_SIZE 100
#define UART1_TX_FIFO_SIZE 10

#define UART1_ISR_PRIORITY 14

static int uart1_dma_puts(const char *data, size_t size);

ssize_t serial0_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t serial0_write(struct file *filp, const char *buf, size_t size, off_t offset);
int serial0_open(struct inode *inode, struct file *file);

void USART1_IRQHandler(void);
void DMA2_Stream7_IRQHandler(void);

void uart1_tx_tasklet_handler(unsigned long data);

uart_dev_t uart1 = {
    .rx_fifo = NULL,
    .rx_wait_size = 0,
    .tx_dma_ready = false,
    .tx_state = UART_TX_IDLE
};

static struct kfifo *uart1_tx_fifo;
static struct tasklet_struct uart1_tx_tasklet;

static struct file_operations serial0_file_ops = {
    .read = serial0_read,
    .write = serial0_write,
    .open = serial0_open
};

void tty_init(void)
{
    uart1_tx_fifo = kfifo_alloc(PRINT_SIZE_MAX, UART1_TX_FIFO_SIZE);
    tasklet_init(&uart1_tx_tasklet, uart1_tx_tasklet_handler, 0);

#ifndef BUILD_QEMU
    /* clean screen */
    char *cls_str = "\x1b[H\x1b[2J";
    console_write(cls_str, strlen(cls_str));
#endif
}

void uart1_init(uint32_t baudrate)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_USART1);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_9,
        .GPIO_Mode = GPIO_Mode_AF,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_UP
    };
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    USART_InitTypeDef USART_InitStruct = {
        .USART_BaudRate = baudrate,
        .USART_Mode = USART_Mode_Rx | USART_Mode_Tx,
        .USART_WordLength = USART_WordLength_8b,
        .USART_StopBits = USART_StopBits_1,
        .USART_Parity = USART_Parity_No
    };
    USART_Init(USART1, &USART_InitStruct);
    USART_Cmd(USART1, ENABLE);
#if (ENABLE_UART1_DMA != 0)
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
#endif
    USART_ClearFlag(USART1, USART_FLAG_TC);

    /* initialize interrupt of the uart1 */
    NVIC_InitTypeDef nvic = {
        .NVIC_IRQChannel = USART1_IRQn,
        .NVIC_IRQChannelPreemptionPriority = UART1_ISR_PRIORITY,
        .NVIC_IRQChannelSubPriority = 0,
        .NVIC_IRQChannelCmd = ENABLE
    };
    NVIC_Init(&nvic);

    request_irq(USART1_IRQn, USART1_IRQHandler, 0, NULL, NULL);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

#if (ENABLE_UART1_DMA != 0)
    /* initialize interrupt of the dma2 channel7 */
    nvic.NVIC_IRQChannel = DMA2_Stream7_IRQn;
    NVIC_Init(&nvic);

    request_irq(DMA2_Stream7_IRQn, DMA2_Stream7_IRQHandler, 0, NULL, NULL);
    DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, DISABLE);
#endif
}

void serial0_init(void)
{
    /* initialize serial0 as character device */
    register_chrdev("serial0", &serial0_file_ops);

    /* create kfifo for reception */
    uart1.rx_fifo = kfifo_alloc(sizeof(uint8_t), UART1_RX_BUF_SIZE);

    /* create wait queues for synchronization */
    init_waitqueue_head(&uart1.tx_wq);
    init_waitqueue_head(&uart1.rx_wq);

    /* initialize mutex for protecting tx resource */
    mutex_init(&uart1.tx_mtx);

    /* initialize uart1 */
    uart1_init(115200);

    printk("chardev serial0: console");
}

int serial0_open(struct inode *inode, struct file *file)
{
    return 0;
}

ssize_t serial0_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    if(kfifo_len(uart1.rx_fifo) >= size) {
        kfifo_out(uart1.rx_fifo, buf, size);
        return size;
    } else {
        init_wait(uart1.rx_wait);
        prepare_to_wait(&uart1.rx_wq, uart1.rx_wait, THREAD_WAIT);
        uart1.rx_wait_size = size;
        return -ERESTARTSYS;
    }
}

ssize_t console_write(const char *buf, size_t size)
{
    /* TODO:
     * place the thread into the waitqueue if kfifo has
     * no free space
     */
    kfifo_in(uart1_tx_fifo, buf, size);
    tasklet_schedule(&uart1_tx_tasklet);
    return size;
}

ssize_t serial0_write(struct file *filp, const char *buf, size_t size, off_t offset)
{
    return console_write(buf, size);
}

void early_tty_print(char *buf, size_t size)
{
    uart_puts(USART1, buf, size);
}

static int uart1_dma_puts(const char *data, size_t size)
{
    switch(uart1.tx_state) {
        case UART_TX_IDLE: {
            /* try claiming usage of the tx resource */
            if(mutex_lock(&uart1.tx_mtx) == -ERESTARTSYS) {
                /* mutex is locked by other threads */
                return -ERESTARTSYS;
            }

            /* configure the dma */
            DMA_InitTypeDef DMA_InitStructure = {
                .DMA_BufferSize = (uint32_t)size,
                .DMA_FIFOMode = DMA_FIFOMode_Disable,
                .DMA_FIFOThreshold = DMA_FIFOThreshold_Full,
                .DMA_MemoryBurst = DMA_MemoryBurst_Single,
                .DMA_MemoryDataSize = DMA_MemoryDataSize_Byte,
                .DMA_MemoryInc = DMA_MemoryInc_Enable,
                .DMA_Mode = DMA_Mode_Normal,
                .DMA_PeripheralBaseAddr = (uint32_t)(&USART1->DR),
                .DMA_PeripheralBurst = DMA_PeripheralBurst_Single,
                .DMA_PeripheralInc = DMA_PeripheralInc_Disable,
                .DMA_Priority = DMA_Priority_Medium,
                .DMA_Channel = DMA_Channel_4,
                .DMA_DIR = DMA_DIR_MemoryToPeripheral,
                .DMA_Memory0BaseAddr = (uint32_t)data
            };
            DMA_Init(DMA2_Stream7, &DMA_InitStructure);

            /* enable dma to copy the data */
            DMA_ClearFlag(DMA2_Stream7, DMA_FLAG_TCIF7);
            DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, ENABLE);
            DMA_Cmd(DMA2_Stream7, ENABLE);

            uart1.tx_state = UART_TX_DMA_BUSY;
            uart1.tx_dma_ready = false;

            /* wait until dma complete data transfer */
            init_wait(uart1.tx_wait);
            prepare_to_wait(&uart1.tx_wq, uart1.tx_wait, THREAD_WAIT);
            return -ERESTARTSYS;
        }
        case UART_TX_DMA_BUSY: {
            /* try to release the tx resource */
            if(mutex_unlock(&uart1.tx_mtx) == -EPERM) {
                /* mutex is locked by other threads,
                 * call mutex_lock() to wait */
                mutex_lock(&uart1.tx_mtx);
                return -ERESTARTSYS;
            }

            /* notified by the dma irq, the data transfer is now complete */
            uart1.tx_state = UART_TX_IDLE;
            return size;
        }
        default: {
            return -EIO;
        }
    }
}

void uart1_tx_tasklet_handler(unsigned long data)
{
    char *buf;
    size_t size;

    while(!kfifo_is_empty(uart1_tx_fifo)) {
        kfifo_dma_out_prepare(uart1_tx_fifo, &buf, &size);
        uart_puts(USART1, buf, size);
        kfifo_dma_out_finish(uart1_tx_fifo);
    }
}

void USART1_IRQHandler(void)
{
    if(USART_GetITStatus(USART1, USART_IT_RXNE) == SET) {
        uint8_t c = USART_ReceiveData(USART1);
        kfifo_put(uart1.rx_fifo, &c);

        if(kfifo_len(uart1.rx_fifo) >= uart1.rx_wait_size) {
            finish_wait(uart1.rx_wait);
            uart1.rx_wait_size = 0;
        }
    }
}

void DMA2_Stream7_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA2_Stream7, DMA_IT_TCIF7) == SET) {
        DMA_ClearITPendingBit(DMA2_Stream7, DMA_IT_TCIF7);
        DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, DISABLE);

        finish_wait(uart1.tx_wait);
        uart1.tx_dma_ready = true;
    }
}
