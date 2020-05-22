#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <client.h>
#include <protocols/lcp.h>
#include <protocols/ppp.h>

void lcp_sendFrame(client_t *client, uint8_t code, uint8_t identifier, uint8_t *buffer, size_t bufferSize) {
    uint8_t bufferEx[bufferSize + 4];
    memcpy(bufferEx + 4, buffer, bufferSize);
    bufferEx[0] = code;
    bufferEx[1] = identifier;
    *(uint16_t *)&bufferEx[2] = htons((uint16_t)(bufferSize + 4));
    ppp_sendFrameEx(client, 0xc021, bufferEx, bufferSize + 4, false, false);
}

void lcp_configurationRequestFrameReceived(client_t *client, uint8_t identifier, uint8_t *payloadBuffer, size_t payloadSize) {
    size_t i = 0;
    
    uint8_t nakBuffer[client->mru];
    size_t nakBufferSize = 0;
    uint8_t rejectBuffer[client->mru];
    size_t rejectBufferSize = 0;
    uint8_t ackBuffer[client->mru];
    size_t ackBufferSize = 0;

    while(i < payloadSize) {
        lcp_option_t *option = (lcp_option_t *)&payloadBuffer[i];

        printf("Found PPP LCP option number %d\n", option->type);
        int result = PPP_LCP_CONFIGURATION_ACK;

        switch(option->type) {
            case PPP_LCP_OPTION_MRU:
                client->mru = ntohs(*(uint16_t *)option->data);
                break;

            case PPP_LCP_OPTION_ACCM: {
                int characterCode = ((payloadSize - 2) << 3) - 1;

                for(int j = 0; j < (int)payloadSize - 2; j++) {
                    int characterCodeByte = payloadBuffer[j + 2];

                    for(int k = 0; k < 8; k++) {
                        int characterCodeBit = (characterCodeByte >> (7 - k)) & 0x01;
                        client->accm[characterCode--] = characterCodeBit;
                    }
                }

                client->accm[0x7d] = true;
                client->accm[0x7e] = true;

                break;
            }

            case PPP_LCP_OPTION_MAGIC_NUMBER:
                break;

            case PPP_LCP_OPTION_PFC:
                client->pfc = true;
                break;

            case PPP_LCP_OPTION_ACFC:
                client->acfc = true;
                break;

            default:
                printf("Received PPP LCP configuration request frame with unknown type value %02x\n", option->type);
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
        lcp_sendFrame(client, PPP_LCP_CONFIGURATION_REJECT, identifier, rejectBuffer, rejectBufferSize);
        printf("Rejected configuration request\n");
    } else if(nakBufferSize > 0) {
        lcp_sendFrame(client, PPP_LCP_CONFIGURATION_NAK, identifier, nakBuffer, nakBufferSize);
        printf("Nack-ed configuration request\n");
    } else if(ackBufferSize > 0) {
        lcp_sendFrame(client, PPP_LCP_CONFIGURATION_ACK, identifier, ackBuffer, ackBufferSize);
        printf("Acknowledged configuration request\n");
        
        // Send config request response
        lcp_sendFrame(client, PPP_LCP_CONFIGURATION_REQUEST, identifier + 1, (uint8_t *)"\x02\x06\xff\xff\xff\xff\x05\x06\x19\x10\x77\xBE\x07\x02\x08\x02", 16);
    }
}

void lcp_frameReceived(client_t *client, uint8_t *lcpFrameBuffer, size_t lcpFrameSize) {
    lcp_header_t *lcp_header = (lcp_header_t *)lcpFrameBuffer;

    switch(lcp_header->code) {
        case PPP_LCP_CONFIGURATION_REQUEST:
            lcp_configurationRequestFrameReceived(client, lcp_header->identifier, lcpFrameBuffer + 4, lcpFrameSize - 4);
            break;
        case PPP_LCP_CONFIGURATION_ACK:
            printf("The remote device has accepted LCP configuration.\n");
            break;
        case PPP_LCP_CONFIGURATION_NAK:
            printf("The remote device has refused LCP configuration.\n");
            break;
        case PPP_LCP_CONFIGURATION_REJECT:
            printf("The remote device has rejected LCP configuration.\n");
            break;
        case PPP_LCP_TERMINATE_REQUEST:
            printf("The remote device has asked for link termination.\n");
            lcp_sendFrame(client, PPP_LCP_TERMINATE_ACK, lcp_header->identifier, lcpFrameBuffer + 4, lcpFrameSize - 4);
            break;
        case PPP_LCP_TERMINATE_ACK:
            printf("The remote device has confirmed the link termination.\n");
            break;
        case PPP_LCP_CODE_REJECT:
            printf("The remote device has rejected the code.\n");
            break;
        case PPP_LCP_PROTOCOL_REJECT:
            printf("The remote device has rejected protocol.\n");
            break;
        case PPP_LCP_ECHO_REQUEST:
            printf("The remote device has sent an echo request.\n");
            lcp_sendFrame(client, PPP_LCP_ECHO_REPLY, lcp_header->identifier, (uint8_t *)"\x19\x10\x77\xBE", 4);
            break;
        case PPP_LCP_ECHO_REPLY:
            printf("The remote device has sent an echo reply.\n");
            break;
        case PPP_LCP_DISCARD_REQUEST:
            printf("The remote device has sent a discard request.\n");
            break;
        default:
            printf("Received PPP LCP frame with unknown code value %02x\n", lcp_header->code);
            break;
    }
}

void lcp_rejectFrame(client_t *client, uint16_t protocol, const uint8_t *buffer, size_t size) {
    uint8_t bufferEx[size - 2];
    bufferEx[0] = protocol >> 8;
    bufferEx[1] = protocol & 0xff;
    memcpy(bufferEx + 2, buffer, size - 4);
    lcp_sendFrame(client, PPP_LCP_PROTOCOL_REJECT, client->currentIdentifier++, bufferEx, size - 2);
}
