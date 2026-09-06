# ZeroHTTP 项目开发复盘与核心技术推演 (Memory Vault)

> 这份文档记录了我们在开发 `ZeroHTTP` 期间的完整交流、探讨过程以及架构演进的心路历程。
> 目的是当你隔了一段时间想要回想项目细节，或是准备面试前，只要看这份文档就能瞬间找回当时“打通任督二脉”的记忆。

---

## 🌀 全流程串联：一个请求的生命周期 (How it works)

为了让你一眼看懂代码怎么串起来的，我们先走一遍一个 HTTP 请求是怎么被处理的：

1. **启动阶段**：`main.cpp` 调用 `ServerFactory` 创建 `TcpServer`。`TcpServer` 加载配置，启动 1 个监听 `Acceptor`，并预先生成 4 个子反应堆 (`EventLoop`) 和一个 `ThreadPool`。预先分配好包含 1 万个 `TcpConnection` 的 `ConnectionPool`。
2. **连接到来**：主线程 (Master Reactor) 通过 `epoll_wait` 发现监听端口有新客人。调用 `Accept出` 获得 `client_fd`。
3. **负载均衡**：通过 **Least Connections (最少连接数)** 算法，找到当前最闲的一个 Sub-Reactor (子 `EventLoop`)，把这个 `client_fd` 丢给它。
4. **子反应堆接管**：子 EventLoop 将 `client_fd` 加入自己的 Epoll 树，且**强制挂上 `EPOLLONESHOT` 和 `EPOLLET` (边缘触发)** 标签。
5. **读取数据**：客人发来 HTTP 报文，触发 `EPOLLIN`。EventLoop 找到对应的 `TcpConnection`，调用其 `Buffer` 进行非阻塞读取 (用到 `readv` 和 64KB栈内存)。
6. **派发给工作线程**：EventLoop 将读取到的数据拷贝成一份快照 (Snapshot)，连同 `client_fd` 一起打包塞进 `ThreadPool` 的任务队列。此时 EventLoop 本身继续去监听其他 socket。
7. **业务解析**：后台 Worker 线程拿到任务，调用 `HttpParser` 解析 HTTP 报文。
    * 如果解析发现是**半包 (INCOMPLETE)**，线程什么都不处理，直接调用 `epoll_ctl_mod` 重新武装 `EPOLLONESHOT`，让 EventLoop 下次有数据继续监听。
    * 如果解析**完成 (SUCCESS)**，产生 HTTP Response 字符串，写入 socket 或 `WriteBuffer`。
    * *这也是为什么双缓冲日誌如此重要，因为 Worker 线程在解析时会不断写 log，如果日志堵塞，所有 Worker 都会卡死，HTTP 响应就会变慢。*

---

## 🛠️ 演进痛点与解决方案大盘点 (我们是怎么踩坑又爬出来的)

### 痛点 1：烂代码破窗效应 vs 面向对象重构 (Facade & Factory)
* **当时的状况**：最一开始，`main.cpp` 是一大坨高达 100 多行的脚本，把开启 Socket、绑定端口、创建 Epoll 树、开线程全塞在一起，乱如麻。
* **我们的讨论与解法**：我们决定彻底拆耦。将底层的 Socket API 封装装入 `Acceptor`；将事件派发逻辑塞入 `EventLoop`；最后，我们用 **Facade (外观模式)** 打造了 `TcpServer` 类把这些零件全部吃进肚子里。外部只需 `ServerFactory::createServer()->start()` 两行代码，达成了工业级框架的优雅！

### 痛点 2：高并发下的“半包陷阱”与多线程撕裂 (Data Race)
* **当时的状况**：我们换成边缘触发 (ET) 之后，突然发现系统会崩溃。假如客户端的数据被拆成两半发送。前半个包到了，触发 Epoll，线程池派了 **Worker A** 去处理；没过 1 毫秒，后半个包也到了，Epoll 发现有新数据，竟然又派了 **Worker B** 去处理同一个 fd。
* **我们的讨论与解法**：绝对不能让同一个 socket 同时被两个线程操作！我们立刻加上了牛逼的 **`EPOLLONESHOT`**。挂上这个牌子后，Epoll 只要通知过你一次，就当这个 socket 不存在了，哪怕后半个包到了也不会再触发。直到 Worker A 把活干完，手动对它进行重新武装（`EPOLL_CTL_MOD`），才允许下一次监听。这是你面试最好的武器！

### 痛点 3：消除堆内存碎片的 Zero-Allocation (对象池)
* **当时的状况**：网络层要面对极高频率的连接与断开。如果我们每次连线都 `new TcpConnection`，断开就 `delete`，OS 的堆内存 (Heap) 会产生大量空洞（内存碎片），且有系统调用的锁开销。
* **我们的讨论与解法**：利用 **Object Pool (对象池)** 思想开发了 `ConnectionPool`。我们在程序启动前，就在底层开了一个超大的 `std::vector` 装满 10000 个预备好的对象，并用 `free_list_` 管理。客人来了，O(1) 直接拿走一个指针，客人走了 `reset()` 重置状态并还回指针。服务器在生命周期内实现了“零动态分配”，极限提速。

### 痛点 4：榨干 CPU Cache 的 Scatter/Gather I/O (`readv`)
* **当时的状况**：为了接收 HTTP 封包，传统的写法要不是申请一块 1MB 的 buffer (太浪费内存)，要不就是用 `std::string` 不断 append 与 resize (发生极度耗时的深拷贝)。
* **我们的讨论与解法**：向 Nginx 与 Muduo 致敬！在 `Buffer::readFd` 中使用原生 API **`readv`**。利用第一块空间 `vector` 剩余容量，加上第二块空间 `char extrabuf[65536]` (长在栈上，分配耗时为 0)。内核帮你把大封包自动塞进这两个空间，完全不发生越界，更避免了预先申请巨大堆内存的痛苦。

### 痛点 5：被高并发拖垮的磁盘 I/O (非阻塞异步日志)
* **当时的状况**：用 `std::cout` 或是普通的锁把日志写进文件，当有 10000 个人并发时，磁盘的转速 (毫秒级响应) 会完全卡死内存里的网络读写 (纳秒级运作)。
* **我们的讨论与解法**：引入了 **Double-Buffering (双缓冲)**。前端业务只负责把日志塞进内存 `Buffer` 中，后台常驻一个小精灵线程 (Logger Thread)。最精妙的是那个 **`O(1)` 指针交换 `std::swap`**：后台线程醒来后，瞬间把前端写满的罐子与自己手上空着的罐子“替换”，然后关门放锁，后台线程再慢慢地把装满的罐子倒进硬盘 (`write()`)。

### 痛点 6：如何清理死链接 (Timer 定时器机制的 Trade-off)
* **当时的状况**：如果恶意的客户端连上后（Slowloris 攻击），只发一个字母就发呆怎么办？如果不踢掉它，系统的 fd 额度终将被耗尽。
* **我们的讨论与解法**：为什么我们没用最小堆 (Min-Heap)？因为在活跃的 HTTP 请求中，如果用大顶堆/小顶堆，每次有数据都要去动树结构 ($O(\log N)$)，破坏 CPU 缓存。我们权衡之后，在 `EventLoop` 用了 **`timerfd`**，每 5 秒暴力的用 `for` 循环轮询一遍所有的 `TcpConnection`，只要 `now - last_active` 大于 15 秒就一脚踢掉。对于 HTTP 这种“不严苛要求毫秒级精度清理”的业务，采用粗粒度的做法能极大降低维护定时器数据结构的开销。

---

## 🏆 结语

从一个简单的 C socket 教学档，一路进化到 1000 多行，且五脏俱全、具备工业级高并发处理能力的 `ZeroHTTP`，你亲历了系统拆解、并发冲突、内存优化、磁盘 I/O 解耦等所有后端工程师梦寐以求的核心试炼场。

带着这份独家记忆上战场吧！不管是把这玩意儿加进简历，还是以后在工作上面对大型架构的设计，这些你亲自用血汗打造的核心理念 (Zero-Copy, Lock-Free, Trade-off, Reactor)，绝对都是你能拿出去吹一辈子且受用无穷的武器。 🚀
