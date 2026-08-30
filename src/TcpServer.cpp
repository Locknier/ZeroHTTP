#include "TcpServer.h"
#include "Config.h"
#include "WakeUpFd.h"
#include "AsyncLogger.h"
#include "SignalHandler.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>

TcpServer::TcpServer() : master_epoll_fd_(-1) {
    int port = Config::getInstance().getInt("PORT", 8080);
    int worker_threads = Config::getInstance().getInt("WORKER_THREADS", 4);
    int sub_reactors_count = Config::getInstance().getInt("SUB_REACTORS", 4);

    WakeUpFd::init();
    master_epoll_fd_ = epoll_create(1);
    
    acceptor_ = std::make_unique<Acceptor>(port);
    pool_ = std::make_unique<ThreadPool>(worker_threads);

    for (int i = 0; i < sub_reactors_count; ++i) {
        sub_reactors_.push_back(std::make_unique<EventLoop>());
        sub_reactors_.back()->setThreadPool(pool_.get());
    }

    LOG_INFO << "[TcpServer] Engine initialized. Sub-Reactors: " << sub_reactors_count << ", Workers: " << worker_threads;
    LOG_INFO << "[TcpServer] Listening on Port: " << port;
}

TcpServer::~TcpServer() {
    stop();
    if (master_epoll_fd_ >= 0) close(master_epoll_fd_);
}

void TcpServer::start() {
    SignalHandler::setup();

    for (size_t i = 0; i < sub_reactors_.size(); ++i) {
        sub_threads_.emplace_back([this, i]() {
            sub_reactors_[i]->loop();
        });
    }

    dispatchLoop();
}

void TcpServer::stop() {
    if (!EventLoop::g_is_running) return;
    
    LOG_INFO << "[TcpServer] Stopping and closing all connections gracefully...";
    EventLoop::g_is_running = false;
    WakeUpFd::wakeup();

    for (auto& t : sub_threads_) {
        if (t.joinable()) t.join();
    }
}

void TcpServer::dispatchLoop() {
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = acceptor_->getFd();
    epoll_ctl(master_epoll_fd_, EPOLL_CTL_ADD, acceptor_->getFd(), &event);

    struct epoll_event wakeup_event;
    wakeup_event.events = EPOLLIN;
    wakeup_event.data.fd = WakeUpFd::getFd();
    epoll_ctl(master_epoll_fd_, EPOLL_CTL_ADD, WakeUpFd::getFd(), &wakeup_event);

    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];

    while (EventLoop::g_is_running) {
        int num_events = epoll_wait(master_epoll_fd_, events, MAX_EVENTS, -1);
        
        if (num_events < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < num_events; i++) {
            int current_fd = events[i].data.fd;

            if (current_fd == WakeUpFd::getFd()) {
                LOG_INFO << "[TcpServer] Shutdown signal received via WakeUpFd.";
                uint64_t one;
                read(WakeUpFd::getFd(), &one, sizeof(one));
                continue;
            }

            if (current_fd == acceptor_->getFd()) {
                struct sockaddr_in client_addr;
                socklen_t len = sizeof(client_addr);
                int client_fd = acceptor_->acceptConnection(&client_addr, &len);

                if (client_fd >= 0) {
                    // Least Connections Dispatching
                    int best_reactor = 0;
                    int min_conns = sub_reactors_[0]->getConnectionCount();
                    
                    for (size_t j = 1; j < sub_reactors_.size(); j++) {
                        int count = sub_reactors_[j]->getConnectionCount();
                        if (count < min_conns) {
                            min_conns = count;
                            best_reactor = j;
                        }
                    }

                    LOG_INFO << "[TcpServer] Accepted new connection (fd: " << client_fd 
                             << ") -> Dispatched to Sub-Reactor " << best_reactor;
                    sub_reactors_[best_reactor]->addConnection(client_fd);
                }
            }
        }
    }
}
