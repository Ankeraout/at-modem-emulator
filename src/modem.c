#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <pthread.h>

#include <libtun/libtun.h>

#include <client.h>
#include <protocols/ethernet.h>
#include <protocols/ipv4.h>

int tunDeviceFd;

void *tunReaderThreadMain(void *arg) {
    while(true) {
        uint8_t buffer[1504];
        
        ssize_t size = read(tunDeviceFd, buffer, 1504);

        if(size == 0) {
            break;
        } else if(size < 0) {
            perror("read() on tunnel failed");
            break;
        }

        uint16_t etherType = ntohs(*(uint16_t *)(buffer + 2));

        if(etherType == ETHERNET_ETHERTYPE_IPV4) {
            ipv4_tunFrameReceived(buffer + 4, (size_t)(size - 4));
        }

    }

    return NULL;
}

int main() {
    // Create the tunnel
    char tunDeviceName[50] = "tun0";
    tunDeviceFd = libtun_open(tunDeviceName);

    if(tunDeviceFd < 0) {
        perror("libtun_open() failed");
        return EXIT_FAILURE;
    }

    char cmdBuffer[250];
    sprintf(cmdBuffer, "ip addr add 192.168.50.1/24 dev %s", tunDeviceName);
    system(cmdBuffer);
    system("ip link set tun0 up");

    // Start the tun reader thread
    pthread_t tunReaderThread;
    pthread_attr_t tunReaderThreadAttr;

    // Initialize the IP network
    if(ipv4_init(0xc0a83200, 0xffffff00, tunDeviceFd)) { // 192.168.50.0/24
        fprintf(stderr, "IPv4 protocol initialization failed.\n");
        return EXIT_FAILURE;
    }

    if(pthread_attr_init(&tunReaderThreadAttr)) {
        perror("main(): failed to initialize tun reader thread attr");
        return EXIT_FAILURE;
    }

    if(pthread_create(&tunReaderThread, &tunReaderThreadAttr, tunReaderThreadMain, NULL)) {
        perror("main(): failed to create tun reader thread");
        return EXIT_FAILURE;
    }
    
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
