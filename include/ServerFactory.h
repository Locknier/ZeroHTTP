#pragma once
#include "IServer.h"
#include "TcpServer.h"
#include <memory>
#include <string>
#include <stdexcept>

class ServerFactory {
public:
    static std::unique_ptr<IServer> createServer(const std::string& protocol) {
        if (protocol == "TCP" || protocol == "HTTP") {
            return std::make_unique<TcpServer>();
        }
        throw std::invalid_argument("Unsupported protocol: " + protocol);
    }
};
