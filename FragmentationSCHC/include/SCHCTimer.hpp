#pragma once

#pragma once

#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <spdlog/spdlog.h>

class SCHCTimer
{
public:
    using Callback = std::function<void()>;

    SCHCTimer();
    ~SCHCTimer();

    void start(std::chrono::milliseconds duration, Callback cb);
    void stop();

private:
    std::thread _thread;
    std::mutex _mtx;
    std::condition_variable _cv;
    bool _active{false};
};
