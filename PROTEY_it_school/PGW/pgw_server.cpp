#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>


std::unordered_set<std::string> blacklisted_imsis = {
    "111111111111111", 
    "999999999999999"
};

struct Session {
    std::string session_id;
    time_t created_at;
    std::string client_ip;
    uint16_t client_port;
};

std::unordered_map<std::string, Session> active_sessions; 


std::string generate_session_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    return "SID-" + std::to_string(dis(gen));
}


void send_response(int sockfd, sockaddr_in client_addr, const std::string& message) {
    sendto(
        sockfd,
        message.c_str(),
        message.size(),
        0,
        (struct sockaddr*)&client_addr,
        sizeof(client_addr)
    );
}


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

    if (!is_valid || imsi.length() != 15) {
        std::string response = "rejected invalid_imsi";
        send_response(sockfd, client_addr, response);
        std::cerr << "Rejected IMSI (invalid): " << imsi << " from " << client_ip << ":" << client_port << std::endl;
        return;
    }

    if (blacklisted_imsis.find(imsi) != blacklisted_imsis.end()) {
        std::string response = "rejected blacklisted";
        send_response(sockfd, client_addr, response);
        std::cerr << "Rejected IMSI (blacklisted): " << imsi << " from " << client_ip << ":" << client_port << std::endl;
        return;
    }

    if (active_sessions.find(imsi) != active_sessions.end()) {
        std::string response = "rejected session_exists";
        send_response(sockfd, client_addr, response);
        std::cerr << "Rejected IMSI (session exists): " << imsi << " from " << client_ip << ":" << client_port << std::endl;
        return;
    }

    Session new_session;
    new_session.session_id = generate_session_id();
    new_session.created_at = time(nullptr);
    new_session.client_ip = client_ip;
    new_session.client_port = client_port;

    active_sessions[imsi] = new_session;

    std::string response = "created " + new_session.session_id;
    send_response(sockfd, client_addr, response);

    std::cout << "New session created:\n"
        << "  IMSI: " << imsi << "\n"
        << "  Session ID: " << new_session.session_id << "\n"
        << "  Client: " << client_ip << ":" << client_port << "\n"
        << "  Time: " << ctime(&new_session.created_at);
}

