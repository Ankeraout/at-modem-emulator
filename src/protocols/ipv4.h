#ifndef __PROTOCOLS_IPV4_H_INCLUDED__
#define __PROTOCOLS_IPV4_H_INCLUDED__

#include <stddef.h>
#include <stdint.h>

struct ts_ipv4Context {
    int dummy;
};

void ipv4Receive(uint8_t *p_buffer, size_t p_size);
void ipv4Send(uint8_t *p_buffer, size_t p_size);

#endif
