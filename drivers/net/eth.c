/**
 * @file
 * The Ethernet controller of an STM32F7 and the transceiver a NUCLEO-144 wires
 * to it.
 *
 * This is the one peripheral the tree does not drive through the Low Layer,
 * for the plain reason that ST never wrote a Low Layer for it. What it wrote
 * is the higher half, and that half is brought in for this peripheral alone.
 * The three things that half asks of the system it is running on are answered
 * at the bottom of this file, and they are all it asks.
 *
 * The controller reaches memory as a bus master, which rules out the first 64
 * KiB: that is tightly coupled to the core and is not on the bus matrix at
 * all. Everything the controller is handed here is therefore a static object,
 * which the linker places past that, and never memory from the page allocator,
 * which lives entirely within it.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/printk.h>
#include <kernel/time.h>

#include "eth.h"
#include "stm32f7xx_hal_conf.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_gpio.h"

/* The transceiver a NUCLEO-144 carries, on the pins it wires to it. All of
 * them speak the reduced interface, which is the eleventh function of each
 *
 *   REF_CLK PA1   MDIO PA2   CRS_DV PA7   TXD1 PB13
 *   MDC     PC1   RXD0 PC4   RXD1   PC5
 *   RXER    PG2   TX_EN PG11  TXD0  PG13
 */
#define RMII_AF LL_GPIO_AF_11
#define RMII_PORTA_PINS (LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_7)
#define RMII_PORTB_PINS (LL_GPIO_PIN_13)
#define RMII_PORTC_PINS (LL_GPIO_PIN_1 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5)
#define RMII_PORTG_PINS (LL_GPIO_PIN_2 | LL_GPIO_PIN_11 | LL_GPIO_PIN_13)

#define ETH_ISR_PRIORITY 7
#define ETH_TX_TIMEOUT 100

/* How long the transceiver is given to agree terms with the far end before
 * the controller is set going anyway
 */
#define ETH_LINK_TIMEOUT 5000

/* What the controller is given to work with. Static, so that the linker keeps
 * them where a bus master can reach them
 */
#define ETH_DMA_MEM __attribute__((section(".eth_dma"), aligned(32)))

static ETH_DMADescTypeDef rx_desc[ETH_RX_DESC_CNT] ETH_DMA_MEM;
static ETH_DMADescTypeDef tx_desc[ETH_TX_DESC_CNT] ETH_DMA_MEM;

/* What the controller is pointed at when it needs somewhere to put the next
 * frame. There is one for each descriptor and they are handed out in the same
 * order the controller fills them, so the two stay in step
 */
static uint8_t rx_buf[ETH_RX_DESC_CNT][ETH_MAX_PACKET_SIZE] ETH_DMA_MEM;
static uint32_t rx_buf_next;

/* Where the frame the controller has just finished is described. Only one is
 * ever outstanding: it is filled while a frame is being read out and copied
 * before the next read begins
 */
static struct {
    uint8_t *data;
    uint32_t len;
} rx_frame;

/* Counted so that what the interface is doing can be seen from outside,
 * whether or not anything is listening on the far end of the cable
 */
volatile uint32_t eth_stat_rx;
volatile uint32_t eth_stat_tx;
volatile uint32_t eth_stat_tx_err;
volatile uint32_t eth_stat_rx_drop;

/* The head of the last frame each way, kept so that what is actually on the
 * wire can be read out and checked by hand
 */
#define ETH_PEEK_LEN 64

/* Only frames carrying the message being chased are kept, because the address
 * this board answers to is one the whole world can reach and most of what
 * arrives at it was not sent by anyone who was asked
 */
#define ETH_PEEK_PROTO_ICMP 1
volatile uint8_t eth_peek_tx[ETH_PEEK_LEN];
volatile uint8_t eth_peek_rx[ETH_PEEK_LEN];
volatile uint32_t eth_peek_tx_len;
volatile uint32_t eth_peek_rx_len;

static ETH_HandleTypeDef heth;
static uint8_t eth_mac[ETH_MAC_ADDR_LEN];
static bool eth_ready;

/*==================================*
 * What the vendor's half asks for  *
 *==================================*/

uint32_t HAL_GetTick(void)
{
    return (uint32_t) ktime_get();
}

/* The kernel's own sleep does nothing yet, and what asks for this asks during
 * bring-up, where spinning costs nothing anyone is waiting on
 */
void HAL_Delay(uint32_t ms)
{
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < ms)
        ;
}

uint32_t HAL_RCC_GetHCLKFreq(void)
{
    return SystemCoreClock;
}

/*=========================*
 * Bringing the pins up    *
 *=========================*/

void HAL_ETH_MspInit(ETH_HandleTypeDef *eth)
{
    if (eth->Instance != ETH)
        return;

    LL_AHB1_GRP1_EnableClock(
        LL_AHB1_GRP1_PERIPH_GPIOA | LL_AHB1_GRP1_PERIPH_GPIOB |
        LL_AHB1_GRP1_PERIPH_GPIOC | LL_AHB1_GRP1_PERIPH_GPIOG);

    LL_GPIO_InitTypeDef gpio = {
        .Mode = LL_GPIO_MODE_ALTERNATE,
        .Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH,
        .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
        .Pull = LL_GPIO_PULL_NO,
        .Alternate = RMII_AF,
    };

    gpio.Pin = RMII_PORTA_PINS;
    LL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = RMII_PORTB_PINS;
    LL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = RMII_PORTC_PINS;
    LL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = RMII_PORTG_PINS;
    LL_GPIO_Init(GPIOG, &gpio);

    /* The controller itself, and the two directions it moves bytes in */
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_ETHMAC |
                             LL_AHB1_GRP1_PERIPH_ETHMACTX |
                             LL_AHB1_GRP1_PERIPH_ETHMACRX);

    NVIC_SetPriority(ETH_IRQn, ETH_ISR_PRIORITY);
}

/*=========================*
 * The transceiver         *
 *=========================*/

static int phy_read(uint16_t reg, uint32_t *value)
{
    return HAL_ETH_ReadPHYRegister(&heth, LAN8742A_PHY_ADDRESS, reg, value) ==
                   HAL_OK
               ? 0
               : -1;
}

static int phy_write(uint16_t reg, uint32_t value)
{
    return HAL_ETH_WritePHYRegister(&heth, LAN8742A_PHY_ADDRESS, reg, value) ==
                   HAL_OK
               ? 0
               : -1;
}

/* The two registers every transceiver answers with what it is: the first
 * carries the maker, the second the model
 */
uint32_t eth_phy_id(void)
{
    uint32_t hi = 0, lo = 0;

    if (phy_read(2, &hi) || phy_read(3, &lo))
        return 0;

    return (hi << 16) | lo;
}

/* Whether the two ends have finished agreeing terms */
static bool phy_autoneg_done(void)
{
    uint32_t status = 0;

    if (phy_read(PHY_BSR, &status))
        return false;

    return (status & PHY_AUTONEGO_COMPLETE) != 0;
}

bool eth_link_is_up(void)
{
    uint32_t status = 0;

    /* The bit latches low, so it is read twice and the second read is the one
     * that says what is true now
     */
    if (phy_read(PHY_BSR, &status) || phy_read(PHY_BSR, &status))
        return false;

    return (status & PHY_LINKED_STATUS) != 0;
}

int eth_link_speed(void)
{
    uint32_t status = 0;

    if (phy_read(PHY_SR, &status))
        return 0;

    return (status & PHY_SPEED_STATUS) ? 10 : 100;
}

bool eth_link_is_full_duplex(void)
{
    uint32_t status = 0;

    if (phy_read(PHY_SR, &status))
        return false;

    return (status & PHY_DUPLEX_STATUS) != 0;
}

/*=========================*
 * Bringing it all up      *
 *=========================*/

/*=========================*
 * Frames                  *
 *=========================*/

/* The three below are the library's own, which it declares weakly and calls
 * by name. They are not registered but replaced: registering is what the
 * library does instead when it is built to carry a table of them, and it is
 * not built that way here.
 */

/* Called when a descriptor needs somewhere to point */
void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
    *buff = rx_buf[rx_buf_next];
    rx_buf_next = (rx_buf_next + 1) % ETH_RX_DESC_CNT;
}

/* Called by the library once the frame is complete. A frame never spans more
 * than one buffer here, because every buffer is as large as the largest frame
 */
void HAL_ETH_RxLinkCallback(void **pStart,
                            void **pEnd,
                            uint8_t *buff,
                            uint16_t len)
{
    rx_frame.data = buff;
    rx_frame.len = len;

    *pStart = &rx_frame;
    *pEnd = &rx_frame;
}

/* Called by the library when the controller is done with what it was sent.
 * Nothing was lent to it, so there is nothing to take back
 */
void HAL_ETH_TxFreeCallback(uint32_t *buff)
{
    (void) buff;
}

/* Keep the head of a frame, if it is one of the frames being looked for */
static void eth_peek(volatile uint8_t *dst,
                     volatile uint32_t *dst_len,
                     const void *frame,
                     size_t len)
{
    const uint8_t *f = frame;

    if (len < 24)
        return;

    /* Carried over the internet protocol, and by the message that asks
     * whether the far end is there
     */
    if (f[12] != 0x08 || f[13] != 0x00 || f[23] != ETH_PEEK_PROTO_ICMP)
        return;

    /* Of that message, only the two halves of the exchange being chased: the
     * asking and the answering. What else arrives at a public address is a
     * great deal, and none of it is this
     */
    if (len < 35 || (f[34] != 0 && f[34] != 8))
        return;

    *dst_len = len;
    memcpy((void *) dst, f, len < ETH_PEEK_LEN ? len : ETH_PEEK_LEN);
}

int eth_transmit(const void *buf, size_t len)
{
    if (!eth_ready || len > ETH_FRAME_MAX)
        return -1;

    ETH_BufferTypeDef tx = {
        .buffer = (uint8_t *) buf,
        .len = len,
        .next = NULL,
    };

    /* The controller is told to leave the checksums alone.
     *
     * It is able to work them out itself, but only for a frame whose checksum
     * fields it finds already cleared, and the stack above does not clear them
     * -- it fills them in. Left to do both, the controller works its own sum
     * over a frame that already carries one and writes something that is
     * neither, which the far end then discards without a word.
     *
     * Saying so has to be done by asking for the feature and then asking for
     * none of it: the library writes that field only when it is told the frame
     * has something to say about checksums.
     */
    ETH_TxPacketConfigTypeDef cfg = {
        .Length = len,
        .TxBuffer = &tx,
        .Attributes =
            ETH_TX_PACKETS_FEATURES_CRCPAD | ETH_TX_PACKETS_FEATURES_CSUM,
        .CRCPadCtrl = ETH_CRC_PAD_INSERT,
        .ChecksumCtrl = ETH_CHECKSUM_DISABLE,
    };

    if (HAL_ETH_Transmit(&heth, &cfg, ETH_TX_TIMEOUT) != HAL_OK) {
        eth_stat_tx_err++;
        return -1;
    }

    eth_stat_tx++;

    eth_peek(eth_peek_tx, &eth_peek_tx_len, buf, len);

    return 0;
}

int eth_receive(void *buf, size_t max)
{
    if (!eth_ready)
        return 0;

    void *frame = NULL;

    if (HAL_ETH_ReadData(&heth, &frame) != HAL_OK || frame == NULL)
        return 0;

    uint32_t len = rx_frame.len;
    if (len > max) {
        eth_stat_rx_drop++;
        return 0;
    }

    memcpy(buf, rx_frame.data, len);
    eth_stat_rx++;

    eth_peek(eth_peek_rx, &eth_peek_rx_len, rx_frame.data, len);

    return (int) len;
}

void eth_get_mac(uint8_t *out)
{
    memcpy(out, eth_mac, ETH_MAC_ADDR_LEN);
}

/* The controller's interrupt is deliberately left unclaimed.
 *
 * Everything this kernel offers a thread for waiting on -- a semaphore, a
 * queue -- it offers as a system call, and a system call is a supervisor
 * instruction, which from inside an interrupt handler is a fault rather than a
 * call. So there is nothing an interrupt here could say that anyone could be
 * listening for, and what listens looks for itself instead.
 *
 * What that costs is the interval below and what it saves is a path into the
 * kernel that does not exist yet. When one does -- an interface of its own in
 * the file system, waited on the way a terminal is -- the interrupt can be
 * claimed and the interval dropped.
 */

int eth_start(void)
{
    if (!eth_ready)
        return -1;

    /* The transceiver agrees terms with the far end on its own and takes its
     * own time over it. What it settles on has to be read after it has
     * settled: read before, it says what was true of a link that did not yet
     * exist, and the controller is then set to terms the far end never agreed
     */
    uint32_t start = HAL_GetTick();
    while (!eth_link_is_up()) {
        if ((HAL_GetTick() - start) > ETH_LINK_TIMEOUT) {
            printk("eth: no link");
            break;
        }
        HAL_Delay(10);
    }

    /* Agreeing that there is a link and agreeing on what it is are two
     * different moments, and it is the second that matters here
     */
    while (!phy_autoneg_done()) {
        if ((HAL_GetTick() - start) > ETH_LINK_TIMEOUT)
            break;
        HAL_Delay(10);
    }

    /* What the transceiver settled on has to be told to the controller: the
     * two negotiate nothing between themselves
     */
    ETH_MACConfigTypeDef mac = {0};
    HAL_ETH_GetMACConfig(&heth, &mac);
    mac.Speed = eth_link_speed() == 100 ? ETH_SPEED_100M : ETH_SPEED_10M;
    mac.DuplexMode =
        eth_link_is_full_duplex() ? ETH_FULLDUPLEX_MODE : ETH_HALFDUPLEX_MODE;
    HAL_ETH_SetMACConfig(&heth, &mac);

    printk("eth: link %d Mbps %s duplex", eth_link_speed(),
           eth_link_is_full_duplex() ? "full" : "half");

    if (HAL_ETH_Start(&heth) != HAL_OK)
        return -1;

    return 0;
}

int eth_init(const uint8_t mac[ETH_MAC_ADDR_LEN])
{
    memcpy(eth_mac, mac, ETH_MAC_ADDR_LEN);

    heth.Instance = ETH;
    heth.Init.MACAddr = eth_mac;
    heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
    heth.Init.TxDesc = tx_desc;
    heth.Init.RxDesc = rx_desc;
    heth.Init.RxBuffLen = ETH_MAX_PACKET_SIZE;

    if (HAL_ETH_Init(&heth) != HAL_OK) {
        printk("eth: the controller would not start");
        return -1;
    }

    /* The rate the management wires are clocked at follows from the rate the
     * bus runs at, and this part runs faster than the range the controller
     * leaves selected. Nothing in the library does this for us: it is an ask
     * of its own, and until it is asked the transceiver is clocked past what
     * it is allowed and answers unreliably
     */
    HAL_ETH_SetMDIOClockRange(&heth);

    uint32_t id = eth_phy_id();
    if (id == 0 || id == 0xFFFFFFFF) {
        printk("eth: nothing answers on the management wires");
        return -1;
    }

    /* Ask the transceiver to agree terms with whatever is on the far end, and
     * let it take its own time over it: what it settles on is read when the
     * link is next looked at, not here
     */
    if (phy_write(PHY_BCR, PHY_RESET))
        return -1;
    HAL_Delay(50);

    if (phy_write(PHY_BCR, PHY_AUTONEGOTIATION))
        return -1;

    eth_ready = true;

    printk("eth: phy id %x at address %d", (unsigned int) id,
           LAN8742A_PHY_ADDRESS);

    return 0;
}
