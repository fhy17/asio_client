#include "asio_timer.h"

using namespace std::placeholders;

namespace asio_net {

AsioTimer::AsioTimer(asio::io_service& io_service, int32_t interval)
    : interval_(interval), timer_(io_service, std::chrono::milliseconds(interval_)), cycle_(false) {}

AsioTimer::AsioTimer(asio::io_service& io_service,
                     int32_t interval,  // millisecond
                     TaskCallback task)
    : interval_(interval),
      task_(std::move(task)),
      timer_(io_service, std::chrono::milliseconds(interval_)),
      cycle_(false) {}

AsioTimer::~AsioTimer() {
    cycle_ = false;
    timer_.cancel();
}

void AsioTimer::startTimer(bool cycle) {
    cycle_ = cycle;
    timer_.async_wait(std::bind(&AsioTimer::handleTimer, shared_from_this(), _1));
}

void AsioTimer::handleTimer(const std::error_code& error_code) {
    if (!error_code) {
        if (task_) {
            try {
                task_();
            } catch (...) {
            }
        }
        if (cycle_) {
            timer_.expires_from_now(std::chrono::milliseconds(interval_));
            timer_.async_wait(std::bind(&AsioTimer::handleTimer, shared_from_this(), _1));
        }
    }
}

}  // namespace asio_net
