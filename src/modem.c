#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#include "client.h"
#include "server.h"
#include "tun.h"

int main(void) {
    char l_tunDeviceName[10] = "tun1";

    // Prepare the server socket
    struct in6_addr l_socketAddress = IN6ADDR_ANY_INIT;

    if(serverInit(&l_socketAddress, 6666) < 0) {
        fprintf(stderr, "Server initialization failed.\n");
        return -1;
    }

    clientInit();

    // Prepare the IPv4 protocol
    ipv4Init();

    // Prepare the tun interface
    //tunOpen(l_tunDeviceName);

    // Accept incoming clients
    while(true) {
        struct in6_addr l_clientAddress;
        socklen_t l_clientAddressLength = sizeof(struct in6_addr);

        int l_clientSocket = serverAccept(
            (struct sockaddr *)&l_clientAddress,
            &l_clientAddressLength
        );

        if(l_clientSocket < 0) {
            fprintf(stderr, "Warning: serverAccept() failed. Exiting...\n");
            break;
        }

        // Handle the new client
        if(clientAccept(l_clientSocket) < 0) {
            fprintf(stderr, "Warning: failed to accept new client.\n");
        }
    }

    return 0;
}
