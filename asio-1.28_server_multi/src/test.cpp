#include <asio.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <deque>
#include <thread>
#include <atomic>
#include <functional>

using asio::ip::tcp;

// 抽象数据处理接口
class DataHandler {
public:
    virtual ~DataHandler() = default;
    
    /**
     * 处理接收到的数据
     * 
     * @param data 接收到的原始数据
     * @param session_id 会话唯一标识符
     * @param thread_id 处理线程ID
     * @return 要发送给客户端的响应数据
     */
    virtual std::string process(const std::string& data, 
                               const std::string& session_id,
                               const std::thread::id& thread_id) = 0;
    
    /**
     * 当新会话创建时调用
     * 
     * @param session_id 会话唯一标识符
     * @param remote_endpoint 客户端地址
     */
    virtual void on_session_created(const std::string& session_id, 
                                   const tcp::endpoint& remote_endpoint) = 0;
    
    /**
     * 当会话关闭时调用
     * 
     * @param session_id 会话唯一标识符
     * @param reason 关闭原因
     */
    virtual void on_session_closed(const std::string& session_id, 
                                  const std::string& reason) = 0;
};

// 会话类
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(asio::io_context& io_ctx, 
            tcp::socket socket, 
            std::shared_ptr<DataHandler> handler,
            const std::string& id)
        : socket_(std::move(socket)), 
          io_ctx_(io_ctx),
          handler_(handler),
          session_id_(id) {}
    
    ~Session() {
        handler_->on_session_closed(session_id_, "Destructor called");
    }

    void start() {
        // 通知处理器新会话创建
        handler_->on_session_created(session_id_, socket_.remote_endpoint());
        
        // 确保在正确的io_context线程中执行
        asio::post(io_ctx_, [self = shared_from_this()] {
            self->do_read();
        });
    }

    // 安全写入方法
    void safe_write(const std::string& data) {
        // 确保在正确的io_context线程中执行
        asio::post(io_ctx_, [self = shared_from_this(), data] {
            bool write_in_progress = !self->write_queue_.empty();
            self->write_queue_.push_back(data);
            
            if (!write_in_progress) {
                self->do_write();
            }
        });
    }

    const std::string& id() const { return session_id_; }
    tcp::endpoint remote_endpoint() const { return socket_.remote_endpoint(); }

private:
    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(asio::buffer(data_),
            [this, self](asio::error_code ec, std::size_t length) {
                if (!ec) {
                    std::string received(data_.data(), length);
                    
                    // 调用处理器处理数据
                    std::string response = handler_->process(
                        received, 
                        session_id_, 
                        std::this_thread::get_id()
                    );
                    
                    // 安全写入响应
                    safe_write(response);
                    
                    // 继续读取
                    do_read();
                } else if (ec == asio::error::eof) {
                    // 客户端正常断开
                    handler_->on_session_closed(session_id_, "Client disconnected");
                } else {
                    std::string reason = "Read error: " + ec.message();
                    handler_->on_session_closed(session_id_, reason);
                }
            });
    }

    void do_write() {
        auto self(shared_from_this());
        const std::string& data = write_queue_.front();
        
        asio::async_write(socket_, asio::buffer(data),
            [this, self](asio::error_code ec, std::size_t /*bytes_transferred*/) {
                if (!ec) {
                    write_queue_.pop_front();
                    
                    if (!write_queue_.empty()) {
                        do_write();
                    }
                } else {
                    std::string reason = "Write error: " + ec.message();
                    handler_->on_session_closed(session_id_, reason);
                    write_queue_.clear();
                    socket_.close();
                }
            });
    }

    tcp::socket socket_;
    asio::io_context& io_ctx_;
    std::array<char, 1024> data_;
    std::shared_ptr<DataHandler> handler_;
    std::deque<std::string> write_queue_;
    std::string session_id_;
};

// 会话ID生成器
class SessionIdGenerator {
public:
    static std::string generate() {
        static std::atomic<size_t> counter = 0;
        size_t id = counter.fetch_add(1, std::memory_order_relaxed);
        return "SESS-" + std::to_string(id) + "-" + 
               std::to_string(std::time(nullptr));
    }
};

// IO上下文池
class IOContextPool {
public:
    IOContextPool(size_t pool_size)
        : next_index_(0) {
        // 创建io_context和工作守卫
        for (size_t i = 0; i < pool_size; ++i) {
            io_contexts_.push_back(std::make_shared<asio::io_context>());
            work_guards_.push_back(
                std::make_unique<WorkGuard>(asio::make_work_guard(*io_contexts_.back()))
            );
        }
    }

    void run() {
        // 为每个io_context创建线程
        for (size_t i = 0; i < io_contexts_.size(); ++i) {
            threads_.emplace_back([ctx = io_contexts_[i], i] {
                std::cout << "Worker thread " << i << " (ID: " 
                          << std::this_thread::get_id() << ") started\n";
                ctx->run();
                std::cout << "Worker thread " << i << " (ID: " 
                          << std::this_thread::get_id() << ") exited\n";
            });
        }
    }

    void stop() {
        // 停止所有io_context
        for (auto& guard : work_guards_) {
            guard->reset();
        }
        
        // 等待所有线程结束
        for (auto& t : threads_) {
            if (t.joinable()) t.join();
        }
    }

    asio::io_context& get_next_io_context() {
        // 轮询选择io_context
        auto& ctx = *io_contexts_[next_index_];
        next_index_ = (next_index_ + 1) % io_contexts_.size();
        return ctx;
    }

    size_t size() const { return io_contexts_.size(); }

private:
    using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

    std::vector<std::shared_ptr<asio::io_context>> io_contexts_;
    std::vector<std::unique_ptr<WorkGuard>> work_guards_;
    std::vector<std::thread> threads_;
    std::atomic<size_t> next_index_;
};

// 服务器主类
class Server {
public:
    Server(short port, 
           std::shared_ptr<DataHandler> handler, 
           size_t thread_pool_size = 0)
        : acceptor_(main_io_ctx_, tcp::endpoint(tcp::v4(), port)),
          handler_(handler),
          pool_(thread_pool_size > 0 ? thread_pool_size : 
                std::max(1u, std::thread::hardware_concurrency())) {
        do_accept();
    }

    void run() {
        // 启动IO线程池
        pool_.run();
        
        // 在主线程运行acceptor的io_context
        std::cout << "Main acceptor thread (ID: " << std::this_thread::get_id() 
                  << ") started\n";
        main_io_ctx_.run();
    }

    void stop() {
        // 停止acceptor
        acceptor_.close();
        
        // 停止主io_context
        main_io_ctx_.stop();
        
        // 停止线程池
        pool_.stop();
        
        std::cout << "Server stopped\n";
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](asio::error_code ec, tcp::socket socket) {
                if (!ec) {
                    // 生成唯一会话ID
                    std::string session_id = SessionIdGenerator::generate();
                    
                    // 选择一个io_context
                    asio::io_context& io_ctx = pool_.get_next_io_context();
                    
                    // 创建会话并绑定到选择的io_context
                    auto session = std::make_shared<Session>(io_ctx, 
                                                            std::move(socket), 
                                                            handler_,
                                                            session_id);
                    session->start();
                    
                    // 存储会话引用
                    sessions_.push_back(session);
                } else {
                    if (ec != asio::error::operation_aborted) {
                        std::cerr << "Accept error: " << ec.message() << "\n";
                    }
                }
                
                if (acceptor_.is_open()) {
                    do_accept();
                }
            });
    }

    asio::io_context main_io_ctx_;
    tcp::acceptor acceptor_;
    std::shared_ptr<DataHandler> handler_;
    IOContextPool pool_;
    std::vector<std::shared_ptr<Session>> sessions_;
};

// 默认回显处理器
class EchoHandler : public DataHandler {
public:
    std::string process(const std::string& data, 
                       const std::string& session_id,
                       const std::thread::id& thread_id) override {
        return "Echo: " + data;
    }
    
    void on_session_created(const std::string& session_id, 
                           const tcp::endpoint& remote_endpoint) override {
        std::cout << "New session [" << session_id << "] from " 
                  << remote_endpoint << std::endl;
    }
    
    void on_session_closed(const std::string& session_id, 
                          const std::string& reason) override {
        std::cout << "Session [" << session_id << "] closed: " 
                  << reason << std::endl;
    }
};

// 示例自定义处理器 - 计算器
class CalculatorHandler : public DataHandler {
public:
    std::string process(const std::string& data, 
                       const std::string& session_id,
                       const std::thread::id& thread_id) override {
        try {
            // 尝试解析为数学表达式
            size_t pos = 0;
            double a = std::stod(data, &pos);
            
            // 跳过空格
            while (pos < data.size() && std::isspace(data[pos])) pos++;
            
            // 获取操作符
            if (pos >= data.size()) throw std::invalid_argument("Missing operator");
            char op = data[pos++];
            
            // 跳过空格
            while (pos < data.size() && std::isspace(data[pos])) pos++;
            
            // 获取第二个操作数
            double b = std::stod(data.substr(pos));
            
            // 执行计算
            double result = 0;
            switch (op) {
                case '+': result = a + b; break;
                case '-': result = a - b; break;
                case '*': result = a * b; break;
                case '/': 
                    if (b == 0) throw std::runtime_error("Division by zero");
                    result = a / b; 
                    break;
                default: 
                    throw std::invalid_argument("Invalid operator");
            }
            
            return "Result: " + std::to_string(result);
        } catch (const std::exception& e) {
            return "Error: " + std::string(e.what());
        }
    }
    
    void on_session_created(const std::string& session_id, 
                           const tcp::endpoint& remote_endpoint) override {
        std::cout << "Calculator session [" << session_id << "] started from " 
                  << remote_endpoint << std::endl;
    }
    
    void on_session_closed(const std::string& session_id, 
                          const std::string& reason) override {
        std::cout << "Calculator session [" << session_id << "] closed: " 
                  << reason << std::endl;
    }
};

// 示例自定义处理器 - 统计处理器
class StatisticsHandler : public DataHandler {
public:
    std::string process(const std::string& data, 
                       const std::string& session_id,
                       const std::thread::id& thread_id) override {
        // 更新统计信息
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_chars_ += data.size();
        message_count_++;
        
        // 为当前会话更新统计
        session_stats_[session_id] += data.size();
        
        // 生成响应
        return "Received " + std::to_string(data.size()) + " chars. " +
               "Total: " + std::to_string(total_chars_) + " chars, " +
               "Messages: " + std::to_string(message_count_);
    }
    
    void on_session_created(const std::string& session_id, 
                           const tcp::endpoint& remote_endpoint) override {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        session_stats_[session_id] = 0;
        active_sessions_++;
        
        std::cout << "Stats session [" << session_id << "] started. "
                  << "Active sessions: " << active_sessions_ << std::endl;
    }
    
    void on_session_closed(const std::string& session_id, 
                          const std::string& reason) override {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        active_sessions_--;
        
        std::cout << "Stats session [" << session_id << "] closed. "
                  << "Total chars: " << session_stats_[session_id] << ". "
                  << "Active sessions: " << active_sessions_ << std::endl;
        
        session_stats_.erase(session_id);
    }
    
    void print_summary() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        std::cout << "\n===== Statistics Summary =====" << std::endl;
        std::cout << "Total messages: " << message_count_ << std::endl;
        std::cout << "Total characters: " << total_chars_ << std::endl;
        std::cout << "Active sessions: " << active_sessions_ << std::endl;
        
        for (const auto& [session, count] : session_stats_) {
            std::cout << "  Session " << session << ": " << count << " chars" << std::endl;
        }
    }

private:
    mutable std::mutex stats_mutex_;
    size_t total_chars_ = 0;
    size_t message_count_ = 0;
    size_t active_sessions_ = 0;
    std::unordered_map<std::string, size_t> session_stats_;
};

int main() {
    try {
        const short port = 12345;
        const size_t thread_count = 4; // 工作线程数
        
        // 创建处理器
        // auto handler = std::make_shared<EchoHandler>();
        // auto handler = std::make_shared<CalculatorHandler>();
        auto stats_handler = std::make_shared<StatisticsHandler>();
        
        // 创建服务器
        //Server server(port, stats_handler, thread_count);
        Server server(port, stats_handler);
        
        std::cout << "Server started on port " << port 
                  << " with " << thread_count << " worker threads\n";
        
        // 在单独线程中运行服务器
        std::thread server_thread([&server] {
            server.run();
        });
        
        // 主线程处理控制台命令
        std::string command;
        while (true) {
            std::cout << "Enter command (stop, stats, quit): ";
            std::getline(std::cin, command);
            
            if (command == "stop") {
                server.stop();
                break;
            } else if (command == "stats") {
                auto stats = dynamic_cast<StatisticsHandler*>(stats_handler.get());
                if (stats) {
                    stats->print_summary();
                } else {
                    std::cout << "Current handler is not a StatisticsHandler\n";
                }
            } else if (command == "quit") {
                server.stop();
                break;
            }
        }
        
        // 等待服务器线程结束
        if (server_thread.joinable()) {
            server_thread.join();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}