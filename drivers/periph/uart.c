#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <fs/fs.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/kfifo.h>
#include <kernel/mutex.h>
#include <kernel/poll.h>
#include <kernel/preempt.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/tty.h>

#include "stm32f4xx_conf.h"
#include "uart.h"

#define UART1_RX_BUF_SIZE 100
#define UART2_RX_BUF_SIZE 100
#define UART3_RX_BUF_SIZE 100

#define UART1_ISR_PRIORITY 14
#define UART2_ISR_PRIORITY 14
#define UART3_ISR_PRIORITY 14

static uart_dev_t uart1;
static uart_dev_t uart2;
static uart_dev_t uart3;

/*====================*
 * UART common driver *
 *====================*/

/* The settings a terminal starts with. The input side is not processed yet,
 * so nothing is claimed for it
 */
static void tty_init(uart_dev_t *dev, speed_t speed)
{
    dev->termios.c_oflag = OPOST | ONLCR;
    dev->termios.c_cflag = CS8;
    dev->termios.c_ispeed = speed;
    dev->termios.c_ospeed = speed;
}

/* Output processing: with ONLCR a newline goes out as a return and a newline.
 * The pair is a local because DMA reads the memory it is handed, and DMA1 has
 * no port to the flash a literal would live in
 */
static int tty_write(uart_dev_t *dev,
                     int (*put)(const char *buf, size_t size),
                     const char *buf,
                     size_t size)
{
    const tcflag_t on = OPOST | ONLCR;

    if ((dev->termios.c_oflag & on) != on)
        return put(buf, size);

    char crlf[2] = {'\r', '\n'};
    size_t done = 0;

    while (done < size) {
        size_t run = done;

        while (run < size && buf[run] != '\n')
            run++;

        if (run > done)
            put(&buf[done], run - done);

        if (run < size)
            put(crlf, sizeof(crlf));

        done = (run < size) ? run + 1 : run;
    }

    return size;
}

/* Answer the requests of the line discipline. A device that is not a terminal
 * has no settings to give, which is what ENOTTY says
 */
static int tty_ioctl(uart_dev_t *dev, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case TCGETS:
        *(struct termios *) arg = dev->termios;
        return 0;
    case TCSETS:
        dev->termios = *(struct termios *) arg;
        return 0;
    default:
        return -ENOTTY;
    }
}

void uart_putc(USART_TypeDef *uart, char c)
{
    while (USART_GetFlagStatus(uart, USART_FLAG_TXE) == RESET)
        ;
    USART_SendData(uart, c);
    while (USART_GetFlagStatus(uart, USART_FLAG_TC) == RESET)
        ;
}

char uart_getc(USART_TypeDef *uart)
{
    while (USART_GetFlagStatus(uart, USART_FLAG_RXNE) == RESET)
        ;
    return USART_ReceiveData(uart);
}

int uart_puts(USART_TypeDef *uart, const char *data, size_t size)
{
    for (int i = 0; i < size; i++)
        uart_putc(uart, data[i]);

    return size;
}

/*==============*
 * UART1 driver *
 *==============*/

static int uart1_open(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t uart1_read(struct file *filp,
                          char *buf,
                          size_t size,
                          off_t offset)
{
    mutex_lock(&uart1.rx_mtx);

    /* Remember the file so that the interrupt handler can report readability
     * to poll(). fs_open_file() never calls the open operation of a driver,
     * so this is the first place the device learns which file it is.
     */
    uart1.filp = filp;

    preempt_disable();

    /* Block until the device has something, then hand over whatever arrived.
     * Waiting for the full request would hang any reader that asks for more
     * than the sender is about to send.
     */
    uart1.rx_wait_size = 1;
    wait_event(uart1.rx_wait_list, kfifo_len(uart1.rx_fifo) > 0);

    size_t avail = kfifo_len(uart1.rx_fifo);
    if (size > avail)
        size = avail;

    preempt_enable();

    /* kfifo_out() pops a single element however large its size argument is,
     * so the bytes have to be taken one at a time. The interrupt handler only
     * ever adds, the buffer cannot shrink under this loop.
     */
    for (size_t i = 0; i < size; i++)
        kfifo_out(uart1.rx_fifo, &buf[i], sizeof(char));

    /* The interrupt handler is masked by the preemption lock, so the buffer
     * cannot refill in between
     */
    preempt_disable();
    if (kfifo_len(uart1.rx_fifo) == 0)
        filp->f_events &= ~POLLIN;
    preempt_enable();

    mutex_unlock(&uart1.rx_mtx);

    return size;
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

static int uart1_put(const char *buf, size_t size)
{
#if (ENABLE_UART1_DMA != 0)
    return uart1_dma_puts(buf, size);
#else
    return uart_puts(USART1, buf, size);
#endif
}

static ssize_t uart1_write(struct file *filp,
                           const char *buf,
                           size_t size,
                           off_t offset)
{
    return tty_write(&uart1, uart1_put, buf, size);
}

static int uart1_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return tty_ioctl(&uart1, cmd, arg);
}

static struct file_operations uart1_file_ops = {
    .read = uart1_read,
    .write = uart1_write,
    .ioctl = uart1_ioctl,
    .open = uart1_open,
};

static void serial1_rx_interrupt_handler(uint8_t c)
{
    kfifo_put(uart1.rx_fifo, &c);

    if (uart1.filp) {
        uart1.filp->f_events |= POLLIN;
        poll_notify(uart1.filp);
    }

    if (uart1.rx_wait_size && kfifo_len(uart1.rx_fifo) >= uart1.rx_wait_size) {
        uart1.rx_wait_size = 0;
        wake_up(&uart1.rx_wait_list);
    }
}

static void uart1_init(uint32_t baudrate, void (*rx_callback)(uint8_t c))
{
    uart1.rx_callback = rx_callback;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

#ifdef DYNAMICS_WIZARD_F4
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);
#else
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_USART1);
#endif

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Mode = GPIO_Mode_AF,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_UP,
    };

#ifdef DYNAMICS_WIZARD_F4
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
#else
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
#endif

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

void serial1_init(uint32_t baudrate, char *dev_name, char *desc)
{
    /* Register UART1 to the file system */
    register_chrdev(dev_name, &uart1_file_ops);

    /* Give the terminal its settings */
    tty_init(&uart1, baudrate);

    /* Create wait queues for synchronization */
    init_waitqueue_head(&uart1.tx_wait_list);
    init_waitqueue_head(&uart1.rx_wait_list);

    /* Create rx buffer */
    uart1.rx_fifo = kfifo_alloc(sizeof(uint8_t), UART1_RX_BUF_SIZE);

    /* Initialize UART1 */
    uart1_init(baudrate, serial1_rx_interrupt_handler);

    mutex_init(&uart1.tx_mtx);
    mutex_init(&uart1.rx_mtx);

    printk("chardev %s: %s", dev_name, desc);
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET) {
        uint8_t c = USART_ReceiveData(USART1);

        if (uart1.rx_callback)
            uart1.rx_callback(c);
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

/*==============*
 * UART2 driver *
 *==============*/

static int uart2_open(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t uart2_read(struct file *filp,
                          char *buf,
                          size_t size,
                          off_t offset)
{
    mutex_lock(&uart2.rx_mtx);

    /* Remember the file so that the interrupt handler can report readability
     * to poll(). fs_open_file() never calls the open operation of a driver,
     * so this is the first place the device learns which file it is.
     */
    uart2.filp = filp;

    preempt_disable();

    /* Block until the device has something, then hand over whatever arrived.
     * Waiting for the full request would hang any reader that asks for more
     * than the sender is about to send.
     */
    uart2.rx_wait_size = 1;
    wait_event(uart2.rx_wait_list, kfifo_len(uart2.rx_fifo) > 0);

    size_t avail = kfifo_len(uart2.rx_fifo);
    if (size > avail)
        size = avail;

    preempt_enable();

    /* kfifo_out() pops a single element however large its size argument is,
     * so the bytes have to be taken one at a time. The interrupt handler only
     * ever adds, the buffer cannot shrink under this loop.
     */
    for (size_t i = 0; i < size; i++)
        kfifo_out(uart2.rx_fifo, &buf[i], sizeof(char));

    /* The interrupt handler is masked by the preemption lock, so the buffer
     * cannot refill in between
     */
    preempt_disable();
    if (kfifo_len(uart2.rx_fifo) == 0)
        filp->f_events &= ~POLLIN;
    preempt_enable();

    mutex_unlock(&uart2.rx_mtx);

    return size;
}

static int uart2_put(const char *buf, size_t size)
{
    return uart_puts(USART2, buf, size);
}

static ssize_t uart2_write(struct file *filp,
                           const char *buf,
                           size_t size,
                           off_t offset)
{
    return tty_write(&uart2, uart2_put, buf, size);
}

static int uart2_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return tty_ioctl(&uart2, cmd, arg);
}

static struct file_operations uart2_file_ops = {
    .read = uart2_read,
    .write = uart2_write,
    .ioctl = uart2_ioctl,
    .open = uart2_open,
};

static void serial2_rx_interrupt_handler(uint8_t c)
{
    kfifo_put(uart2.rx_fifo, &c);

    if (uart2.filp) {
        uart2.filp->f_events |= POLLIN;
        poll_notify(uart2.filp);
    }

    if (uart2.rx_wait_size && kfifo_len(uart2.rx_fifo) >= uart2.rx_wait_size) {
        uart2.rx_wait_size = 0;
        wake_up(&uart2.rx_wait_list);
    }
}

void uart2_init(uint32_t baudrate, void (*rx_callback)(uint8_t c))
{
    uart2.rx_callback = rx_callback;

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

void serial2_init(uint32_t baudrate, char *dev_name, char *desc)
{
    /* Register UART2 to the file system */
    register_chrdev(dev_name, &uart2_file_ops);

    /* Give the terminal its settings */
    tty_init(&uart2, baudrate);

    /* Create wait queues for synchronization */
    init_waitqueue_head(&uart2.tx_wait_list);
    init_waitqueue_head(&uart2.rx_wait_list);

    /* Create kfifo for UART2 rx */
    uart2.rx_fifo = kfifo_alloc(sizeof(uint8_t), UART2_RX_BUF_SIZE);

    /* Initialize UART2 */
    uart2_init(baudrate, serial2_rx_interrupt_handler);

    mutex_init(&uart2.tx_mtx);
    mutex_init(&uart2.rx_mtx);

    printk("chardev %s: %s", dev_name, desc);
}

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET) {
        uint8_t c = USART_ReceiveData(USART2);

        if (uart2.rx_callback)
            uart2.rx_callback(c);
    }
}

/*==============*
 * UART3 driver *
 *==============*/

static int uart3_open(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t uart3_read(struct file *filp,
                          char *buf,
                          size_t size,
                          off_t offset)
{
    mutex_lock(&uart3.rx_mtx);

    /* Remember the file so that the interrupt handler can report readability
     * to poll(). fs_open_file() never calls the open operation of a driver,
     * so this is the first place the device learns which file it is.
     */
    uart3.filp = filp;

    preempt_disable();

    /* Block until the device has something, then hand over whatever arrived.
     * Waiting for the full request would hang any reader that asks for more
     * than the sender is about to send.
     */
    uart3.rx_wait_size = 1;
    wait_event(uart3.rx_wait_list, kfifo_len(uart3.rx_fifo) > 0);

    size_t avail = kfifo_len(uart3.rx_fifo);
    if (size > avail)
        size = avail;

    preempt_enable();

    /* kfifo_out() pops a single element however large its size argument is,
     * so the bytes have to be taken one at a time. The interrupt handler only
     * ever adds, the buffer cannot shrink under this loop.
     */
    for (size_t i = 0; i < size; i++)
        kfifo_out(uart3.rx_fifo, &buf[i], sizeof(char));

    /* The interrupt handler is masked by the preemption lock, so the buffer
     * cannot refill in between
     */
    preempt_disable();
    if (kfifo_len(uart3.rx_fifo) == 0)
        filp->f_events &= ~POLLIN;
    preempt_enable();

    mutex_unlock(&uart3.rx_mtx);

    return size;
}

static int uart3_dma_puts(const char *data, size_t size)
{
    mutex_lock(&uart3.tx_mtx);

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
        .DMA_PeripheralBaseAddr = (uint32_t) (&USART3->DR),
        .DMA_PeripheralBurst = DMA_PeripheralBurst_Single,
        .DMA_PeripheralInc = DMA_PeripheralInc_Disable,
        .DMA_Priority = DMA_Priority_Medium,
        .DMA_Channel = DMA_Channel_7,
        .DMA_DIR = DMA_DIR_MemoryToPeripheral,
        .DMA_Memory0BaseAddr = (uint32_t) data,
    };
    DMA_Init(DMA1_Stream4, &DMA_InitStructure);

    /* Enable DMA to copy the data */
    DMA_ClearFlag(DMA1_Stream4, DMA_FLAG_TCIF4);
    DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA1_Stream4, ENABLE);

    /* Wait until DMA complete data transfer */
    uart3.tx_ready = false;
    wait_event(uart3.tx_wait_list, uart3.tx_ready);

    preempt_enable();

    mutex_unlock(&uart3.tx_mtx);

    return size;
}

static int uart3_put(const char *buf, size_t size)
{
#if (ENABLE_UART3_DMA != 0)
    return uart3_dma_puts(buf, size);
#else
    return uart_puts(USART3, buf, size);
#endif
}

static ssize_t uart3_write(struct file *filp,
                           const char *buf,
                           size_t size,
                           off_t offset)
{
    return tty_write(&uart3, uart3_put, buf, size);
}

static int uart3_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return tty_ioctl(&uart3, cmd, arg);
}

static struct file_operations uart3_file_ops = {
    .read = uart3_read,
    .write = uart3_write,
    .ioctl = uart3_ioctl,
    .open = uart3_open,
};

static void serial3_rx_interrupt_handler(uint8_t c)
{
    kfifo_put(uart3.rx_fifo, &c);

    if (uart3.filp) {
        uart3.filp->f_events |= POLLIN;
        poll_notify(uart3.filp);
    }

    if (uart3.rx_wait_size && kfifo_len(uart3.rx_fifo) >= uart3.rx_wait_size) {
        uart3.rx_wait_size = 0;
        wake_up(&uart3.rx_wait_list);
    }
}

static void uart3_init(uint32_t baudrate, void (*rx_callback)(uint8_t c))
{
    uart3.rx_callback = rx_callback;

    /* Initialize the RCC */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);

    /* Initialize the GPIO */
#ifdef DYNAMICS_WIZARD_F4
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3);
    GPIO_InitTypeDef gpio = {
        .GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9,
        .GPIO_Mode = GPIO_Mode_AF,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_UP,
    };
    GPIO_Init(GPIOD, &gpio);
#else
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
#endif

    /* Initialize the UART3 */
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

    /* Initialize interrupt of the UART3 */
    NVIC_InitTypeDef nvic = {
        .NVIC_IRQChannel = USART3_IRQn,
        .NVIC_IRQChannelPreemptionPriority = UART3_ISR_PRIORITY,
        .NVIC_IRQChannelSubPriority = 0,
        .NVIC_IRQChannelCmd = ENABLE,
    };
    NVIC_Init(&nvic);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
#if (ENABLE_UART3_DMA != 0)
    /* Initialize interrupt of the DMA1 channel4 */
    nvic.NVIC_IRQChannel = DMA1_Stream4_IRQn;
    NVIC_Init(&nvic);

    DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, DISABLE);
#endif
}

void serial3_init(uint32_t baudrate, char *dev_name, char *desc)
{
    /* Register UART3 to the file system */
    register_chrdev(dev_name, &uart3_file_ops);

    /* Give the terminal its settings */
    tty_init(&uart3, baudrate);

    /* Create wait queues for synchronization */
    init_waitqueue_head(&uart3.tx_wait_list);
    init_waitqueue_head(&uart3.rx_wait_list);

    /* Create kfifo for UART3 rx */
    uart3.rx_fifo = kfifo_alloc(sizeof(uint8_t), UART3_RX_BUF_SIZE);

    mutex_init(&uart3.tx_mtx);
    mutex_init(&uart3.rx_mtx);

    /* Initialize UART3 */
    uart3_init(baudrate, serial3_rx_interrupt_handler);

    printk("chardev %s: %s", dev_name, desc);
}

void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(USART3, USART_IT_RXNE) == SET) {
        uint8_t c = USART_ReceiveData(USART3);

        if (uart3.rx_callback)
            uart3.rx_callback(c);
    }
}

void DMA1_Stream4_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_Stream4, DMA_IT_TCIF4) == SET) {
        DMA_ClearITPendingBit(DMA1_Stream4, DMA_IT_TCIF4);
        DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, DISABLE);

        uart3.tx_ready = true;
        wake_up(&uart3.tx_wait_list);
    }
}
