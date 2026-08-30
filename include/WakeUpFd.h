#pragma once
class WakeUpFd {
public:
    static void init();
    static void wakeup();
    static int getFd();
private:
    static int wakeup_fd_;
};
