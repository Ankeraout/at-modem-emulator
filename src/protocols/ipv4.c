#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <protocols/ipv4.h>

uint8_t *ipv4_bitmap;
uint32_t ipv4_networkAddress;
uint32_t ipv4_subnetworkMask;
uint32_t ipv4_local;

int ipv4_init(uint32_t address, uint32_t subnetMask) {
    // Check the subnetwork mask
    int zeros = 0;
    
    for(int i = 0; i < 32; i++) {
        uint32_t bit = (subnetMask >> (31 - i)) & 0x00000001;
        
        if(bit) {
            if(zeros) {
                fprintf(stderr, "ipv4_init(): invalid subnetwork mask.\n");
                return 1;
            }
        } else {
            zeros++;
        }
    }

    if(zeros < 2) {
        fprintf(stderr, "ipv4_init(): invalid subnetwork mask: expected a network of size /30 at least.\n");
        return 1;
    }

    // Check the network address
    if((address & subnetMask) != address) {
        fprintf(stderr, "ip_init(): invalid network address.\n");
        return 1;
    }

    int ipCount = 1 << zeros;

    // 8 IPs per byte
    ipv4_bitmap = malloc((ipCount + 7) >> 3);

    if(!ipv4_bitmap) {
        perror("ipv4_init(): malloc() failed");
        return 1;
    }

    memset(ipv4_bitmap, 0, (ipCount + 7) >> 3);

    ipv4_networkAddress = address;
    ipv4_subnetworkMask = subnetMask;
    ipv4_local = ipv4_alloc();

    char buffer[16];
    ipv4_toString(buffer, ipv4_local);

    printf("ipv4_init(): local IPv4: %s\n", buffer);

    return 0;
}

uint32_t ipv4_alloc() {
    uint32_t broadcastAddress = (ipv4_networkAddress | ~ipv4_subnetworkMask);
    uint32_t currentAddressIndex = 1;

    while(ipv4_networkAddress + currentAddressIndex < broadcastAddress) {
        uint8_t currentByte = ipv4_bitmap[currentAddressIndex >> 3];
        uint8_t currentBit = (currentByte >> (7 - (currentAddressIndex & 0x07))) & 0x01;

        if(!currentBit) {
            currentByte |= 1 << (7 - (currentAddressIndex & 0x07));
            ipv4_bitmap[currentAddressIndex >> 3] = currentByte;

            return ipv4_networkAddress + currentAddressIndex;
        }

        currentAddressIndex++;
    }

    return 0;
}

void ipv4_free(uint32_t ip) {
    // Check that the IP is in the network
    if((ip & ipv4_subnetworkMask) != ipv4_networkAddress) {
        fprintf(stderr, "ipv4_free(): wrong IP address.\n");
    }

    // Compute the IP index
    uint32_t currentAddressIndex = ip - ipv4_networkAddress;
    uint8_t currentByte = ipv4_bitmap[currentAddressIndex >> 3];
    currentByte &= ~(1 << (7 - (currentAddressIndex & 0x07)));
    ipv4_bitmap[currentAddressIndex >> 3] = currentByte;
}

uint32_t ipv4_getLocalIP() {
    return ipv4_local;
}

int ipv4_toString(char *buffer, uint32_t ip) {
    return sprintf(buffer, "%d.%d.%d.%d", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
}
