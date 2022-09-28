#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>

#include "protocols/lcp.h"
#include "protocols/ppp.h"

static void lcpHandleConfigureRequest(
    struct ts_client *p_client,
    const struct ts_lcpPacket *p_lcpPacket,
    size_t p_size
);
static void lcpHandleTerminateRequest(
    struct ts_client *p_client,
    const struct ts_lcpPacket *p_lcpPacket,
    size_t p_size
);
static void lcpRejectCode(
    struct ts_client *p_client,
    const struct ts_lcpPacket *p_lcpPacket,
    size_t p_size
);

void lcpReceive(
    struct ts_client *p_client,
    const uint8_t *p_buffer,
    size_t p_size
) {
    const struct ts_lcpPacket *l_lcpPacket =
        (const struct ts_lcpPacket *)p_buffer;

    if(l_lcpPacket->header.code == E_LCP_CODE_CONFIGURE_REQUEST) {
        lcpHandleConfigureRequest(p_client, l_lcpPacket, p_size);
    } else if(l_lcpPacket->header.code == E_LCP_CODE_CONFIGURE_ACK) {
        printf("lcp: Client #%d acknowledged configuration.\n", p_client->id);
    } else if(l_lcpPacket->header.code == E_LCP_CODE_CONFIGURE_NAK) {
        printf("lcp: Client #%d nacked configuration.\n", p_client->id);
    } else if(l_lcpPacket->header.code == E_LCP_CODE_CONFIGURE_REJECT) {
        printf("lcp: Client #%d rejected configuration.\n", p_client->id);
    } else if(l_lcpPacket->header.code == E_LCP_CODE_TERMINATE_REQUEST) {
        lcpHandleTerminateRequest(p_client, l_lcpPacket, p_size);
    } else {
        lcpRejectCode(p_client, l_lcpPacket, p_size);
    }
}

void lcpRejectProtocol(
    struct ts_client *p_client,
    uint16_t p_protocolNumber,
    const uint8_t *p_buffer,
    size_t p_size
) {
    size_t l_packetLength = p_size + 6;

    if(l_packetLength > p_client->pppContext.mru) {
        l_packetLength = p_client->pppContext.mru;
    }

    uint8_t l_buffer[l_packetLength];
    struct ts_lcpProtocolRejectPacket *l_lcpPacket =
        (struct ts_lcpProtocolRejectPacket *)l_buffer;

    l_lcpPacket->header.code = E_LCP_CODE_PROTOCOL_REJECT;
    l_lcpPacket->header.identifier = 0;
    l_lcpPacket->header.length = l_packetLength;
    l_lcpPacket->protocolNumber = htons(p_protocolNumber);

    memcpy(l_lcpPacket->data, p_buffer, l_packetLength - 6);

    pppSend(
        p_client,
        C_PPP_PROTOCOLNUMBER_LCP,
        l_buffer,
        l_packetLength
    );

    printf(
        "lcp: Rejected packet from client #0: Unknown protocol 0x%04x\n",
        p_protocolNumber
    );
}

static void lcpHandleConfigureRequest(
    struct ts_client *p_client,
    const struct ts_lcpPacket *p_lcpPacket,
    size_t p_size
) {
    size_t l_optionOffset = 0;
    uint8_t l_responsePacketBuffer[p_size];
    struct ts_lcpPacket *l_responsePacket =
        (struct ts_lcpPacket *)l_responsePacketBuffer;
    size_t l_responsePacketDataLength = 0;

    l_responsePacket->header.code = E_LCP_CODE_CONFIGURE_ACK;
    l_responsePacket->header.identifier = p_lcpPacket->header.identifier;

    while((l_optionOffset + 4) < ntohs(p_lcpPacket->header.length)) {
        const struct ts_lcpOption *l_lcpOption =
            (const struct ts_lcpOption *)&p_lcpPacket->data[l_optionOffset];
        bool l_nak = false;
        bool l_reject = false;

        if(l_lcpOption->type == E_LCP_TYPE_MRU) {
            uint16_t l_mru = ntohs(*(uint16_t *)l_lcpOption->data);

            if(l_mru < C_PPP_MRU_MIN) {
                l_reject = true;
            } else {
                p_client->pppContext.mru = l_mru;
            }
        } else if(l_lcpOption->type == E_LCP_TYPE_ACCM) {
            p_client->hdlcContext.accm = ntohl(*(uint32_t *)l_lcpOption->data);
        } else if(l_lcpOption->type == E_LCP_TYPE_MAGIC_NUMBER) {
            memcpy(p_client->pppContext.magicNumber, l_lcpOption->data, 4);
        } else if(l_lcpOption->type == E_LCP_TYPE_PFC) {
            p_client->pppContext.pfcEnabled = true;
        } else if(l_lcpOption->type == E_LCP_TYPE_ACFC) {
            p_client->hdlcContext.acfcEnabled = true;
        } else {
            l_reject = true;

            printf(
                "lcp: Unknown option #%d from client #%d\n",
                l_lcpOption->type,
                p_client->id
            );
        }

        if(l_nak) {
            // Note: if the packet code is Configure-Reject, do not change it.
            // Configure-Reject has a higher priority than Configure-Nak.
            if(l_responsePacket->header.code == E_LCP_CODE_CONFIGURE_ACK) {
                // Change the packet to Configure-Nak
                l_responsePacket->header.code = E_LCP_CODE_CONFIGURE_NAK;
                l_responsePacketDataLength = 0;
            }

            if(l_responsePacket->header.code == E_LCP_CODE_CONFIGURE_NAK) {
                // Copy the option in the Configure-Nak packet
                memcpy(
                    &l_responsePacket->data[l_responsePacketDataLength],
                    l_lcpOption,
                    l_lcpOption->length
                );

                l_responsePacketDataLength += l_lcpOption->length;
            }
        } else if(l_reject) {
            if(l_responsePacket->header.code != E_LCP_CODE_CONFIGURE_REJECT) {
                // Change the packet to Configure-Reject
                l_responsePacket->header.code = E_LCP_CODE_CONFIGURE_REJECT;
                l_responsePacketDataLength = 0;
            }

            // Copy the option in the Configure-Reject packet
            memcpy(
                &l_responsePacket->data[l_responsePacketDataLength],
                l_lcpOption,
                l_lcpOption->length
            );

            l_responsePacketDataLength += l_lcpOption->length;
        } else {
            if(l_responsePacket->header.code == E_LCP_CODE_CONFIGURE_ACK) {
                // Copy the option in the Configure-Ack packet
                memcpy(
                    &l_responsePacket->data[l_responsePacketDataLength],
                    l_lcpOption,
                    l_lcpOption->length
                );

                l_responsePacketDataLength += l_lcpOption->length;
            }
        }

        l_optionOffset += l_lcpOption->length;
    }

    // Compute the response packet length
    l_responsePacket->header.length = htons(l_responsePacketDataLength + 4);

    // Send the response packet
    pppSend(
        p_client,
        C_PPP_PROTOCOLNUMBER_LCP,
        l_responsePacketBuffer,
        ntohs(l_responsePacket->header.length)
    );

    if(l_responsePacket->header.code == E_LCP_CODE_CONFIGURE_ACK) {
        uint8_t l_lcpPacketBuffer[18];
        struct ts_lcpPacket *l_lcpPacket =
            (struct ts_lcpPacket *)l_lcpPacketBuffer;

        l_lcpPacket->header.code = E_LCP_CODE_CONFIGURE_REQUEST;
        l_lcpPacket->header.identifier = 0;
        l_lcpPacket->header.length = htons(sizeof(l_lcpPacketBuffer));

        struct ts_lcpOption *l_lcpOption =
            (struct ts_lcpOption *)l_lcpPacket->data;
        l_lcpOption->type = E_LCP_TYPE_MRU;
        l_lcpOption->length = 4;
        *(uint16_t *)l_lcpOption->data = htons(p_client->pppContext.mru);

        l_lcpOption = (struct ts_lcpOption *)&l_lcpPacket->data[4];
        l_lcpOption->type = E_LCP_TYPE_ACCM;
        l_lcpOption->length = 6;
        *(uint32_t *)l_lcpOption->data = 0x00000000;

        l_lcpOption = (struct ts_lcpOption *)&l_lcpPacket->data[10];
        l_lcpOption->type = E_LCP_TYPE_PFC;
        l_lcpOption->length = 2;

        l_lcpOption = (struct ts_lcpOption *)&l_lcpPacket->data[12];
        l_lcpOption->type = E_LCP_TYPE_ACFC;
        l_lcpOption->length = 2;

        // Send configure-request
        pppSend(
            p_client,
            C_PPP_PROTOCOLNUMBER_LCP,
            l_lcpPacketBuffer,
            sizeof(l_lcpPacketBuffer)
        );

        printf(
            "lcp: Client #%d's configuration was acknowledged.\n",
            p_client->id
        );

        printf(
            "lcp: Sent Configure-Request to client #%d.\n",
            p_client->id
        );
    } else if(l_responsePacket->header.code == E_LCP_CODE_CONFIGURE_NAK) {
        printf("lcp: Client #%d's configuration was nacked.\n", p_client->id);
    } else if(l_responsePacket->header.code == E_LCP_CODE_CONFIGURE_REJECT) {
        printf("lcp: Client #%d's configuration was rejected.\n", p_client->id);
    }
}

static void lcpHandleTerminateRequest(
    struct ts_client *p_client,
    const struct ts_lcpPacket *p_lcpPacket,
    size_t p_size
) {
    uint8_t l_buffer[p_size];

    struct ts_lcpPacket *l_lcpPacket = (struct ts_lcpPacket *)l_buffer;

    l_lcpPacket->header.code = E_LCP_CODE_TERMINATE_ACK;
    l_lcpPacket->header.identifier = p_lcpPacket->header.identifier;
    l_lcpPacket->header.length = p_lcpPacket->header.length;

    memcpy(l_lcpPacket->data, p_lcpPacket->data, p_size - 4);

    pppSend(
        p_client,
        C_PPP_PROTOCOLNUMBER_LCP,
        l_lcpPacket,
        p_size
    );

    printf("lcp: Client #%d asked for link termination.\n", p_client->id);

    if(p_client->ipv4Context.address != 0) {
        ipv4Free(p_client->ipv4Context.address);
        p_client->ipv4Context.address = 0;
    }
}

static void lcpRejectCode(
    struct ts_client *p_client,
    const struct ts_lcpPacket *p_lcpPacket,
    size_t p_size
) {
    // Remove warning
    (void)p_size;

    printf(
        "lcp: Rejected unknown code #%d from client #%d\n",
        p_lcpPacket->header.code,
        p_client->id
    );
}
