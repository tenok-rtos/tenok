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

#define UART3_RX_BUF_SIZE 100

static int uart3_dma_puts(const char *data, size_t size);

ssize_t serial2_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t serial2_write(struct file *filp, const char *buf, size_t size, off_t offset);

wait_queue_head_t uart3_tx_wq;
wait_queue_head_t uart3_rx_wq;

uart_dev_t uart3 = {
    .rx_fifo = NULL,
    .rx_wait_size = 0,
    .tx_dma_ready = false,
    .state = UART_TX_IDLE
};

static struct file_operations serial2_file_ops = {
    .read = serial2_read,
    .write = serial2_write
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
        .GPIO_PuPd = GPIO_PuPd_UP
    };
    GPIO_Init(GPIOC, &gpio);

    /* initialize the uart3 */
    USART_InitTypeDef uart3 = {
        .USART_BaudRate = baudrate,
        .USART_Mode = USART_Mode_Rx | USART_Mode_Tx,
        .USART_WordLength = USART_WordLength_8b,
        .USART_StopBits = USART_StopBits_1,
        .USART_Parity = USART_Parity_No
    };
    USART_Init(USART3, &uart3);
    USART_Cmd(USART3, ENABLE);
#if (ENABLE_UART3_DMA != 0)
    USART_DMACmd(USART3, USART_DMAReq_Tx, ENABLE);
#endif
    USART_ClearFlag(USART3, USART_FLAG_TC);
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    /* initialize interrupt of the uart3*/
    NVIC_InitTypeDef nvic = {
        .NVIC_IRQChannel = USART3_IRQn,
        .NVIC_IRQChannelPreemptionPriority = KERNEL_INT_PRI + 1,
        .NVIC_IRQChannelSubPriority = 0,
        .NVIC_IRQChannelCmd = ENABLE
    };
    NVIC_Init(&nvic);

#if (ENABLE_UART3_DMA != 0)
    /* initialize interrupt of the dma1 channel4 */
    nvic.NVIC_IRQChannel = DMA1_Stream4_IRQn;
    NVIC_Init(&nvic);
    DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, DISABLE);
#endif
}

void serial2_init(void)
{
    /* initialize serial2 as character device */
    register_chrdev("serial2", &serial2_file_ops);

    /* create pipe for reception */
    uart3.rx_fifo = ringbuf_create(sizeof(uint8_t), UART3_RX_BUF_SIZE);

    list_init(&uart3_tx_wq);
    list_init(&uart3_rx_wq);

    /* initialize uart3 */
    uart3_init(115200);
}

ssize_t serial2_read(struct file *filp, char *buf, size_t size, off_t offset)
{
    bool ready = ringbuf_get_cnt(uart3.rx_fifo) >= size;
    wait_event(&uart3_rx_wq, ready);

    if(ready) {
        ringbuf_out(uart3.rx_fifo, buf, size);
        return size;
    } else {
        uart3.rx_wait_size = size;
        return 0;
    }
}

ssize_t serial2_write(struct file *filp, const char *buf, size_t size, off_t offset)
{
#if (ENABLE_UART3_DMA != 0)
    return uart3_dma_puts(buf, size);
#else
    return uart_puts(USART3, buf, size);
#endif
}

static int uart3_dma_puts(const char *data, size_t size)
{
    if(uart3.state == UART_TX_IDLE) {
        /* configure the dma */
        DMA_InitTypeDef DMA_InitStructure = {
            .DMA_BufferSize = (uint32_t)size,
            .DMA_FIFOMode = DMA_FIFOMode_Disable,
            .DMA_FIFOThreshold = DMA_FIFOThreshold_Full,
            .DMA_MemoryBurst = DMA_MemoryBurst_Single,
            .DMA_MemoryDataSize = DMA_MemoryDataSize_Byte,
            .DMA_MemoryInc = DMA_MemoryInc_Enable,
            .DMA_Mode = DMA_Mode_Normal,
            .DMA_PeripheralBaseAddr = (uint32_t)(&USART3->DR),
            .DMA_PeripheralBurst = DMA_PeripheralBurst_Single,
            .DMA_PeripheralInc = DMA_PeripheralInc_Disable,
            .DMA_Priority = DMA_Priority_Medium,
            .DMA_Channel = DMA_Channel_7,
            .DMA_DIR = DMA_DIR_MemoryToPeripheral,
            .DMA_Memory0BaseAddr = (uint32_t)data
        };
        DMA_Init(DMA1_Stream4, &DMA_InitStructure);

        /* enable dma to copy the data */
        DMA_ClearFlag(DMA1_Stream4, DMA_FLAG_TCIF4);
        DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, ENABLE);
        DMA_Cmd(DMA1_Stream4, ENABLE);

        uart3.state = UART_TX_DMA_BUSY;
        uart3.tx_dma_ready = false;
    } else if(uart3.state == UART_TX_DMA_BUSY) {
        wait_event(&uart3_tx_wq, uart3.tx_dma_ready);

        if(uart3.tx_dma_ready) {
            return size;
        } else {
            return -EAGAIN;
        }
    }
}

void USART3_IRQHandler(void)
{
    if(USART_GetITStatus(USART3, USART_IT_RXNE) == SET) {
        uint8_t c = USART_ReceiveData(USART3);
        ringbuf_put(uart3.rx_fifo, &c);

        if(ringbuf_get_cnt(uart3.rx_fifo) >= uart3.rx_wait_size) {
            wake_up(&uart3_rx_wq);
            uart3.rx_wait_size = 0;
        }
    }
}

void DMA1_Stream4_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA1_Stream4, DMA_IT_TCIF4) == SET) {
        DMA_ClearITPendingBit(DMA1_Stream4, DMA_IT_TCIF4);
        DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, DISABLE);

        uart3.tx_dma_ready = true;
        wake_up(&uart3_rx_wq);
    }
}

HOOK_DRIVER(serial2_init);
