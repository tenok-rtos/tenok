#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <fs/fs.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/kfifo.h>
#include <kernel/mutex.h>
#include <kernel/preempt.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/tty.h>

#include "stm32f4xx_conf.h"
#include "uart.h"

#define UART1_RX_BUF_SIZE 100
#define UART1_TX_FIFO_SIZE 10

#define UART1_ISR_PRIORITY 14

static int uart1_dma_puts(const char *data, size_t size);

ssize_t serial0_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t serial0_write(struct file *filp,
                      const char *buf,
                      size_t size,
                      off_t offset);
int serial0_open(struct inode *inode, struct file *file);

void USART1_IRQHandler(void);
void DMA2_Stream7_IRQHandler(void);

uart_dev_t uart1 = {
    .rx_fifo = NULL,
    .rx_wait_size = 0,
};

static struct file_operations serial0_file_ops = {
    .read = serial0_read,
    .write = serial0_write,
    .open = serial0_open,
};

void uart1_init(uint32_t baudrate)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_USART1);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_9,
        .GPIO_Mode = GPIO_Mode_AF,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_UP,
    };
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    USART_InitTypeDef USART_InitStruct = {
        .USART_BaudRate = baudrate,
        .USART_Mode = USART_Mode_Rx | USART_Mode_Tx,
        .USART_WordLength = USART_WordLength_8b,
        .USART_StopBits = USART_StopBits_1,
        .USART_Parity = USART_Parity_No,
    };
    USART_Init(USART1, &USART_InitStruct);
    USART_Cmd(USART1, ENABLE);
#if (ENABLE_UART1_DMA != 0)
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
#endif
    USART_ClearFlag(USART1, USART_FLAG_TC);

    /* Initialize interrupt of the UART1 */
    NVIC_InitTypeDef nvic = {
        .NVIC_IRQChannel = USART1_IRQn,
        .NVIC_IRQChannelPreemptionPriority = UART1_ISR_PRIORITY,
        .NVIC_IRQChannelSubPriority = 0,
        .NVIC_IRQChannelCmd = ENABLE,
    };
    NVIC_Init(&nvic);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

#if (ENABLE_UART1_DMA != 0)
    /* Initialize interrupt of the DMA2 channel7 */
    nvic.NVIC_IRQChannel = DMA2_Stream7_IRQn;
    NVIC_Init(&nvic);

    DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, DISABLE);
#endif
}

void tty_init(void)
{
    /* Create wait queues for synchronization */
    init_waitqueue_head(&uart1.tx_wait_list);
    init_waitqueue_head(&uart1.rx_wait_list);

    /* Create rx buffer */
    uart1.rx_fifo = kfifo_alloc(sizeof(uint8_t), UART1_RX_BUF_SIZE);

    mutex_init(&uart1.tx_mtx);
    mutex_init(&uart1.rx_mtx);

    /* Initialize UART1 */
    uart1_init(115200);
}

void serial0_init(void)
{
    /* Register serial0 to the file system */
    register_chrdev("serial0", &serial0_file_ops);

    printk("chardev serial0: console");
}

int serial0_open(struct inode *inode, struct file *file)
{
    return 0;
}

ssize_t serial0_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    mutex_lock(&uart1.rx_mtx);

    preempt_disable();
    uart1.rx_wait_size = size;
    wait_event(uart1.rx_wait_list, kfifo_len(uart1.rx_fifo) >= size);
    preempt_enable();

    kfifo_out(uart1.rx_fifo, buf, size);

    mutex_unlock(&uart1.rx_mtx);

    return size;
}

ssize_t serial0_write(struct file *filp,
                      const char *buf,
                      size_t size,
                      off_t offset)
{
#if (ENABLE_UART1_DMA != 0)
    return uart1_dma_puts(buf, size);
#else
    return uart_puts(USART1, buf, size);
#endif
}

void early_write(char *buf, size_t size)
{
    uart_puts(USART1, buf, size);
}

static int uart1_dma_puts(const char *data, size_t size)
{
    mutex_lock(&uart1.tx_mtx);

    preempt_disable();

    /* Configure the DMA */
    DMA_InitTypeDef DMA_InitStructure = {
        .DMA_BufferSize = (uint32_t) size,
        .DMA_FIFOMode = DMA_FIFOMode_Disable,
        .DMA_FIFOThreshold = DMA_FIFOThreshold_Full,
        .DMA_MemoryBurst = DMA_MemoryBurst_Single,
        .DMA_MemoryDataSize = DMA_MemoryDataSize_Byte,
        .DMA_MemoryInc = DMA_MemoryInc_Enable,
        .DMA_Mode = DMA_Mode_Normal,
        .DMA_PeripheralBaseAddr = (uint32_t) (&USART1->DR),
        .DMA_PeripheralBurst = DMA_PeripheralBurst_Single,
        .DMA_PeripheralInc = DMA_PeripheralInc_Disable,
        .DMA_Priority = DMA_Priority_Medium,
        .DMA_Channel = DMA_Channel_4,
        .DMA_DIR = DMA_DIR_MemoryToPeripheral,
        .DMA_Memory0BaseAddr = (uint32_t) data,
    };
    DMA_Init(DMA2_Stream7, &DMA_InitStructure);

    /* Enable DMA to copy the data */
    DMA_ClearFlag(DMA2_Stream7, DMA_FLAG_TCIF7);
    DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA2_Stream7, ENABLE);

    /* Wait until DMA completed data transfer */
    uart1.tx_ready = false;
    wait_event(uart1.tx_wait_list, uart1.tx_ready);

    preempt_enable();

    mutex_unlock(&uart1.tx_mtx);

    return size;
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET) {
        uint8_t c = USART_ReceiveData(USART1);
        kfifo_put(uart1.rx_fifo, &c);

        if (uart1.rx_wait_size &&
            kfifo_len(uart1.rx_fifo) >= uart1.rx_wait_size) {
            uart1.rx_wait_size = 0;
            wake_up(&uart1.rx_wait_list);
        }
    }
}

void DMA2_Stream7_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA2_Stream7, DMA_IT_TCIF7) == SET) {
        DMA_ClearITPendingBit(DMA2_Stream7, DMA_IT_TCIF7);
        DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, DISABLE);

        uart1.tx_ready = true;
        wake_up(&uart1.tx_wait_list);
    }
}
