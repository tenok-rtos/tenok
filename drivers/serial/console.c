#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include <fs/fs.h>
#include <kernel/pipe.h>
#include <kernel/wait.h>
#include <kernel/kernel.h>
#include <kernel/semaphore.h>

#include "uart.h"
#include "stm32f4xx.h"

#define UART1_RX_BUF_SIZE 100

static int uart1_dma_puts(const char *data, size_t size);

ssize_t serial0_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t serial0_write(struct file *filp, const char *buf, size_t size, off_t offset);

wait_queue_head_t uart1_tx_wq;
wait_queue_head_t uart1_rx_wq;

uart_dev_t uart1 = {
    .rx_fifo = NULL,
    .rx_wait_size = 0,
    .tx_dma_ready = false,
    .state = UART_TX_IDLE
};

static struct file_operations serial0_file_ops = {
    .read = serial0_read,
    .write = serial0_write
};

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
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    /* initialize interrupt of the uart1 */
    NVIC_InitTypeDef nvic = {
        .NVIC_IRQChannel = USART1_IRQn,
        .NVIC_IRQChannelPreemptionPriority = KERNEL_INT_PRI + 1,
        .NVIC_IRQChannelSubPriority = 0,
        .NVIC_IRQChannelCmd = ENABLE
    };
    NVIC_Init(&nvic);

#if (ENABLE_UART1_DMA != 0)
    /* initialize interrupt of the dma2 channel7 */
    nvic.NVIC_IRQChannel = DMA2_Stream7_IRQn;
    NVIC_Init(&nvic);
    DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, DISABLE);
#endif
}

void serial0_init(void)
{
    /* initialize serial0 as character device */
    register_chrdev("serial0", &serial0_file_ops);

    /* create pipe for reception */
    uart1.rx_fifo = ringbuf_create(sizeof(uint8_t), UART1_RX_BUF_SIZE);

    list_init(&uart1_tx_wq);
    list_init(&uart1_rx_wq);

    /* initialize uart1 */
    uart1_init(115200);
}

ssize_t serial0_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    bool ready = ringbuf_get_cnt(uart1.rx_fifo) >= size;
    wait_event(&uart1_rx_wq, ready);

    if(ready) {
        ringbuf_out(uart1.rx_fifo, buf, size);
        return size;
    } else {
        uart1.rx_wait_size = size;
        return 0;
    }
}

ssize_t serial0_write(struct file *filp, const char *buf, size_t size, off_t offset)
{
#if (ENABLE_UART1_DMA != 0)
    return uart1_dma_puts(buf, size);
#else
    return uart_puts(USART1, buf, size);
#endif
}

static int uart1_dma_puts(const char *data, size_t size)
{
    if(uart1.state == UART_TX_IDLE) {
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

        uart1.state = UART_TX_DMA_BUSY;
        uart1.tx_dma_ready = false;
    } else if(uart1.state == UART_TX_DMA_BUSY) {
        wait_event(&uart1_tx_wq, uart1.tx_dma_ready);

        if(uart1.tx_dma_ready) {
            return size;
        } else {
            return -EAGAIN;
        }
    }
}

void USART1_IRQHandler(void)
{
    if(USART_GetITStatus(USART1, USART_IT_RXNE) == SET) {
        uint8_t c = USART_ReceiveData(USART1);
        ringbuf_put(uart1.rx_fifo, &c);

        if(ringbuf_get_cnt(uart1.rx_fifo) >= uart1.rx_wait_size) {
            wake_up(&uart1_rx_wq);
            uart1.rx_wait_size = 0;
        }
    }
}

void DMA2_Stream7_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA2_Stream7, DMA_IT_TCIF7) == SET) {
        DMA_ClearITPendingBit(DMA2_Stream7, DMA_IT_TCIF7);
        DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, DISABLE);

        uart1.tx_dma_ready = true;
        wake_up(&uart1_rx_wq);
    }
}

HOOK_DRIVER(serial0_init);
