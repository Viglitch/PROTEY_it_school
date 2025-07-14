#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <cstring>
#include <cmath>
#include <algorithm>

constexpr int MAX_EVENTS = 10;
constexpr int BUFFER_SIZE = 1024;

double calculate_expression(const std::string& expr) {
    std::istringstream iss(expr);
    double result = 0;
    char op;
    double num;

    if (!(iss >> result)) {
        throw std::runtime_error("Invalid expression");
    }

    while (iss >> op >> num) {
        switch (op) {
        case '+': result += num; break;
        case '-': result -= num; break;
        case '*': result *= num; break;
        case '/':
            if (num == 0) throw std::runtime_error("Division by zero");
            result /= num;
            break;
        case '^': result = pow(result, num); break;
        default:
            throw std::runtime_error("Invalid operator");
        }
    }

    return result;
}

void handle_client_data(int fd, char* buffer, int bytes_read) {
    static std::unordered_map<int, std::string> client_buffers;

    client_buffers[fd] += std::string(buffer, bytes_read);

    size_t pos;
    while ((pos = client_buffers[fd].find('\n')) != std::string::npos) {
        std::string expr = client_buffers[fd].substr(0, pos);
        client_buffers[fd].erase(0, pos + 1);

        try {
            double result = calculate_expression(expr);
            std::string response = std::to_string(result) + "\n";
            send(fd, response.c_str(), response.size(), 0);
        }
        catch (const std::exception& e) {
            std::string error = "ERROR: " + std::string(e.what()) + "\n";
            send(fd, error.c_str(), error.size(), 0);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    int server_fd, epoll_fd;
    struct sockaddr_in address;
    struct epoll_event ev, events[MAX_EVENTS];

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl: server_fd");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port " << port << std::endl;

    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }

                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                std::cout << "New connection from " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;

                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    perror("epoll_ctl: client_fd");
                    close(client_fd);
                }
            }
            else {
                int client_fd = events[i].data.fd;
                char buffer[BUFFER_SIZE];
                int bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);

                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        std::cout << "Client disconnected" << std::endl;
                    }
                    else {
                        perror("recv");
                    }
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                }
                else {
                    handle_client_data(client_fd, buffer, bytes_read);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}