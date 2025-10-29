#pragma once

#include <asio.hpp>
#include <deque>
#include <string>
#include <memory>
#include <thread>
#include <functional>

class AsyncClient : public std::enable_shared_from_this<AsyncClient> {
public:
    using CallbackString = std::function<void(const std::string&)>;
    using CallbackVoid   = std::function<void()>;

    AsyncClient(asio::io_context& io_context, const std::string& host, const std::string& port);
    ~AsyncClient();

    void open();
    void send(const std::string& msg);
    void close();

    // 设置回调接口
    void set_on_receive(CallbackString cb);
    void set_on_connected(CallbackVoid cb);
    void set_on_disconnected(CallbackString cb);

private:
    void connect();
    // void do_read_header();
    // void do_read_body();
    void do_read();
    void do_write();
    void schedule_reconnect();

    void invoke_connected();
    void invoke_disconnected(const std::string& reason);
    void invoke_received(const std::string& msg);

private:
    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    asio::ip::tcp::resolver resolver_;
    std::shared_ptr<asio::steady_timer> timer_;

    std::string host_;
    std::string port_;

    std::deque<std::string> write_msgs_;

    CallbackString on_receive_;
    CallbackVoid   on_connected_;
    CallbackString on_disconnected_;

    enum { max_length = 1024 };
    char reply_[max_length];

    uint32_t body_len_ = 0;
    std::vector<char> body_buf_;
    char head_buf_[4];

    std::atomic<bool> stop_;
};