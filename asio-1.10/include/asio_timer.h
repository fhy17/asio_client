#pragma once

#include <memory>
#include <chrono>
#include <cstdint>
#include <atomic>
#include <functional>
#include <system_error>
#include "noncopy.h"
#include "asio.hpp"
#include "asio/basic_waitable_timer.hpp"

namespace asio_net {

class AsioTimer : public NonCopy, public std::enable_shared_from_this<AsioTimer> {
public:
    typedef std::function<void()> TaskCallback;
    typedef asio::basic_waitable_timer<std::chrono::steady_clock> SteadyTimer;

    AsioTimer(asio::io_service& io_service, int32_t interval);

    AsioTimer(asio::io_service& io_service,
              int32_t interval,  // millisecond
              TaskCallback task);
    ~AsioTimer();

    void setTaskCallback(TaskCallback task) { task_ = std::move(task); }

    void startTimer(bool cycle = false);
    void stopTimer() { cycle_ = false; }

private:
    void handleTimer(const std::error_code& error_code);

    int32_t interval_;
    TaskCallback task_;
    SteadyTimer timer_;
    std::atomic<bool> cycle_;
};

}  // namespace asio_net
