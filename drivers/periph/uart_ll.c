/**
 * @file
 * The serial driver of the parts ST never wrote a Standard Peripheral Library
 * for, written against the Low Layer that took its place.
 *
 * Only one port is brought up, and it is brought up as a terminal: the board
 * this serves has no second thing to talk to. What it does with the bytes once
 * it has them is not its own business but the line discipline's, which it
 * shares with the older driver.
 *
 * The transmit side waits on the port rather than handing the bytes to a DMA
 * stream the way the older driver can. A console is not where that is worth
 * the streams it would take.
 */

#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>

#include <fs/fs.h>
#include <kernel/kfifo.h>
#include <kernel/mutex.h>
#include <kernel/poll.h>
#include <kernel/printk.h>
#include <kernel/wait.h>

#include "serial_tty.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_gpio.h"
#include "stm32f7xx_ll_rcc.h"
#include "stm32f7xx_ll_usart.h"
#include "uart_ll.h"

#define CONSOLE_RX_BUF_SIZE 100
#define CONSOLE_ISR_PRIORITY 14

/* The one port this driver brings up, and the port itself */
static uart_dev_t console;
static USART_TypeDef *console_uart;

/*=========================*
 * Talking to the register *
 *=========================*/

void uart_putc(USART_TypeDef *uart, char c)
{
    while (!LL_USART_IsActiveFlag_TXE(uart))
        ;
    LL_USART_TransmitData8(uart, (uint8_t) c);
}

char uart_getc(USART_TypeDef *uart)
{
    while (!LL_USART_IsActiveFlag_RXNE(uart))
        ;
    return (char) LL_USART_ReceiveData8(uart);
}

int uart_puts(USART_TypeDef *uart, const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        uart_putc(uart, data[i]);

    return size;
}

/*=============================*
 * The terminal it is dressed as *
 *=============================*/

static int console_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int console_put(const char *buf, size_t size)
{
    mutex_lock(&console.tx_mtx);
    uart_puts(console_uart, buf, size);
    mutex_unlock(&console.tx_mtx);

    return size;
}

static ssize_t console_read(struct file *filp,
                            char *buf,
                            size_t size,
                            off_t offset)
{
    return tty_read(&console, filp, console_put, buf, size);
}

static ssize_t console_write(struct file *filp,
                             const char *buf,
                             size_t size,
                             off_t offset)
{
    return tty_write(&console, console_put, buf, size);
}

static int console_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return tty_ioctl(&console, cmd, arg);
}

static struct file_operations console_file_ops = {
    .read = console_read,
    .write = console_write,
    .ioctl = console_ioctl,
    .open = console_open,
};

static void console_rx_interrupt_handler(uint8_t c)
{
    kfifo_put(console.rx_fifo, &c);

    if (console.filp) {
        console.filp->f_events |= POLLIN;
        poll_notify(console.filp);
    }

    if (console.rx_wait_size &&
        kfifo_len(console.rx_fifo) >= console.rx_wait_size) {
        console.rx_wait_size = 0;
        wake_up(&console.rx_wait_list);
    }
}

/*===========*
 * Bringing up *
 *===========*/

/* Which interrupt the port raises, and what clock its baud rate is counted
 * against. Both are things the port decides and neither is anywhere the Low
 * Layer will answer for a port it is handed, so they are answered here.
 */
static IRQn_Type console_irqn(USART_TypeDef *uart)
{
    if (uart == USART1)
        return USART1_IRQn;
    if (uart == USART2)
        return USART2_IRQn;
    if (uart == USART3)
        return USART3_IRQn;
    if (uart == UART4)
        return UART4_IRQn;
    if (uart == UART5)
        return UART5_IRQn;
    if (uart == USART6)
        return USART6_IRQn;

    return UsageFault_IRQn;
}

static uint32_t console_clock(USART_TypeDef *uart)
{
    if (uart == USART1)
        return LL_RCC_GetUSARTClockFreq(LL_RCC_USART1_CLKSOURCE);
    if (uart == USART2)
        return LL_RCC_GetUSARTClockFreq(LL_RCC_USART2_CLKSOURCE);
    if (uart == USART3)
        return LL_RCC_GetUSARTClockFreq(LL_RCC_USART3_CLKSOURCE);
    if (uart == UART4)
        return LL_RCC_GetUARTClockFreq(LL_RCC_UART4_CLKSOURCE);
    if (uart == UART5)
        return LL_RCC_GetUARTClockFreq(LL_RCC_UART5_CLKSOURCE);

    return LL_RCC_GetUSARTClockFreq(LL_RCC_USART6_CLKSOURCE);
}

void serial_console_init(USART_TypeDef *uart,
                         uint32_t baudrate,
                         char *dev_name,
                         char *desc)
{
    console_uart = uart;
    console.rx_callback = console_rx_interrupt_handler;

    register_chrdev(dev_name, &console_file_ops);

    tty_init(&console, baudrate);

    init_waitqueue_head(&console.tx_wait_list);
    init_waitqueue_head(&console.rx_wait_list);

    console.rx_fifo = kfifo_alloc(sizeof(uint8_t), CONSOLE_RX_BUF_SIZE);

    mutex_init(&console.tx_mtx);
    mutex_init(&console.rx_mtx);

    /* The pins are the board's to say, and it has already put them in the
     * alternate function the port answers to
     */
    LL_USART_InitTypeDef usart = {
        .BaudRate = baudrate,
        .DataWidth = LL_USART_DATAWIDTH_8B,
        .StopBits = LL_USART_STOPBITS_1,
        .Parity = LL_USART_PARITY_NONE,
        .TransferDirection = LL_USART_DIRECTION_TX_RX,
        .HardwareFlowControl = LL_USART_HWCONTROL_NONE,
        .OverSampling = LL_USART_OVERSAMPLING_16,
    };
    LL_USART_Init(uart, &usart);
    LL_USART_ConfigAsyncMode(uart);
    LL_USART_Enable(uart);

    IRQn_Type irqn = console_irqn(uart);
    NVIC_SetPriority(irqn, CONSOLE_ISR_PRIORITY);
    NVIC_EnableIRQ(irqn);
    LL_USART_EnableIT_RXNE(uart);

    printk("chardev %s: %s", dev_name, desc);
}

/* The port the board chose is the only one whose interrupt is claimed, so
 * every handler leads to the same place and asks whether it was theirs
 */
static void console_isr(void)
{
    if (!console_uart || !LL_USART_IsActiveFlag_RXNE(console_uart))
        return;

    uint8_t c = LL_USART_ReceiveData8(console_uart);

    if (console.rx_callback)
        console.rx_callback(c);
}

void USART1_IRQHandler(void)
{
    console_isr();
}

void USART2_IRQHandler(void)
{
    console_isr();
}

void USART3_IRQHandler(void)
{
    console_isr();
}

void UART4_IRQHandler(void)
{
    console_isr();
}

void UART5_IRQHandler(void)
{
    console_isr();
}

void USART6_IRQHandler(void)
{
    console_isr();
}
