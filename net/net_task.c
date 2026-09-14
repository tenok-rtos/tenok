/**
 * @file
 * Bringing the network up.
 *
 * The address is fixed rather than asked for: what hands addresses out is not
 * something this board can count on being there, and the one it answers to is
 * a thing the person who plugged it in decided.
 */

#include <stdio.h>

#include <task.h>
#include <tenok.h>
#include <unistd.h>

#include "eth.h"
#include "ethernetif.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"

#include "kconfig.h"

static struct netif eth_netif;

/* Told to the stack once, when it has finished setting itself up */
static void tcpip_ready(void *arg)
{
    sem_t *done = arg;
    sem_post(done);
}

void net_task(void)
{
    setprogname("net");

    if (!eth_link_is_up())
        printf("net: no cable\n");

    if (eth_start()) {
        printf("net: the controller would not start\n");
        return;
    }

    /* The stack starts a thread of its own and is not to be spoken to until
     * that thread says it is ready
     */
    sem_t done;
    sem_init(&done, 0, 0);
    tcpip_init(tcpip_ready, &done);
    sem_wait(&done);

    ip4_addr_t addr, mask, gw;
    IP4_ADDR(&addr, CONFIG_NET_IPV4_ADDR0, CONFIG_NET_IPV4_ADDR1,
             CONFIG_NET_IPV4_ADDR2, CONFIG_NET_IPV4_ADDR3);
    IP4_ADDR(&mask, CONFIG_NET_IPV4_MASK0, CONFIG_NET_IPV4_MASK1,
             CONFIG_NET_IPV4_MASK2, CONFIG_NET_IPV4_MASK3);
    IP4_ADDR(&gw, CONFIG_NET_IPV4_GW0, CONFIG_NET_IPV4_GW1, CONFIG_NET_IPV4_GW2,
             CONFIG_NET_IPV4_GW3);

    if (ethernetif_start(&eth_netif, &addr, &mask, &gw)) {
        printf("net: the interface would not come up\n");
        return;
    }

    /* Where a name is asked about. Nothing on this segment hands the address
     * of one out, so it is named here beside the rest
     */
    ip_addr_t dns;
    IP4_ADDR(&dns, CONFIG_NET_IPV4_DNS0, CONFIG_NET_IPV4_DNS1,
             CONFIG_NET_IPV4_DNS2, CONFIG_NET_IPV4_DNS3);
    dns_setserver(0, &dns);

    printf("net: %d.%d.%d.%d up\n", CONFIG_NET_IPV4_ADDR0,
           CONFIG_NET_IPV4_ADDR1, CONFIG_NET_IPV4_ADDR2, CONFIG_NET_IPV4_ADDR3);

    while (1)
        sleep(10);
}

HOOK_USER_TASK(net_task, 3, 4096);
