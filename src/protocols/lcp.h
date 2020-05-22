#ifndef __LCP_H__
#define __LCP_H__

#include <stddef.h>
#include <stdint.h>

#include <client.h>

enum {
    PPP_LCP_CONFIGURATION_REQUEST = 1,
    PPP_LCP_CONFIGURATION_ACK = 2,
    PPP_LCP_CONFIGURATION_NAK = 3,
    PPP_LCP_CONFIGURATION_REJECT = 4,
    PPP_LCP_TERMINATE_REQUEST = 5,
    PPP_LCP_TERMINATE_ACK = 6,
    PPP_LCP_CODE_REJECT = 7,
    PPP_LCP_PROTOCOL_REJECT = 8,
    PPP_LCP_ECHO_REQUEST = 9,
    PPP_LCP_ECHO_REPLY = 10,
    PPP_LCP_DISCARD_REQUEST = 11
};

enum {
    PPP_LCP_OPTION_MRU = 1,
    PPP_LCP_OPTION_ACCM = 2,
    PPP_LCP_OPTION_MAGIC_NUMBER = 5,
    PPP_LCP_OPTION_PFC = 7,
    PPP_LCP_OPTION_ACFC = 8
};

typedef struct {
    uint8_t code;
    uint8_t identifier;
    uint16_t length;
} __attribute__((packed)) lcp_header_t;

typedef struct {
    uint8_t type;
    uint8_t length;
    uint8_t data[];
} __attribute__((packed)) lcp_option_t;

void lcp_frameReceived(client_t *client, uint8_t *lcpFrameBuffer, size_t lcpFrameSize);
void lcp_rejectFrame(client_t *client, uint16_t protocol, const uint8_t *buffer, size_t size);

#endif
