#ifndef __ETHERNETIF_H__
#define __ETHERNETIF_H__

#include "lwip/ip_addr.h"
#include "lwip/netif.h"

/* Give the stack an interface backed by the controller, at the address it is
 * to answer to, and start the thread that feeds it
 */
int ethernetif_start(struct netif *netif,
                     const ip4_addr_t *addr,
                     const ip4_addr_t *mask,
                     const ip4_addr_t *gw);

#endif
