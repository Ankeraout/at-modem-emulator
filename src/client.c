#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

#include "client.h"
#include "protocols/hayes.h"
#include "protocols/ipv4.h"
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
    hayesInit(l_client);
    pppInit(l_client);
    ipv4InitClient(l_client);

    printf("client: Client %d connected.\n", l_client->id);

    char l_ipAddressBuffer[16];

    ipv4ToString(l_ipAddressBuffer, l_client->ipv4Context.address);

    printf("client: Client %d has IP address %s\n", l_client->id, l_ipAddressBuffer);

    while(true) {
        ssize_t l_bytesRead = read(
            l_client->socket,
            l_readBuffer,
            C_READ_BUFFER_SIZE
        );

        if(l_bytesRead > 0) {
            for(
                int l_bufferIndex = 0;
                l_bufferIndex < l_bytesRead;
                l_bufferIndex++
            ) {
                hayesReceive(l_client, l_readBuffer[l_bufferIndex]);
            }
        } else {
            break;
        }
    }

    printf("client: Client %d disconnected.\n", l_client->id);

    ipv4Free(l_client->ipv4Context.address);
    l_client->present = false;

    return NULL;
}

void clientWriteString(struct ts_client *p_client, const char *p_s) {
    size_t l_stringLength = strlen(p_s);

    write(p_client->socket, p_s, l_stringLength);
}

void clientWrite(
    struct ts_client *p_client,
    const void *p_buffer,
    size_t p_size
) {
    write(p_client->socket, p_buffer, p_size);
}
