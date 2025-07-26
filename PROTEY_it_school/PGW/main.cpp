#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unordered_set>
#include <chrono>
#include <fstream>
#include "pgw_server.cpp"

void handle_client(int sockfd, sockaddr_in client_addr, char* buffer, int bytes_received) {
    std::string imsi(buffer, bytes_received);

    if (imsi.length() < 15 || imsi.length() > 16) {
        std::cerr << "Invalid IMSI length: " << imsi << std::endl;
        return;
    }

    if (imsi.find_first_not_of("0123456789") != std::string::npos) {
        std::cerr << "Invalid IMSI format (non-digit characters): " << imsi << std::endl;
        return;
    }

    handle_udp_request(sockfd, imsi, client_addr);
}

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config.udp_port);

    std::thread http_thread(start_http_server);
    http_thread.detach();

    while (server_running) {
        char buffer[1024];
        sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0,
            (struct sockaddr*)&client_addr, &len);

        if (bytes_received > 0) {
            std::thread(handle_client, sockfd, client_addr, buffer, bytes_received).detach();
        }
    }

    close(sockfd);
    return 0;
}