#include "WakeUpFd.h"
#include "AsyncLogger.h"
#include <sys/eventfd.h>
#include <unistd.h>

int WakeUpFd::wakeup_fd_ = -1;

void WakeUpFd::init() {
    wakeup_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeup_fd_ < 0) {
        LOG_INFO << "[WakeUpFd] Failed to create eventfd.";
    }
}

void WakeUpFd::wakeup() {
    if (wakeup_fd_ < 0) return;
    uint64_t one = 1;
    int n = write(wakeup_fd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_INFO << "[WakeUpFd] Write error during wakeup signal.";
    }
}

int WakeUpFd::getFd() {
    return wakeup_fd_;
}
