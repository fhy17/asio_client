#include "tcp_client.h"

static const int reconnect_interval = 3000;  // ms

TcpClient::TcpClient(asio::io_service& io_service, const std::string& ip, uint16_t port)
    : ip_(ip),
      port_(port),
      io_service_(io_service),
      socket_(io_service),
      reconnect_(false),
      timer_(io_service),
      is_connected_(ConnectStatus::DISCONNECT) {
    // tcp::resolver resolver(io_service_);
    // endpoint_ = resolver.resolve(ip_.c_str(), std::to_string(port_));
}  // namespace TcpClient::TcpClient(asio::io_context&io_context,conststd::string&ip,uint16_tport)

TcpClient::~TcpClient() {
    reconnect_ = false;
    socket_.close();
}

void TcpClient::connect(bool reconnect) {
    reconnect_ = reconnect;
    tcp::resolver::query query(ip_, std::to_string(port_));
    tcp::resolver resolver(io_service_);
    endpoint_ = resolver.resolve(query);
    doConnect(endpoint_);
}

void TcpClient::close() {
    reconnect_ = false;
    is_connected_ = ConnectStatus::DISCONNECT;
    socket_.cancel();
    asio::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close();
    // asio::post(io_context_, [this]() { socket_.close(); });
}

bool TcpClient::send(const std::string& msg) {
    if (ConnectStatus ::CONNECTED != is_connected_) {
        std::cout << "send err, socket is not connected " << std::endl;
        return false;
    }
    io_service_.post([this, msg]() {
        bool send_in_progress = !send_msgs_.empty();
        send_msgs_.push_back(msg);
        if (!send_in_progress) {
            doSend();
        }
    });
    return true;
}

void TcpClient::setRecvCb(RecvCallback cb) { recv_cb_ = cb; }

void TcpClient::setConnectCb(ConnectCallback cb) { connect_cb_ = cb; }

void TcpClient::doConnect(const tcp::resolver::iterator& endpoints) {
    is_connected_ = ConnectStatus::CONNECTING;
    auto self = shared_from_this();
    asio::async_connect(socket_, endpoints, [this, self](std::error_code ec, tcp::resolver::iterator) {
        std::cout << "async_connect err: " << ec.value() << std::endl;
        doConnectCb(0 == ec.value() ? ConnectStatus::CONNECTED : ConnectStatus::ERR);
        if (!ec) {
            doRecv();
        } else {
            doReconnect();
        }
    });
}

void TcpClient::doReconnect() {
    is_connected_ = ConnectStatus::DISCONNECT;
    clearSendMsgs();
    socket_.close();
    if (!reconnect_) return;
    timer_.expires_from_now(std::chrono::milliseconds(reconnect_interval));
    auto self = shared_from_this();
    timer_.async_wait([this, self](const std::error_code& ec) {
        if (ec == asio::error::operation_aborted) {
            std::cout << "timer async_wait err: " << ec.value() << std::endl;
            return;
        }
        timer_.cancel();
        doConnect(endpoint_);
    });
}

void TcpClient::doConnectCb(const ConnectStatus& status) {
    if (ConnectStatus ::CONNECTED == status)
        is_connected_ = ConnectStatus::CONNECTED;
    else
        is_connected_ = ConnectStatus::DISCONNECT;
    std::string ip;
    try {
        ip = socket_.remote_endpoint().address().to_string();
    } catch (...) {
        ip = "";
    }
    if (connect_cb_) connect_cb_(status, ip);
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
                                    std::cout << "async_read_some err: " << ec.value() << std::endl;
                                    doConnectCb(2 == ec.value() ? ConnectStatus::DISCONNECT : ConnectStatus::ERR);
                                    doReconnect();
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
                              // std::cout << "async_write err: " << ec.value() << std::endl;
                              doConnectCb(ConnectStatus::ERR);
                              //   doReconnect();
                          }
                      });
}

void TcpClient::clearSendMsgs() { send_msgs_.clear(); }
