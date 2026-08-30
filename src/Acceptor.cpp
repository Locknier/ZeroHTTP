#include "Acceptor.h"
#include "AsyncLogger.h"
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>

Acceptor::Acceptor(int port) : listen_fd_(-1) {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) throw std::runtime_error("Acceptor socket creation failed");

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(listen_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_fd_);
        throw std::runtime_error("Acceptor bind failed");
    }

    if (listen(listen_fd_, SOMAXCONN) < 0) {
        close(listen_fd_);
        throw std::runtime_error("Acceptor listen failed");
    }
}

Acceptor::~Acceptor() {
    if (listen_fd_ >= 0) {
        close(listen_fd_);
        LOG_INFO << "[Acceptor] Listen socket closed.";
    }
}

int Acceptor::getFd() const { return listen_fd_; }

int Acceptor::acceptConnection(struct sockaddr_in* client_addr, socklen_t* client_len) {
    return accept(listen_fd_, (struct sockaddr*)client_addr, client_len);
}
