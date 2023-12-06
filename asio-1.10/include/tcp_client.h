#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include <atomic>
#include <system_error>
#include "noncopy.h"
#include "callback_function.h"
#include "asio_timer.h"
#include "asio.hpp"

namespace asio_net {

class TcpClient : public NonCopy, public std::enable_shared_from_this<TcpClient> {
public:
    TcpClient(asio::io_service& io_service, const std::string& ip, uint16_t port);
    ~TcpClient();

    void connect(bool reconnect = true);
    void closeClient();
    void sendMessage(const std::string& data);

    void setConnectionCallback(ConnectionCallback cb) { connectioncallback_ = std::move(cb); }

    void setReceiveCallback(ReceiveCallback cb) { receivecallback_ = std::move(cb); }

private:
    void connectInter();
    void handleResolver(const std::error_code& error_code, asio::ip::tcp::resolver::iterator endpoint_itr);
    void handleConnect(const std::error_code& error_code, asio::ip::tcp::resolver::iterator endpoint_itr);
    void handleSending(const std::string& data);

    asio::io_service& io_service_;
    asio::ip::tcp::resolver resolver_;
    const std::string ip_;
    const uint16_t port_;
    TcpConnPtr conn_;
    std::atomic<bool> reconnect_;
    int32_t interval_;  //	millisecond
    std::shared_ptr<AsioTimer> timer_;
    ConnectionCallback connectioncallback_;
    ReceiveCallback receivecallback_;

    static std::atomic<uint32_t> conn_sequence;
};

}  // namespace asio_net
