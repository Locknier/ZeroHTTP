#pragma once
#include <string>
#include "../../ZeroNet/include/TcpConnection.h"

// 解析状态枚举：用于外层框架判断当前接收到的网络数据包是否完整
enum class ParseResult {
    SUCCESS,    // 解析成功，找到了完整的 HTTP Header
    INCOMPLETE, // 半包，未发现 "\r\n\r\n" 结束符，等待更多数据
    ERROR       // 报文格式错误
};

class HttpParser {
public:
    // 解析 HTTP Header
    // 采用指针偏移直接解析，不分配新的字符串 (Zero-Copy 理念)
    // 成功时将解析出的 method 与 path 赋值，并通过 header_length 返回报头总长度
    static ParseResult parse(const char* data, size_t len, std::string& method, std::string& path, size_t& header_length);
    
    // 直接将静态文件写入 Socket
    // 内部采用 sendfile() 零拷贝系统调用，绕过用户态内存，极致压榨 I/O 性能
    static void serveStaticFile(TcpConnection* conn, const std::string& path);
};
