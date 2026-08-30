# ZeroHTTP

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/c++-11%2B-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)

[English Version](#english-version) | [中文说明](#中文版)

---

<a id="english-version"></a>
## English Version

### Introduction
`ZeroHTTP` is a high-performance, asynchronous HTTP Web Server written in modern C++11 from scratch. It is designed to handle high concurrency using a strictly non-blocking, multi-threaded event-driven architecture, inspired by top-tier frameworks like Muduo and Nginx. 

This project aims to demonstrate the capabilities of Linux system programming, zero-allocation resource management, and robust concurrent architectures.

### Key Features
- **Master-Slave Reactor Architecture**: Utilizes a single Main Reactor for listening and dispatching, and multiple Sub-Reactors (EventLoops) for non-blocking I/O multiplexing.
- **Epoll Edge-Triggered (ET) & EPOLLONESHOT**: Maximizes I/O efficiency with fully drained non-blocking sockets and prevents thread contention on the same file descriptor.
- **Zero-Copy Scatter/Gather Buffer**: Implements `readv` and a dynamic sliding window buffer equipped with 64KB stack-fallback to minimize context switches and heap allocations.
- **Zero-Allocation Object Pool**: Pre-allocates a massive pool of reusable TCP connections avoiding `new`/`delete` fragmentation during high traffic.
- **Double-Buffering Async Logger**: Wait-free background disk logging utilizing atomic pointer `swap()`, ensuring the frontend EventLoops are never blocked by disk I/O.
- **Robust Connection Management**: Implements a `timerfd`-based timing wheel to efficiently clean up dead or slowloris Keep-Alive socket connections.
- **Graceful Shutdown**: Utilizes Linux `eventfd` to notify and wake up all sleeping threads instantly when a `SIGINT` (Ctrl+C) signal is received.
- **Static HTTP Router**: A built-in safe URL router serving HTML/CSS/Images straight from the `www/` directory while preventing path traversal attacks.

### Project Structure
```text
ZeroHTTP/
├── CMakeLists.txt
├── build/       (CMake build artifacts)
├── conf/        (Configuration files e.g., server.conf)
├── docs/        (Architecture and research documentation)
├── include/     (Header files)
├── logs/        (Asynchronous log output directory)
├── src/         (C++ source files)
└── www/         (Static web document root)
```

### Build & Run
1. Ensure your Linux environment has `g++` (supports C++11) and `cmake` installed.
2. Build the project:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```
3. Run the server (the executable `zero_httpd` is generated in the project root):
   ```bash
   cd ..
   ./zero_httpd
   ```
4. Access the server via your browser: `http://127.0.0.1:8080/`
5. Check backend logs at `logs/server.log`. Configuration can be altered via `conf/server.conf`.

---

<a id="中文版"></a>
## 中文版

### 项目简介
`ZeroHTTP` 是一个从零手写的高性能、全异步 C++11 HTTP Web 伺服器框架。它的底层设计深受著名开源网络库 Muduo 与 Nginx 架构的启发，采用了严格的非阻塞事件驱动设计与多核心并发模型。

本项目旨在深研 Linux 底层系统编程、零内存分配（Zero-Allocation）实践以及工业级高并发架构之精髓，是一件为榨干单机硬体效能而生的艺术品。

### 核心特性
- **Master-Slave Reactor 架构**：采用 One-Loop-Per-Thread。主线程专职连线接收与智能分发，多个子反应堆在后台执行 I/O 多路复用。
- **Epoll 边缘触发 (ET) 与 EPOLLONESHOT**：将非阻塞读写压榨到极致，并保证同一 Socket FD 在多线程解析中绝对不会发生数据竞争。
- **Zero-Copy 零拷贝缓冲 (Scatter/Gather I/O)**：抛弃传统的 `std::string` 深拷贝，运用 `readv` 结合 64KB 栈内存分配机制（Stack-fallback），实现真正的内核态零拷贝。
- **无分配对象池 (Object Pool)**：在启动时预先开辟一万个连线对象，面对洪峰流量时实现 `new`/`delete` 零开销，彻底杜绝内存碎片。
- **双缓冲异步日志系统 (Double Buffering Logger)**：前端记录无感知，后端通过 `O(1)` 指针 `swap()` 同步缓冲区。绝佳的无锁态后台刷盘机制。
- **防御性连接管理**：底层整合 `timerfd` 与时间轮算法，高效清理沉默的 Keep-Alive 连线，对 Slowloris 攻击免疫。
- **优雅关机机制**：透过轻量化内核机制 `eventfd`，在接收到 `SIGINT` (Ctrl+C) 信号瞬间，无损唤醒全部休眠线程实现资源回收。
- **HTTP 静态路由器**：自带抵御路径穿越攻击 (Directory Traversal Attack) 的静态路由，支持全类型多媒体二进位文件读取。

### 目录结构
```text
ZeroHTTP/
├── CMakeLists.txt
├── build/       (CMake 编译中间文件存放区)
├── conf/        (设定档目录，如 server.conf)
├── docs/        (架构解析与重构历程文档)
├── include/     (所有的 .h 头文件)
├── logs/        (异步双缓冲日志输出位置)
├── src/         (所有的 C++ 源代码文件)
└── www/         (HTTP 服务器静态资源根目录)
```

### 编译与执行
1. 请确保你的 Linux 环境已安装支援 C++11 的 `g++` 编译器以及 `cmake`。
2. 编译专案：
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```
3. 运行伺服器（编译出来的执行档 `zero_httpd` 会自动生成在项目根目录）：
   ```bash
   cd ..
   ./zero_httpd
   ```
4. 打开浏览器连线：`http://127.0.0.1:8080/`
5. 你可以查看 `logs/server.log` 观测后端运作逻辑，或修改 `conf/server.conf` 进行热配置。

---

