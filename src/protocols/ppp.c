#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "client.h"
#include "protocols/lcp.h"
#include "protocols/ppp.h"

void pppInit(struct ts_client *p_client) {
    p_client->pppContext.mru = C_PPP_MRU_DEFAULT;
}

void pppReceive(
    struct ts_client *p_client,
    const uint8_t *p_buffer,
    size_t p_size
) {
    uint_fast16_t l_dataOffset = 1;

    // Read protocol number
    uint16_t l_protocolNumber = p_buffer[0];

    if((l_protocolNumber & 0x01) == 0) {
        l_dataOffset = 2;
        l_protocolNumber = (l_protocolNumber << 8) | p_buffer[1];
    }

    // Handle packet
    switch(l_protocolNumber) {
        case 0xc021:
            lcpReceive(
                p_client,
                &p_buffer[l_dataOffset],
                p_size - l_dataOffset
            );

            break;

        default:
            lcpRejectProtocol(
                p_client,
                l_protocolNumber,
                &p_buffer[l_dataOffset],
                p_size - l_dataOffset
            );

            break;
    }
}

void pppSend(
    struct ts_client *p_client,
    uint16_t p_protocolNumber,
    const uint8_t *p_buffer,
    size_t p_size
) {
    uint8_t l_buffer[C_PPP_MAX_FRAME_SIZE];
    size_t l_bufferIndex = 0;

    // Check for invalid protocol numbers
    if((p_protocolNumber & 0x0100) != 0) {
        return;
    }

    if((p_protocolNumber & 0x0001) != 1) {
        return;
    }

    // Check if PFC is enabled
    bool l_pfc =
        (p_client->pppContext.pfcEnabled && p_protocolNumber <= 0x00ff);

    // Write protocol number
    if(!l_pfc) {
        l_buffer[l_bufferIndex++] = p_protocolNumber >> 8;
    }

    l_buffer[l_bufferIndex++] = p_protocolNumber;

    // Write data
    memcpy(&l_buffer[l_bufferIndex], p_buffer, p_size);

    // Send frame
    hdlcSend(p_client, l_buffer, p_size + l_bufferIndex);
}
