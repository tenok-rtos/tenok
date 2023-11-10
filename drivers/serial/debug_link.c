#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <fs/fs.h>
#include <kernel/errno.h>
#include <kernel/interrupt.h>
#include <kernel/kernel.h>
#include <kernel/pipe.h>
#include <kernel/printk.h>
#include <kernel/wait.h>

#include "stm32f4xx.h"
#include "uart.h"

#define UART3_RX_BUF_SIZE 100

#define UART3_ISR_PRIORITY 14

static int uart3_dma_puts(const char *data, size_t size);

ssize_t serial2_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t serial2_write(struct file *filp,
                      const char *buf,
                      size_t size,
                      off_t offset);
int serial2_open(struct inode *inode, struct file *file);

void USART3_IRQHandler(void);
void DMA1_Stream4_IRQHandler(void);

uart_dev_t uart3 = {
    .rx_fifo = NULL,
    .rx_wait_size = 0,
    .tx_dma_ready = false,
    .tx_state = UART_TX_IDLE,
};

static struct file_operations serial2_file_ops = {
    .read = serial2_read,
    .write = serial2_write,
    .open = serial2_open,
};

void uart3_init(uint32_t baudrate)
{
    /* initialize the rcc */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);

    /* initialize the gpio */
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_USART3);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_USART3);
    GPIO_InitTypeDef gpio = {
        .GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11,
        .GPIO_Mode = GPIO_Mode_AF,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_UP,
    };
    GPIO_Init(GPIOC, &gpio);

    /* initialize the uart3 */
    USART_InitTypeDef uart3 = {
        .USART_BaudRate = baudrate,
        .USART_Mode = USART_Mode_Rx | USART_Mode_Tx,
        .USART_WordLength = USART_WordLength_8b,
        .USART_StopBits = USART_StopBits_1,
        .USART_Parity = USART_Parity_No,
    };
    USART_Init(USART3, &uart3);
    USART_Cmd(USART3, ENABLE);
#if (ENABLE_UART3_DMA != 0)
    USART_DMACmd(USART3, USART_DMAReq_Tx, ENABLE);
#endif
    USART_ClearFlag(USART3, USART_FLAG_TC);

    /* initialize interrupt of the uart3*/
    NVIC_InitTypeDef nvic = {
        .NVIC_IRQChannel = USART3_IRQn,
        .NVIC_IRQChannelPreemptionPriority = UART3_ISR_PRIORITY,
        .NVIC_IRQChannelSubPriority = 0,
        .NVIC_IRQChannelCmd = ENABLE,
    };
    NVIC_Init(&nvic);

    request_irq(USART3_IRQn, USART3_IRQHandler, 0, NULL, NULL);
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
#if (ENABLE_UART3_DMA != 0)
    /* initialize interrupt of the dma1 channel4 */
    nvic.NVIC_IRQChannel = DMA1_Stream4_IRQn;
    NVIC_Init(&nvic);

    request_irq(DMA1_Stream4_IRQn, DMA1_Stream4_IRQHandler, 0, NULL, NULL);
    DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, DISABLE);
#endif
}

void serial2_init(void)
{
    /* initialize serial2 as character device */
    register_chrdev("serial2", &serial2_file_ops);

    /* create kfifo for reception */
    uart3.rx_fifo = kfifo_alloc(sizeof(uint8_t), UART3_RX_BUF_SIZE);

    /* create wait queues for synchronization */
    init_waitqueue_head(&uart3.tx_wq);
    init_waitqueue_head(&uart3.rx_wq);

    /* initialize uart3 */
    uart3_init(115200);

    printk("chardev serial2: debug-link");
}

int serial2_open(struct inode *inode, struct file *file)
{
    return 0;
}

ssize_t serial2_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    if (kfifo_len(uart3.rx_fifo) >= size) {
        kfifo_out(uart3.rx_fifo, buf, size);
        return size;
    } else {
        init_wait(uart3.rx_wait);
        prepare_to_wait(&uart3.rx_wq, uart3.rx_wait, THREAD_WAIT);
        uart3.rx_wait_size = size;
        return -ERESTARTSYS;
    }
}

ssize_t serial2_write(struct file *filp,
                      const char *buf,
                      size_t size,
                      off_t offset)
{
#if (ENABLE_UART3_DMA != 0)
    return uart3_dma_puts(buf, size);
#else
    return uart_puts(USART3, buf, size);
#endif
}

static int uart3_dma_puts(const char *data, size_t size)
{
    switch (uart3.tx_state) {
    case UART_TX_IDLE: {
        /* configure the dma */
        DMA_InitTypeDef DMA_InitStructure = {
            .DMA_BufferSize = (uint32_t) size,
            .DMA_FIFOMode = DMA_FIFOMode_Disable,
            .DMA_FIFOThreshold = DMA_FIFOThreshold_Full,
            .DMA_MemoryBurst = DMA_MemoryBurst_Single,
            .DMA_MemoryDataSize = DMA_MemoryDataSize_Byte,
            .DMA_MemoryInc = DMA_MemoryInc_Enable,
            .DMA_Mode = DMA_Mode_Normal,
            .DMA_PeripheralBaseAddr = (uint32_t) (&USART3->DR),
            .DMA_PeripheralBurst = DMA_PeripheralBurst_Single,
            .DMA_PeripheralInc = DMA_PeripheralInc_Disable,
            .DMA_Priority = DMA_Priority_Medium,
            .DMA_Channel = DMA_Channel_7,
            .DMA_DIR = DMA_DIR_MemoryToPeripheral,
            .DMA_Memory0BaseAddr = (uint32_t) data,
        };
        DMA_Init(DMA1_Stream4, &DMA_InitStructure);

        /* enable dma to copy the data */
        DMA_ClearFlag(DMA1_Stream4, DMA_FLAG_TCIF4);
        DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, ENABLE);
        DMA_Cmd(DMA1_Stream4, ENABLE);

        uart3.tx_state = UART_TX_DMA_BUSY;
        uart3.tx_dma_ready = false;

        /* wait until dma complete data transfer */
        init_wait(uart3.tx_wait);
        prepare_to_wait(&uart3.tx_wq, uart3.tx_wait, THREAD_WAIT);
        return -ERESTARTSYS;
    }
    case UART_TX_DMA_BUSY: {
        /* notified by the dma irq, the data transfer is now complete */
        uart3.tx_state = UART_TX_IDLE;
        return size;
    }
    default: {
        return -EIO;
    }
    }
}

void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(USART3, USART_IT_RXNE) == SET) {
        uint8_t c = USART_ReceiveData(USART3);
        kfifo_put(uart3.rx_fifo, &c);

        if (kfifo_len(uart3.rx_fifo) >= uart3.rx_wait_size) {
            finish_wait(uart3.rx_wait);
            uart3.rx_wait_size = 0;
        }
    }
}

void DMA1_Stream4_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_Stream4, DMA_IT_TCIF4) == SET) {
        DMA_ClearITPendingBit(DMA1_Stream4, DMA_IT_TCIF4);
        DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, DISABLE);

        finish_wait(uart3.tx_wait);
        uart3.tx_dma_ready = true;
    }
}
