#include <iostream>
#include <thread>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

void handle_client(int sockfd, sockaddr_in client_addr, char* buffer, int bytes_received) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr.sin_port);

    buffer[bytes_received] = '\0';
    std::string imsi(buffer);

    bool is_valid = true;
    for (char c : imsi) {
        if (!isdigit(c)) {
            is_valid = false;
            break;
        }
    }

    if (is_valid && (imsi.length() == 15 || imsi.length() <= 16)) {
        std::cout << "Received IMSI: " << imsi
            << " from " << client_ip << ":" << client_port << std::endl;

    }
    else {
        std::cerr << "Invalid IMSI received: " << imsi
            << " from " << client_ip << ":" << client_port << std::endl;
    }
}
