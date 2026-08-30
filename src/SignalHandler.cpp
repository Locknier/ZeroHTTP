#include "SignalHandler.h"
#include "EventLoop.h"
#include "AsyncLogger.h"
#include <csignal>

static void handle_sigint(int sig) {
    LOG_INFO << "[Signal] SIGINT received. Initiating graceful shutdown...";
    EventLoop::g_is_running = false;
}

void SignalHandler::setup() {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}
