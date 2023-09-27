#ifndef __INCLUDE_PROTOCOLS_IPCP_H__
#define __INCLUDE_PROTOCOLS_IPCP_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocols/ppp.h"

struct ts_ipcpConfiguration {
    uint8_t localAddress[4];
    uint8_t remoteAddress[4];
    uint8_t dnsAddress1[4];
    uint8_t dnsAddress2[4];
    uint8_t netBiosAddress1[4];
    uint8_t netBiosAddress2[4];
};

struct ts_ipcpContext {
    bool ackSent;
    bool ackReceived;
    uint8_t identifier;
    struct ts_pppContext *pppContext;
    struct ts_ipcpConfiguration config;
    struct ts_ipcpConfiguration currentConfig;
    bool configureAddress;
    bool configureDns1;
    bool configureDns2;
    bool configureNbns1;
    bool configureNbns2;
};

void ipcpInit(
    struct ts_ipcpContext *p_context,
    struct ts_pppContext *p_pppContext,
    const struct ts_ipcpConfiguration *p_configuration
);

void ipcpReceive(
    struct ts_ipcpContext *p_context,
    const void *p_buffer,
    size_t p_size
);

void ipcpNetwork(struct ts_ipcpContext *p_context);

#endif
