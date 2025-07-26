#include <iostream>
#include <thread>
#include <unordered_map>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <ctime>
#include <mutex>
#include <chrono>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>

using json = nlohmann::json;

struct Session {
    std::string session_id;
    std::string created_at;
};

struct Config {
    int udp_port;
    int http_port;
    int session_timeout_sec;
    std::string cdr_file;
    int graceful_shutdown_rate;
    std::string log_file;
    std::string log_level;
    std::unordered_set<std::string> blacklist;
} config;

void load_config(const std::string& filename) {
    std::ifstream config_file(filename);
    json j;
    config_file >> j;
    config.udp_port = j["udp_port"];
    config.http_port = j["http_port"];
    config.session_timeout_sec = j["session_timeout_sec"];
    config.cdr_file = j["cdr_file"];
    config.graceful_shutdown_rate = j["graceful_shutdown_rate"];
    config.log_file = j["log_file"];
    config.log_level = j["log_level"];
    config.blacklist = j["blacklist"];
}

std::unordered_map<std::string, Session> active_sessions;
std::mutex sessions_mutex;
bool server_running = true;

void handle_udp_request(int sockfd, const std::string& imsi, const sockaddr_in& client_addr) {
    std::string response;
    std::string action;

    std::lock_guard<std::mutex> lock(sessions_mutex);

    if (config.blacklist.find(imsi) != config.blacklist.end()) {
        response = "rejected";
        action = "rejected (blacklisted)";
    }
    else if (active_sessions.find(imsi) != active_sessions.end()) {
        response = "rejected";
        action = "rejected (session exists)";
    }
    else {
        std::string session_id = std::to_string(std::time(nullptr)) + "-" + imsi.substr(0, 4);

        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        std::string timestamp = std::ctime(&now_time);
        timestamp.erase(timestamp.find_last_not_of("\n") + 1);

        Session new_session;
        new_session.session_id = session_id;
        new_session.created_at = timestamp;

        active_sessions[imsi] = new_session;
        response = "created";
        action = "created";
    }

    socklen_t len = sizeof(client_addr);
    sendto(sockfd, response.c_str(), response.size(), 0,
        (struct sockaddr*)&client_addr, len);

    std::ofstream cdr_file(config.cdr_path, std::ios::app);
    if (cdr_file.is_open()) {
        cdr_file << std::time(nullptr) << "," << imsi << "," << action << "\n";
    }
}

void start_http_server() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config.http_port);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    std::cout << "HTTP API started on port " << config.http_port << std::endl;

    while (server_running) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        char buffer[4096] = { 0 };
        read(client_fd, buffer, 4096);

        std::string request(buffer);
        std::string response;


        if (request.find("GET /check_subscriber?imsi=") != std::string::npos) {
            size_t start = request.find("imsi=") + 5;
            size_t end = request.find(" ", start);
            std::string imsi = request.substr(start, end - start);

            std::lock_guard<std::mutex> lock(sessions_mutex);
            auto it = active_sessions.find(imsi);

            json j;
            if (it != active_sessions.end()) {
                j["status"] = "active";
                j["session_id"] = it->second.session_id;
                j["created_at"] = it->second.created_at;
            }
            else {
                j["status"] = "inactive";
            }
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + j.dump();
        }

        else if (request.find("GET /stop") != std::string::npos) {
            server_running = false;
            response = "HTTP/1.1 200 OK\r\n\r\nServer stopping...";
        }
        else {
            response = "HTTP/1.1 404 Not Found\r\n\r\n";
        }

        write(client_fd, response.c_str(), response.size());
        close(client_fd);
    }
    close(server_fd);
}
