#ifndef __IPV4_H__
#define __IPV4_H__

#include <stdint.h>

int ipv4_init(uint32_t address, uint32_t subnetMask);
uint32_t ipv4_alloc();
void ipv4_free(uint32_t ip);
uint32_t ipv4_getLocalIP();
int ipv4_toString(char *buffer, uint32_t ip);

#endif
