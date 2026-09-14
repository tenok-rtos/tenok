/**
 * @file
 * How the stack is set up for this system.
 *
 * It runs with an operating system under it rather than on its own, because
 * what asks it for anything is a program expecting Berkeley sockets and those
 * only exist in that mode. Everything it is given comes from static storage:
 * the page allocator this kernel has lives in the one part of memory the
 * Ethernet controller cannot reach, so nothing the controller will be handed
 * may come from there.
 */
#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#include <stdlib.h>

#include "kconfig.h"

#define NO_SYS 0
#define SYS_LIGHTWEIGHT_PROT 0
#define LWIP_TCPIP_CORE_LOCKING 1

/* The system has its own, and two of them in one program is one too many */
#define LWIP_PROVIDE_ERRNO 0
#define LWIP_ERRNO_STDINCLUDE 1

/* The names the stack exports are its own. What the rest of the system calls
 * a socket is a thing of the kernel's, and the kernel is what puts the two
 * together
 */
#define LWIP_COMPAT_SOCKETS 0
#define LWIP_POSIX_SOCKETS_IO_NAMES 0
/* The stack numbers its sockets from zero. They never meet a descriptor of
 * this system: each is wrapped in a file, and it is the file that is given a
 * descriptor
 */
#define LWIP_SOCKET_OFFSET 0

#define LWIP_ARP 1

/* Needed by the interface, which puts into the table what it learns from the
 * packets themselves. See the note beside it for why it has to
 */
#define ETHARP_SUPPORT_STATIC_ENTRIES 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_DNS 1
#define LWIP_DHCP 0
#define LWIP_IPV6 0
#define LWIP_IGMP 0
#define LWIP_STATS 0

#define LWIP_NETCONN 1
#define LWIP_SOCKET 1
#define LWIP_SOCKET_SELECT 1
#define LWIP_SOCKET_POLL 1
#define LWIP_NETIF_HOSTNAME 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_STATUS_CALLBACK 1

/* What memory the stack has, all of it laid out by the linker */
#define MEM_LIBC_MALLOC 0
#define MEMP_MEM_MALLOC 0
#define MEM_ALIGNMENT 4
#define MEM_SIZE (10 * 1024)

#define MEMP_NUM_PBUF 12
#define MEMP_NUM_UDP_PCB 6
#define MEMP_NUM_TCP_PCB 6
#define MEMP_NUM_TCP_PCB_LISTEN 4
#define MEMP_NUM_TCP_SEG 12
#define MEMP_NUM_RAW_PCB 6
#define MEMP_NUM_NETBUF 6
#define MEMP_NUM_NETCONN 8
#define MEMP_NUM_TCPIP_MSG_API 8
#define MEMP_NUM_TCPIP_MSG_INPKT 8
#define MEMP_NUM_SYS_TIMEOUT 8

#define PBUF_POOL_SIZE 8
#define PBUF_POOL_BUFSIZE 1536

#define TCP_MSS 1460
#define TCP_SND_BUF (4 * TCP_MSS)
#define TCP_WND (4 * TCP_MSS)
#define TCP_SND_QUEUELEN (2 * TCP_SND_BUF / TCP_MSS)

/* The checksums are worked out here rather than by the controller, which is
 * one thing less to have got wrong while the rest is being brought up
 */
#define CHECKSUM_GEN_IP 1
#define CHECKSUM_GEN_UDP 1
#define CHECKSUM_GEN_TCP 1
#define CHECKSUM_GEN_ICMP 1
#define CHECKSUM_CHECK_IP 1
#define CHECKSUM_CHECK_UDP 1
#define CHECKSUM_CHECK_TCP 1
#define CHECKSUM_CHECK_ICMP 1

/* The threads the stack starts for itself. A thread of this kernel is given
 * its stack in bytes
 */
#define TCPIP_THREAD_NAME "tcpip"
#define TCPIP_THREAD_STACKSIZE 4096
#define TCPIP_THREAD_PRIO 4
#define TCPIP_MBOX_SIZE 8
#define DEFAULT_THREAD_STACKSIZE 2048
#define DEFAULT_THREAD_PRIO 3
#define DEFAULT_RAW_RECVMBOX_SIZE 8
#define DEFAULT_UDP_RECVMBOX_SIZE 8
#define DEFAULT_TCP_RECVMBOX_SIZE 8
#define DEFAULT_ACCEPTMBOX_SIZE 8

/* The stack wants a number nothing can guess for the ports it picks and the
 * sequences it starts from. What the library offers is enough for that
 */
#define LWIP_RAND() ((u32_t) rand())

#define LWIP_DEBUG 0

#endif
