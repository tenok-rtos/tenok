#ifndef __ETH_H__
#define __ETH_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ETH_MAC_ADDR_LEN 6

/* The largest thing that goes on the wire, tag and checksum included */
#define ETH_FRAME_MAX 1524

/* Bring the controller and the transceiver up. The address is the one the
 * interface answers to and is kept by the caller
 */
int eth_init(const uint8_t mac[ETH_MAC_ADDR_LEN]);

/* Whether the transceiver says there is a cable with something on the end of
 * it, and at what speed the two of them settled
 */
bool eth_link_is_up(void);
int eth_link_speed(void);
bool eth_link_is_full_duplex(void);

/* Set the controller going, once the transceiver has settled terms. What
 * arrives from then on is announced by eth_rx_notify(), which the interrupt
 * calls and the caller provides
 */
int eth_start(void);

/* Put one frame on the wire, and take one off it. The second answers the
 * length it copied out, or zero when there was nothing waiting
 */
int eth_transmit(const void *buf, size_t len);
int eth_receive(void *buf, size_t max);

void eth_get_mac(uint8_t *out);

/* What the transceiver says it is, read over the management wires. Answered
 * so that a failure to talk to it at all can be told apart from a cable that
 * is merely unplugged
 */
uint32_t eth_phy_id(void);

#endif
