#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>

#include "common.h"
#include "protocols/ipcp.h"
#include "protocols/ppp.h"

#define C_IPCP_OVERHEAD sizeof(struct ts_ipcpPacketHeader)

enum te_ipcpCode {
    E_IPCP_CODE_CONFIGURE_REQUEST = 1,
    E_IPCP_CODE_CONFIGURE_ACK,
    E_IPCP_CODE_CONFIGURE_NAK,
    E_IPCP_CODE_CONFIGURE_REJECT,
    E_IPCP_CODE_TERMINATE_REQUEST,
    E_IPCP_CODE_TERMINATE_ACK,
    E_IPCP_CODE_CODE_REJECT
};

enum te_ipcpType {
    E_IPCP_TYPE_ADDRESSES = 1,
    E_IPCP_TYPE_COMPRESSION,
    E_IPCP_TYPE_ADDRESS,
    E_IPCP_TYPE_DNS1 = 129,
    E_IPCP_TYPE_NBNS1,
    E_IPCP_TYPE_DNS2,
    E_IPCP_TYPE_NBNS2
};

struct ts_ipcpPacketHeader {
    uint8_t code;
    uint8_t identifier;
    uint16_t length;
} __attribute__((packed));

struct ts_ipcpPacket {
    struct ts_ipcpPacketHeader header;
    uint8_t data[];
} __attribute__((packed));

struct ts_ipcpProtocolRejectPacket {
    struct ts_ipcpPacketHeader header;
    uint16_t protocolNumber;
    uint8_t data[];
} __attribute__((packed));

struct ts_ipcpOption {
    uint8_t type;
    uint8_t length;
    uint8_t data[];
} __attribute__((packed));

struct ts_ipcpConfigurationContext {
    uint8_t buffer[C_PPP_MAX_MRU - C_IPCP_OVERHEAD];
};

static uint8_t s_ipcpAddressZero[4] = {0, 0, 0, 0};

static void ipcpReceiveConfigureRequest(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
);
static void ipcpReceiveConfigureAck(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
);
static void ipcpReceiveConfigureNak(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
);
static void ipcpReceiveConfigureReject(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
);
static void ipcpReceiveTerminateRequest(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
);
static void ipcpReceiveTerminateAck(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
);
static void ipcpReceiveCodeReject(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
);
static void ipcpReceiveUnknownCode(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
);
static void ipcpSendConfigureRequest(struct ts_ipcpContext *p_context);
static void ipcpCheckLinkEstablished(struct ts_ipcpContext *p_context);
static void ipcpAckOption(
    struct ts_ipcpConfigurationContext *p_context,
    const struct ts_ipcpOption *p_option
);
static void ipcpNakOption(
    struct ts_ipcpConfigurationContext *p_context,
    const struct ts_ipcpOption *p_option
);
static void ipcpRejectOption(
    struct ts_ipcpConfigurationContext *p_context,
    const struct ts_ipcpOption *p_option
);
static void ipcpCopyOption(
    struct ts_ipcpConfigurationContext *p_context,
    const struct ts_ipcpOption *p_option
);
static void ipcpInitializeConfigurationContext(
    struct ts_ipcpConfigurationContext *p_context,
    const struct ts_ipcpPacket *p_packet
);
static void ipcpProcessConfigureRequestAddress(
    struct ts_ipcpConfigurationContext *p_configurationContext,
    const struct ts_ipcpOption *p_option,
    uint8_t *p_currentConfigurationAddress,
    uint8_t *p_configurationAddress
);

void ipcpInit(
    struct ts_ipcpContext *p_context,
    struct ts_pppContext *p_pppContext,
    const struct ts_ipcpConfiguration *p_configuration
) {
    p_context->ackSent = false;
    p_context->ackReceived = false;
    p_context->identifier = 0;
    p_context->pppContext = p_pppContext;

    memcpy(&p_context->config, p_configuration, sizeof(p_context->config));

    p_context->configureAddress = true;
    p_context->configureDns1 = true;
    p_context->configureDns2 = true;
    p_context->configureNbns1 = true;
    p_context->configureNbns2 = true;
}

void ipcpReceive(
    struct ts_ipcpContext *p_context,
    const void *p_buffer,
    size_t p_size
) {
    const struct ts_ipcpPacket *l_packet = (const struct ts_ipcpPacket *)p_buffer;

    switch(l_packet->header.code) {
        case E_IPCP_CODE_CONFIGURE_REQUEST:
            ipcpReceiveConfigureRequest(
                p_context,
                l_packet,
                p_size - C_IPCP_OVERHEAD
            );

            break;

        case E_IPCP_CODE_CONFIGURE_ACK:
            ipcpReceiveConfigureAck(
                p_context,
                l_packet,
                p_size - C_IPCP_OVERHEAD
            );

            break;

        case E_IPCP_CODE_CONFIGURE_NAK:
            ipcpReceiveConfigureNak(
                p_context,
                l_packet,
                p_size - C_IPCP_OVERHEAD
            );

            break;

        case E_IPCP_CODE_CONFIGURE_REJECT:
            ipcpReceiveConfigureReject(
                p_context,
                l_packet,
                p_size - C_IPCP_OVERHEAD
            );

            break;

        case E_IPCP_CODE_TERMINATE_REQUEST:
            ipcpReceiveTerminateRequest(
                p_context,
                l_packet,
                p_size - C_IPCP_OVERHEAD
            );

            break;

        case E_IPCP_CODE_TERMINATE_ACK:
            ipcpReceiveTerminateAck(
                p_context,
                l_packet,
                p_size - C_IPCP_OVERHEAD
            );

            break;

        case E_IPCP_CODE_CODE_REJECT:
            ipcpReceiveCodeReject(
                p_context,
                l_packet,
                p_size - C_IPCP_OVERHEAD
            );

            break;

        default:
            ipcpReceiveUnknownCode(
                p_context,
                l_packet,
                p_size - C_IPCP_OVERHEAD
            );

            break;
    }
}

void ipcpNetwork(struct ts_ipcpContext *p_context) {
    p_context->ackReceived = false;
    p_context->ackSent = false;
    p_context->identifier = 0;

    memcpy(
        &p_context->currentConfig,
        &p_context->config,
        sizeof(struct ts_ipcpConfiguration)
    );

    ipcpSendConfigureRequest(p_context);
}

static void ipcpReceiveConfigureRequest(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
) {
    int l_optionOffset = 0;
    struct ts_ipcpConfigurationContext l_configurationContext;

    ipcpInitializeConfigurationContext(&l_configurationContext, p_packet);

    while(l_optionOffset < (int)p_size) {
        const struct ts_ipcpOption *l_option =
            (const struct ts_ipcpOption *)&p_packet->data[l_optionOffset];

        switch(l_option->type) {
            case E_IPCP_TYPE_ADDRESS:
                ipcpProcessConfigureRequestAddress(
                    &l_configurationContext,
                    l_option,
                    p_context->currentConfig.remoteAddress,
                    p_context->config.remoteAddress
                );
                break;

            case E_IPCP_TYPE_DNS1:
                ipcpProcessConfigureRequestAddress(
                    &l_configurationContext,
                    l_option,
                    p_context->currentConfig.dnsAddress1,
                    p_context->config.dnsAddress1
                );
                break;

            case E_IPCP_TYPE_DNS2:
                ipcpProcessConfigureRequestAddress(
                    &l_configurationContext,
                    l_option,
                    p_context->currentConfig.dnsAddress2,
                    p_context->config.dnsAddress2
                );
                break;

            case E_IPCP_TYPE_NBNS1:
                ipcpProcessConfigureRequestAddress(
                    &l_configurationContext,
                    l_option,
                    p_context->currentConfig.netBiosAddress1,
                    p_context->config.netBiosAddress1
                );
                break;

            case E_IPCP_TYPE_NBNS2:
                ipcpProcessConfigureRequestAddress(
                    &l_configurationContext,
                    l_option,
                    p_context->currentConfig.netBiosAddress2,
                    p_context->config.netBiosAddress2
                );
                break;

            default:
                ipcpRejectOption(&l_configurationContext, l_option);
                break;
        }

        l_optionOffset += l_option->length;
    }

    struct ts_ipcpPacket *l_packet =
        (struct ts_ipcpPacket *)l_configurationContext.buffer;

    // If the configuration is acknowledged, update the link establishment
    // status.
    if(l_packet->header.code == E_IPCP_CODE_CONFIGURE_ACK) {
        p_context->ackSent = true;
        ipcpCheckLinkEstablished(p_context);
    }

    // Send response
    pppSend(
        p_context->pppContext,
        E_PPPPROTOCOLNUMBER_IPCP,
        l_configurationContext.buffer,
        ntohs(l_packet->header.length)
    );
}

static void ipcpReceiveConfigureAck(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    p_context->ackReceived = true;
    ipcpCheckLinkEstablished(p_context);
}

static void ipcpReceiveConfigureNak(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_context);
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    // TODO
}

static void ipcpReceiveConfigureReject(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_context);
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    int l_optionOffset = 0;

    while(l_optionOffset < (int)p_size) {
        const struct ts_ipcpOption *l_option =
            (const struct ts_ipcpOption *)&p_packet->data[l_optionOffset];

        switch(l_option->type) {
            case E_IPCP_TYPE_ADDRESS:
                p_context->configureAddress = false;
                break;

            case E_IPCP_TYPE_DNS1:
                p_context->configureDns1 = false;
                break;

            case E_IPCP_TYPE_NBNS1:
                p_context->configureNbns1 = false;
                break;

            case E_IPCP_TYPE_DNS2:
                p_context->configureDns2 = false;
                break;

            case E_IPCP_TYPE_NBNS2:
                p_context->configureNbns2 = false;
                break;

            default:
                break;
        }

        l_optionOffset += l_option->length;
    }

    ipcpSendConfigureRequest(p_context);
}

static void ipcpReceiveTerminateRequest(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    struct ts_ipcpPacketHeader l_ipcpPacket;

    l_ipcpPacket.code = E_IPCP_CODE_TERMINATE_ACK;
    l_ipcpPacket.identifier = p_context->identifier++;
    l_ipcpPacket.length = ntohs(4);

    pppSend(p_context->pppContext, E_PPPPROTOCOLNUMBER_IPCP, &l_ipcpPacket, 4);
}

static void ipcpReceiveTerminateAck(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_context);
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    // Do nothing
}

static void ipcpReceiveCodeReject(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_context);
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    // Do nothing
}

static void ipcpReceiveUnknownCode(
    struct ts_ipcpContext *p_context,
    const struct ts_ipcpPacket *p_packet,
    size_t p_size
) {
    // TODO: truncate packet

    uint8_t l_buffer[4 + p_size];
    struct ts_ipcpPacket *l_packet = (struct ts_ipcpPacket *)l_buffer;

    l_packet->header.code = E_IPCP_CODE_CODE_REJECT;
    l_packet->header.identifier = p_context->identifier++;
    l_packet->header.length = 4 + p_size;
    memcpy(l_packet->data, p_packet, p_size);

    pppSend(p_context->pppContext, E_PPPPROTOCOLNUMBER_IPCP, l_packet, p_size + 4);
}

static void ipcpSendConfigureRequest(struct ts_ipcpContext *p_context) {
    uint8_t l_buffer[C_PPP_MAX_MRU];
    struct ts_ipcpPacket *l_packet = (struct ts_ipcpPacket *)l_buffer;

    l_packet->header.code = E_IPCP_CODE_CONFIGURE_REQUEST;
    l_packet->header.identifier = p_context->identifier++;

    size_t l_optionOffset = 0;

    if(p_context->configureAddress) {
        struct ts_ipcpOption *l_option =
            (struct ts_ipcpOption *)&l_packet->data[l_optionOffset];

        l_option->type = E_IPCP_TYPE_ADDRESS;
        l_option->length = 6;

        memcpy(l_option->data, p_context->currentConfig.localAddress, 4);

        l_optionOffset += 6;
    }

    if(p_context->configureDns1) {
        struct ts_ipcpOption *l_option =
            (struct ts_ipcpOption *)&l_packet->data[l_optionOffset];

        l_option->type = E_IPCP_TYPE_DNS1;
        l_option->length = 6;

        memcpy(l_option->data, p_context->currentConfig.dnsAddress1, 4);

        l_optionOffset += 6;
    }

    if(p_context->configureNbns1) {
        struct ts_ipcpOption *l_option =
            (struct ts_ipcpOption *)&l_packet->data[l_optionOffset];

        l_option->type = E_IPCP_TYPE_NBNS1;
        l_option->length = 6;

        memcpy(l_option->data, p_context->currentConfig.netBiosAddress1, 4);

        l_optionOffset += 6;
    }

    if(p_context->configureDns2) {
        struct ts_ipcpOption *l_option =
            (struct ts_ipcpOption *)&l_packet->data[l_optionOffset];

        l_option->type = E_IPCP_TYPE_DNS2;
        l_option->length = 6;

        memcpy(l_option->data, p_context->currentConfig.dnsAddress2, 4);

        l_optionOffset += 6;
    }

    if(p_context->configureNbns2) {
        struct ts_ipcpOption *l_option =
            (struct ts_ipcpOption *)&l_packet->data[l_optionOffset];

        l_option->type = E_IPCP_TYPE_NBNS2;
        l_option->length = 6;

        memcpy(l_option->data, p_context->currentConfig.netBiosAddress2, 4);

        l_optionOffset += 6;
    }

    l_packet->header.length = htons(l_optionOffset + C_IPCP_OVERHEAD);

    pppSend(
        p_context->pppContext,
        E_PPPPROTOCOLNUMBER_IPCP,
        l_packet,
        l_optionOffset + C_IPCP_OVERHEAD
    );
}

static void ipcpCheckLinkEstablished(struct ts_ipcpContext *p_context) {
    if(p_context->ackReceived && p_context->ackSent) {
        // TODO
    }
}

static void ipcpAckOption(
    struct ts_ipcpConfigurationContext *p_context,
    const struct ts_ipcpOption *p_option
) {
    struct ts_ipcpPacket *l_packet = (struct ts_ipcpPacket *)p_context->buffer;

    if(l_packet->header.code != E_IPCP_CODE_CONFIGURE_ACK) {
        return;
    }

    ipcpCopyOption(p_context, p_option);
}

static void ipcpNakOption(
    struct ts_ipcpConfigurationContext *p_context,
    const struct ts_ipcpOption *p_option
) {
    struct ts_ipcpPacket *l_packet = (struct ts_ipcpPacket *)p_context->buffer;

    if(l_packet->header.code == E_IPCP_CODE_CONFIGURE_ACK) {
        l_packet->header.code = E_IPCP_CODE_CONFIGURE_NAK;
        l_packet->header.length = htons(4);
    } else if(l_packet->header.code == E_IPCP_CODE_CONFIGURE_REJECT) {
        return;
    }

    ipcpCopyOption(p_context, p_option);
}

static void ipcpRejectOption(
    struct ts_ipcpConfigurationContext *p_context,
    const struct ts_ipcpOption *p_option
) {
    struct ts_ipcpPacket *l_packet = (struct ts_ipcpPacket *)p_context->buffer;

    if(l_packet->header.code != E_IPCP_CODE_CONFIGURE_REJECT) {
        l_packet->header.code = E_IPCP_CODE_CONFIGURE_REJECT;
        l_packet->header.length = htons(4);
    }

    ipcpCopyOption(p_context, p_option);
}

static void ipcpCopyOption(
    struct ts_ipcpConfigurationContext *p_context,
    const struct ts_ipcpOption *p_option
) {
    struct ts_ipcpPacket *l_packet = (struct ts_ipcpPacket *)p_context->buffer;

    memcpy(
        &l_packet->data[ntohs(l_packet->header.length) - C_IPCP_OVERHEAD],
        p_option,
        p_option->length
    );

    l_packet->header.length =
        htons(ntohs(l_packet->header.length) + p_option->length);
}

static void ipcpInitializeConfigurationContext(
    struct ts_ipcpConfigurationContext *p_context,
    const struct ts_ipcpPacket *p_packet
) {
    struct ts_ipcpPacket *l_packet = (struct ts_ipcpPacket *)p_context->buffer;

    l_packet->header.code = E_IPCP_CODE_CONFIGURE_ACK;
    l_packet->header.identifier = p_packet->header.identifier;
    l_packet->header.length = htons(4);
}

static void ipcpProcessConfigureRequestAddress(
    struct ts_ipcpConfigurationContext *p_configurationContext,
    const struct ts_ipcpOption *p_option,
    uint8_t *p_currentConfigurationAddress,
    uint8_t *p_configurationAddress
) {
    const bool l_peerRequestsAddress =
        memcmp(p_option->data, s_ipcpAddressZero, 4) == 0;
    const bool l_addressConfigured =
        memcmp(p_configurationAddress, s_ipcpAddressZero, 4) != 0;
    const bool l_sameAddress =
        memcmp(p_currentConfigurationAddress, p_option->data, 4) == 0;

    if(l_peerRequestsAddress) {
        if(l_addressConfigured) {
            uint8_t l_buffer[6];

            struct ts_ipcpOption *l_newOption =
                (struct ts_ipcpOption *)l_buffer;

            l_newOption->type = p_option->type;
            l_newOption->length = 6;

            memcpy(l_newOption->data, p_currentConfigurationAddress, 4);

            ipcpNakOption(p_configurationContext, l_newOption);
        } else {
            ipcpRejectOption(p_configurationContext, p_option);
        }
    } else {
        if(l_sameAddress) {
            ipcpAckOption(p_configurationContext, p_option);
        } else if(l_addressConfigured) {
            uint8_t l_buffer[6];

            struct ts_ipcpOption *l_newOption =
                (struct ts_ipcpOption *)l_buffer;

            l_newOption->type = p_option->type;
            l_newOption->length = 6;

            memcpy(l_newOption->data, p_currentConfigurationAddress, 4);

            ipcpNakOption(p_configurationContext, l_newOption);
        } else {
            memcpy(p_currentConfigurationAddress, p_option->data, 4);
            ipcpAckOption(p_configurationContext, p_option);
        }
    }
}
