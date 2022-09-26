#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>

#include "protocols/ipcp.h"
#include "protocols/lcp.h"
#include "protocols/ppp.h"

static void ipcpHandleConfigureRequest(
    struct ts_client *p_client,
    const struct ts_lcpPacket *p_ipcpPacket,
    size_t p_size
);
static void ipcpRejectCode(
    struct ts_client *p_client,
    const struct ts_lcpPacket *p_ipcpPacket,
    size_t p_size
);

void ipcpReceive(
    struct ts_client *p_client,
    const uint8_t *p_buffer,
    size_t p_size
) {
    const struct ts_lcpPacket *l_ipcpPacket =
        (const struct ts_lcpPacket *)p_buffer;

    if(l_ipcpPacket->header.code == E_LCP_CODE_CONFIGURE_REQUEST) {
        ipcpHandleConfigureRequest(p_client, l_ipcpPacket, p_size);
    } else if(l_ipcpPacket->header.code == E_LCP_CODE_CONFIGURE_ACK) {
        printf("ipcp: Client #%d acknowledged configuration.\n", p_client->id);
    } else if(l_ipcpPacket->header.code == E_LCP_CODE_CONFIGURE_NAK) {
        printf("ipcp: Client #%d nacked configuration.\n", p_client->id);
    } else if(l_ipcpPacket->header.code == E_LCP_CODE_CONFIGURE_REJECT) {
        printf("ipcp: Client #%d rejected configuration.\n", p_client->id);
    } else {
        ipcpRejectCode(p_client, l_ipcpPacket, p_size);
    }
}

static void ipcpHandleConfigureRequest(
    struct ts_client *p_client,
    const struct ts_lcpPacket *p_ipcpPacket,
    size_t p_size
) {
    size_t l_optionOffset = 0;
    uint8_t l_responsePacketBuffer[p_size];
    struct ts_lcpPacket *l_responsePacket =
        (struct ts_lcpPacket *)l_responsePacketBuffer;
    size_t l_responsePacketDataLength = 0;

    l_responsePacket->header.code = E_LCP_CODE_CONFIGURE_ACK;
    l_responsePacket->header.identifier = p_ipcpPacket->header.identifier;

    while((l_optionOffset + 4) < ntohs(p_ipcpPacket->header.length)) {
        const struct ts_lcpOption *l_ipcpOption =
            (const struct ts_lcpOption *)&p_ipcpPacket->data[l_optionOffset];
        uint8_t l_nackBuffer[l_ipcpOption->length];
        struct ts_lcpOption *l_nackOption = (struct ts_lcpOption *)l_nackBuffer;
        bool l_nak = false;
        bool l_reject = false;

        if(l_ipcpOption->type == E_IPCP_TYPE_IP_ADDRESS) {
            uint32_t l_ipAddress = ntohl(*(uint32_t *)l_ipcpOption->data);

            if(l_ipAddress != p_client->ipv4Context.address) {
                l_nackOption->type = E_IPCP_TYPE_IP_ADDRESS;
                l_nackOption->length = 6;
                *(uint32_t *)l_nackOption->data =
                    htonl(p_client->ipv4Context.address);

                l_ipcpOption = l_nackOption;
                l_nak = true;
            }
        } else {
            l_reject = true;

            printf(
                "ipcp: Unknown option #%d from client #%d\n",
                l_ipcpOption->type,
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
                    l_ipcpOption,
                    l_ipcpOption->length
                );

                l_responsePacketDataLength += l_ipcpOption->length;
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
                l_ipcpOption,
                l_ipcpOption->length
            );

            l_responsePacketDataLength += l_ipcpOption->length;
        } else {
            if(l_responsePacket->header.code == E_LCP_CODE_CONFIGURE_ACK) {
                // Copy the option in the Configure-Ack packet
                memcpy(
                    &l_responsePacket->data[l_responsePacketDataLength],
                    l_ipcpOption,
                    l_ipcpOption->length
                );

                l_responsePacketDataLength += l_ipcpOption->length;
            }
        }

        l_optionOffset += l_ipcpOption->length;
    }

    // Compute the response packet length
    l_responsePacket->header.length = htons(l_responsePacketDataLength + 4);

    // Send the response packet
    pppSend(
        p_client,
        C_PPP_PROTOCOLNUMBER_IPCP,
        l_responsePacketBuffer,
        ntohs(l_responsePacket->header.length)
    );

    if(l_responsePacket->header.code == E_LCP_CODE_CONFIGURE_ACK) {
        printf(
            "ipcp: Client #%d's configuration was acknowledged.\n",
            p_client->id
        );
    } else if(l_responsePacket->header.code == E_LCP_CODE_CONFIGURE_NAK) {
        printf("ipcp: Client #%d's configuration was nacked.\n", p_client->id);
    } else if(l_responsePacket->header.code == E_LCP_CODE_CONFIGURE_REJECT) {
        printf("ipcp: Client #%d's configuration was rejected.\n", p_client->id);
    }
}

static void ipcpRejectCode(
    struct ts_client *p_client,
    const struct ts_lcpPacket *p_ipcpPacket,
    size_t p_size
) {
    // Remove warning
    (void)p_size;

    printf(
        "ipcp: Rejected unknown code #%d from client #%d\n",
        p_ipcpPacket->header.code,
        p_client->id
    );
}
