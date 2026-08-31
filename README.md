# ZeroHTTP: High-Performance C++11 Asynchronous Web Server

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/c++-11%2B-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)
![Build](https://img.shields.io/badge/build-CMake-green.svg)

> **ZeroHTTP** 是一個從零手寫的高性能、全非同步 C++11 HTTP Web 伺服器框架。底層架構深受 **Muduo** 與 **Nginx** 啟發，採用嚴格的非阻塞事件驅動 (Non-blocking Event-driven) 設計與 One-Loop-Per-Thread 多核心併發模型。

本作旨在深研 Linux 底層系統編程、**Zero-Copy (零拷貝)** 機制、**Zero-Allocation (零動態分配)** 實踐，以及工業級高併發架構之精髓，是一件為榨乾單機硬體效能而生的藝術品。

---

## 🚀 核心極致優化 (Core Optimizations)

### 1. 徹底消滅資料競爭 (Race Condition Free)
* **痛點**：在 Epoll 邊緣觸發 (ET) 模式下，同一個 Socket 的資料若分次抵達，極易觸發多個 ThreadPool 執行緒同時讀寫該 Socket，導致 Context 嚴重錯亂。
* **解法**：全面引入 `EPOLLONESHOT` 機制。確保 Socket 觸發讀寫事件後自動於 Epoll 中「隱身」，直到工作執行緒完成 HTTP 解析並針對半包/全包狀態使用 `EPOLL_CTL_MOD` 重新武裝，從根源上保證並發環境的執行緒安全。

### 2. Zero-Copy 零拷貝與 64KB Stack-fallback
* **痛點**：傳統網路緩衝區依賴 `std::string::append` 或巨型 Heap `new`，頻繁的系統呼叫 (System Calls) 與深拷貝會導致極高的延遲與 CPU Cache 失效。
* **解法**：捨棄傳統字串拼接，底層採用作業系統原生 API `readv` 實作 Scatter/Gather I/O 分散收集機制。結合 64KB 的棧記憶體 (Stack-fallback)，超大 HTTP 封包也能一次性切入 Stack 中，大幅減少 `read` 呼叫次數，完成極致的 Cache-friendly 吞吐。

### 3. 無分配物件池 (Zero-Allocation Object Pool)
* **痛點**：面對每秒上萬次併發請求，頻繁的 `new TcpConnection()` 會導致 OS 堆積 (Heap) 產生千瘡百孔的記憶體碎片，引發 OOM 或分配速度斷崖下跌。
* **解法**：於伺服器啟動時，一次性預配置上萬個連線物件與 `free_list_`。在洪峰流量來襲時，新連線的分發僅為 $O(1)$ 的指標彈出，達成伺服器生命週期內的 Zero-Allocation。

### 4. 雙緩衝非同步日誌 (Double-Buffering Async Logger)
* **痛點**：高併發下直接使用 `std::cout` 或普通 Mutex 寫檔，會讓底層磁碟 I/O 嚴重阻塞 Reactor 的事件派發迴圈，拖垮整個系統吞吐。
* **解法**：前端業務執行緒僅將日誌寫入 1MB 記憶體區塊，後台寫檔執行緒甦醒時，使用底層 `std::swap` 直接對調前端與自己的緩衝區指標。**這個指標對調動作為 $O(1)$ 時間複雜度且僅需幾奈秒**，完美達成前後端無鎖解耦 (Lock-free)。

### 5. 防禦性連接管理 (Defensive Connection Management)
* **痛點**：惡意連線 (如 Slowloris 攻擊) 會霸佔 Socket fd 不放，導致伺服器資源枯竭。
* **解法**：利用 `timerfd` 設計底層時間輪演算法 (Timing Wheel)，每 5 秒批次剔除惡意佔用或超時的 Keep-Alive 連線，對攻擊全面免疫。

---

## 📊 系統架構圖解 (Architecture)

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

## 📂 原始碼結構與職責分佈

本專案採用最嚴謹的微服務/引擎開發目錄結構設計：

```text
ZeroHTTP/
├── CMakeLists.txt              # 現代 C++ 構建腳本
├── README.md                   # 本說明文件
├── build/                      # Out-of-source 構建中介檔目錄
├── conf/
│   └── server.conf             # 伺服器動態配置檔 (Port, Threads, Timeout)
├── include/                    # 標頭檔 (API 介面層)
│   ├── TcpServer.h             # 封裝好的 Facade 外觀模式伺服器引擎
│   ├── EventLoop.h             # 核心 Sub-Reactor 事件迴圈
│   ├── Buffer.h                # Zero-Copy 記憶體緩衝區機制
│   └── ...
├── src/                        # 原始碼 (實作層)
│   ├── main.cpp                # 伺服器啟動腳本
│   ├── HttpParser.cpp          # HTTP 靜態路由器與解析引擎
│   ├── AsyncLogger.cpp         # 雙緩衝日誌引擎
│   └── ...
├── logs/                       # AsyncLogger 非同步寫檔輸出區 (server.log)
└── www/                        # HTTP 伺服器掛載之靜態資源根目錄 (HTML/CSS)
```

---

## 🛠️ 快速開始與編譯指南

### 環境要求
* C++ 支援至 C++11 (預設 g++ 或 clang++)
* CMake >= 3.10
* OS: Linux (支援 Epoll 與 eventfd/timerfd 等特性)

### 獲取與編譯步驟

```bash
# 1. Clone 專案並進入目錄
git clone https://github.com/<your-username>/ZeroHTTP.git
cd ZeroHTTP

# 2. 建立並進入 build 暫存目錄
mkdir -p build && cd build

# 3. 執行 CMake 配置
cmake ..

# 4. 進行多執行緒平行編譯
make -j4

# 5. 編譯完成，執行檔會產出於專案根目錄
cd ..
./zero_httpd
```

### 配置與服務測試
1. 伺服器預設讀取 `conf/server.conf` 中的配置 (預設啟動 `PORT=8080`)。
2. 打開瀏覽器訪問：[http://127.0.0.1:8080/](http://127.0.0.1:8080/)
3. 可以在 `logs/server.log` 觀察伺服器極高效率的非同步 I/O 排程狀況。如果你擁有 Nginx 架設經驗，強烈建議將 Nginx 放置於前端作為 SSL 卸載 (SSL Termination)，與後端 `ZeroHTTP` 達成最強雙劍合璧！

---
