#pragma once

#include <iostream>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#include <asio.hpp>

class Session {
public:
    Session(asio::io_context& io_context, const std::string& server_address, unsigned short server_port)
        : socket_(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0)), server_endpoint_(asio::ip::make_address(server_address), server_port) {}

    void AsyncSendData(const std::string& data) {
        auto buffer = std::make_shared<std::string>(data);
        socket_.async_send_to(asio::buffer(*buffer), server_endpoint_,
            [this, buffer](const asio::error_code& ec, std::size_t bytes_transferred) {
                if (!ec) {
                    // std::cout << "Data sent successfully." << std::endl;
                } else {
                    std::cerr << "async send error: " << ec.message() << std::endl;
                }
            });
    }

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint server_endpoint_;
};

class UdpClient: public std::enable_shared_from_this<UdpClient>{
public:
    UdpClient(const std::string& ip, unsigned short port): ip_(ip), port_(port), run_(false) {}
    ~UdpClient(){}

    void start(){
        run_ = true;
        auto self(shared_from_this());
        t_ = std::thread([this, self](){
            while(true){
                std::string msg;
                {
                    std::unique_lock<std::mutex> lck(mtx_);
                    cv_.wait(lck, [this, self]() { return !msg_queue_.empty() || !run_; });
                    if (!run_) break;
                    if(!msg_queue_.empty()){
                        msg = std::move(msg_queue_.front());
                        msg_queue_.pop();
                    }
                }
                if(!msg.empty()){
                    try {
                        asio::io_context io_context;
                        Session udp_session(io_context, ip_, port_);
                        udp_session.AsyncSendData(msg);
                        io_context.run();
                    } catch (const std::exception& e) {
                        std::cerr << "Exception: " << e.what() << std::endl;
                    }
                }
            }
        });
    }

    void stop(){ 
        run_ = false;
        cv_.notify_all();
        t_.join();
    }

    void send(const std::string& data){
        std::unique_lock<std::mutex> lck(mtx_);
        msg_queue_.emplace(data);
        cv_.notify_one();
    }

private:
    std::string ip_;
    u_short port_;
    std::atomic<bool> run_;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<std::string> msg_queue_;

    std::thread t_;
};