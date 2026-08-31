# ZeroHTTP: High-Performance C++11 Asynchronous Web Server

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/c++-11%2B-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)
![Build](https://img.shields.io/badge/build-CMake-green.svg)

> **ZeroHTTP** 是一个从零手写的高性能、全异步 C++11 HTTP Web 服务器框架。底层架构深受 **Muduo** 与 **Nginx** 启发，采用严格的非阻塞事件驱动 (Non-blocking Event-driven) 设计与 One-Loop-Per-Thread 多核心并发模型。

本作旨在深研 Linux 底层系统编程、**Zero-Copy (零拷贝)** 机制、**Zero-Allocation (零动态分配)** 实践，以及工业级高并发架构之精髓，是一件为榨干单机硬件效能而生的艺术品。

---

## 🚀 核心极致优化 (Core Optimizations)

### 1. 彻底消灭数据竞争 (Race Condition Free)
* **痛点**：在 Epoll 边缘触发 (ET) 模式下，同一个 Socket 的数据若分次抵达，极易触发多个 ThreadPool 线程同时读写该 Socket，导致 Context 严重错乱。
* **解法**：全面引入 `EPOLLONESHOT` 机制。确保 Socket 触发读写事件后自动于 Epoll 中“隐身”，直到工作线程完成 HTTP 解析并针对半包/全包状态使用 `EPOLL_CTL_MOD` 重新武装，从根源上保证并发环境的线程安全。

### 2. Zero-Copy 零拷贝与 64KB Stack-fallback
* **痛点**：传统网络缓冲区依赖 `std::string::append` 或巨型 Heap `new`，频繁的系统调用 (System Calls) 与深拷贝会导致极高的延迟与 CPU Cache 失效。
* **解法**：舍弃传统字符串拼接，底层采用操作系统原生 API `readv` 实现 Scatter/Gather I/O 分散收集机制。结合 64KB 的栈内存 (Stack-fallback)，超大 HTTP 数据包也能一次性切入 Stack 中，大幅减少 `read` 调用次数，完成极致的 Cache-friendly 吞吐。

### 3. 无分配对象池 (Zero-Allocation Object Pool)
* **痛点**：面对每秒上万次并发请求，频繁的 `new TcpConnection()` 会导致 OS 堆区 (Heap) 产生千疮百孔的内存碎片，引发 OOM 或分配速度断崖下跌。
* **解法**：于服务器启动时，一次性预配置上万个连接对象与 `free_list_`。在洪峰流量来袭时，新连接的分发仅为 $O(1)$ 的指针弹出，达成服务器生命周期内的 Zero-Allocation。

### 4. 双缓冲异步日志 (Double-Buffering Async Logger)
* **痛点**：高并发下直接使用 `std::cout` 或普通 Mutex 写文件，会让底层磁盘 I/O 严重阻塞 Reactor 的事件派发循环，拖垮整个系统吞吐。
* **解法**：前端业务线程仅将日志写入 1MB 内存区块，后台写文件线程苏醒时，使用底层 `std::swap` 直接对调前端与自己的缓冲区指针。**这个指针对调动作为 $O(1)$ 时间复杂度且仅需几纳秒**，完美达成前后端无锁解耦 (Lock-free)。

### 5. 防御性连接管理 (Defensive Connection Management)
* **痛点**：恶意连接 (如 Slowloris 攻击) 会霸占 Socket fd 不放，导致服务器资源枯竭。
* **解法**：利用 `timerfd` 设计底层时间轮算法 (Timing Wheel)，每 5 秒批次剔除恶意占用或超时的 Keep-Alive 连接，对攻击全面免疫。

---

## 📊 系统架构图解 (Architecture)

```text
                      +-----------------------------------+
                      |      Incoming HTTP Requests       |
                      +-----------------+-----------------+
                                        | (Least Connections Dispatch)
                                        v
                       +----------------------------------+
                       |      Master Reactor (Epoll)      |
                       +----------------+-----------------+
                                        |
                 +----------------------+----------------------+
                 |                      |                      |
                 v                      v                      v
     +-------------------+  +-------------------+  +-------------------+
     | Sub-Reactor 1     |  | Sub-Reactor 2     |  | Sub-Reactor N     |
     | (Epoll + ONESHOT) |  | (Epoll + ONESHOT) |  | (Epoll + ONESHOT) |
     +---------+---------+  +---------+---------+  +---------+---------+
               |                      |                      |
               +----------------------+----------------------+
                                      |
                                      v (Async Parsing)
                             +-------------------+
                             |    ThreadPool     |
                             | (Worker Threads)  |
                             +--------+----------+
                                      |
                                      v (Lock-free O(1) swap)
                             +-------------------+
                             |  AsyncLogger disk |
                             +-------------------+
```

---

## 📂 源码结构与职责分布

本项目采用最严谨的微服务/引擎开发目录结构设计：

```text
ZeroHTTP/
├── CMakeLists.txt              # 现代 C++ 构建脚本
├── README.md                   # 本说明文件
├── build/                      # Out-of-source 构建中间档目录
├── conf/
│   └── server.conf             # 服务器动态配置文件 (Port, Threads, Timeout)
├── include/                    # 头文件 (API 接口层)
│   ├── TcpServer.h             # 封装好的 Facade 外观模式服务器引擎
│   ├── EventLoop.h             # 核心 Sub-Reactor 事件循环
│   ├── Buffer.h                # Zero-Copy 内存缓冲区机制
│   └── ...
├── src/                        # 源码 (实现层)
│   ├── main.cpp                # 服务器启动脚本
│   ├── HttpParser.cpp          # HTTP 静态路由器与解析引擎
│   ├── AsyncLogger.cpp         # 双缓冲日志引擎
│   └── ...
├── logs/                       # AsyncLogger 异步写文件输出区 (server.log)
└── www/                        # HTTP 服务器挂载之静态资源根目录 (HTML/CSS)
```

---

## 🛠️ 快速开始与编译指南

### 环境要求
* C++ 支持至 C++11 (默认 g++ 或 clang++)
* CMake >= 3.10
* OS: Linux (支持 Epoll 与 eventfd/timerfd 等特性)

### 获取与编译步骤

```bash
# 1. Clone 项目并进入目录
git clone https://github.com/<your-username>/ZeroHTTP.git
cd ZeroHTTP

# 2. 创建并进入 build 暂存目录
mkdir -p build && cd build

# 3. 执行 CMake 配置
cmake ..

# 4. 进行多线程并行编译
make -j4

# 5. 编译完成，执行档会产出于项目根目录
cd ..
./zero_httpd
```

### 配置与服务测试
1. 服务器默认读取 `conf/server.conf` 中的配置 (默认启动 `PORT=8080`)。
2. 打开浏览器访问：[http://127.0.0.1:8080/](http://127.0.0.1:8080/)
3. 可以在 `logs/server.log` 观察服务器极高效率的异步 I/O 调度状况。如果你拥有 Nginx 架设经验，强烈建议将 Nginx 放置于前端作为 SSL 卸载 (SSL Termination)，与后端 `ZeroHTTP` 达成最强双剑合璧！

---
