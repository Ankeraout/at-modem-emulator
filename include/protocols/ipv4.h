#ifndef __PROTOCOLS_IPV4_H_INCLUDED__
#define __PROTOCOLS_IPV4_H_INCLUDED__

#include <stddef.h>
#include <stdint.h>

struct ts_ipv4Context {
    uint32_t address;
    bool isConfigured;
};

void ipv4Init(void);
void ipv4InitClient(struct ts_client *p_client);
void ipv4ToString(char *p_buffer, uint32_t p_address);
void ipv4Allocate(struct ts_client *p_client);
void ipv4Free(struct ts_client *p_client);
void ipv4Receive(struct ts_client *p_client, const uint8_t *p_buffer, size_t p_size);
void ipv4Send(struct ts_client *p_client, uint8_t *p_buffer, size_t p_size);
void ipv4GetGatewayAddress(void *p_buffer);

#endif
