#pragma once
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include <sstream>

// 异步双缓冲日志系统
class AsyncLogger {
public:
    static AsyncLogger& getInstance() {
        static AsyncLogger instance;
        return instance;
    }

    void start(const std::string& basename = "server.log");
    void stop();
    void append(const std::string& msg);

private:
    AsyncLogger() : running_(false) {}
    ~AsyncLogger() { stop(); }

    void threadFunc();

    std::string basename_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;

    using Buffer = std::string;
    using BufferPtr = std::unique_ptr<Buffer>;

    static const size_t kBufferSize = 1024 * 1024; // 1MB 缓冲大小

    BufferPtr current_buffer_;
    BufferPtr next_buffer_;
    std::vector<BufferPtr> buffers_;
};

class LogStream {
public:
    LogStream() {}
    ~LogStream() {
        AsyncLogger::getInstance().append(ss_.str() + "\n");
    }
    template<typename T>
    LogStream& operator<<(const T& v) {
        ss_ << v;
        return *this;
    }
private:
    std::ostringstream ss_;
};

#define LOG_INFO LogStream()
