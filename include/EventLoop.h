#pragma once
#include <sys/epoll.h>
#include <unordered_map>
#include <memory>
#include "TcpConnection.h"
#include "ThreadPool.h"
#include "ConnectionPool.h"
#include <vector>
#include <atomic>

class EventLoop {
public:
    static std::atomic<bool> g_is_running;

    EventLoop();
    ~EventLoop();

    void loop();
    
    size_t getConnectionCount() const {
        return connections_.size();
    }

    void addConnection(int client_fd);
    void setThreadPool(ThreadPool* pool) { pool_ = pool; }

private:
    void handleRead(int client_fd);
    void handleWrite(int client_fd);
    void handleTimer();

    int epoll_fd_;
    int timer_fd_;
    std::unordered_map<int, TcpConnection*> connections_;
    ConnectionPool conn_pool_;
    ThreadPool* pool_;
};
