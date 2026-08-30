#include "AsyncLogger.h"
#include <fstream>
#include <iostream>
#include <chrono>

void AsyncLogger::start(const std::string& basename) {
    if (running_) return;
    basename_ = basename;
    running_ = true;
    
    current_buffer_ = std::make_unique<Buffer>();
    current_buffer_->reserve(kBufferSize);
    next_buffer_ = std::make_unique<Buffer>();
    next_buffer_->reserve(kBufferSize);
    
    thread_ = std::thread(&AsyncLogger::threadFunc, this);
}

void AsyncLogger::stop() {
    if (!running_) return;
    running_ = false;
    cond_.notify_one();
    if (thread_.joinable()) thread_.join();
}

void AsyncLogger::append(const std::string& msg) {
    if (!running_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (current_buffer_->size() + msg.size() < kBufferSize) {
        current_buffer_->append(msg);
    } else {
        buffers_.push_back(std::move(current_buffer_));
        if (next_buffer_) {
            current_buffer_ = std::move(next_buffer_);
        } else {
            current_buffer_ = std::make_unique<Buffer>();
            current_buffer_->reserve(kBufferSize);
        }
        current_buffer_->append(msg);
        cond_.notify_one(); 
    }
}

void AsyncLogger::threadFunc() {
    std::ofstream output(basename_, std::ios::app);
    
    BufferPtr newBuffer1 = std::make_unique<Buffer>();
    newBuffer1->reserve(kBufferSize);
    BufferPtr newBuffer2 = std::make_unique<Buffer>();
    newBuffer2->reserve(kBufferSize);
    
    std::vector<BufferPtr> buffersToWrite;
    buffersToWrite.reserve(16);

    while (running_) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (buffers_.empty()) {
                cond_.wait_for(lock, std::chrono::seconds(3));
            }
            
            buffers_.push_back(std::move(current_buffer_));
            current_buffer_ = std::move(newBuffer1);
            
            buffersToWrite.swap(buffers_);
            
            if (!next_buffer_) {
                next_buffer_ = std::move(newBuffer2);
            }
        }

        for (const auto& buf : buffersToWrite) {
            output.write(buf->data(), buf->size());
        }
        output.flush();

        if (buffersToWrite.size() > 2) {
            buffersToWrite.resize(2); 
        }

        if (!newBuffer1) {
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->clear();
        }
        if (!newBuffer2 && !buffersToWrite.empty()) {
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->clear();
        }
        
        buffersToWrite.clear();
    }
}
