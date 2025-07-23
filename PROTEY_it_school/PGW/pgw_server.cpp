#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>
#include <fstream>
#include <ctime>
#include <mutex>
#include <chrono>


const std::string CDR_LOG_FILE = "cdr.log";
const int SESSION_TIMEOUT_SEC = 1800;


std::unordered_set<std::string> blacklisted_imsis = { "111111111111111", "999999999999999" };
std::unordered_map<std::string, Session> active_sessions;
std::mutex sessions_mutex;

struct Session {
    std::string session_id;
    time_t created_at;
    std::string client_ip;
    uint16_t client_port;
};


void log_cdr(const std::string& imsi, const std::string& action) {
    std::ofstream cdr_file(CDR_LOG_FILE, std::ios::app);
    if (!cdr_file.is_open()) {
        std::cerr << "Error: Cannot open CDR log file!" << std::endl;
        return;
    }

    time_t now = time(nullptr);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    cdr_file << timestamp << " | IMSI: " << imsi << " | Action: " << action << "\n";
    cdr_file.close();
}


void cleanup_sessions() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(60));

        std::lock_guard<std::mutex> lock(sessions_mutex);
        time_t now = time(nullptr);

        for (auto it = active_sessions.begin(); it != active_sessions.end(); ) {
            if (now - it->second.created_at >= SESSION_TIMEOUT_SEC) {
                log_cdr(it->first, "expired (session ID: " + it->second.session_id + ")");
                std::cout << "Session expired: " << it->first
                    << " (ID: " << it->second.session_id << ")\n";
                it = active_sessions.erase(it);
            }
            else {
                ++it;
            }
        }
    }
}


std::string generate_session_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    return "SID-" + std::to_string(dis(gen));
}


void send_response(int sockfd, sockaddr_in client_addr, const std::string& message) {
    sendto(sockfd, message.c_str(), message.size(), 0,
        (struct sockaddr*)&client_addr, sizeof(client_addr));
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
        log_cdr(imsi, "rejected (invalid IMSI)");
        return;
    }

    if (blacklisted_imsis.find(imsi) != blacklisted_imsis.end()) {
        std::string response = "rejected blacklisted";
        send_response(sockfd, client_addr, response);
        log_cdr(imsi, "rejected (blacklisted)");
        return;
    }


    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        if (active_sessions.find(imsi) != active_sessions.end()) {
            std::string response = "rejected session_exists";
            send_response(sockfd, client_addr, response);
            log_cdr(imsi, "rejected (session exists)");
            return;
        }


        Session new_session;
        new_session.session_id = generate_session_id();
        new_session.created_at = time(nullptr);
        new_session.client_ip = client_ip;
        new_session.client_port = client_port;

        active_sessions[imsi] = new_session;
        log_cdr(imsi, "created (session ID: " + new_session.session_id + ")");

        std::string response = "created " + new_session.session_id;
        send_response(sockfd, client_addr, response);
    }
}
