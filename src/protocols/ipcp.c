#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <client.h>

#include <protocols/ipcp.h>
#include <protocols/ipv4.h>
#include <protocols/lcp.h>
#include <protocols/ppp.h>

// IPCP uses the same format as LCP, therefore the LCP structures will be used.

void ipcp_sendFrame(client_t *client, uint8_t code, uint8_t identifier, const uint8_t *buffer, size_t size) {
    uint8_t bufferEx[size + 4];
    memcpy(bufferEx + 4, buffer, size);
    bufferEx[0] = code;
    bufferEx[1] = identifier;
    *(uint16_t *)&bufferEx[2] = htons((uint16_t)(size + 4));
    ppp_sendFrameEx(client, 0x8021, bufferEx, size + 4, false, false);
}

static inline void ipcp_configurationRequestFrameReceived_replaceIP(uint8_t optionType, uint32_t ip, uint8_t *buffer, size_t size) {
    size_t i = 0;

    while(i < size) {
        lcp_option_t *option = (lcp_option_t *)&buffer[i];

        if(option->type == optionType) {
            option->data[0] = (ip >> 24) & 0xff;
            option->data[1] = (ip >> 16) & 0xff;
            option->data[2] = (ip >> 8) & 0xff;
            option->data[3] = ip & 0xff;
            break;
        } else {
            i += option->length;
        }
    }
}

void ipcp_configurationRequestFrameReceived(client_t *client, uint8_t identifier, const uint8_t *buffer, size_t size) {
    size_t i = 0;
    
    uint8_t nakBuffer[client->mru];
    size_t nakBufferSize = 0;
    uint8_t rejectBuffer[client->mru];
    size_t rejectBufferSize = 0;
    uint8_t ackBuffer[client->mru];
    size_t ackBufferSize = 0;

    bool provideIP = false;
    bool providePrimaryDNS = false;
    bool providePrimaryNBNS = false;
    bool provideSecondaryDNS = false;
    bool provideSecondaryNBNS = false;

    while(i < size) {
        lcp_option_t *option = (lcp_option_t *)&buffer[i];

        printf("Found PPP IPCP option number %d\n", option->type);
        int result = PPP_LCP_CONFIGURATION_ACK;

        switch(option->type) {
            case IPCP_OPTION_IP_ADDRESS:
                {
                    uint32_t ip = (option->data[0] << 24) | (option->data[1] << 16) | (option->data[2] << 8) | option->data[3];

                    if(ip == 0) {
                        result = PPP_LCP_CONFIGURATION_NAK;
                        provideIP = true;
                    } else {
                        if((ip & 0xffffff00) != 0xc0a83200) {
                            result = PPP_LCP_CONFIGURATION_NAK;
                        }
                    }
                }

                break;
            
            case IPCP_OPTION_PRIMARY_DNS:
                {
                    uint32_t ip = (option->data[0] << 24) | (option->data[1] << 16) | (option->data[2] << 8) | option->data[3];

                    if(ip != ipv4_getLocalIP()) {
                        result = PPP_LCP_CONFIGURATION_NAK;
                        providePrimaryDNS = true;
                    }
                }

                break;
            
            case IPCP_OPTION_PRIMARY_NBNS:
                {
                    uint32_t ip = (option->data[0] << 24) | (option->data[1] << 16) | (option->data[2] << 8) | option->data[3];

                    if(ip != ipv4_getLocalIP()) {
                        result = PPP_LCP_CONFIGURATION_NAK;
                        providePrimaryNBNS = true;
                    }
                }

                break;
            
            case IPCP_OPTION_SECONDARY_DNS:
                {
                    uint32_t ip = (option->data[0] << 24) | (option->data[1] << 16) | (option->data[2] << 8) | option->data[3];

                    if(ip != ipv4_getLocalIP()) {
                        result = PPP_LCP_CONFIGURATION_NAK;
                        provideSecondaryDNS = true;
                    }
                }

                break;
            
            case IPCP_OPTION_SECONDARY_NBNS:
                {
                    uint32_t ip = (option->data[0] << 24) | (option->data[1] << 16) | (option->data[2] << 8) | option->data[3];

                    if(ip != ipv4_getLocalIP()) {
                        result = PPP_LCP_CONFIGURATION_NAK;
                        provideSecondaryNBNS = true;
                    }
                }

                break;

            default:
                printf("Received PPP IPCP configuration request frame with unknown type value %02x\n", option->type);
                result = PPP_LCP_CONFIGURATION_REJECT;
                break;
        }

        uint8_t *buffer = NULL;
        size_t *bufferSize = NULL;
        
        if(result == PPP_LCP_CONFIGURATION_ACK) {
            buffer = ackBuffer;
            bufferSize = &ackBufferSize;
        } else if(result == PPP_LCP_CONFIGURATION_NAK) {
            buffer = nakBuffer;
            bufferSize = &nakBufferSize;
        } else if(result == PPP_LCP_CONFIGURATION_REJECT) {
            buffer = rejectBuffer;
            bufferSize = &rejectBufferSize;
        }
        
        memcpy(buffer + *bufferSize, option, option->length);
        *bufferSize += option->length;

        i += option->length;
    }

    // Send response
    if(rejectBufferSize > 0) {
        ipcp_sendFrame(client, PPP_LCP_CONFIGURATION_REJECT, identifier, rejectBuffer, rejectBufferSize);
        printf("Rejected configuration request\n");
    } else if(nakBufferSize > 0) {
        if(provideIP) {
            printf("Remote end asked for IP address.\n");
            ipcp_configurationRequestFrameReceived_replaceIP(IPCP_OPTION_IP_ADDRESS, client->ipv4, nakBuffer, nakBufferSize);
        }

        if(providePrimaryDNS) {
            printf("Remote end asked for primary DNS.\n");
            ipcp_configurationRequestFrameReceived_replaceIP(IPCP_OPTION_PRIMARY_DNS, ipv4_getLocalIP(), nakBuffer, nakBufferSize);
        }

        if(providePrimaryNBNS) {
            printf("Remote end asked for primary NBNS.\n");
            ipcp_configurationRequestFrameReceived_replaceIP(IPCP_OPTION_PRIMARY_NBNS, ipv4_getLocalIP(), nakBuffer, nakBufferSize);
        }

        if(provideSecondaryDNS) {
            printf("Remote end asked for secondary DNS.\n");
            ipcp_configurationRequestFrameReceived_replaceIP(IPCP_OPTION_SECONDARY_DNS, ipv4_getLocalIP(), nakBuffer, nakBufferSize);
        }

        if(provideSecondaryNBNS) {
            printf("Remote end asked for secondary NBNS.\n");
            ipcp_configurationRequestFrameReceived_replaceIP(IPCP_OPTION_SECONDARY_NBNS, ipv4_getLocalIP(), nakBuffer, nakBufferSize);
        }

        ipcp_sendFrame(client, PPP_LCP_CONFIGURATION_NAK, identifier, nakBuffer, nakBufferSize);
        printf("Nack-ed configuration request\n");
    } else if(ackBufferSize > 0) {
        ipcp_sendFrame(client, PPP_LCP_CONFIGURATION_ACK, identifier, ackBuffer, ackBufferSize);
        printf("Acknowledged configuration request\n");
        
        // Send config request response
        uint32_t localIP = ipv4_getLocalIP();

        uint8_t buffer[6] = {
            0x03,
            0x06,
            (localIP >> 24) & 0xff,
            (localIP >> 16) & 0xff,
            (localIP >> 8) & 0xff,
            localIP & 0xff,
        };

        ipcp_sendFrame(client, PPP_LCP_CONFIGURATION_REQUEST, client->currentIdentifier++, buffer, 6);
    }
}

void ipcp_frameReceived(client_t *client, const uint8_t *buffer, size_t size) {
    lcp_header_t *ipcp_header = (lcp_header_t *)buffer;

    switch(ipcp_header->code) {
        case PPP_LCP_CONFIGURATION_REQUEST:
            ipcp_configurationRequestFrameReceived(client, ipcp_header->identifier, buffer + 4, size - 4);
            break;
        case PPP_LCP_CONFIGURATION_ACK:
            printf("The remote device has accepted IPCP configuration.\n");
            break;
        case PPP_LCP_CONFIGURATION_NAK:
            printf("The remote device has refused IPCP configuration.\n");
            break;
        case PPP_LCP_CONFIGURATION_REJECT:
            printf("The remote device has rejected IPCP configuration.\n");
            break;
        case PPP_LCP_TERMINATE_REQUEST:
            printf("The remote device has asked for link termination.\n");
            ipcp_sendFrame(client, PPP_LCP_TERMINATE_ACK, ipcp_header->identifier, buffer + 4, size - 4);
            break;
        case PPP_LCP_TERMINATE_ACK:
            printf("The remote device has confirmed the link termination.\n");
            break;
        case PPP_LCP_CODE_REJECT:
            printf("The remote device has rejected the IPCP code.\n");
            break;
        default:
            printf("Received PPP IPCP frame with unknown code value %02x\n", ipcp_header->code);
            break;
    }
}
