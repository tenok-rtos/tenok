#ifndef __TENOK_LINK__
#define __TENOK_LINK__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    int size;
} tenok_payload_t;

void pack_tenok_msg_header(tenok_payload_t *payload, int message_id);
void pack_tenok_msg_field_bool(bool *data, tenok_payload_t *payload, size_t n);
void pack_tenok_msg_field_uint8_t(uint8_t *data,
                                  tenok_payload_t *payload,
                                  size_t n);
void pack_tenok_msg_field_int8_t(int8_t *data,
                                 tenok_payload_t *payload,
                                 size_t n);
void pack_tenok_msg_field_uint16_t(uint16_t *data,
                                   tenok_payload_t *payload,
                                   size_t n);
void pack_tenok_msg_field_int16_t(int16_t *data,
                                  tenok_payload_t *payload,
                                  size_t n);
void pack_tenok_msg_field_uint32_t(uint32_t *data,
                                   tenok_payload_t *payload,
                                   size_t n);
void pack_tenok_msg_field_int32_t(int32_t *data,
                                  tenok_payload_t *payload,
                                  size_t n);
void pack_tenok_msg_field_uint64_t(uint64_t *data,
                                   tenok_payload_t *payload,
                                   size_t n);
void pack_tenok_msg_field_int64_t(int64_t *data,
                                  tenok_payload_t *payload,
                                  size_t n);
void pack_tenok_msg_field_float(float *data,
                                tenok_payload_t *payload,
                                size_t n);
void pack_tenok_msg_field_double(double *data,
                                 tenok_payload_t *payload,
                                 size_t n);
void generate_tenok_msg_checksum(tenok_payload_t *payload);

#endif
