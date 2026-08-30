#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include "TcpConnection.h"
#include "AsyncLogger.h"

// 零分配对象池机制
class ConnectionPool {
public:
    ConnectionPool(size_t pool_size = 10000) {
        for (size_t i = 0; i < pool_size; ++i) {
            pool_.push_back(std::make_unique<TcpConnection>());
            free_list_.push_back(pool_.back().get());
        }
        LOG_INFO << "[ConnectionPool] Initialized with " << pool_size << " reusable connections.";
    }

    TcpConnection* acquire(int fd) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (free_list_.empty()) return nullptr;
        TcpConnection* conn = free_list_.back();
        free_list_.pop_back();
        conn->init(fd);
        return conn;
    }

    void release(TcpConnection* conn) {
        if (!conn) return;
        conn->reset();
        std::lock_guard<std::mutex> lock(mutex_);
        free_list_.push_back(conn);
    }

private:
    std::vector<std::unique_ptr<TcpConnection>> pool_;
    std::vector<TcpConnection*> free_list_;
    std::mutex mutex_;
};
