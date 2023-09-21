#include <string.h>
#include <errno.h>
#include <stdio.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netinet/tcp.h>

static int s_serverSocket;

int serverInit(struct in6_addr *p_address, int p_port) {
    // Create the server socket
    s_serverSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    // If the socket creation failed, return -1.
    if(s_serverSocket < 0) {
        perror("Failed to create server socket");
        return -1;
    }

    // Bind the server socket
    struct sockaddr_in6 l_socketAddress = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(p_port),
        .sin6_flowinfo = 0,
        .sin6_addr = *p_address,
        .sin6_scope_id = 0
    };

    // If the socket bind failed, return -1.
    if(
        bind(
            s_serverSocket,
            (const struct sockaddr *)&l_socketAddress,
            sizeof(struct sockaddr_in6)
        ) < 0
    ) {
        perror("Failed to bind server socket");
        return -1;
    }

    // Set the socket in listening mode
    if(listen(s_serverSocket, 1) < 0) {
        perror("Failed to set server socket to listening mode");
        return -1;
    }

    return 0;
}

int serverClose(void) {
    return close(s_serverSocket);
}

int serverAccept(
    struct sockaddr *p_clientAddress,
    socklen_t *p_clientAddressLength
) {
    int l_clientSocket = accept(
        s_serverSocket,
        p_clientAddress,
        p_clientAddressLength
    );

    int l_flag = 1;
    int l_result = setsockopt(
        l_clientSocket,
        IPPROTO_TCP,
        TCP_NODELAY,
        (char *)&l_flag,
        sizeof(int)
    );

    if(l_result < 0) {
        perror("Failed to set TCP_NODELAY");
    }

    return l_clientSocket;
}
