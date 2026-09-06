#include "../include/HttpParser.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <iostream>

ParseResult HttpParser::parse(const char* data, size_t len, std::string& method, std::string& path, size_t& header_length) {
    // 使用底层的 C 字符串匹配，定位 HTTP 头部的结束标志，绝不拷贝字符串数据
    const char* crlf_crlf = strstr(data, "\r\n\r\n");
    if (!crlf_crlf) {
        return ParseResult::INCOMPLETE; 
    }
    
    // 指针相减直接计算 Header 占用的字节数，加上结束符自身的 4 字节
    header_length = (crlf_crlf - data) + 4; 

    // 取出 Header 内容用于解析方法和路径
    std::string request(data, crlf_crlf - data);
    std::istringstream stream(request);
    std::string line;
    
    if (std::getline(stream, line)) {
        size_t first_space = line.find(' ');
        if (first_space == std::string::npos) return ParseResult::ERROR;
        method = line.substr(0, first_space);
        
        size_t second_space = line.find(' ', first_space + 1);
        if (second_space == std::string::npos) return ParseResult::ERROR;
        path = line.substr(first_space + 1, second_space - first_space - 1);
        
        return ParseResult::SUCCESS;
    }
    
    return ParseResult::ERROR;
}

static std::string getMimeType(const std::string& path) {
    if (path.find(".html") != std::string::npos) return "text/html";
    if (path.find(".css") != std::string::npos) return "text/css";
    if (path.find(".js") != std::string::npos) return "application/javascript";
    if (path.find(".png") != std::string::npos) return "image/png";
    if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) return "image/jpeg";
    return "text/plain";
}

void HttpParser::serveStaticFile(TcpConnection* conn, const std::string& path) {
    std::string safe_path = path;
    if (safe_path == "/") safe_path = "/index.html";
    
    // 基本的路径穿越防范
    if (safe_path.find("..") != std::string::npos) {
        std::string response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 9\r\n\r\nForbidden";
        conn->getWriteBuffer().append(response.c_str(), response.length());
        return;
    }

    std::string filepath = "./www" + safe_path;
    int file_fd = open(filepath.c_str(), O_RDONLY);
    
    if (file_fd < 0) {
        std::string body = "<h1>404 Not Found</h1>";
        std::string response = "HTTP/1.1 404 Not Found\r\n";
        response += "Content-Type: text/html\r\n";
        response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
        response += "Connection: keep-alive\r\n\r\n" + body;
        conn->getWriteBuffer().append(response.c_str(), response.length());
        return;
    }

    struct stat stat_buf;
    fstat(file_fd, &stat_buf);
    size_t file_size = stat_buf.st_size;

    std::string header = "HTTP/1.1 200 OK\r\n";
    header += "Content-Type: " + getMimeType(safe_path) + "\r\n";
    header += "Content-Length: " + std::to_string(file_size) + "\r\n";
    header += "Connection: keep-alive\r\n\r\n";

    int client_fd = conn->getFd();
    
    // 发送 HTTP 标头
    write(client_fd, header.c_str(), header.length()); 

    // 高性能核心：sendfile 将文件数据直接从内核磁盘缓存发送至 Socket 发送缓冲区
    // 零用户态内存拷贝，实现极限 I/O 吞吐
    off_t offset = 0;
    while (offset < file_size) {
        ssize_t sent = sendfile(client_fd, file_fd, &offset, file_size - offset);
        if (sent <= 0) {
            break; 
        }
    }

    close(file_fd);
}
