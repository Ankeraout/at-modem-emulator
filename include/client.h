#ifndef __CLIENT_H_INCLUDED__
#define __CLIENT_H_INCLUDED__

#include <stdbool.h>

#include <pthread.h>

#include "protocols/hayes.h"
#include "protocols/hdlc.h"
#include "protocols/ppp.h"
#include "protocols/ipv4.h"

struct ts_client {
    bool present;
    int id;
    int socket;
    pthread_t thread;
    pthread_mutex_t mutex;
    struct ts_hayesContext hayesContext;
    struct ts_hdlcContext hdlcContext;
    struct ts_pppContext pppContext;
    struct ts_ipv4Context ipv4Context;
};

void clientInit(void);
int clientAccept(int p_clientSocket);
void clientWriteString(struct ts_client *p_client, const char *p_s);
void clientWrite(
    struct ts_client *p_client,
    const void *p_buffer,
    size_t p_size
);

#endif
