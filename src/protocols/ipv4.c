#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
    p_client->ipv4Context.address = ipv4Allocate();
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

uint32_t ipv4Allocate(void) {
    for(int l_byteIndex = 0; l_byteIndex < 32; l_byteIndex++) {
        uint8_t l_mask = 0x80;

        for(int l_shift = 0; l_shift < 8; l_shift++) {
            if((s_networkBitmap[l_byteIndex] & l_mask) == 0) {
                s_networkBitmap[l_byteIndex] ^= l_mask;

                return C_IPV4_NETWORK_ADDRESS | (l_byteIndex << 3) | l_shift;
            } else {
                l_mask >>= 1;
            }
        }
    }

    return 0x00000000;
}

void ipv4Free(uint32_t p_address) {
    if((p_address & 0xffffff00) == C_IPV4_NETWORK_ADDRESS) {
        int l_byteIndex = (p_address & 0x000000f8) >> 3;
        int l_shift = p_address & 0x00000007;

        s_networkBitmap[l_byteIndex] ^= 0x80 >> l_shift;
    }
}

void ipv4Receive(uint8_t *p_buffer, size_t p_size) {
    // Remove warnings temporarily
    (void)p_buffer;
    (void)p_size;
}

void ipv4Send(uint8_t *p_buffer, size_t p_size) {
    // Remove warnings temporarily
    (void)p_buffer;
    (void)p_size;
}
