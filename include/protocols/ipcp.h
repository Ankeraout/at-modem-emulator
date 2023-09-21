#ifndef __PROTOCOLS_IPCP_H_INCLUDED__
#define __PROTOCOLS_IPCP_H_INCLUDED__

#include "client.h"
#include "protocols/lcp.h"

enum te_ipcpType {
    E_IPCP_TYPE_IP_ADDRESSES = 1,
    E_IPCP_TYPE_IP_COMPRESSION_PROTOCOL,
    E_IPCP_TYPE_IP_ADDRESS,
    E_IPCP_TYPE_DNS1 = 129,
    E_IPCP_TYPE_NBNS1,
    E_IPCP_TYPE_DNS2,
    E_IPCP_TYPE_NBNS2
};

void ipcpReceive(
    struct ts_client *p_client,
    const uint8_t *p_buffer,
    size_t p_size
);

#endif
