#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <random>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <cstring>
#include <algorithm>

constexpr int MAX_EVENTS = 10;
constexpr int BUFFER_SIZE = 1024;

std::string generate_expression(int n) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> num_dist(1, 100);
    static std::uniform_int_distribution<> op_dist(0, 4);

    static const char ops[] = { '+', '-', '*', '/', '^' };

    std::ostringstream oss;
    oss << num_dist(gen);

    for (int i = 1; i < n; ++i) {
        char op = ops[op_dist(gen)];
        int num = num_dist(gen);

        if (op == '/' && num == 0) {
            num = 1;
        }

        oss << ' ' << op << ' ' << num;
    }

    return oss.str();
}

std::vector<std::string> split_into_fragments(const std::string& expr, int fragments) {
    std::vector<std::string> result;
    if (fragments <= 1 || expr.size() < fragments) {
        result.push_back(expr);
        return result;
    }

    int avg_size = expr.size() / fragments;
    int remainder = expr.size() % fragments;
    int pos = 0;

    for (int i = 0; i < fragments; ++i) {
        int size = avg_size + (i < remainder ? 1 : 0);
        result.push_back(expr.substr(pos, size));
        pos += size;
    }

    return result;
}

double calculate_expression(const std::string& expr) {
    std::istringstream iss(expr);
    double result = 0;
    char op;
    double num;

    iss >> result;

    while (iss >> op >> num) {
        switch (op) {
        case '+': result += num; break;
        case '-': result -= num; break;
        case '*': result *= num; break;
        case '/': result /= num; break;
        case '^': result = pow(result, num); break;
        default: throw std::runtime_error("Invalid operator");
        }
    }

    return result;
}

void run_verification_session(const std::string& server_addr, int server_port, int n, int session_num) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_addr.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/Address not supported" << std::endl;
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return;
    }

    std::string expr = generate_expression(n);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(1, 5);
    int fragments = dist(gen);
    auto fragments_vec = split_into_fragments(expr + "\n", fragments);

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(sock);
        return;
    }

    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLOUT;
    ev.data.fd = sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev) == -1) {
        perror("epoll_ctl: sock");
        close(sock);
        close(epoll_fd);
        return;
    }

    bool writing = true;
    size_t current_fragment = 0;
    std::string response;
    bool success = false;

    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 10000);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }

        if (nfds == 0) {
            std::cerr << "Timeout in session " << session_num << std::endl;
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (writing && (events[i].events & EPOLLOUT)) {
                if (current_fragment < fragments_vec.size()) {
                    const std::string& fragment = fragments_vec[current_fragment];
                    ssize_t sent = send(fd, fragment.c_str(), fragment.size(), 0);
                    if (sent < 0) {
                        perror("send");
                        close(fd);
                        close(epoll_fd);
                        return;
                    }
                    current_fragment++;
                }

                if (current_fragment >= fragments_vec.size()) {
                    writing = false;
                    ev.events = EPOLLIN;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
                        perror("epoll_ctl: mod to read");
                        close(fd);
                        close(epoll_fd);
                        return;
                    }
                }
            }
            else if (!writing && (events[i].events & EPOLLIN)) {
                char buffer[BUFFER_SIZE];
                ssize_t bytes_read = recv(fd, buffer, BUFFER_SIZE, 0);
                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        success = true;
                    }
                    else {
                        perror("recv");
                    }
                    close(fd);
                    close(epoll_fd);
                    goto verification;
                }
                else {
                    response.append(buffer, bytes_read);
                    if (response.find('\n') != std::string::npos) {
                        close(fd);
                        close(epoll_fd);
                        success = true;
                        goto verification;
                    }
                }
            }
        }
    }

verification:
    if (success) {
        try {
            double server_result = std::stod(response);
            double expected_result = calculate_expression(expr);

            if (fabs(server_result - expected_result) > 1e-9) {
                std::cerr << "ERROR in session " << session_num << ":\n"
                    << "Expression: " << expr << "\n"
                    << "Server result: " << server_result << "\n"
                    << "Expected result: " << expected_result << std::endl;
            }
            else {
                std::cout << "Session " << session_num << " verified successfully" << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "ERROR in session " << session_num << ":\n"
                << "Expression: " << expr << "\n"
                << "Server response: " << response << "\n"
                << "Error: " << e.what() << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <n> <connections> <server_addr> <server_port>" << std::endl;
        return 1;
    }

    int n = std::stoi(argv[1]);
    int connections = std::stoi(argv[2]);
    std::string server_addr = argv[3];
    int server_port = std::stoi(argv[4]);

    for (int i = 0; i < connections; ++i) {
        run_verification_session(server_addr, server_port, n, i + 1);
    }

    return 0;
}