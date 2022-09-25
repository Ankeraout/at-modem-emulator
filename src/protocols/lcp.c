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

void lcpReceive(
    struct ts_client *p_client,
    const uint8_t *p_buffer,
    size_t p_size
) {
    const struct ts_lcpPacket *l_lcpPacket =
        (const struct ts_lcpPacket *)p_buffer;

    size_t l_optionOffset = 0;

    if(l_lcpPacket->header.code == E_LCP_CODE_CONFIGURE_REQUEST) {
        lcpHandleConfigureRequest(p_client, l_lcpPacket, p_size);
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
    l_lcpPacket->protocolNumber = p_protocolNumber;

    memcpy(l_lcpPacket->data, p_buffer, l_packetLength - 6);

    pppSend(
        p_client,
        C_PPP_PROTOCOLNUMBER_LCP,
        p_buffer,
        l_packetLength - 6
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
        } else if(l_lcpOption->type == E_LCP_TYPE_AUTH_PROTOCOL) {
            l_reject = true;
        } else if(l_lcpOption->type == E_LCP_TYPE_QUALITY_PROTOCOL) {
            l_reject = true;
        } else if(l_lcpOption->type == E_LCP_TYPE_MAGIC_NUMBER) {
            memcpy(p_client->pppContext.magicNumber, l_lcpOption->data, 4);
        } else if(l_lcpOption->type == E_LCP_TYPE_PFC) {
            p_client->pppContext.pfcEnabled = true;
        } else if(l_lcpOption->type == E_LCP_TYPE_ACFC) {
            p_client->hdlcContext.acfcEnabled = true;
        } else {
            l_reject = true;
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
}
