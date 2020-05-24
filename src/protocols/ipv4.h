#ifndef __IPV4_H__
#define __IPV4_H__

#include <stddef.h>
#include <stdint.h>

#include <client.h>

int ipv4_init(uint32_t address, uint32_t subnetMask, int tunDeviceFd);
int ipv4_alloc(client_t *client);
void ipv4_free(uint32_t ip);
uint32_t ipv4_getLocalIP();
int ipv4_toString(char *buffer, uint32_t ip);
void ipv4_frameReceived(client_t *client, const uint8_t *buffer, size_t size);
void ipv4_tunFrameReceived(const uint8_t *buffer, size_t size);

#endif
