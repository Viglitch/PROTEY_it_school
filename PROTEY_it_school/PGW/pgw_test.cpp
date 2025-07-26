#include <gtest/gtest.h>
#include "pgw_server.h"
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <arpa/inet.h>
#include <sys/socket.h>

class PGWServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::ofstream config_file("test_config.json");
        config_file << R"({
            "udp_port": 9001,
            "udp_ip": "0.0.0.0",
            "http_port": 8081,
            "session_timeout_sec": 30,
            "cdr_file": "test_cdr.log",
            "graceful_shutdown_rate": 10,
            "log_file": "test_pgw.log",
            "log_level": "DEBUG",
            "blacklist": ["001010123456789", "001010000000001"]
        })";
        config_file.close();

        load_config("test_config.json");
    }

    void TearDown() override {
        std::remove("test_config.json");
        std::remove("test_cdr.log");
        std::remove("test_pgw.log");
    }
};

TEST_F(PGWServerTest, ConfigLoading) {
    EXPECT_EQ(config.udp_port, 9001);
    EXPECT_EQ(config.http_port, 8081);
    EXPECT_EQ(config.session_timeout_sec, 30);
    EXPECT_EQ(config.cdr_path, "test_cdr.log");
    EXPECT_EQ(config.graceful_shutdown_rate, 10);
    EXPECT_EQ(config.log_file, "test_pgw.log");
    EXPECT_EQ(config.log_level, "DEBUG");
    EXPECT_TRUE(config.blacklist.count("001010123456789") > 0);
    EXPECT_TRUE(config.blacklist.count("001010000000001") > 0);
}

TEST_F(PGWServerTest, HandleUDPRequestNewSession) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    std::string valid_imsi = "123456789012345";

    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        active_sessions.clear();
    }

    handle_udp_request(sockfd, valid_imsi, client_addr);

    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        EXPECT_EQ(active_sessions.size(), 1);
        EXPECT_TRUE(active_sessions.find(valid_imsi) != active_sessions.end());
    }

    close(sockfd);
}

TEST_F(PGWServerTest, HandleUDPRequestBlacklisted) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    std::string blacklisted_imsi = "001010123456789";

    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        active_sessions.clear();
    }

    handle_udp_request(sockfd, blacklisted_imsi, client_addr);

    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        EXPECT_EQ(active_sessions.size(), 0);
    }

    close(sockfd);
}

TEST_F(PGWServerTest, HandleUDPRequestExistingSession) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    std::string valid_imsi = "123456789012345";

    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        active_sessions.clear();
        Session session;
        session.session_id = "test-session";
        session.created_at = "now";
        active_sessions[valid_imsi] = session;
    }

    handle_udp_request(sockfd, valid_imsi, client_addr);

    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        EXPECT_EQ(active_sessions.size(), 1);
    }

    close(sockfd);
}

TEST_F(PGWServerTest, CDRFileCreation) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    std::string valid_imsi = "123456789012345";

    std::remove(config.cdr_path.c_str());

    handle_udp_request(sockfd, valid_imsi, client_addr);

    std::ifstream cdr_file(config.cdr_path);
    EXPECT_TRUE(cdr_file.is_open());

    std::string line;
    std::getline(cdr_file, line);
    EXPECT_FALSE(line.empty());

    close(sockfd);
}

TEST_F(PGWServerTest, HTTPCheckSubscriberActive) {
    std::string valid_imsi = "123456789012345";

    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        active_sessions.clear();
        Session session;
        session.session_id = "test-session";
        session.created_at = "now";
        active_sessions[valid_imsi] = session;
    }

    std::thread http_thread(start_http_server);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config.http_port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    std::string request = "GET /check_subscriber?imsi=" + valid_imsi + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    send(sockfd, request.c_str(), request.size(), 0);

    char buffer[4096] = { 0 };
    recv(sockfd, buffer, sizeof(buffer), 0);

    std::string response(buffer);

    EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(response.find("\"status\":\"active\""), std::string::npos);
    EXPECT_NE(response.find("\"session_id\":\"test-session\""), std::string::npos);

    close(sockfd);
    server_running = false;
    http_thread.join();
}

TEST_F(PGWServerTest, HTTPCheckSubscriberInactive) {
    std::string unknown_imsi = "987654321098765";

    std::thread http_thread(start_http_server);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config.http_port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    std::string request = "GET /check_subscriber?imsi=" + unknown_imsi + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    send(sockfd, request.c_str(), request.size(), 0);

    char buffer[4096] = { 0 };
    recv(sockfd, buffer, sizeof(buffer), 0);

    std::string response(buffer);

    EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(response.find("\"status\":\"inactive\""), std::string::npos);

    close(sockfd);
    server_running = false;
    http_thread.join();
}

TEST_F(PGWServerTest, HTTPStopServer) {
    std::thread http_thread(start_http_server);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config.http_port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    std::string request = "GET /stop HTTP/1.1\r\nHost: localhost\r\n\r\n";
    send(sockfd, request.c_str(), request.size(), 0);

    char buffer[4096] = { 0 };
    recv(sockfd, buffer, sizeof(buffer), 0);

    std::string response(buffer);

    EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(response.find("Server stopping"), std::string::npos);

    close(sockfd);
    http_thread.join();

    EXPECT_FALSE(server_running);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}