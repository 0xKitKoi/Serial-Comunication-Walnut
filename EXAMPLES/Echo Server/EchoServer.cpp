// echo_server_handshake.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
#  define CLOSE_SOCKET(s)  closesocket(s)
#else
#  include <sys/socket.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  define CLOSE_SOCKET(s)  close(s)
#endif

//#include <unistd.h>     // close()
//#include <arpa/inet.h>  // inet_addr
//#include <netinet/in.h>
//#include <sys/socket.h>

#define SERVER_PORT 5000
#define BUFFER_SIZE 256

int main() {
    #ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    #endif
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char client_type;

    // 1. Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("[-] Socket creation failed");
        return 1;
    }

    // 2. Bind socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    memset(server_addr.sin_zero, 0, sizeof(server_addr.sin_zero));

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Bind failed");
        CLOSE_SOCKET(server_socket);
        return 1;
    }

    // 3. Listen
    if (listen(server_socket, 1) < 0) {
        perror("[-] Listen failed");
        CLOSE_SOCKET(server_socket);
        return 1;
    }

    printf("[+] Echo server listening on port %d...\n", SERVER_PORT);

    // 4. Accept client
    client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_socket < 0) {
        perror("[-] Accept failed");
        CLOSE_SOCKET(server_socket);
        return 1;
    }

    printf("[+] Client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    // 5. Read 1-byte client type
    ssize_t n = recv(client_socket, &client_type, 1, 0); // This is a very simple way to auto reject connections that should not happen
    if (n != 1) {
        fprintf(stderr, "[-] Failed to read client type\n");
        CLOSE_SOCKET(client_socket);
        CLOSE_SOCKET(server_socket);
        return 1;
    }

    printf("[*] Client type: %c\n", client_type);

    // 6. Send back 1-byte acknowledgment
    char ack = 'A';
    if (send(client_socket, &ack, 1, 0) != 1) {
        fprintf(stderr, "[-] Failed to send ack\n");
        CLOSE_SOCKET(client_socket);
        CLOSE_SOCKET(server_socket);
        return 1;
    }

    // 7. Echo loop
    while (1) {
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            printf("[-] Client disconnected or error.\n");
            break;
        }

        buffer[bytes_received] = '\0';
        printf("[>] Received: %s", buffer);

        // Echo back
        send(client_socket, buffer, bytes_received, 0);
    }

    // 8. Cleanup   
    //#ifdef _WIN32
    //    closesocket(client_socket);
     //   closesocket(server_socket);
     //   WSACleanup();
    CLOSE_SOCKET(client_socket);
    CLOSE_SOCKET(server_socket);
    #ifdef _WIN32
        WSACleanup();
    #else
        close(client_socket);
        close(server_socket);
    #endif
    return 0;
}
