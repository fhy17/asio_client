#include "async_client.h"

#include <iostream>
#include <stdlib.h>
#include <memory>
class Test {
public:
    Test() {}
    void run() {
        std::thread t([this]() {
            try {
                std::string ip = "172.16.100.247";
                uint16_t port = 777;

                client_ = std::make_shared<AsyncClient>(io_context_, ip, std::to_string(port));
                client_->set_on_connected([this]() {
                        std::cout << "Connected to server." << std::endl;
                });
                client_->set_on_disconnected([this](const std::string& reason) {
                        std::cout << "disconnected to server: " << reason << std::endl;
                });
                client_->set_on_receive(
                    [](const std::string& msg) { std::cout << "recv:" << msg << std::endl; });
                client_->open();

                t_ = std::thread([this]() { io_context_.run(); });

                if(t_.joinable())
                    t_.join();

            } catch (std::exception& e) {
                std::cerr << "Exception: " << e.what() << "\n";
            }
        });
        t.detach();
    }

    void stop() {
        client_->close();
        // io_context_.stop();
        t_.join();
    }

    void send(std::string& msg) { client_->send(msg); }

private:
    asio::io_context io_context_;
    // asio::io_context::work& work_;
    std::shared_ptr<AsyncClient> client_;
    std::thread t_;
};
int main(int argc, char* argv[]) {
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
    std::cout << "test run" << std::endl;
    test->stop();
    std::cout << "test run" << std::endl;
    return 0;
}