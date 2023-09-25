#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>

#include "common.h"
#include "protocols/ppp.h"

#define C_LCP_OVERHEAD sizeof(struct ts_lcpPacketHeader)

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
    E_LCP_TYPE_ACCM,
    E_LCP_TYPE_AUTH_PROTOCOL,
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

struct ts_lcpConfigurationContext {
    uint8_t buffer[C_PPP_MAX_MRU - C_LCP_OVERHEAD];
};

static void lcpReceiveConfigureRequest(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
);
static void lcpReceiveConfigureAck(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
);
static void lcpReceiveConfigureNak(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
);
static void lcpReceiveConfigureReject(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
);
static void lcpReceiveTerminateRequest(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
);
static void lcpReceiveTerminateAck(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
);
static void lcpReceiveCodeReject(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
);
static void lcpReceiveProtocolReject(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
);
static void lcpReceiveEchoRequest(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
);
static void lcpReceiveEchoReply(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
);
static void lcpReceiveDiscardRequest(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
);
static void lcpReceiveUnknownCode(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
);
static void lcpSendConfigureRequest(struct ts_pppContext *p_context);
static void lcpCheckLinkEstablished(struct ts_pppContext *p_context);
static void lcpAckOption(
    struct ts_lcpConfigurationContext *p_context,
    const struct ts_lcpOption *p_option
);
static void lcpNakOption(
    struct ts_lcpConfigurationContext *p_context,
    const struct ts_lcpOption *p_option
);
static void lcpRejectOption(
    struct ts_lcpConfigurationContext *p_context,
    const struct ts_lcpOption *p_option
);
static void lcpCopyOption(
    struct ts_lcpConfigurationContext *p_context,
    const struct ts_lcpOption *p_option
);
static void lcpInitializeConfigurationContext(
    struct ts_lcpConfigurationContext *p_context,
    const struct ts_lcpPacket *p_packet
);

void lcpReceive(void *p_arg, const void *p_buffer, size_t p_size) {
    struct ts_pppContext *l_context = (struct ts_pppContext *)p_arg;
    const struct ts_lcpPacket *l_packet = (const struct ts_lcpPacket *)p_buffer;

    switch(l_packet->header.code) {
        case E_LCP_CODE_CONFIGURE_REQUEST:
            lcpReceiveConfigureRequest(
                l_context,
                l_packet,
                p_size - C_LCP_OVERHEAD
            );

            break;

        case E_LCP_CODE_CONFIGURE_ACK:
            lcpReceiveConfigureAck(
                l_context,
                l_packet,
                p_size - C_LCP_OVERHEAD
            );

            break;

        case E_LCP_CODE_CONFIGURE_NAK:
            lcpReceiveConfigureNak(
                l_context,
                l_packet,
                p_size - C_LCP_OVERHEAD
            );

            break;

        case E_LCP_CODE_CONFIGURE_REJECT:
            lcpReceiveConfigureReject(
                l_context,
                l_packet,
                p_size - C_LCP_OVERHEAD
            );

            break;

        case E_LCP_CODE_TERMINATE_REQUEST:
            lcpReceiveTerminateRequest(
                l_context,
                l_packet,
                p_size - C_LCP_OVERHEAD
            );

            break;

        case E_LCP_CODE_TERMINATE_ACK:
            lcpReceiveTerminateAck(
                l_context,
                l_packet,
                p_size - C_LCP_OVERHEAD
            );

            break;

        case E_LCP_CODE_CODE_REJECT:
            lcpReceiveCodeReject(
                l_context,
                l_packet,
                p_size - C_LCP_OVERHEAD
            );

            break;

        case E_LCP_CODE_PROTOCOL_REJECT:
            lcpReceiveProtocolReject(
                l_context,
                l_packet,
                p_size - C_LCP_OVERHEAD
            );

            break;

        case E_LCP_CODE_ECHO_REQUEST:
            lcpReceiveEchoRequest(
                l_context,
                l_packet,
                p_size - C_LCP_OVERHEAD
            );

            break;

        case E_LCP_CODE_ECHO_REPLY:
            lcpReceiveEchoReply(
                l_context,
                l_packet,
                p_size - C_LCP_OVERHEAD
            );

            break;

        case E_LCP_CODE_DISCARD_REQUEST:
            lcpReceiveDiscardRequest(
                l_context,
                l_packet,
                p_size - C_LCP_OVERHEAD
            );

            break;

        default:
            lcpReceiveUnknownCode(
                l_context,
                l_packet,
                p_size - C_LCP_OVERHEAD
            );

            break;
    }
}

void lcpUp(struct ts_pppContext *p_context) {
    p_context->lcpContext.ackReceived = false;
    p_context->lcpContext.ackSent = false;
    p_context->lcpContext.identifier = 0;
    p_context->magicNumber = time(NULL);

    lcpSendConfigureRequest(p_context);
}

void lcpSendProtocolReject(
    struct ts_pppContext *p_context,
    const uint8_t *p_buffer,
    size_t p_size
) {
    // TODO: truncate packet
    const uint8_t l_buffer[p_size + C_LCP_OVERHEAD];

    struct ts_lcpPacket *l_packet = (struct ts_lcpPacket *)l_buffer;

    l_packet->header.code = E_LCP_CODE_PROTOCOL_REJECT;
    l_packet->header.identifier = p_context->lcpContext.identifier++;
    l_packet->header.length = htons(sizeof(l_buffer));

    memcpy(l_packet->data, p_buffer, p_size);

    pppSend(p_context, E_PPPPROTOCOLNUMBER_LCP, l_packet, sizeof(l_buffer));
}

static void lcpReceiveConfigureRequest(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
) {
    int l_optionOffset = 0;
    struct ts_lcpConfigurationContext l_configurationContext;

    lcpInitializeConfigurationContext(&l_configurationContext, p_packet);

    while(l_optionOffset < (int)p_size) {
        const struct ts_lcpOption *l_option =
            (const struct ts_lcpOption *)&p_packet->data[l_optionOffset];

        switch(l_option->type) {
            case E_LCP_TYPE_MRU:
                if(p_context->mru < ntohs(*(uint16_t *)p_packet->data)) {
                    uint8_t l_optionBuffer[4];
                    struct ts_lcpOption *l_newOption =
                        (struct ts_lcpOption *)l_optionBuffer;

                    l_newOption->type = E_LCP_TYPE_MRU;
                    l_newOption->length = 4;
                    *(uint16_t *)l_newOption->data = htons(p_context->mru);

                    lcpNakOption(&l_configurationContext, l_newOption);
                } else {
                    // TODO: check if the value is not too low

                    if(p_context->mru > ntohs(*(uint16_t *)p_packet->data)) {
                        p_context->mru = ntohs(*(uint16_t *)p_packet->data);
                    }

                    lcpAckOption(&l_configurationContext, l_option);
                }

                break;

            case E_LCP_TYPE_MAGIC_NUMBER:
                if(memcmp("\x00\x00\x00\x00", l_option->data, 4) == 0) {
                    uint8_t l_optionBuffer[6];
                    struct ts_lcpOption *l_newOption =
                        (struct ts_lcpOption *)l_optionBuffer;

                    l_newOption->type = E_LCP_TYPE_MAGIC_NUMBER;
                    l_newOption->length = 6;
                    *(uint32_t *)l_newOption->data = ~p_context->magicNumber;

                    lcpNakOption(&l_configurationContext, l_newOption);
                } else {
                    lcpAckOption(&l_configurationContext, l_option);
                }

                break;

            default:
                lcpRejectOption(&l_configurationContext, l_option);
                break;
        }

        l_optionOffset += l_option->length;
    }

    struct ts_lcpPacket *l_packet =
        (struct ts_lcpPacket *)l_configurationContext.buffer;

    // If the configuration is acknowledged, update the link establishment
    // status.
    if(l_packet->header.code == E_LCP_CODE_CONFIGURE_ACK) {
        p_context->lcpContext.ackSent = true;
        lcpCheckLinkEstablished(p_context);
    }

    // Send response
    pppSend(
        p_context,
        E_PPPPROTOCOLNUMBER_LCP,
        l_configurationContext.buffer,
        ntohs(l_packet->header.length)
    );
}

static void lcpReceiveConfigureAck(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    p_context->lcpContext.ackReceived = true;
    lcpCheckLinkEstablished(p_context);
}

static void lcpReceiveConfigureNak(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_context);
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    // TODO
}

static void lcpReceiveConfigureReject(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_context);
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    // TODO
}

static void lcpReceiveTerminateRequest(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    struct ts_lcpPacketHeader l_lcpPacket;

    l_lcpPacket.code = E_LCP_CODE_TERMINATE_ACK;
    l_lcpPacket.identifier = p_context->lcpContext.identifier++;
    l_lcpPacket.length = ntohs(4);

    pppSend(p_context, E_PPPPROTOCOLNUMBER_LCP, &l_lcpPacket, 4);

    pppDown(p_context);
}

static void lcpReceiveTerminateAck(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_context);
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    // Do nothing
}

static void lcpReceiveCodeReject(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_context);
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    // Do nothing
}

static void lcpReceiveProtocolReject(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_context);
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    // TODO: deactivate protocol
}

static void lcpReceiveEchoRequest(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
) {
    // Echo-Requests are ignored if the link is not in opened state (RFC 1661).
    if(
        p_context->linkState == E_PPPLINKSTATE_AUTHENTICATE
        || p_context->linkState == E_PPPLINKSTATE_NETWORK
    ) {
        // Clone the packet
        uint8_t l_buffer[p_size];
        struct ts_lcpPacket *l_packet = (struct ts_lcpPacket *)l_buffer;

        memcpy(l_packet, p_packet, p_size);

        // Change the code from Echo-Request to Echo-Reply
        l_packet->header.code = E_LCP_CODE_ECHO_REPLY;

        // Replace the magic number
        *(uint32_t *)l_packet->data = p_context->magicNumber;

        // Send the packet.
        pppSend(p_context, E_PPPPROTOCOLNUMBER_LCP, l_packet, p_size);
    }
}

static void lcpReceiveEchoReply(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_context);
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    // Do nothing
}

static void lcpReceiveDiscardRequest(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
) {
    M_UNUSED_PARAMETER(p_context);
    M_UNUSED_PARAMETER(p_packet);
    M_UNUSED_PARAMETER(p_size);

    // Do nothing
}

static void lcpReceiveUnknownCode(
    struct ts_pppContext *p_context,
    const struct ts_lcpPacket *p_packet,
    size_t p_size
) {
    // TODO: truncate packet

    uint8_t l_buffer[4 + p_size];
    struct ts_lcpPacket *l_packet = (struct ts_lcpPacket *)l_buffer;

    l_packet->header.code = E_LCP_CODE_CODE_REJECT;
    l_packet->header.identifier = p_context->lcpContext.identifier++;
    l_packet->header.length = 4 + p_size;
    memcpy(l_packet->data, p_packet, p_size);

    pppSend(p_context, E_PPPPROTOCOLNUMBER_LCP, l_packet, p_size + 4);
}

static void lcpSendConfigureRequest(struct ts_pppContext *p_context) {
    uint8_t l_buffer[14];

    struct ts_lcpPacket *l_packet = (struct ts_lcpPacket *)l_buffer;

    l_packet->header.code = E_LCP_CODE_CONFIGURE_REQUEST;
    l_packet->header.identifier = p_context->lcpContext.identifier++;
    l_packet->header.length = htons(14);

    struct ts_lcpOption *l_optionMru = (struct ts_lcpOption *)l_packet->data;
    struct ts_lcpOption *l_optionMagicNumber =
        (struct ts_lcpOption *)&l_packet->data[4];

    l_optionMru->type = E_LCP_TYPE_MRU;
    l_optionMru->length = 4;
    *(uint16_t *)l_optionMru->data = htons(p_context->mru);

    l_optionMagicNumber->type = E_LCP_TYPE_MAGIC_NUMBER;
    l_optionMagicNumber->length = 6;
    memcpy(l_optionMagicNumber->data, &p_context->magicNumber, 4);

    pppSend(p_context, E_PPPPROTOCOLNUMBER_LCP, l_packet, 14);
}

static void lcpCheckLinkEstablished(struct ts_pppContext *p_context) {
    if(p_context->lcpContext.ackReceived && p_context->lcpContext.ackSent) {
        pppNetwork(p_context);
    }
}

static void lcpAckOption(
    struct ts_lcpConfigurationContext *p_context,
    const struct ts_lcpOption *p_option
) {
    struct ts_lcpPacket *l_packet = (struct ts_lcpPacket *)p_context->buffer;

    if(l_packet->header.code != E_LCP_CODE_CONFIGURE_ACK) {
        return;
    }

    lcpCopyOption(p_context, p_option);
}

static void lcpNakOption(
    struct ts_lcpConfigurationContext *p_context,
    const struct ts_lcpOption *p_option
) {
    struct ts_lcpPacket *l_packet = (struct ts_lcpPacket *)p_context->buffer;

    if(l_packet->header.code == E_LCP_CODE_CONFIGURE_ACK) {
        l_packet->header.code = E_LCP_CODE_CONFIGURE_NAK;
        l_packet->header.length = htons(4);
    } else if(l_packet->header.code == E_LCP_CODE_CONFIGURE_REJECT) {
        return;
    }

    lcpCopyOption(p_context, p_option);
}

static void lcpRejectOption(
    struct ts_lcpConfigurationContext *p_context,
    const struct ts_lcpOption *p_option
) {
    struct ts_lcpPacket *l_packet = (struct ts_lcpPacket *)p_context->buffer;

    if(l_packet->header.code != E_LCP_CODE_CONFIGURE_REJECT) {
        l_packet->header.code = E_LCP_CODE_CONFIGURE_REJECT;
        l_packet->header.length = htons(4);
    }

    lcpCopyOption(p_context, p_option);
}

static void lcpCopyOption(
    struct ts_lcpConfigurationContext *p_context,
    const struct ts_lcpOption *p_option
) {
    struct ts_lcpPacket *l_packet = (struct ts_lcpPacket *)p_context->buffer;

    memcpy(
        &l_packet->data[ntohs(l_packet->header.length) - C_LCP_OVERHEAD],
        p_option,
        p_option->length
    );

    l_packet->header.length =
        htons(ntohs(l_packet->header.length) + p_option->length);
}

static void lcpInitializeConfigurationContext(
    struct ts_lcpConfigurationContext *p_context,
    const struct ts_lcpPacket *p_packet
) {
    struct ts_lcpPacket *l_packet = (struct ts_lcpPacket *)p_context->buffer;

    l_packet->header.code = E_LCP_CODE_CONFIGURE_ACK;
    l_packet->header.identifier = p_packet->header.identifier;
    l_packet->header.length = htons(4);
}
