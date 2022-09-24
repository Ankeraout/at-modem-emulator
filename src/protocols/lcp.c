#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "protocols/lcp.h"
#include "protocols/ppp.h"

void lcpReceive(
    struct ts_client *p_client,
    const uint8_t *p_buffer,
    size_t p_size
) {
    const struct ts_lcpPacket *l_lcpPacket =
        (const struct ts_lcpPacket *)p_buffer;

    printf("lcp: Received LCP frame with code %d\n", l_lcpPacket->header.code);
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
