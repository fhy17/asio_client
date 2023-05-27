#include "tcp_client.h"

static const int reconnect_interval = 3000;  // ms

TcpClient::TcpClient(asio::io_context& io_context, const std::string& ip, uint16_t port)
    : ip_(ip), port_(port), io_context_(io_context), socket_(io_context), reconnect_(false) {
    tcp::resolver resolver(io_context_);
    endpoint_ = resolver.resolve(ip_, std::to_string(port_));
}

TcpClient::~TcpClient() {
    reconnect_ = false;
    socket_.close();
}

void TcpClient::connect(bool reconnect) {
    reconnect_ = reconnect;
    doConnect(endpoint_);
}

void TcpClient::close() {
    asio::post(io_context_, [this]() { socket_.close(); });
}

void TcpClient::send(const std::string& msg) {
    asio::post(io_context_, [this, msg]() {
        bool write_in_progress = !send_msgs_.empty();
        send_msgs_.push_back(msg);
        if (!write_in_progress) {
            doSend();
        }
    });
}

void TcpClient::setRecvCb(RecvCallback cb) { recv_cb_ = cb; }

void TcpClient::setConnectCb(ConnectCallback cb) { connect_cb_ = cb; }

void TcpClient::doConnect(const tcp::resolver::results_type& endpoints) {
    auto self = shared_from_this();
    asio::async_connect(socket_, endpoint_, [this, self](std::error_code ec, tcp::endpoint) {
        // std::cout << "async_connect err: " << ec.value() << std::endl;
        if (connect_cb_) {
            ConnectType connect_type = (0 == ec.value() ? ConnectType::CONNECT : ConnectType::ERR);
            std::string ip =
                connect_type == ConnectType::CONNECT ? socket_.remote_endpoint().address().to_string() : "";
            connect_cb_(connect_type, ip);
        }
        if (!ec) {
            doRecv();
        } else {
            if (reconnect_) {
                socket_.close();
                std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_interval));
                doConnect(endpoint_);
            }
        }
    });
}

void TcpClient::doRecv() {
    auto self = shared_from_this();
    socket_.async_read_some(asio::buffer(recv_msg_, sizeof(recv_msg_) - 1),
                            [this, self](std::error_code ec, std::size_t length) {
                                if (!ec) {
                                    if (length > 0) {
                                        if (recv_cb_) recv_cb_(recv_msg_, length);
                                        memset(recv_msg_, 0x00, sizeof(recv_msg_));
                                    }
                                    doRecv();
                                } else {
                                    // std::cout << "err: " << ec.value() << std::endl;
                                    if (connect_cb_)
                                        connect_cb_((2 == ec.value() ? ConnectType::DISCONNECT : ConnectType::ERR),
                                                    socket_.remote_endpoint().address().to_string());
                                    socket_.close();
                                    std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_interval));
                                    doConnect(endpoint_);
                                }
                            });
}

void TcpClient::doSend() {
    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(send_msgs_.front()),
                      [this, self](std::error_code ec, std::size_t /*length*/) {
                          if (!ec) {
                              send_msgs_.pop_front();
                              if (!send_msgs_.empty()) {
                                  doSend();
                              }
                          } else {
                              if (connect_cb_)
                                  connect_cb_((0 == ec.value() ? ConnectType::CONNECT : ConnectType::ERR),
                                              socket_.remote_endpoint().address().to_string());
                              socket_.close();
                          }
                      });
}