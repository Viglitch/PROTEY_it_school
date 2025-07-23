#include <iostream>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Отправка UDP-пакета с IMSI
void send_imsi(const std::string& imsi, const std::string& server_ip, int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

    sendto(sockfd, imsi.c_str(), imsi.size(), 0,
        (struct sockaddr*)&server_addr, sizeof(server_addr));

    char buffer[1024];
    socklen_t len = sizeof(server_addr);
    int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0,
        (struct sockaddr*)&server_addr, &len);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        std::cout << "Server response: " << buffer << std::endl;
    }

    close(sockfd);
}

// Проверка статуса абонента через HTTP API
void check_subscriber(const std::string& imsi, const std::string& server_ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        return;
    }

    std::string request = "GET /check_subscriber?imsi=" + imsi + " HTTP/1.1\r\n\r\n";
    send(sockfd, request.c_str(), request.size(), 0);

    char buffer[4096];
    int bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        // Парсим JSON из HTTP-ответа
        std::string response(buffer);
        size_t json_start = response.find("\r\n\r\n");
        if (json_start != std::string::npos) {
            json j = json::parse(response.substr(json_start + 4));
            std::cout << "Subscriber status:\n" << j.dump(2) << std::endl;
        }
    }

    close(sockfd);
}