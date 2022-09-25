#ifndef __PROTOCOLS_LCP_H_INCLUDED__
#define __PROTOCOLS_LCP_H_INCLUDED__

#include <stddef.h>
#include <stdint.h>

#include "client.h"

enum te_lcpCode {
    E_LCP_CODE_CONFIGURE_REQUEST = 1,
    E_LCP_CODE_CONFIGURE_ACK,
    E_LCP_CODE_CONFIGURE_NAK,
    E_LCP_CODE_CONFIGURE_REJECT,
    E_LCP_CODE_TERMINATE_REQUEST,
    E_LCP_CODE_TERMINATE_ACK,
    E_LCP_CODE_CODE_REJECT,
    E_LCP_CODE_PROTOCOL_REJECT,
    E_LCP_CODE_ECHO_REQUEST,
    E_LCP_CODE_ECHO_REPLY,
    E_LCP_CODE_DISCARD_REQUEST
};

enum te_lcpType {
    E_LCP_TYPE_MRU = 1,
    E_LCP_TYPE_AUTH_PROTOCOL = 3,
    E_LCP_TYPE_QUALITY_PROTOCOL,
    E_LCP_TYPE_MAGIC_NUMBER,
    E_LCP_TYPE_PFC = 7,
    E_LCP_TYPE_ACFC
};

struct ts_lcpPacketHeader {
    uint8_t code;
    uint8_t identifier;
    uint16_t length;
} __attribute__((packed));

struct ts_lcpPacket {
    struct ts_lcpPacketHeader header;
    uint8_t data[];
} __attribute__((packed));

struct ts_lcpProtocolRejectPacket {
    struct ts_lcpPacketHeader header;
    uint16_t protocolNumber;
    uint8_t data[];
} __attribute__((packed));

struct ts_lcpOption {
    uint8_t type;
    uint8_t length;
    uint8_t data[];
} __attribute__((packed));

void lcpReceive(
    struct ts_client *p_client,
    const uint8_t *p_buffer,
    size_t p_size
);
void lcpRejectProtocol(
    struct ts_client *p_client,
    uint16_t p_protocolNumber,
    const uint8_t *p_buffer,
    size_t p_size
);

#endif
