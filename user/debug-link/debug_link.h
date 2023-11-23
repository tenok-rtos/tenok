#ifndef __DEBUG_LINK__
#define __DEBUG_LINK__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    int size;
} debug_link_payload_t;

void pack_debug_link_msg_header(debug_link_payload_t *payload, int message_id);
void pack_debug_link_msg_bool(bool *data,
                              debug_link_payload_t *payload,
                              size_t n);
void pack_debug_link_msg_uint8_t(uint8_t *data,
                                 debug_link_payload_t *payload,
                                 size_t n);
void pack_debug_link_msg_int8_t(int8_t *data,
                                debug_link_payload_t *payload,
                                size_t n);
void pack_debug_link_msg_uint16_t(uint16_t *data,
                                  debug_link_payload_t *payload,
                                  size_t n);
void pack_debug_link_msg_int16_t(int16_t *data,
                                 debug_link_payload_t *payload,
                                 size_t n);
void pack_debug_link_msg_uint32_t(uint32_t *data,
                                  debug_link_payload_t *payload,
                                  size_t n);
void pack_debug_link_msg_int32_t(int32_t *data,
                                 debug_link_payload_t *payload,
                                 size_t n);
void pack_debug_link_msg_uint64_t(uint64_t *data,
                                  debug_link_payload_t *payload,
                                  size_t n);
void pack_debug_link_msg_int64_t(int64_t *data,
                                 debug_link_payload_t *payload,
                                 size_t n);
void pack_debug_link_msg_float(float *data,
                               debug_link_payload_t *payload,
                               size_t n);
void pack_debug_link_msg_double(double *data,
                                debug_link_payload_t *payload,
                                size_t n);
void generate_debug_link_msg_checksum(debug_link_payload_t *payload);

#endif
