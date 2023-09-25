#ifndef __CLIENT_H_INCLUDED__
#define __CLIENT_H_INCLUDED__

#include <stdbool.h>

#include <pthread.h>

#include "protocols/hayes.h"
#include "protocols/hdlc.h"
#include "protocols/ppp.h"

struct ts_client {
    bool present;
    int id;
    int socket;
    pthread_t thread;
    pthread_mutex_t mutex;
    struct ts_hayesContext hayesContext;
    struct ts_hdlcContext hdlcContext;
    struct ts_pppContext pppContext;
};

void clientInit(void);
int clientAccept(int p_clientSocket);
void clientSend(void *p_arg, const void *p_buffer, size_t p_size);

#endif
