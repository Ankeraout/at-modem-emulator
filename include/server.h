#ifndef __SERVER_H_INCLUDED__
#define __SERVER_H_INCLUDED__

#include <arpa/inet.h>

int serverInit(struct in6_addr *p_address, int p_port);
int serverClose(void);
int serverAccept(
    struct sockaddr *p_clientAddress,
    socklen_t *p_clientAddressLength
);

#endif
