#pragma once

#include "noncopy.h"
#include "callback_function.h"
#include "connection_buffer.h"
#include "asio.hpp"
#include <memory>
#include <functional>
#include <string>
#include <atomic>

namespace asio_net {

using asio::ip::tcp;

class TcpConnection : NonCopy, public std::enable_shared_from_this<TcpConnection> {
public:
    enum ConnectState {
        CONNECTING,
        CONNECTED,
        DISCONNECTED,
    };

    TcpConnection(asio::io_context& io_context, const std::string& name);
    ~TcpConnection();

    void startReceive();
    void sendMessage(const char* dataptr, size_t size);
    void sendMessage(const std::string& data);
    void forceClose();

    void setConnectionCallback(ConnectionCallback cb) { connect_callback_ = std::move(cb); }

    void setCloseCallback(CloseCallback cb) { close_callback_ = std::move(cb); }

    void setReceiveCallback(ReceiveCallback cb) { recv_callback_ = std::move(cb); }

    bool connected() const { return state_ == CONNECTED; }

    void setNoDelay(bool option) { socket_.set_option(tcp::no_delay(option)); }

    std::string localIP() const { return socket_.local_endpoint().address().to_string(); }

    std::string remoteIP() const { return socket_.remote_endpoint().address().to_string(); }

    tcp::socket& socket() { return socket_; }

    void setConnState(ConnectState state) { state_ = state; }

private:
    void receiveInService();
    void handleReceive(const std::error_code& errcode, size_t bytes_transferred);
    void sendInService(const std::string& data);
    void handleSend(const std::error_code& errcode, size_t bytes_transferred);
    void closeInService();

    asio::io_context& io_context_;
    tcp::socket socket_;
    std::atomic<bool> receiving_;
    std::atomic<bool> sending_;
    ConnectState state_;
    ConnectionBuffer buf_in_;
    ConnectionBuffer buf_out_;
    ConnectionCallback connect_callback_;
    CloseCallback close_callback_;
    ReceiveCallback recv_callback_;
};

}  // namespace asio_net
