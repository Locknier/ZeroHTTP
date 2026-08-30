#pragma once
#include <vector>
#include <string>
#include <algorithm>

// Zero-Copy 分散收集缓冲区
class Buffer {
public:
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initial_size = kInitialSize)
        : buffer_(initial_size), read_index_(0), write_index_(0) {}

    size_t readableBytes() const { return write_index_ - read_index_; }
    size_t writableBytes() const { return buffer_.size() - write_index_; }
    size_t prependableBytes() const { return read_index_; }

    const char* peek() const { return buffer_.data() + read_index_; }

    void retrieve(size_t len) {
        if (len < readableBytes()) {
            read_index_ += len;
        } else {
            retrieveAll();
        }
    }

    void retrieveAll() {
        read_index_ = 0;
        write_index_ = 0;
    }

    void append(const char* data, size_t len) {
        ensureSpace(len);
        std::copy(data, data + len, buffer_.data() + write_index_);
        write_index_ += len;
    }

    ssize_t readFd(int fd, int* savedErrno);

private:
    void ensureSpace(size_t len) {
        if (writableBytes() < len) {
            if (writableBytes() + prependableBytes() < len) {
                buffer_.resize(write_index_ + len);
            } else {
                size_t readable = readableBytes();
                std::copy(buffer_.data() + read_index_, 
                          buffer_.data() + write_index_, 
                          buffer_.data());
                read_index_ = 0;
                write_index_ = readable;
            }
        }
    }

    std::vector<char> buffer_;
    size_t read_index_;
    size_t write_index_;
};
