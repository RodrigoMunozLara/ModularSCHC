#include "SCHCTimer.hpp"

SCHCTimer::SCHCTimer() = default;

SCHCTimer::~SCHCTimer()
{
    stop();
    SPDLOG_DEBUG("Executing SCHCTimer destructor()");
}

void SCHCTimer::start(std::chrono::milliseconds duration, Callback cb)
{
    stop(); // evita timers simultáneos

    _active = true;

    _thread = std::thread([this, duration, cb]()
    {
        std::unique_lock<std::mutex> lock(_mtx);

        if (_cv.wait_for(lock, duration, [&] { return !_active; }))
        {
            return; // timer cancelled
        }

        if (_active && cb)
        {
            cb();
        }
    });
}

void SCHCTimer::stop()
{
    {
        std::lock_guard<std::mutex> lock(_mtx);
        _active = false;
    }

    _cv.notify_all();

    if (_thread.joinable())
    {
        _thread.join();
    }
}
