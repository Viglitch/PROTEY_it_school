#include <iostream>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <ctime>
#include "pgw_server.h"

using json = nlohmann::json;

std::ofstream log_file;
bool logging_enabled = true;

void init_logger(const std::string& log_filename) {
    log_file.open(log_filename, std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "Failed to open log file: " << log_filename << std::endl;
        logging_enabled = false;
    }
}

void log_message(const std::string& message) {
    if (!logging_enabled) return;

    std::time_t now = std::time(nullptr);
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    log_file << "[" << timestamp << "] " << message << std::endl;
    std::cout << "LOG: " << message << std::endl;
}

void send_imsi(const std::string& imsi, const std::string& server_ip, int port) {
    log_message("Attempting to send IMSI: " + imsi + " to " + server_ip + ":" + std::to_string(port));

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::string error = "Socket creation failed: " + std::string(strerror(errno));
        log_message(error);
        return;
    }

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        log_message("Warning: failed to set socket timeout");
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::string error = "Invalid server IP address: " + server_ip;
        log_message(error);
        close(sockfd);
        return;
    }

    ssize_t sent_bytes = sendto(sockfd, imsi.c_str(), imsi.size(), 0,
        (struct sockaddr*)&server_addr, sizeof(server_addr));

    if (sent_bytes < 0) {
        std::string error = "Failed to send IMSI: " + std::string(strerror(errno));
        log_message(error);
        close(sockfd);
        return;
    }

    log_message("Successfully sent " + std::to_string(sent_bytes) + " bytes with IMSI: " + imsi);

    char buffer[1024];
    socklen_t len = sizeof(server_addr);
    ssize_t bytes_received = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
        (struct sockaddr*)&server_addr, &len);

    if (bytes_received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            log_message("Timeout: no response from server");
        }
        else {
            std::string error = "Failed to receive response: " + std::string(strerror(errno));
            log_message(error);
        }
    }
    else {
        buffer[bytes_received] = '\0';
        std::string response(buffer);
        log_message("Received server response: " + response);
        std::cout << "Server response: " << response << std::endl;
    }

    close(sockfd);
}

void check_subscriber(const std::string& imsi, const std::string& server_ip, int port) {
    log_message("Checking subscriber status for IMSI: " + imsi + " via HTTP API");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::string error = "Socket creation failed: " + std::string(strerror(errno));
        log_message(error);
        return;
    }

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        log_message("Warning: failed to set socket timeout");
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::string error = "Invalid server IP address: " + server_ip;
        log_message(error);
        close(sockfd);
        return;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::string error = "Connection failed: " + std::string(strerror(errno));
        log_message(error);
        close(sockfd);
        return;
    }

    log_message("Successfully connected to HTTP server");

    std::string request = "GET /check_subscriber?imsi=" + imsi + " HTTP/1.1\r\n"
        "Host: " + server_ip + "\r\n"
        "Connection: close\r\n"
        "\r\n";

    ssize_t sent_bytes = send(sockfd, request.c_str(), request.size(), 0);
    if (sent_bytes < 0) {
        std::string error = "Failed to send HTTP request: " + std::string(strerror(errno));
        log_message(error);
        close(sockfd);
        return;
    }

    log_message("Successfully sent HTTP request");

    std::string response;
    char buffer[4096];
    ssize_t bytes_received;

    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        response += buffer;
    }

    if (bytes_received < 0) {
        std::string error = "Failed to receive HTTP response: " + std::string(strerror(errno));
        log_message(error);
        close(sockfd);
        return;
    }

    log_message("Received HTTP response: " + response.substr(0, 100) + "...");

    try {
        size_t json_start = response.find("\r\n\r\n");
        if (json_start != std::string::npos) {
            json j = json::parse(response.substr(json_start + 4));
            std::string pretty_json = j.dump(2);
            log_message("Subscriber status: " + pretty_json);
            std::cout << "Subscriber status:\n" << pretty_json << std::endl;
        }
        else {
            log_message("Invalid HTTP response format");
            std::cout << "Invalid HTTP response format" << std::endl;
        }
    }
    catch (const json::parse_error& e) {
        std::string error = "JSON parse error: " + std::string(e.what());
        log_message(error);
        std::cerr << error << std::endl;
    }

    close(sockfd);
}