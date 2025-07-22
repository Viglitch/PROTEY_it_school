#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <map>
#include <thread>

struct ClientSession {
    std::string ip;
    uint64_t total_bytes;
};

std::map<std::string, ClientSession> sessions;

void handle_client(int sockfd, sockaddr_in client_addr, char* buffer, int bytes_received) {
    std::string client_ip = inet_ntoa(client_addr.sin_addr);

    if (sessions.find(client_ip) == sessions.end()) {
        sessions[client_ip] = { client_ip, 0 };
        std::cout << "New session: " << client_ip << std::endl;
    }

    sessions[client_ip].total_bytes += bytes_received;

    sendto(sockfd, buffer, bytes_received, 0,
        (struct sockaddr*)&client_addr, sizeof(client_addr));

    std::cout << "Traffic from " << client_ip << ": " << sessions[client_ip].total_bytes << " bytes\n";
}