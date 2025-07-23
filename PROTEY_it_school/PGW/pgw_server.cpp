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

using json = nlohmann::json;

struct Config {
    int port;
    int session_timeout_sec;
    int max_offload_sessions;
    std::unordered_set<std::string> blacklist;
    std::string cdr_path;
    int max_log_size_mb;
};

struct Session {
    std::string session_id;
    time_t created_at;
    std::string client_ip;
    uint16_t client_port;
};


Config config;
std::unordered_map<std::string, Session> active_sessions;
std::mutex sessions_mutex;


void load_config(const std::string& path) {
    std::ifstream config_file(path);
    if (!config_file) {
        throw std::runtime_error("Cannot open config file");
    }

    json j;
    config_file >> j;

    config.port = j["server"]["port"];
    config.session_timeout_sec = j["server"]["session_timeout_sec"];
    config.max_offload_sessions = j["server"]["max_offload_sessions"];
    config.cdr_path = j["logging"]["cdr_path"];
    config.max_log_size_mb = j["logging"]["max_file_size_mb"];

    for (const auto& imsi : j["blacklist"]) {
        config.blacklist.insert(imsi.get<std::string>());
    }
}


void log_cdr(const std::string& imsi, const std::string& action) {
    std::ofstream cdr_file(config.cdr_path, std::ios::app);
    if (!cdr_file) {
        std::cerr << "Cannot open CDR log file" << std::endl;
        return;
    }

    time_t now = time(nullptr);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    cdr_file << timestamp << " | IMSI: " << imsi << " | Action: " << action << "\n";
}


void cleanup_sessions() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(60));

        std::lock_guard<std::mutex> lock(sessions_mutex);
        time_t now = time(nullptr);

        for (auto it = active_sessions.begin(); it != active_sessions.end(); ) {
            if (now - it->second.created_at >= config.session_timeout_sec) {
                log_cdr(it->first, "expired (session ID: " + it->second.session_id + ")");
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


void handle_client(int sockfd, sockaddr_in client_addr, char* buffer, int bytes_received) {

}
