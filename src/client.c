#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <client.h>

#include <protocols/ipv4.h>
#include <protocols/ppp.h>

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
    client->callProgress = 0;
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
    client->acfc = false;
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

    if(ipv4_alloc(client)) {
        fprintf(stderr, "client_init(): failed to allocate an IP address for the client.\n");
        return 1;
    }

    char ipbuffer[16];
    ipv4_toString(ipbuffer, client->ipv4);
    printf("client_init(): allocated IP address %s for a new client from %s.\n", ipbuffer, inet_ntoa(((struct sockaddr_in *)socketAddress)->sin_addr));

    return 0;
}

void sendResult(client_t *client, int resultCode) {
    char buffer[50];
    int size;

    if(client->verbose) {
        if(resultCode == RESULTCODE_CONNECT && client->callProgress > 0) {
            size = strcpy(buffer, "\r\nCONNECT 115200\r\n");
        } else {
            size = sprintf(buffer, "\r\n%s\r\n", resultCodesStr[resultCode]);
        }
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

                case 'H': 
                    {
                        bool value;

                        if(parseBooleanParameter(commandLine[index], &value)) {
                            index++;
                        }
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

                case 'X':
                    {
                        char arg = commandLine[index++];

                        if(arg < '0' || arg > '4') {
                            resultOk = false;
                            keepExecuting = false;
                        }

                        client->callProgress = arg - '0';
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

    int plusCount = 0;

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
                    if(receivedByte == '+') {
                        plusCount++;

                        if(plusCount == 3) {
                            printf("Received 3 '+' characters, returning to command mode.\n");
                            client->mode = CLIENTMODE_COMMAND;
                            plusCount = 0;

                            if(readingFrame) {
                                pppFrameSize -= 3;
                            }
                        }
                    } else {
                        plusCount = 0;
                    }

                    if(receivedByte == 0x7d) {
                        escape = true;
                    } else if(receivedByte == 0x7e) {
                        if(readingFrame) {
                            if(!rejectFrame) {
                                ppp_frameReceived(client, pppFrameBuffer, pppFrameSize);
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

    ipv4_free(client->ipv4);

    return NULL;
}
