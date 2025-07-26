#ifndef PGW_SERVER_H
#define PGW_SERVER_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <netinet/in.h>

struct Session {
    std::string session_id;
    std::string created_at;
};

struct Config {
    int udp_port;
    int http_port;
    int session_timeout_sec;
    std::string cdr_path;
    int graceful_shutdown_rate;
    std::string log_file;
    std::string log_level;
    std::unordered_set<std::string> blacklist;
};

extern Config config;
extern std::unordered_map<std::string, Session> active_sessions;
extern std::mutex sessions_mutex;
extern bool server_running;

void load_config(const std::string& filename);
void handle_udp_request(int sockfd, const std::string& imsi, const sockaddr_in& client_addr);
void start_http_server();

#endif
