#pragma once
#include <netinet/in.h>

class Acceptor {
public:
    explicit Acceptor(int port);
    ~Acceptor();
    int getFd() const;
    int acceptConnection(struct sockaddr_in* client_addr, socklen_t* client_len);
private:
    int listen_fd_;
};
