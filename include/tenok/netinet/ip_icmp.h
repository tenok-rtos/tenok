/**
 * @file
 * The message that asks whether the far end is there, and the answer to it.
 */
#ifndef _TENOK_NETINET_IP_ICMP_H
#define _TENOK_NETINET_IP_ICMP_H

#include <stddef.h>
#include <stdint.h>

#include <netinet/in.h>

#define ICMP_ECHOREPLY 0
#define ICMP_DEST_UNREACH 3
#define ICMP_SOURCE_QUENCH 4
#define ICMP_REDIRECT 5
#define ICMP_ECHO 8
#define ICMP_TIME_EXCEEDED 11
#define ICMP_PARAMETERPROB 12
#define ICMP_TIMESTAMP 13
#define ICMP_TIMESTAMPREPLY 14
#define ICMP_INFO_REQUEST 15
#define ICMP_INFO_REPLY 16
#define ICMP_ADDRESS 17
#define ICMP_ADDRESSREPLY 18

#define ICMP_MINLEN 8

struct icmphdr {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    union {
        struct {
            uint16_t id;
            uint16_t sequence;
        } echo;
        uint32_t gateway;
        struct {
            uint16_t __unused;
            uint16_t mtu;
        } frag;
    } un;
};

struct iphdr {
    uint8_t ihl : 4;
    uint8_t version : 4;
    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
};


/* The same message written the way the older Berkeley systems wrote it, which
 * is the shape programs carried over from them still expect
 */
struct icmp {
    uint8_t icmp_type;
    uint8_t icmp_code;
    uint16_t icmp_cksum;
    union {
        struct {
            uint16_t icd_id;
            uint16_t icd_seq;
        } ih_idseq;
        uint32_t ih_gwaddr;
        struct {
            uint16_t ipm_void;
            uint16_t ipm_nextmtu;
        } ih_pmtu;
    } icmp_hun;
    union {
        uint8_t id_data[1];
        struct {
            uint32_t its_otime;
            uint32_t its_rtime;
            uint32_t its_ttime;
        } id_ts;
    } icmp_dun;
};

#define icmp_id icmp_hun.ih_idseq.icd_id
#define icmp_seq icmp_hun.ih_idseq.icd_seq
#define icmp_gwaddr icmp_hun.ih_gwaddr
#define icmp_nextmtu icmp_hun.ih_pmtu.ipm_nextmtu
#define icmp_data icmp_dun.id_data
#define icmp_otime icmp_dun.id_ts.its_otime
#define icmp_rtime icmp_dun.id_ts.its_rtime
#define icmp_ttime icmp_dun.id_ts.its_ttime

#endif
