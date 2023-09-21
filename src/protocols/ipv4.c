#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>

#include "client.h"

#define C_IPV4_NETWORK_ADDRESS 0xc0a83200

static uint8_t s_networkBitmap[32];

void ipv4Init(void) {
    memset(s_networkBitmap, 0, sizeof(s_networkBitmap));

    // Network, broadcast and first address (gateway) are reserved
    s_networkBitmap[0] = 0xc0;
    s_networkBitmap[31] = 0x01;
}

void ipv4InitClient(struct ts_client *p_client) {
    p_client->ipv4Context.address = 0;
}

void ipv4ToString(char *p_buffer, uint32_t p_address) {
    sprintf(
        p_buffer, "%d.%d.%d.%d",
        p_address >> 24,
        (p_address >> 16) & 0xff,
        (p_address >> 8) & 0xff,
        p_address & 0xff
    );
}

void ipv4Allocate(struct ts_client *p_client) {
    uint32_t l_address = p_client->ipv4Context.address;

    if(l_address == 0) {
        for(int l_byteIndex = 0; l_byteIndex < 32; l_byteIndex++) {
            uint8_t l_mask = 0x80;

            for(int l_shift = 0; l_shift < 8; l_shift++) {
                if((s_networkBitmap[l_byteIndex] & l_mask) == 0) {
                    s_networkBitmap[l_byteIndex] ^= l_mask;

                    p_client->ipv4Context.address =
                        C_IPV4_NETWORK_ADDRESS | (l_byteIndex << 3) | l_shift;

                    char l_buffer[16];

                    ipv4ToString(l_buffer, p_client->ipv4Context.address);

                    printf(
                        "ipv4: Allocated address %s for client #%d\n",
                        l_buffer,
                        p_client->id
                    );

                    return;
                } else {
                    l_mask >>= 1;
                }
            }
        }
    }
}

void ipv4Free(struct ts_client *p_client) {
    uint32_t l_address = p_client->ipv4Context.address;

    if(l_address != 0) {
        int l_byteIndex = (l_address & 0x000000f8) >> 3;
        int l_shift = l_address & 0x00000007;

        s_networkBitmap[l_byteIndex] ^= 0x80 >> l_shift;

        p_client->ipv4Context.address = 0;

        char l_buffer[16];

        ipv4ToString(l_buffer, l_address);

        printf(
            "ipv4: Freed address %s from client #%d\n",
            l_buffer,
            p_client->id
        );
    }
}

void ipv4Receive(struct ts_client *p_client, const uint8_t *p_buffer, size_t p_size) {
    if(!p_client->ipv4Context.isConfigured) {
        return;
    }

    printf("ipv4: Received packet from client #%d\n", p_client->id);
}

void ipv4Send(struct ts_client *p_client, uint8_t *p_buffer, size_t p_size) {
    if(!p_client->ipv4Context.isConfigured) {
        return;
    }

    pppSend(p_client, C_PPP_PROTOCOLNUMBER_IPV4, p_buffer, p_size);
}

void ipv4GetGatewayAddress(void *p_buffer) {
    *(uint32_t *)p_buffer = htonl(C_IPV4_NETWORK_ADDRESS + 1);
}
