#pragma once
#include <string>
#include <chrono>
#include <unistd.h>
#include "Buffer.h"

// 封装单一客户端连接的状态和资源
class TcpConnection {
public:
    TcpConnection() : fd_(-1) {}

    ~TcpConnection() {
        if (fd_ >= 0) closeSocket();
    }

    void init(int fd) {
        fd_ = fd;
        updateLastActiveTime();
    }

    void reset() {
        read_buffer_.retrieveAll();
        write_buffer_.retrieveAll();
        if (fd_ >= 0) closeSocket();
    }

    void closeSocket() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    Buffer& getReadBuffer() { return read_buffer_; }
    Buffer& getWriteBuffer() { return write_buffer_; }

    void updateLastActiveTime() { 
        last_active_time_ = std::chrono::steady_clock::now(); 
    }
    
    std::chrono::steady_clock::time_point getLastActiveTime() const { 
        return last_active_time_; 
    }
    
    bool isTimeout(int timeout_seconds) const {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_active_time_).count();
        return duration > timeout_seconds;
    }

    int getFd() const { return fd_; }

private:
    int fd_;
    std::chrono::steady_clock::time_point last_active_time_;
    Buffer read_buffer_;
    Buffer write_buffer_;
};
