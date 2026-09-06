# ZeroHTTP: High-Performance Static Web Server

ZeroHTTP 是一个基于自研事件驱动网络库 (`ZeroNet`) 构建的高性能静态 Web 服务器。通过深度的 I/O 层面优化与内存管理，在处理高吞吐量静态资源分配时，展现出色的低延迟与高并发能力。

ZeroHTTP is a high-performance static Web server built on a self-developed event-driven network library (`ZeroNet`). Through deep I/O level optimization and memory management, it demonstrates excellent low latency and high concurrency capabilities when handling high-throughput static resource distribution.

## 核心亮点 / Core Highlights

- **基于 ZeroNet 网络基建 (Based on ZeroNet Infrastructure)**
  摒弃单体架构，将核心 I/O 处理剥离至 `ZeroNet` 框架，利用 Reactor 模型 + 线程池 (`EventLoop` + `Epoll Edge-Triggered`) 处理上万级并发 TCP 报文连接。
  *Eliminates monolithic architecture by decoupling the core I/O handling into the `ZeroNet` framework. Uses Reactor pattern + Thread Pool (`EventLoop` + `Epoll Edge-Triggered`) to manage tens of thousands of concurrent TCP connections.*

- **Sendfile 零拷贝直传 (Sendfile Zero-Copy Transmission)**
  不再使用 `std::ifstream` 读入字符串的传统做法。在对外发送静态文件（HTML/CSS/图片）时，通过 `sendfile` 系统调用使得磁盘数据直通网卡，避免拷贝到户态（User Space），彻底粉碎大文件传输的内存消耗瓶颈。
  *Abandons the traditional `std::ifstream` into string approach. When serving static files, invokes the `sendfile` system call to stream data directly from OS disk cache to the socket buffer, bypassing User Space and obliterating memory bottlenecks for large files.*

- **内存指针偏移替代字符串拷贝 (Pointer Offset replacing String Copy)**
  在解析 HTTP 请求标头时，弃用大量新建 `std::string` 和 `string::find` 的昂贵操作，改用原生内存探针 (`strstr`) 和指针减法以 `O(1)` 时间复杂度获取 Header 大小，极大保护了 CPU L1/L2 Cache 命中率并消除了堆内存分配产生的碎片。
  *When parsing HTTP headers, abandons expensive `std::string` object creation and `.find()` operations. Instead, it utilizes raw memory probing (`strstr`) and pointer arithmetic to retrieve header lengths in `O(1)` time, protecting CPU cache rates and eliminating heap memory fragmentation.*

## 架构概览 / Architecture Overview

1. `ZeroNet/TcpServer` 接收前端报文连接，负载均衡至特定的子 `EventLoop` 工作线程中。
2. 工作线程触发 `MessageCallback`，将原始缓冲区的**连续内存指针**注入至 `HttpParser`。
3. `HttpParser` 执行半包黏包验证，并定位请求路径。
4. 获取 File Descriptor，通过 `sendfile` 直接将文件响应发送回 Socket。

## 编译运行 / Build & Run

```bash
# 生成 cmake 编译系统 / Generate CMake build
mkdir build && cd build
cmake ..

# 编译项目 / Compile
make

# 运行服务器 / Run Server
./ZeroHTTPApp
```
