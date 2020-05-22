#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <client.h>
#include <fcs.h>

int main() {
    // Create the socket to the server
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(sock < 0) {
        perror("Failed to create socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in socketAddress;
    memset(&socketAddress, 0, sizeof(struct sockaddr_in));
    socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_port = htons(6666);

    if(bind(sock, (const struct sockaddr *)&socketAddress, sizeof(struct sockaddr_in))) {
        perror("Failed to bind socket");
        return EXIT_FAILURE;
    }

    if(listen(sock, 10)) {
        perror("Failed to listen to the socket.");
        return EXIT_FAILURE;
    }

    struct sockaddr clientAddress;
    socklen_t clientAddressLength;

    while(true) {
        int clientSocket = accept(sock, &clientAddress, &clientAddressLength);
        
        if(clientSocket == -1) {
            perror("accept() failed");
            break;
        }

        client_t *client = malloc(sizeof(client_t));

        if(!client) {
            perror("malloc() failed");
            break;
        }

        if(client_init(client, clientSocket, &clientAddress, clientAddressLength)) {
            fprintf(stderr, "client_init() failed.\n");
            break;
        }
    }

    return EXIT_SUCCESS;
}
