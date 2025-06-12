#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <chrono>
#include <fcntl.h>
#define SERVER_PORT 5000

std::mutex mutex;
std::queue<std::pair<int, std::string>> message_queue;

void handle_pico_client(int client_socket);
void handle_windows_client(int client_socket);
void forward_data(int client_socket_pico, int client_socket_windows);

int main() {
    // Create server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "[-] Failed to create server socket." << std::endl;
        return 1;
    }

    // Bind server socket
    struct sockaddr_in server_address = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr = { .s_addr = INADDR_ANY },
    };

    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        std::cerr << "Failed to bind server socket." << std::endl;
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 2) < 0) {
        std::cerr << "Failed to listen on server socket." << std::endl;
        return 1;
    }

    std::cout << "Server listening on port " << SERVER_PORT << "..." << std::endl;

    // Start a thread for forwarding data
    //std::thread forward_thread(forward_data);
    int pico;
    int windows;
    bool P = false;
    bool W = false;
    while (true) {
        // Accept connections
        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);

        int client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_address_len);
        if (client_socket < 0) {
            std::cerr << "Failed to accept client connection." << std::endl;
            continue;
        }

        // Determine client type and handle in separate thread
        char client_type;
        recv(client_socket, &client_type, sizeof(client_type), 0);
        if (client_type == 'P') {
            std::cout << "Pico client connected." << std::endl;
            std::thread pico_thread(handle_pico_client, client_socket);
            pico_thread.detach(); // Detach the thread to avoid blocking
            P = true;
            pico = client_socket;
        } else if (client_type == 'W') {
            std::cout << "Windows client connected." << std::endl;
            std::thread windows_thread(handle_windows_client, client_socket);
            windows_thread.detach(); // Detach the thread to avoid blocking
            W = true;
            windows = client_socket;
        } else {
            std::cerr << "Unknown client type." << std::endl;
            close(client_socket);
        }
        if (P && W) {
          //std::thread forward_thread(forward_data, pico, windows);
          std::thread forward_thread([&]() {
            forward_data(pico, windows);
          });
          forward_thread.detach(); // Detach the thread to avoid blocking
        }
    }

    return 0;
}


void handle_pico_client(int client_socket) {
    char buffer[256];
    ssize_t bytes_received;

    // Set non-blocking mode for the client socket
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

    while (true) {
        // Receive data from the client
        bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received == -1) {
            // Check if there was no data available
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available, continue
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            } else {
                // Handle other receive errors
                std::cerr << "Error receiving data from Pico client" << std::endl;
                break;
            }
        } else if (bytes_received == 0) {
            // Client disconnected
            std::cerr << "Pico client disconnected" << std::endl;
            break;
        }

        // Process received data
        buffer[bytes_received] = '\0';
        std::cout << "Received from Pico: " << buffer << std::endl;

        // Put the received data in the queue for forwarding
        std::lock_guard<std::mutex> lock(mutex);
        message_queue.push(std::make_pair(client_socket, std::string(buffer)));
    }

    // Close the client socket
    close(client_socket);
}



void handle_windows_client(int client_socket) {
    char buffer[256];
    ssize_t bytes_received;

    while (true) {
        bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            std::cerr << "Windows client disconnected." << std::endl;
            close(client_socket);
            return;
        }

        buffer[bytes_received] = '\0';
        std::cout << "Received from Windows: " << buffer << std::endl;

        // Put the received data in the queue for forwarding
        std::lock_guard<std::mutex> lock(mutex);
        message_queue.push(std::make_pair(client_socket, std::string(buffer)));
    }
}

void forward_data(int client_socket_pico, int client_socket_windows) {
    while (true) {
        //std::lock_guard<std::mutex> lock(mutex);

        // Check if there is any message in the queue
        while (!message_queue.empty()) {

        std::lock_guard<std::mutex> lock(mutex);
            // Get the message from the front of the queue
            auto message_pair = message_queue.front();
            message_queue.pop();

            int sender_socket = message_pair.first;
            std::string message = message_pair.second;

            // Print the received message to the terminal
            //std::cout << "Received message: " << message << std::endl;
            std::cout << "\n\tForwarding.\n";

            // Forward the message to both clients
            send(client_socket_pico, message.c_str(), message.length(), 0);
            send(client_socket_windows, message.c_str(), message.length(), 0);
        }

        // Add a sleep to avoid unnecessary CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
