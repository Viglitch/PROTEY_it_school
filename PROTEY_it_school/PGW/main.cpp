int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(5000);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    std::cout << "PGW Server started on port 5000\n";

    while (true) {
        char buffer[1024];
        sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0,
            (struct sockaddr*)&client_addr, &len);

        if (bytes_received > 0) {
            std::thread(handle_client, sockfd, client_addr, buffer, bytes_received).detach();
        }
    }

    close(sockfd);
    return 0;
}