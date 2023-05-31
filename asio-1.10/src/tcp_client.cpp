#include "tcp_client.h"
#include "tcp_connection.h"

#include <string>
#include <functional>
#include <memory>
#include <iostream>

using namespace std::placeholders;

namespace {

const int32_t kMinInterval = 1 * 1000;   // ms
const int32_t kMaxInterval = 3 * 1000;  // ms

}  // namespace

namespace asio_net {

using asio::ip::tcp;

std::atomic<uint32_t> TcpClient::conn_sequence = 0;

TcpClient::TcpClient(asio::io_service& io_service, const std::string& ip, uint16_t port)
    : io_service_(io_service),
      resolver_(io_service_),
      ip_(ip),
      port_(port),
      conn_(new TcpConnection(io_service_, std::string("tcpclient#") + std::to_string(++conn_sequence))),
      reconnect_(false),
      interval_(kMinInterval) {}

TcpClient::~TcpClient() {
    // std::cout << "~TcpClient()" << std::endl;
}

void TcpClient::connect(bool reconnect) {
    reconnect_ = reconnect;
    connectInter();
}

void TcpClient::closeClient() {
    reconnect_ = false;
    if (conn_) conn_->forceClose();
}

void TcpClient::sendMessage(const std::string& data) {
    io_service_.post(std::bind(&TcpClient::handleSending, shared_from_this(), data));
}

void TcpClient::connectInter() {
    std::cout << __FUNCTION__ << std::endl;
    tcp::resolver::query query(ip_, std::to_string(port_));
    resolver_.async_resolve(query, std::bind(&TcpClient::handleResolver, shared_from_this(), _1, _2));
}

void TcpClient::handleResolver(const std::error_code& error_code, asio::ip::tcp::resolver::iterator endpoint_itr) {
     std::cout << "handleResolver " << error_code.value() << std::endl;
    if (!error_code) {
        asio::async_connect(conn_->socket(), endpoint_itr,
                            std::bind(&TcpClient::handleConnect, shared_from_this(), _1, _2));
    }
}

void TcpClient::handleConnect(const std::error_code& error_code, asio::ip::tcp::resolver::iterator endpoint_itr) {
     std::cout << "handleConnect " << error_code.value() << " interval_=" << interval_ << " tcpclient=" <<
     conn_->connName() << std::endl;
    if (!error_code) {
        conn_->setConnState(TcpConnection::CONNECTED);
        conn_->setConnectionCallback(connectioncallback_);
        conn_->setReceiveCallback(receivecallback_);
        conn_->setCloseCallback(connectioncallback_);
        conn_->startReceive();
        if (connectioncallback_) connectioncallback_(conn_);
    } else {
        if (connectioncallback_) connectioncallback_(conn_);
        conn_.reset(new TcpConnection(io_service_, std::string("tcpclient#") + std::to_string(++conn_sequence)));
        if (endpoint_itr != tcp::resolver::iterator()) {
            asio::async_connect(conn_->socket(), ++endpoint_itr,
                                std::bind(&TcpClient::handleConnect, shared_from_this(), _1, _2));
        } else {
            std::cout << "interval_ " << interval_ << std::endl;
            if (reconnect_) {
                timer_.reset(new AsioTimer(io_service_, interval_, std::bind(&TcpClient::connectInter, this)));
                if (interval_ < kMaxInterval)
                    timer_->startTimer();
                else
                    timer_->startTimer(true);
            }
        }
    }
}

void TcpClient::handleSending(const std::string& data) { conn_->sendMessage(data); }

}  // namespace asio_net
