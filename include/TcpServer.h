#pragma once
#include "IServer.h"
#include "Acceptor.h"
#include "ThreadPool.h"
#include "EventLoop.h"
#include <vector>
#include <thread>
#include <memory>

class TcpServer : public IServer {
public:
    TcpServer();
    ~TcpServer() override;

    void start() override;
    void stop() override;

private:
    void dispatchLoop();

    int master_epoll_fd_;
    std::unique_ptr<Acceptor> acceptor_;
    std::unique_ptr<ThreadPool> pool_;
    
    std::vector<std::unique_ptr<EventLoop>> sub_reactors_;
    std::vector<std::thread> sub_threads_;
};
