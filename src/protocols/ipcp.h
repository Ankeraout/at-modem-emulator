#ifndef __IPCP_H__
#define __IPCP_H__

#include <stddef.h>
#include <stdint.h>

#include <client.h>

enum {
    IPCP_OPTION_IP_ADDRESSES = 1,
    IPCP_OPTION_IP_COMPRESSION_PROTOCOL = 2,
    IPCP_OPTION_IP_ADDRESS = 3,
    IPCP_OPTION_PRIMARY_DNS = 129,
    IPCP_OPTION_PRIMARY_NBNS = 130,
    IPCP_OPTION_SECONDARY_DNS = 131,
    IPCP_OPTION_SECONDARY_NBNS = 132
};

void ipcp_frameReceived(client_t *client, const uint8_t *buffer, size_t size);

#endif
