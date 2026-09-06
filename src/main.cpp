#include "../../ZeroNet/include/TcpServer.h"
#include "../include/HttpParser.h"
#include <iostream>
#include <csignal>
#include <unistd.h>

TcpServer* g_server = nullptr;

// 注册系统退出信号，实现服务器优雅关机
void SignalHandler(int signum) {
    if (g_server) {
        std::cout << "\n[ZeroHTTP] Captured signal " << signum << ". Initiating graceful shutdown...\n";
        g_server->stop();
    }
}

int main() {
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    TcpServer server;
    g_server = &server;

    // 向底层 ZeroNet 引擎注册数据抵达的回调函数
    server.setMessageCallback([](TcpConnection* conn, const char* data, size_t len) -> size_t {
        std::string method, path;
        size_t header_length = 0;
        
        // 传递原生内存指针给解析器，避免生成巨大的 std::string
        ParseResult result = HttpParser::parse(data, len, method, path, header_length);
        
        if (result == ParseResult::INCOMPLETE) {
            // TCP 粘包/半包防御：返回 0 告知引擎暂不消费，继续存放缓冲
            return 0; 
        } else if (result == ParseResult::SUCCESS) {
            std::cout << "[ZeroHTTP] Received Request -> Method: " << method << ", Path: " << path << "\n";
            // 发送静态文件，内部封装了 sendfile 系统调用
            HttpParser::serveStaticFile(conn, path);
            
            // 返回消耗的 Header 长度，ZeroNet 会自动在内存中对齐剩余缓冲
            return header_length; 
        } else {
            // 报文非法直接截断丢弃
            return len;
        }
    });

    std::cout << "[ZeroHTTP] Server started, powered by ZeroNet (Zero-Copy HTTP/1.1 Engine).\n";
    
    // 阻塞在 dispatchLoop 内处理所有监听
    server.start();

    std::cout << "[ZeroHTTP] Server execution terminated safely.\n";
    return 0;
}
