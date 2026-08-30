#include "ServerFactory.h"
#include "Config.h"
#include "AsyncLogger.h"
#include <iostream>

int main() {
    AsyncLogger::getInstance().start("logs/server.log");
    Config::getInstance().load("conf/server.conf");
    
    try {
        LOG_INFO << "========== Server Initialization Started ==========";
        auto server = ServerFactory::createServer("HTTP");
        server->start();
        LOG_INFO << "========== Server Shutdown Successfully ==========";
    } 
    catch (const std::exception& e) {
        LOG_INFO << "[Main] Fatal error: " << e.what();
    }

    AsyncLogger::getInstance().stop();
    return 0;
}
