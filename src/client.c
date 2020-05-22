#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>

#include <client.h>
#include <fcs.h>
#include <ppp.h>

#define PPP_MAX_FRAME_SIZE 2048
#define READ_BUFFER_SIZE 1500
#define CLIENT_COMMAND_BUFFER_SIZE 80

void *client_thread_main(void *arg);

static const char *resultCodesStr[] = {
    "OK",
    "CONNECT",
    "RING",
    "NO CARRIER",
    "ERROR",
    "NO DIALTONE",
    "BUSY",
    "NO ANSWER"
};

enum {
    RESULTCODE_OK,
    RESULTCODE_CONNECT,
    RESULTCODE_RING,
    RESULTCODE_NO_CARRIER,
    RESULTCODE_ERROR,
    RESULTCODE_NO_DIALTONE,
    RESULTCODE_BUSY,
    RESULTCODE_NO_ANSWER
};

void reset(client_t *client) {
    client->verbose = true;
    client->echo = true;
    client->quiet = false;
    client->mode = CLIENTMODE_COMMAND;
}

void ppp_reset(client_t *client) {
    client->mru = PPP_MAX_FRAME_SIZE;

    for(int i = 0; i < 32; i++) {
        client->accm[i] = true;
    }

    for(int i = 32; i < 256; i++) {
        client->accm[i] = false;
    }

    client->accm[0x7d] = true;
    client->accm[0x7e] = true;
    client->pfc = false;
    client->currentIdentifier = 0;
}

int client_init(client_t *client, int fd, const struct sockaddr *socketAddress, socklen_t socketAddressLength) {
    client->fd = fd;
    memcpy(&client->socketAddress, socketAddress, sizeof(struct sockaddr));
    client->socketAddressLength = socketAddressLength;

    pthread_mutexattr_t mutexAttributes;
    
    if(pthread_mutexattr_init(&mutexAttributes)) {
        return 1;
    }

    if(pthread_mutex_init(&client->mutex, &mutexAttributes)) {
        pthread_mutexattr_destroy(&mutexAttributes);
        return 1;
    }

    pthread_mutexattr_destroy(&mutexAttributes);

    pthread_attr_t threadAttributes;

    if(pthread_attr_init(&threadAttributes)) {
        pthread_mutex_destroy(&client->mutex);
        return 1;
    }

    if(pthread_create(&client->readerThread, &threadAttributes, client_thread_main, client)) {
        pthread_mutex_destroy(&client->mutex);
        pthread_attr_destroy(&threadAttributes);
        return 1;
    }
    
    pthread_attr_destroy(&threadAttributes);

    reset(client);
    ppp_reset(client);

    return 0;
}

void sendResult(client_t *client, int resultCode) {
    char buffer[50];
    int size;

    if(client->verbose) {
        size = sprintf(buffer, "\r\n%s\r\n", resultCodesStr[resultCode]);
    } else {
        size = sprintf(buffer, "%d\r", resultCode);
    }

    write(client->fd, buffer, size);
}

int parseBooleanParameter(char arg, bool *output) {
    if(arg == '0') {
        *output = false;
    } else if(arg == '1') {
        *output = true;
    } else {
        return 1;
    }

    return 0;
}

void executeCommandLine(client_t *client, const char *commandLine) {
    printf("Execute command '%s'\n", commandLine);

    bool sendResultCode = true;
    bool keepExecuting = true;
    bool resultOk = true;

    int index = 0;

    while(keepExecuting) {
        char command = commandLine[index++];

        if(!command) {
            keepExecuting = false;
        } else {
            switch(command) {
                case 'D':
                    keepExecuting = false;
                    sendResultCode = false;
                    client->mode = CLIENTMODE_DATA;
                    sendResult(client, RESULTCODE_CONNECT);
                    break;

                case 'E':
                    if(parseBooleanParameter(commandLine[index++], &client->echo)) {
                        keepExecuting = false;
                        resultOk = false;
                    }

                    break;

                case 'Q':
                    if(parseBooleanParameter(commandLine[index++], &client->quiet)) {
                        keepExecuting = false;
                        resultOk = false;
                    }
                    
                    break;

                case 'V':
                    if(parseBooleanParameter(commandLine[index++], &client->verbose)) {
                        keepExecuting = false;
                        resultOk = false;
                    }
                    
                    break;

                case 'Z':
                    keepExecuting = false;
                    reset(client);
                    break;

                default:
                    keepExecuting = false;
                    resultOk = false;
                    break;
            }

            if(!resultOk) {
                keepExecuting = false;
            }
        }
    }

    if(sendResultCode) {
        if(resultOk) {
            sendResult(client, RESULTCODE_OK);
        } else {
            sendResult(client, RESULTCODE_ERROR);
        }
    }
}

void ppp_sendFrameSerial(client_t *client, uint8_t *buffer, size_t bufferSize) {
    printf("OUTGOING\n7e ");

    for(size_t i = 0; i < bufferSize; i++) {
        printf("%02x ", buffer[i]);

        if(((i + 1) % 16) == 0) {
            printf("\n");
        }
    }

    if(((bufferSize + 1) % 16) == 0) {
        printf("\n7e\n");
    } else {
        printf("7e\n");
    }

    uint8_t bufferEx[bufferSize * 2];
    size_t bufferExSize = 0;
    
    bufferEx[bufferExSize++] = 0x7e;

    for(size_t i = 0; i < bufferSize; i++) {
        uint8_t currentByte = buffer[i];

        if(currentByte < 0x20 || currentByte == 0x7d || currentByte == 0x7e) {
            bufferEx[bufferExSize++] = 0x7d;
            bufferEx[bufferExSize++] = currentByte ^ 0x20;
        } else {
            bufferEx[bufferExSize++] = currentByte;
        }
    }

    bufferEx[bufferExSize++] = 0x7e;

    write(client->fd, bufferEx, bufferExSize);
}

void ppp_sendFrameEx(client_t *client, uint16_t protocol, uint8_t *buffer, size_t bufferSize, bool acfc, bool pfc) {
    size_t bufferExSize = bufferSize + 6;

    if(acfc) {
        bufferExSize -= 2;
    }

    pfc &= (protocol <= 0xff);

    if(pfc) {
        bufferExSize -= 1;
    }

    uint8_t bufferEx[bufferExSize];
    
    size_t index = 0;

    if(!acfc) {
        bufferEx[index++] = 0xff;
        bufferEx[index++] = 0x03;
    }

    if(!pfc) {
        bufferEx[index++] = (uint8_t)(protocol >> 8);
    }

    bufferEx[index++] = protocol & 0xff;

    memcpy(bufferEx + index, buffer, bufferSize);

    index += bufferSize;

    uint16_t fcs = pppfcs16(bufferEx, bufferExSize - 2);

    bufferEx[index++] = fcs & 0xff;
    bufferEx[index++] = fcs >> 8;

    ppp_sendFrameSerial(client, bufferEx, bufferExSize);
}

void ppp_sendFrame(client_t *client, uint16_t protocol, uint8_t *buffer, size_t bufferSize) {
    ppp_sendFrameEx(client, protocol, buffer, bufferSize, client->acfc, client->pfc);
}

void ppp_lcpSendFrame(client_t *client, uint8_t code, uint8_t identifier, uint8_t *buffer, size_t bufferSize) {
    uint8_t bufferEx[bufferSize + 4];
    memcpy(bufferEx + 4, buffer, bufferSize);
    bufferEx[0] = code;
    bufferEx[1] = identifier;
    *(uint16_t *)&bufferEx[2] = htons((uint16_t)(bufferSize + 4));
    ppp_sendFrameEx(client, 0xc021, bufferEx, bufferSize + 4, false, false);
}

void ppp_lcp_configurationRequestFrameReceived(client_t *client, uint8_t identifier, uint8_t *payloadBuffer, size_t payloadSize) {
    size_t i = 0;
    
    uint8_t nakBuffer[client->mru];
    size_t nakBufferSize = 0;
    uint8_t rejectBuffer[client->mru];
    size_t rejectBufferSize = 0;
    uint8_t ackBuffer[client->mru];
    size_t ackBufferSize = 0;

    bool sendMagic = false;

    while(i < payloadSize) {
        ppp_lcp_option_t *option = (ppp_lcp_option_t *)&payloadBuffer[i];

        printf("Found PPP LCP option number %d\n", option->type);
        int result = 0; // 0 = Ack, 1 = Nak, 2 = Reject

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
                sendMagic = true;
                break;

            case PPP_LCP_OPTION_PFC:
                client->pfc = true;
                break;

            case PPP_LCP_OPTION_ACFC:
                client->acfc = true;
                break;

            default:
                printf("Received PPP LCP configuration request frame with unknown type value %02x\n", option->type);
                result = 2;
                break;
        }

        uint8_t *buffer = NULL;
        size_t *bufferSize = NULL;
        
        if(result == 0) {
            buffer = ackBuffer;
            bufferSize = &ackBufferSize;
        } else if(result == 1) {
            buffer = nakBuffer;
            bufferSize = &nakBufferSize;
        } else if(result == 2) {
            buffer = rejectBuffer;
            bufferSize = &rejectBufferSize;
        }
        
        memcpy(buffer + *bufferSize, option, option->length);
        *bufferSize += option->length;

        i += option->length;
    }

    // Send response
    if(rejectBufferSize > 0) {
        ppp_lcpSendFrame(client, 4, identifier, rejectBuffer, rejectBufferSize);

        printf("Rejected configuration request\n");
    } else if(nakBufferSize > 0) {
        ppp_lcpSendFrame(client, 3, identifier, nakBuffer, nakBufferSize);
        printf("Nack-ed configuration request\n");
    } else if(ackBufferSize > 0) {
        ppp_lcpSendFrame(client, 2, identifier, ackBuffer, ackBufferSize);
        printf("Acknowledged configuration request\n");
        
        // Send config request response
        ppp_lcpSendFrame(client, 1, identifier + 1, (uint8_t *)"\x02\x06\xff\xff\xff\xff\x05\x06\x19\x10\x77\xBE\x07\x02\x08\x02", 16);
    }
}

void ppp_lcpFrameReceived(client_t *client, uint8_t *lcpFrameBuffer, size_t lcpFrameSize) {
    ppp_lcp_header_t *lcp_header = (ppp_lcp_header_t *)lcpFrameBuffer;

    switch(lcp_header->code) {
        case PPP_LCP_CONFIGURATION_REQUEST:
            ppp_lcp_configurationRequestFrameReceived(client, lcp_header->identifier, lcpFrameBuffer + 4, lcpFrameSize - 4);
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
            ppp_lcpSendFrame(client, PPP_LCP_TERMINATE_ACK, lcp_header->identifier, lcpFrameBuffer + 4, lcpFrameSize - 4);
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
            ppp_lcpSendFrame(client, PPP_LCP_ECHO_REPLY, lcp_header->identifier, (uint8_t *)"\x19\x10\x77\xBE", 4);
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

void pppFrameReceived(client_t *client, uint8_t *pppFrameBuffer, size_t pppFrameSize) {
    ppp_header_t *ppp_header = (ppp_header_t *)pppFrameBuffer;
    bool compressedProtocolField = false;
    bool acfc = false;
    uint8_t *protocolField;

    uint16_t expectedFcs = pppfcs16(pppFrameBuffer, pppFrameSize - 2);
    uint16_t actualFcs = pppFrameBuffer[pppFrameSize - 2] | (pppFrameBuffer[pppFrameSize - 1] << 8);

    if(actualFcs != expectedFcs) {
        printf("Wrong FCS (expected 0x%04x, got 0x%04x).\n", expectedFcs, actualFcs);
        return;
    }

    if(ppp_header->address == 0xff && ppp_header->control == 0x03) {
        protocolField = ppp_header->protocol;
    } else {
        if(client->acfc) {
            acfc = true;
            protocolField = pppFrameBuffer;
        } else {
            printf("Received PPP frame with unexpected address or command value (%02x %02x).\n", ppp_header->address, ppp_header->control);
            return;
        }
    }

    pppFrameSize -= 2;

    uint16_t protocol = protocolField[0];

    if(!(protocol & 0x01) || !client->pfc) {
        protocol = (protocol << 8) | protocolField[1];
    }

    uint8_t *dataStart = pppFrameBuffer + 4;

    if(acfc) {
        dataStart -= 2;
    }

    if(compressedProtocolField) {
        dataStart -= 1;
    }

    switch(protocol) {
        case 0xc021:
            ppp_lcpFrameReceived(client, pppFrameBuffer + 4, pppFrameSize - 4);
            break;
        default:
            printf("Received PPP frame with unknown protocol value %04x\n", protocol);

            {
                uint8_t bufferEx[pppFrameSize - 2];
                bufferEx[0] = protocol >> 8;
                bufferEx[1] = protocol & 0xff;
                memcpy(bufferEx + 2, pppFrameBuffer, pppFrameSize - 4);
                ppp_lcpSendFrame(client, PPP_LCP_PROTOCOL_REJECT, client->currentIdentifier++, bufferEx, pppFrameSize - 2);
            }
            break;
    }
}

void *client_thread_main(void *arg) {
    client_t *client = (client_t *)arg;
    char buffer[READ_BUFFER_SIZE];
    uint8_t commandBuffer[CLIENT_COMMAND_BUFFER_SIZE];
    uint8_t pppFrameBuffer[PPP_MAX_FRAME_SIZE];
    size_t pppFrameSize = 0;
    size_t commandLength = 0;

    bool readA = false;
    bool attention = false;
    bool escape = false;
    bool readingFrame = false;
    bool rejectFrame = false;

    while(true) {
        ssize_t bytesRead = read(client->fd, buffer, READ_BUFFER_SIZE);

        if(bytesRead == -1) {
            perror("read() failed");
            break;
        }
        
        if(bytesRead == 0) {
            printf("Connection closed\n");
            break;
        }

        if(client->mode == CLIENTMODE_COMMAND) {
            bool rejectCurrentCommand = false;

            for(ssize_t i = 0; i < bytesRead; i++) {
                char c = buffer[i];

                if(client->echo) {
                    if(c == '\b') {
                        write(client->fd, "\x08 \x08", 3);
                    } else {
                        write(client->fd, &buffer[i], 1);
                    }
                }

                if(commandLength >= CLIENT_COMMAND_BUFFER_SIZE) {
                    rejectCurrentCommand = true;
                } else {
                    if(attention) {
                        if(buffer[i] == '\r') {
                            commandBuffer[commandLength] = 0;
                            attention = false;
                            bool echo = client->echo;

                            if(!rejectCurrentCommand) {
                                executeCommandLine(client, (const char *)commandBuffer);
                            }

                            if(echo && !client->echo) {
                                write(client->fd, "\r", 1);
                            }
                        } else if(buffer[i] == '\x08') {
                            if(commandLength) {
                                commandLength--;

                                if(client->echo) {
                                    write(client->fd, "\x08 \x08", 3);
                                }
                            }
                        } else {
                            commandBuffer[commandLength++] = buffer[i];
                        }
                    } else {
                        if(readA && buffer[i] == 'T') {
                            attention = true;
                            commandLength = 0;
                        } else {
                            readA = buffer[i] == 'A';
                        }
                    }
                }
            }
        } else {
            for(ssize_t i = 0; i < bytesRead; i++) {
                uint8_t receivedByte = buffer[i];

                if(escape) {
                    if(pppFrameSize >= client->mru) {
                        rejectFrame = true;
                    } else {
                        pppFrameBuffer[pppFrameSize++] = receivedByte ^ 0x20;
                    }

                    escape = false;
                } else {
                    if(receivedByte == 0x7d) {
                        escape = true;
                    } else if(receivedByte == 0x7e) {
                        if(readingFrame) {
                            if(!rejectFrame) {
                                printf("INCOMING\n7e ");

                                for(size_t i = 0; i < pppFrameSize; i++) {
                                    printf("%02x ", pppFrameBuffer[i]);

                                    if(((i + 1) % 16) == 0) {
                                        printf("\n");
                                    }
                                }

                                if(((pppFrameSize + 1) % 16) == 0) {
                                    printf("\n");
                                }

                                printf("7e\n");

                                pppFrameReceived(client, pppFrameBuffer, pppFrameSize);
                            }

                            rejectFrame = false;
                            readingFrame = false;
                        } else {
                            readingFrame = true;
                            pppFrameSize = 0;
                        }
                    } else {
                        if(pppFrameSize >= client->mru) {
                            rejectFrame = true;
                        } else {
                            pppFrameBuffer[pppFrameSize++] = receivedByte;
                        }
                    }
                }
            }
        }
    }

    return NULL;
}
