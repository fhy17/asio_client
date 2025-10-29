#include "async_client.h"
#include <iostream>
#include <chrono>

using asio::ip::tcp;

AsyncClient::AsyncClient(asio::io_context& io_context, const std::string& host, const std::string& port)
    : io_context_(io_context),
      socket_(io_context),
      resolver_(io_context),
      host_(host),
      port_(port),
      stop_(false) {}

AsyncClient::~AsyncClient() {}

void AsyncClient::open() {
    stop_ = true;
    connect();
}

void AsyncClient::send(const std::string& msg) {
    asio::post(io_context_, [self = shared_from_this(), msg] {
        if (!self->socket_.is_open()) {
            std::cerr << "Socket not connected. Message dropped.\n";
            return;
        }
        bool writing = !self->write_msgs_.empty();
        self->write_msgs_.push_back(msg);
        if (!writing) {
            self->do_write();
        }
    });
}

// void AsyncClient::send(const std::string& msg) {
//     asio::post(io_context_, [self = shared_from_this(), msg] {
//         if (!self->socket_.is_open()) {
//             std::cerr << "Socket not connected. Message dropped.\n";
//             return;
//         }

//         // 构造带有固定 head 的完整包
//         uint32_t len = htonl(static_cast<uint32_t>(msg.size()));
//         std::string full_msg;
//         full_msg.resize(4 + msg.size());
//         std::memcpy(&full_msg[0], &len, 4);
//         std::memcpy(&full_msg[4], msg.data(), msg.size());

//         bool writing = !self->write_msgs_.empty();
//         self->write_msgs_.push_back(std::move(full_msg));
//         if (!writing) {
//             self->do_write();
//         }
//     });
// }

void AsyncClient::close() {
    stop_ = false;
    asio::post(io_context_, [self = shared_from_this()] {
        self->socket_.close();
    });
}

void AsyncClient::set_on_receive(CallbackString cb) {
    on_receive_ = std::move(cb);
}

void AsyncClient::set_on_connected(CallbackVoid cb) {
    on_connected_ = std::move(cb);
}

void AsyncClient::set_on_disconnected(CallbackString cb) {
    on_disconnected_ = std::move(cb);
}

void AsyncClient::invoke_connected() {
    if (on_connected_) on_connected_();
}

void AsyncClient::invoke_disconnected(const std::string& reason) {
    if (on_disconnected_) on_disconnected_(reason);
}

void AsyncClient::invoke_received(const std::string& msg) {
    if (on_receive_) on_receive_(msg);
}

void AsyncClient::connect() {
    auto endpoints = resolver_.resolve(host_, port_);
    asio::async_connect(socket_, endpoints,
        [self = shared_from_this()](std::error_code ec, tcp::endpoint) {
            if (!ec) {
                std::cout << "[Client] Connected.\n";
                self->invoke_connected();
                self->do_read();
            } else {
                self->invoke_disconnected(ec.message());
                self->schedule_reconnect();
            }
        });
}

// void do_read_header() {
//     asio::async_read(socket_, asio::buffer(head_buf_, 4),
//         [self = shared_from_this()](std::error_code ec, std::size_t /*length*/) {
//             if (!ec) {
//                 uint32_t net_len = 0;
//                 std::memcpy(&net_len, self->head_buf_, 4);
//                 self->body_len_ = ntohl(net_len);  // 网络字节序转主机字节序

//                 if (self->body_len_ > 0 && self->body_len_ < 10 * 1024 * 1024) { // 防御性判断
//                     self->body_buf_.resize(self->body_len_);
//                     self->do_read_body();
//                 } else {
//                     std::cerr << "[Client] Invalid body length: " << self->body_len_ << "\n";
//                     self->socket_.close();
//                     self->schedule_reconnect();
//                 }
//             } else {
//                 self->invoke_disconnected(ec.message());
//                 self->socket_.close();
//                 self->schedule_reconnect();
//             }
//         });
// }

// void do_read_body() {
//     asio::async_read(socket_, asio::buffer(body_buf_),
//         [self = shared_from_this()](std::error_code ec, std::size_t length) {
//             if (!ec) {
//                 std::string msg(self->body_buf_.begin(), self->body_buf_.begin() + length);
//                 self->invoke_received(msg);
//                 self->do_read_header();  // 继续接收下一个包
//             } else {
//                 self->invoke_disconnected(ec.message());
//                 self->socket_.close();
//                 self->schedule_reconnect();
//             }
//         });
// }

void AsyncClient::do_read() {
    socket_.async_read_some(asio::buffer(reply_, max_length),
        [self = shared_from_this()](std::error_code ec, std::size_t length) {
            if (!ec) {
                self->invoke_received(std::string(self->reply_, length));
                self->do_read();
            } else {
                self->invoke_disconnected(ec.message());
                self->socket_.close();
                self->schedule_reconnect();
            }
        });
}

void AsyncClient::do_write() {
    auto msg = std::make_shared<std::string>(write_msgs_.front());
    asio::async_write(socket_, asio::buffer(*msg),
        [self = shared_from_this(), msg](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                self->write_msgs_.pop_front();
                if (!self->write_msgs_.empty()) {
                    self->do_write();
                }
            } else {
                self->invoke_disconnected(ec.message());
                self->socket_.close();
                self->schedule_reconnect();
            }
        });
}

void AsyncClient::schedule_reconnect() {
    if(!stop_) return;
        std::cout << "Reconnecting in 3 seconds...\n";
        timer_ = std::make_shared<asio::steady_timer>(io_context_, std::chrono::seconds(3));
        timer_->async_wait([self = shared_from_this()](const std::error_code& ec) {
            if (!ec) {
                self->socket_.close();
                self->socket_ = tcp::socket(self->io_context_); // 重置 socket
                self->connect();
            }
        });
    }

