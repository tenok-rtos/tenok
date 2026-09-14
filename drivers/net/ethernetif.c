/**
 * @file
 * The interface the stack sees, and the controller behind it.
 *
 * Frames arrive at an interrupt, which does nothing but say so: a thread of
 * this file does the waiting and hands what arrives to the stack, so that
 * nothing the stack owns is ever touched from an interrupt.
 *
 * On the way out a frame is copied into a buffer of this file's own before the
 * controller is pointed at it. That costs a copy per frame and buys not having
 * to keep the caller's buffer alive until the controller is done with it,
 * which at this stage is the better trade.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <pthread.h>
#include <task.h>
#include <tenok.h>
#include <unistd.h>

#include "eth.h"
#include "lwip/err.h"
#include "lwip/etharp.h"
#include "lwip/ethip6.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"

#include "ethernetif.h"

#define IFNAME0 'e'
#define IFNAME1 'n'

#define ETHIF_THREAD_STACKSIZE 2048
#define ETHIF_THREAD_PRIO 5

/* How often the controller is asked whether anything arrived */
#define ETHIF_POLL_INTERVAL_US 1000

static struct netif *eth_netif;

/* What goes out. One frame at a time, which the lock makes true */
static uint8_t tx_buf[ETH_FRAME_MAX] __attribute__((aligned(4)));

/* Where a frame is put down between coming off the controller and being
 * copied into what the stack keeps it in. Only the one thread touches it
 */
static uint8_t rx_frame[ETH_FRAME_MAX];
static pthread_mutex_t tx_mtx;

/*=================*
 * Out of the box  *
 *=================*/

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    if (p->tot_len > sizeof(tx_buf))
        return ERR_MEM;

    pthread_mutex_lock(&tx_mtx);

    pbuf_copy_partial(p, tx_buf, p->tot_len, 0);

    int err = eth_transmit(tx_buf, p->tot_len);

    pthread_mutex_unlock(&tx_mtx);

    return err ? ERR_IF : ERR_OK;
}

/*=================*
 * Into the box    *
 *=================*/

/* Put into the address table what the frame itself says, when the table says
 * something else or says nothing.
 *
 * The stack would otherwise learn an address only by asking for it, and on
 * this segment the gateway answers for every address on it -- so asking who
 * holds an address on the same segment gets the gateway back, and the reply
 * is then sent to the gateway, which does not carry it the last step. What
 * arrived already says who sent it, and that is believed here.
 *
 * The entry is made a fixed one because that is the only kind the stack lets
 * anything outside it make. What that costs is an entry that does not age;
 * what it buys is a reply that arrives.
 */
static void arp_learn(struct netif *netif, const uint8_t *frame, int len)
{
    if (len < 34 || frame[12] != 0x08 || frame[13] != 0x00)
        return;

    ip4_addr_t src;
    struct eth_addr mac;

    memcpy(&src.addr, frame + 26, sizeof(src.addr));
    memcpy(mac.addr, frame + 6, ETH_HWADDR_LEN);

    /* Only what is near enough to be spoken to without the gateway */
    if (!ip4_addr_netcmp(&src, netif_ip4_addr(netif), netif_ip4_netmask(netif)))
        return;

    struct eth_addr *known = NULL;
    const ip4_addr_t *known_ip = NULL;

    if (etharp_find_addr(netif, &src, &known, &known_ip) >= 0 && known &&
        memcmp(known->addr, mac.addr, ETH_HWADDR_LEN) == 0)
        return;

    etharp_add_static_entry(&src, &mac);
}

/* The thread that carries what arrives up to the stack. It is woken by the
 * interrupt and then takes everything the controller has, because one
 * interrupt may stand for more than one frame
 */
static void *ethernetif_input(void *arg)
{
    struct netif *netif = arg;

    setprogname("ethif");

    while (1) {
        int len;
        while ((len = eth_receive(rx_frame, sizeof(rx_frame))) > 0) {
            struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
            if (!p)
                continue;

            pbuf_take(p, rx_frame, len);

            arp_learn(netif, rx_frame, len);

            if (netif->input(p, netif) != ERR_OK)
                pbuf_free(p);
        }

        usleep(ETHIF_POLL_INTERVAL_US);
    }

    return NULL;
}

/*=================*
 * Bringing it up  *
 *=================*/

static err_t ethernetif_init_cb(struct netif *netif)
{
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;
    netif->mtu = 1500;
    netif->hwaddr_len = ETH_MAC_ADDR_LEN;
    netif->flags =
        NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    eth_get_mac(netif->hwaddr);

    return ERR_OK;
}

int ethernetif_start(struct netif *netif,
                     const ip4_addr_t *addr,
                     const ip4_addr_t *mask,
                     const ip4_addr_t *gw)
{
    eth_netif = netif;

    if (pthread_mutex_init(&tx_mtx, NULL) != 0)
        return -1;

    if (netif_add(netif, addr, mask, gw, NULL, ethernetif_init_cb,
                  tcpip_input) == NULL)
        return -1;

    netif_set_default(netif);
    netif_set_up(netif);
    netif_set_link_up(netif);

    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, ETHIF_THREAD_STACKSIZE);

    struct sched_param param = {.sched_priority = ETHIF_THREAD_PRIO};
    pthread_attr_setschedparam(&attr, &param);

    if (pthread_create(&tid, &attr, ethernetif_input, netif) != 0)
        return -1;

    return 0;
}
