#ifndef __PPP_H__
#define __PPP_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <client.h>

typedef struct {
    uint8_t address;
    uint8_t control;
    uint8_t protocol[2];
} __attribute__((packed)) ppp_header_t;

typedef struct {
    uint8_t code;
    uint8_t identifier;
    uint16_t length;
} __attribute__((packed)) ppp_lcp_header_t;

typedef struct {
    uint8_t type;
    uint8_t length;
    uint8_t data[];
} __attribute__((packed)) ppp_lcp_option_t;

enum {
    PPP_PROTOCOL_IPCP = 0x8021,
    PPP_PROTOCOL_LCP = 0xc021
};

void ppp_sendFrameSerial(client_t *client, uint8_t *buffer, size_t bufferSize);
void ppp_sendFrameEx(client_t *client, uint16_t protocol, uint8_t *buffer, size_t bufferSize, bool acfc, bool pfc);
void ppp_sendFrame(client_t *client, uint16_t protocol, uint8_t *buffer, size_t bufferSize);
void ppp_frameReceived(client_t *client, uint8_t *pppFrameBuffer, size_t pppFrameSize);
uint16_t ppp_fcs16(const uint8_t *buffer, size_t bufferSize);

#endif
