#include "tcp_client.h"
#include "tcp_connection.h"
#include "callback_function.h"

#include <iostream>
#include <stdlib.h>
#include <memory>
#include <thread>

using namespace asio_net;
class Test {
public:
    Test() {}
    void run() {
        t_ = std::thread([this]() {
            std::cout << "thread id: " << std::this_thread::get_id() << std::endl;
            try {
                std::string ip = "172.16.100.247";
                uint16_t port = 777;

                // client_ = std::make_shared<TcpClient>(io_context_, ip, port);
                // client_->setConnectCb([](ConnectStatus status, const std::string ip) {
                //     std::string status_str = "";
                //     switch (status) {
                //         case ConnectStatus::ERR:
                //             status_str = "connect err";
                //             break;
                //         case ConnectStatus::CONNECTED:
                //             status_str = "connect";
                //             break;
                //         case ConnectStatus::DISCONNECT:
                //             status_str = "disconnect";
                //             break;
                //     }
                //     std::cout << status_str << " from: " << ip << std::endl;
                // });
                // client_->setRecvCb(
                //     [](const char* msg, size_t len) { std::cout << "recv:" << std::string(msg, len) << std::endl; });
                // client_->connect(true);

                client_ = std::make_shared<TcpClient>(io_service_, ip, port);
                client_->setConnectionCallback([this](const TcpConnPtr& conn_ptr) {
                    std::cout << (conn_ptr->connected() ? "Connected " : "Disconnected ") << std::endl;
                });
                client_->setReceiveCallback([this](const TcpConnPtr& conn_ptr, ConnectionBuffer* bufptr) {
                    std::cout << conn_ptr->remoteIP() << ", " << std::string(bufptr->readPtr(), bufptr->readableCount())
                              << std::endl;
                    bufptr->retriveReadIndex(bufptr->readableCount());
                });
                client_->connect();
                // t_ = std::thread([this]() { io_service_.run(); });
                io_service_.run();
            } catch (std::exception& e) {
                std::cerr << "Exception: " << e.what() << "\n";
            }
        });
        // t.detach();
    }

    void stop() {
        client_->closeClient();
        t_.join();
    }

    void send(std::string& msg) { client_->sendMessage(msg); }

private:
    asio::io_service io_service_;
    // asio::io_context::work& work_;
    std::shared_ptr<TcpClient> client_;
    std::thread t_;
};
int main(int argc, char* argv[]) {
    std::cout << "main thread id: " << std::this_thread::get_id() << std::endl;
    std::shared_ptr<Test> test = std::make_shared<Test>();
    test->run();
    // while (true) {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // }
    char line[1024];
    std::cout << "test run" << std::endl;
    while (std::cin.getline(line, sizeof(line) - 1)) {
        // std::cout << "getline length: " << strlen(line) << std::endl;
        std::string msg(line);
        if (msg == "exit") break;

        test->send(msg);
        memset(line, 0x00, sizeof(line));
    }
    test->stop();
    std::cout << "test end" << std::endl;
    return 0;
}