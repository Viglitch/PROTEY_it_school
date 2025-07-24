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
    std::unordered_set<std::string> blacklist;
    std::string cdr_path;
} config;

std::unordered_map<std::string, Session> active_sessions;
std::mutex sessions_mutex;
bool server_running = true;

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
