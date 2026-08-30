#include "EventLoop.h"
#include "Config.h"
#include "HttpParser.h"
#include "WakeUpFd.h"
#include "AsyncLogger.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <sys/timerfd.h>
#include <iostream>

#define MAX_EVENTS 1024

std::atomic<bool> EventLoop::g_is_running{true};

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

EventLoop::EventLoop() : pool_(nullptr) {
    epoll_fd_ = epoll_create(1);
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    
    struct itimerspec ts{};
    ts.it_value.tv_sec = 5;
    ts.it_interval.tv_sec = 5;
    timerfd_settime(timer_fd_, 0, &ts, NULL);
    
    if (epoll_fd_ >= 0) {
        struct epoll_event timer_event;
        timer_event.events = EPOLLIN;
        timer_event.data.fd = timer_fd_;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd_, &timer_event);
    }
}

EventLoop::~EventLoop() {
    if (timer_fd_ >= 0) close(timer_fd_);
    if (epoll_fd_ >= 0) close(epoll_fd_);
}

void EventLoop::addConnection(int client_fd) {
    set_nonblocking(client_fd);
    TcpConnection* conn = conn_pool_.acquire(client_fd);
    if (!conn) {
        LOG_INFO << "[EventLoop] Connection pool exhausted. Dropping fd: " << client_fd;
        close(client_fd);
        return;
    }

    connections_[client_fd] = conn;
    
    // EPOLLONESHOT 机制确保同一 fd 不会被多个线程争用
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    event.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &event);
}

void EventLoop::handleTimer() {
    uint64_t expirations;
    read(timer_fd_, &expirations, sizeof(expirations));

    std::vector<int> timeout_fds;
    int timeout_sec = Config::getInstance().getInt("KEEPALIVE_TIMEOUT_SEC", 15);
    for (auto& pair : connections_) {
        if (pair.second->isTimeout(timeout_sec)) {
            timeout_fds.push_back(pair.first);
        }
    }

    for (int fd : timeout_fds) {
        LOG_INFO << "[EventLoop] Connection timeout. Kicking fd: " << fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, NULL);
        conn_pool_.release(connections_[fd]);
        connections_.erase(fd);
    }
}

void EventLoop::handleRead(int client_fd) {
    if (connections_.find(client_fd) == connections_.end()) return;
    auto conn = connections_[client_fd];

    int savedErrno = 0;
    while (true) {
        ssize_t n = conn->getReadBuffer().readFd(client_fd, &savedErrno);
        
        if (n < 0) {
            if (savedErrno == EAGAIN) break; // 缓冲区数据已读尽
            LOG_INFO << "[EventLoop] Client read error (Error: " << strerror(savedErrno) << "), fd: " << client_fd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, NULL);
            conn_pool_.release(connections_[client_fd]);
            connections_.erase(client_fd);
            return;
        } else if (n == 0) {
            LOG_INFO << "[EventLoop] Client disconnected naturally, fd: " << client_fd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, NULL);
            conn_pool_.release(connections_[client_fd]);
            connections_.erase(client_fd);
            return;
        } else {
            conn->updateLastActiveTime(); // 刷新活跃时间
        }
    }

    std::string snapshot(conn->getReadBuffer().peek(), conn->getReadBuffer().readableBytes());
    if (snapshot.empty()) return;

    if (pool_) {
        int my_epoll_fd = epoll_fd_;
        pool_->enqueue([client_fd, snapshot, conn, my_epoll_fd]() {
            std::string method, path;
            ParseResult result = HttpParser::parse(snapshot, method, path);

            if (result == ParseResult::INCOMPLETE) {
                // 收到半包，重新武装 ONESHOT
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                ev.data.fd = client_fd;
                epoll_ctl(my_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
            } else if (result == ParseResult::SUCCESS) {
                LOG_INFO << "[Worker] Parse success. Method: " << method << ", Path: " << path;
                
                std::string http_response = HttpParser::buildStaticResponse(path);
                int bytes_written = write(client_fd, http_response.c_str(), http_response.length());
                
                if (bytes_written < (int)http_response.length()) {
                    int actual_written = (bytes_written < 0) ? 0 : bytes_written;
                    conn->getWriteBuffer().append(http_response.data() + actual_written, http_response.length() - actual_written);
                    conn->updateLastActiveTime();
                    
                    struct epoll_event ev;
                    ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT;
                    ev.data.fd = client_fd;
                    epoll_ctl(my_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
                } else if (bytes_written == (int)http_response.length()) {
                    conn->updateLastActiveTime();
                    
                    struct epoll_event ev;
                    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                    ev.data.fd = client_fd;
                    epoll_ctl(my_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
                }

                conn->getReadBuffer().retrieve(snapshot.length());
            } else if (result == ParseResult::ERROR) {
                LOG_INFO << "[Worker] Malformed request (fd: " << client_fd << ")";
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                ev.data.fd = client_fd;
                epoll_ctl(my_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
            }
        });
    }
}

void EventLoop::handleWrite(int client_fd) {
    if (connections_.find(client_fd) == connections_.end()) return;
    auto conn = connections_[client_fd];

    Buffer& wbuf = conn->getWriteBuffer();
    if (wbuf.readableBytes() == 0) {
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        ev.data.fd = client_fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev);
        return;
    }

    int bytes_written = write(client_fd, wbuf.peek(), wbuf.readableBytes());
    if (bytes_written > 0) {
        wbuf.retrieve(bytes_written);
        conn->updateLastActiveTime();
        
        if (wbuf.readableBytes() == 0) {
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
            ev.data.fd = client_fd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev);
        } else {
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT;
            ev.data.fd = client_fd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev);
        }
    } else if (bytes_written < 0 && errno != EAGAIN) {
        LOG_INFO << "[EventLoop] Client disconnected during async write.";
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, NULL);
        conn_pool_.release(connections_[client_fd]);
        connections_.erase(client_fd);
    }
}

void EventLoop::loop() {
    struct epoll_event events[MAX_EVENTS];
    
    while (g_is_running) {
        int num_events = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
        
        if (num_events < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < num_events; i++) {
            int current_fd = events[i].data.fd;

            if (current_fd == timer_fd_) {
                handleTimer();
                continue;
            }

            if (events[i].events & EPOLLIN) {
                handleRead(current_fd);
            }
            
            if (events[i].events & EPOLLOUT) {
                handleWrite(current_fd);
            }
        }
    }
}
