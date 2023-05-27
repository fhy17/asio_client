#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <string>
#include <atomic>

#include "asio.hpp"
#include "asio/steady_timer.hpp"

using asio::steady_timer;
using asio::ip::tcp;

enum class ConnectType {
    ERR,
    CONNECT,
    DISCONNECT,
};
typedef std::deque<std::string> send_message_queue;
typedef std::function<void(const char* msg, size_t len)> RecvCallback;
//
typedef std::function<void(ConnectType connect, const std::string ip)> ConnectCallback;

class TcpClient : public std::enable_shared_from_this<TcpClient> {
public:
    // TcpClient(asio::io_context& io_context, asio::io_context::work& work, const std::string& ip, uint16_t port);
    TcpClient(asio::io_context& io_context, const std::string& ip, uint16_t port);
    ~TcpClient();

    void connect(bool reconnect = false);
    void close();

    void setRecvCb(RecvCallback cb);
    void setConnectCb(ConnectCallback cb);
    void send(const std::string& msg);

private:
    void doConnect(const tcp::resolver::results_type& endpoints);
    void doReconnect();
    void doConnectCb(const ConnectType& connect_type, const std::string& ip);
    void doRecv();
    void doSend();

private:
    std::string ip_;
    uint16_t port_;
    asio::io_context& io_context_;
    // asio::io_context::work& work_;
    tcp::resolver::results_type endpoint_;
    tcp::socket socket_;
    char recv_msg_[1024];
    send_message_queue send_msgs_;
    RecvCallback recv_cb_;
    ConnectCallback connect_cb_;
    std::atomic<bool> reconnect_;
    steady_timer timer_;
};
