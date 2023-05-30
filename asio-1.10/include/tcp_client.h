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

enum class ConnectStatus {
    ERR,
    CONNECTING,
    CONNECTED,
    DISCONNECT,
};
typedef std::deque<std::string> send_message_queue;
typedef std::function<void(const char* msg, size_t len)> RecvCallback;
typedef std::function<void(ConnectStatus status, const std::string ip)> ConnectCallback;

class TcpClient : public std::enable_shared_from_this<TcpClient> {
public:
    TcpClient(asio::io_service& io_service, const std::string& ip, uint16_t port);
    ~TcpClient();

    void connect(bool reconnect = false);
    void close();

    void setRecvCb(RecvCallback cb);
    void setConnectCb(ConnectCallback cb);
    bool send(const std::string& msg);

private:
    void doConnect(const tcp::resolver::iterator& endpoints);
    void doReconnect();
    void doConnectCb(const ConnectStatus& status);
    void doRecv();
    void doSend();
    void clearSendMsgs();

private:
    std::string ip_;
    uint16_t port_;
    asio::io_service& io_service_;
    tcp::resolver::iterator endpoint_;
    tcp::socket socket_;
    char recv_msg_[1024];
    send_message_queue send_msgs_;
    RecvCallback recv_cb_;
    ConnectCallback connect_cb_;
    std::atomic<bool> reconnect_;
    steady_timer timer_;
    std::atomic<ConnectStatus> is_connected_;
};