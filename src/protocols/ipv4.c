#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <client.h>
#include <protocols/ipv4.h>
#include <protocols/ppp.h>

uint8_t *ipv4_bitmap;
uint32_t ipv4_networkAddress;
uint32_t ipv4_subnetworkMask;
uint32_t ipv4_local;
uint32_t ipv4_broadcast;
int ipv4_tunDeviceFd;

client_t *clientList[256];

uint32_t ipv4_allocInt() {
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

int ipv4_init(uint32_t address, uint32_t subnetMask, int tunDeviceFd) {
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
    ipv4_broadcast = address | ~subnetMask;
    ipv4_local = ipv4_allocInt();

    char buffer[16];
    ipv4_toString(buffer, ipv4_local);

    printf("ipv4_init(): local IPv4: %s\n", buffer);

    ipv4_tunDeviceFd = tunDeviceFd;

    return 0;
}

int ipv4_alloc(client_t *client) {
    uint32_t ip = ipv4_allocInt();

    if(ip == 0) {
        return 1;
    }

    client->ipv4 = ip;

    int clientIndex = ip & ~ipv4_subnetworkMask;
    clientList[clientIndex] = client;

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

    clientList[currentAddressIndex] = NULL;
}

uint32_t ipv4_getLocalIP() {
    return ipv4_local;
}

int ipv4_toString(char *buffer, uint32_t ip) {
    return sprintf(buffer, "%d.%d.%d.%d", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
}

void ipv4_frameReceived(client_t *client, const uint8_t *buffer, size_t size) {
    uint8_t bufferEx[size + 4];
    bufferEx[0] = 0x00;
    bufferEx[1] = 0x00;
    bufferEx[2] = 0x08;
    bufferEx[3] = 0x00;

    memcpy(bufferEx + 4, buffer, size);

    write(ipv4_tunDeviceFd, bufferEx, size + 4);
}

void ipv4_tunFrameReceived(const uint8_t *buffer, size_t size) {
    // Get the destination IP address
    uint32_t sourceIP = (buffer[12] << 24) | (buffer[13] << 16) | (buffer[14] << 8) | buffer[15];
    uint32_t destinationIP = (buffer[16] << 24) | (buffer[17] << 16) | (buffer[18] << 8) | buffer[19];

    // If the destination is not in the subnetwork, ignore the packet
    if((destinationIP & ipv4_subnetworkMask) != ipv4_networkAddress) {
        return;
    }

    // If the destination is the network address, discard the frame
    if(destinationIP == ipv4_networkAddress) {
        return;
    }

    // If the destination is the broadcast address, forward the frame to all
    // clients
    if(destinationIP == ipv4_broadcast) {
        for(uint32_t currentAddress = ipv4_local; currentAddress < ipv4_broadcast; currentAddress++) {
            if(currentAddress != sourceIP) {
                // Determine the index of the client in the client list
                int clientIndex = currentAddress & ~ipv4_subnetworkMask;

                if(clientList[clientIndex]) {
                    ppp_sendFrame(clientList[clientIndex], PPP_PROTOCOL_IPV4, buffer, size);
                }
            }
        }
    } else {
        // Determine the index of the client in the client list
        int clientIndex = destinationIP & ~ipv4_subnetworkMask;

        // Forward the frame to the client, if it exists
        if(clientList[clientIndex]) {
            ppp_sendFrame(clientList[clientIndex], PPP_PROTOCOL_IPV4, buffer, size);
        }
    }
}
