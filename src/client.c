#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

#include "client.h"
#include "protocols/hayes.h"
#include "protocols/hdlc.h"
#include "protocols/ppp.h"

#define C_CLIENT_TABLE_SIZE 64
#define C_READ_BUFFER_SIZE 1024

static struct ts_client s_clientTable[C_CLIENT_TABLE_SIZE];

static void *clientMain(void *p_arg);

void clientInit(void) {
    memset((void *)s_clientTable, 0, sizeof(s_clientTable));
}

int clientAccept(int p_clientSocket) {
    // Look for a free space in the client table
    bool l_foundId = false;
    int l_clientId;

    for(int l_id = 0; l_id < C_CLIENT_TABLE_SIZE; l_id++) {
        if(!s_clientTable[l_id].present) {
            l_clientId = l_id;
            l_foundId = true;
            break;
        }
    }

    if(!l_foundId) {
        return -1;
    }

    s_clientTable[l_clientId].present = true;
    s_clientTable[l_clientId].id = l_clientId;
    s_clientTable[l_clientId].socket = p_clientSocket;

    // Create client thread and mutex
    if(pthread_mutex_init(&s_clientTable[l_clientId].mutex, NULL) != 0) {
        return -1;
    }

    if(
        pthread_create(
            &s_clientTable[l_clientId].thread,
            NULL,
            clientMain,
            &s_clientTable[l_clientId]
        ) != 0
    ) {
        return -1;
    }

    return 0;
}

static void *clientMain(void *p_arg) {
    struct ts_client *l_client = (struct ts_client *)p_arg;
    uint8_t l_readBuffer[C_READ_BUFFER_SIZE];

    // Initialize client protocols
    // Hayes modem: socket <--> hayes <--> hdlc
    hayesInit(
        &l_client->hayesContext,
        clientSend,
        l_client,
        hdlcReceive,
        &l_client->hdlcContext
    );

    // PPP-HDLC (RFC 1662): hayes <--> hdlc <--> ppp
    hdlcInit(
        &l_client->hdlcContext,
        hayesSend,
        &l_client->hayesContext,
        pppReceive,
        &l_client->pppContext
    );

    // PPP: hdlc <--> ppp
    pppInit(
        &l_client->pppContext,
        hdlcSend,
        &l_client->hdlcContext
    );

    while(true) {
        ssize_t l_bytesRead = read(
            l_client->socket,
            l_readBuffer,
            C_READ_BUFFER_SIZE
        );

        hayesReceive(&l_client->hayesContext, l_readBuffer, l_bytesRead);
    }

    printf("client: Client %d disconnected.\n", l_client->id);

    l_client->present = false;

    return NULL;
}

void clientSend(void *p_arg, const void *p_buffer, size_t p_size) {
    struct ts_client *l_client = (struct ts_client *)p_arg;

    write(l_client->socket, p_buffer, p_size);
}
