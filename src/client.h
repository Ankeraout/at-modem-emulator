#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <stdbool.h>
#include <stdint.h>

#include <arpa/inet.h>
#include <pthread.h>

typedef enum {
    CLIENTMODE_COMMAND,
    CLIENTMODE_DATA
} client_mode_t;

typedef struct {
    int fd;
    struct sockaddr socketAddress;
    socklen_t socketAddressLength;
    pthread_t readerThread;
    pthread_mutex_t mutex;

    // AT modem options
    bool echo;
    bool quiet;
    bool verbose;
    client_mode_t mode;
    int callProgress;

    // LCP options
    uint8_t currentIdentifier;
    uint_least16_t mru;
    bool accm[256];
    bool pfc;
    bool acfc;
    uint32_t ipv4;
} client_t;

int client_init(client_t *client, int fd, const struct sockaddr *socketAddress, socklen_t socketAddressLength);

#endif
