#ifndef __PROTOCOLS_IPV4_H_INCLUDED__
#define __PROTOCOLS_IPV4_H_INCLUDED__

#include <stddef.h>
#include <stdint.h>

struct ts_ipv4Context {
    uint32_t address;
};

void ipv4Init(void);
void ipv4InitClient(struct ts_client *p_client);
void ipv4ToString(char *p_buffer, uint32_t p_address);
uint32_t ipv4Allocate(void);
void ipv4Free(uint32_t p_address);
void ipv4Receive(uint8_t *p_buffer, size_t p_size);
void ipv4Send(uint8_t *p_buffer, size_t p_size);

#endif
