// 文件名：asio_echo_server.cpp

#include <asio.hpp>
#include <iostream>
#include <memory>
#include <string>

using asio::ip::tcp;

// 单个连接会话类
class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket socket)
        : socket_(std::move(socket)) {}

    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this(); // 保持智能指针生命周期
        socket_.async_read_some(asio::buffer(data_),
            [this, self](std::error_code ec, std::size_t length) {
                if (!ec) {
                    // std::cout << "Received: " << std::string(data_.data(), length) << std::endl;
                    do_write(length);
                }
            });
    }

    void do_write(std::size_t length) {
        auto self = shared_from_this();

        asio::async_write(socket_, asio::buffer(data_, length),
            [this, self, length](std::error_code ec, std::size_t /*len*/) {
                if (!ec) {
                    // std::cout << "Sent: " << std::string(data_.data(), length) << std::endl;
                    do_read(); // 回到读，循环 echo
                }
            });
    }

    tcp::socket socket_;
    std::array<char, 1024> data_;
};

// 监听器类
class Server {
public:
    Server(asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                }
                do_accept(); // 继续接收下一个连接
            });
    }

    tcp::acceptor acceptor_;
};

int main() {
    try {
        asio::io_context io_context;
        Server server(io_context, 12345);
        std::cout << "ASIO echo server running on port 12345\n";
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
