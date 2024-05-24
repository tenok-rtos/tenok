#include <stdint.h>
#include <string.h>

#include "debug_link.h"

#define DBGLINK_HEADER_SIZE 3
#define DBGLINK_CHECKSUM_INIT_VAL 63

void pack_debug_link_msg_header(debug_link_payload_t *payload, int message_id)
{
    payload->size = DBGLINK_HEADER_SIZE;
    payload->data[2] = message_id;
}

void pack_debug_link_msg_bool(bool *data,
                              debug_link_payload_t *payload,
                              size_t n)
{
    while (n--) {
        memcpy((uint8_t *) payload->data + payload->size, (uint8_t *) data,
               sizeof(bool));
        payload->size += sizeof(bool);
    }
}


void pack_debug_link_msg_uint8_t(uint8_t *data,
                                 debug_link_payload_t *payload,
                                 size_t n)
{
    for (int i = 0; i < n; i++) {
        memcpy((uint8_t *) payload->data + payload->size, (uint8_t *) &data[i],
               sizeof(uint8_t));
        payload->size += sizeof(uint8_t);
    }
}

void pack_debug_link_msg_int8_t(int8_t *data,
                                debug_link_payload_t *payload,
                                size_t n)
{
    for (int i = 0; i < n; i++) {
        memcpy((uint8_t *) payload->data + payload->size, (uint8_t *) &data[i],
               sizeof(int8_t));
        payload->size += sizeof(int8_t);
    }
}

void pack_debug_link_msg_uint16_t(uint16_t *data,
                                  debug_link_payload_t *payload,
                                  size_t n)
{
    for (int i = 0; i < n; i++) {
        memcpy((uint8_t *) payload->data + payload->size, (uint8_t *) &data[i],
               sizeof(uint16_t));
        payload->size += sizeof(uint16_t);
    }
}

void pack_debug_link_msg_int16_t(int16_t *data,
                                 debug_link_payload_t *payload,
                                 size_t n)
{
    for (int i = 0; i < n; i++) {
        memcpy((uint8_t *) payload->data + payload->size, (uint8_t *) &data[i],
               sizeof(int16_t));
        payload->size += sizeof(int16_t);
    }
}

void pack_debug_link_msg_uint32_t(uint32_t *data,
                                  debug_link_payload_t *payload,
                                  size_t n)
{
    for (int i = 0; i < n; i++) {
        memcpy((uint8_t *) payload->data + payload->size, (uint8_t *) &data[i],
               sizeof(uint32_t));
        payload->size += sizeof(uint32_t);
    }
}

void pack_debug_link_msg_int32_t(int32_t *data,
                                 debug_link_payload_t *payload,
                                 size_t n)
{
    for (int i = 0; i < n; i++) {
        memcpy((uint8_t *) payload->data + payload->size, (uint8_t *) &data[i],
               sizeof(int32_t));
        payload->size += sizeof(int32_t);
    }
}

void pack_debug_link_msg_uint64_t(uint64_t *data,
                                  debug_link_payload_t *payload,
                                  size_t n)
{
    for (int i = 0; i < n; i++) {
        memcpy((uint8_t *) payload->data + payload->size, (uint8_t *) &data[i],
               sizeof(uint64_t));
        payload->size += sizeof(uint64_t);
    }
}

void pack_debug_link_msg_int64_t(int64_t *data,
                                 debug_link_payload_t *payload,
                                 size_t n)
{
    for (int i = 0; i < n; i++) {
        memcpy((uint8_t *) payload->data + payload->size, (uint8_t *) &data[i],
               sizeof(int64_t));
        payload->size += sizeof(int64_t);
    }
}

void pack_debug_link_msg_float(float *data,
                               debug_link_payload_t *payload,
                               size_t n)
{
    for (int i = 0; i < n; i++) {
        memcpy((uint8_t *) payload->data + payload->size, (uint8_t *) &data[i],
               sizeof(float));
        payload->size += sizeof(float);
    }
}

void pack_debug_link_msg_double(double *data,
                                debug_link_payload_t *payload,
                                size_t n)
{
    for (int i = 0; i < n; i++) {
        memcpy((uint8_t *) payload->data + payload->size, (uint8_t *) data,
               sizeof(double));
        payload->size += sizeof(double);
    }
}

void generate_debug_link_msg_checksum(debug_link_payload_t *payload)
{
    uint8_t *data = payload->data + DBGLINK_HEADER_SIZE;
    size_t size = payload->size - DBGLINK_HEADER_SIZE;
    uint8_t checksum = DBGLINK_CHECKSUM_INIT_VAL;

    for (int i = 0; i < size; i++)
        checksum ^= data[i];

    payload->data[0] = '@';
    payload->data[1] = payload->size - 3;
    payload->data[payload->size] = checksum;
    payload->size++;
}
