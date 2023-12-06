#include "tcp_connection.h"

using namespace asio_net;
using namespace std::placeholders;

TcpConnection::TcpConnection(asio::io_context& io_context, const std::string& name)
    : io_context_(io_context), socket_(io_context_), receiving_(false), sending_(false), state_(CONNECTING) {}

void TcpConnection::startReceive() {
    // boost::asio::async_read(socket_, boost::asio::buffer(buf),
    //     std::bind(&TcpConnection::handleReceive, shared_from_this(), _1, _2));
    if (receiving_) return;
    io_context_.post(std::bind(&TcpConnection::receiveInService, shared_from_this()));
    receiving_ = true;
}

TcpConnection::~TcpConnection() {
    socket_.close();
    // std::cout << conn_name_.c_str() << "~TcpConnection()" << std::endl;
}

void TcpConnection::sendMessage(const char* dataptr, size_t size) { sendMessage(std::string(dataptr, size)); }

void TcpConnection::sendMessage(const std::string& data) {
    io_context_.post(
        std::bind(&TcpConnection::sendInService, shared_from_this(), data));  //	optimization for copying data
}

void TcpConnection::forceClose() { io_context_.post(std::bind(&TcpConnection::closeInService, shared_from_this())); }

void TcpConnection::receiveInService() {
    socket_.async_read_some(asio::buffer(buf_in_.writePtr(), buf_in_.writableCount()),
                            std::bind(&TcpConnection::handleReceive, shared_from_this(), _1, _2));
}

void TcpConnection::handleReceive(const std::error_code& errcode, size_t bytes_transferred) {
    // std::cout << "TcpConnection-handleReceive " << errcode.value() << " " << errcode.message() << std::endl;
    if (!errcode) {
        buf_in_.retriveWriteIndex(static_cast<uint32_t>(bytes_transferred));
        buf_in_.adjustInternal();
        if (recv_callback_) recv_callback_(shared_from_this(), &buf_in_);
        receiveInService();
    } else if (errcode.value() == asio::error::eof && close_callback_) {
        state_ = DISCONNECTED;
        close_callback_(shared_from_this());
    } else {
        //	log error
        // std::cout << "Connection Receive " << conn_name_.c_str() << " " << remoteIPPort().c_str()
        //	<< " ErrorCode is " << errcode.value() << " " << errcode.message().c_str() << std::endl;
        // LOG(jf_log::LT_ERROR) << "Connection Receive " << conn_name_.c_str() << " " << remoteIPPort().c_str()
        //	<< " ErrorCode is " << errcode.value() << " " << errcode.message().c_str() << LOG_END;
        socket_.cancel();
        state_ = DISCONNECTED;
        if (close_callback_) close_callback_(shared_from_this());
    }
}

void TcpConnection::sendInService(const std::string& data) {
    buf_out_.append(data);
    if (!sending_ && connected()) {
        asio::async_write(socket_, asio::buffer(buf_out_.readPtr(), buf_out_.readableCount()),
                          std::bind(&TcpConnection::handleSend, shared_from_this(), _1, _2));
        sending_ = true;
    }
}

void TcpConnection::handleSend(const std::error_code& errcode, size_t bytes_transferred) {
    // std::cout << "TcpConnection-handleSend " << errcode.value() << std::endl;
    if (!errcode) {
        buf_out_.retriveReadIndex(static_cast<uint32_t>(bytes_transferred));
        if (buf_out_.readableCount() > 0 && connected()) {
            asio::async_write(socket_, asio::buffer(buf_out_.readPtr(), buf_out_.readableCount()),
                              std::bind(&TcpConnection::handleSend, shared_from_this(), _1, _2));
        } else {
            sending_ = false;
        }
    } else {
        //	log error
        // std::cout << "Connection Send " << conn_name_.c_str() << " " << remoteIPPort().c_str()
        //	<< " ErrorCode is " << errcode.value() << " " << errcode.message().c_str() << std::endl;
        // LOG(jf_log::LT_ERROR) << "Connection Send " << conn_name_.c_str() << " " << remoteIPPort().c_str()
        //	<< " ErrorCode is " << errcode.value() << " " << errcode.message().c_str() << LOG_END;
        socket_.cancel();
        state_ = DISCONNECTED;
        if (close_callback_) close_callback_(shared_from_this());
    }
}

void TcpConnection::closeInService() {
    socket_.cancel();
    state_ = DISCONNECTED;
    if (close_callback_) close_callback_(shared_from_this());
}
