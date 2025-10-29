#include <asio.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <deque>
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <deque>
#include <cstring>

using asio::ip::tcp;

// 前向声明
class Session;

// 协议抽象接口
class ProtocolHandler {
public:
    virtual ~ProtocolHandler() = default;
    
    /**
     * 处理接收到的数据
     * 
     * @param data 接收到的原始数据
     * @param length 数据长度
     * @param session 会话引用
     * @return 处理消耗的字节数（0表示需要更多数据）
     */
    virtual size_t process_data(const char* data, 
                               size_t length, 
                               Session& session) = 0;
    
    /**
     * 准备要发送的数据
     * 
     * @param data 业务逻辑生成的数据
     * @param output 协议封装后的输出
     */
    virtual void prepare_send(const std::string& data, 
                             std::vector<char>& output) = 0;
    
    /**
     * 获取协议名称
     */
    virtual std::string name() const = 0;
};

// 抽象数据处理接口
class DataHandler {
public:
    virtual ~DataHandler() = default;
    
    /**
     * 处理解析后的应用数据
     * 
     * @param data 应用层数据
     * @param session 会话引用
     * @return 要发送给客户端的响应数据
     */
    virtual std::string process_app_data(const std::string& data, 
                                        Session& session) = 0;
    
    /**
     * 当新会话创建时调用
     * 
     * @param session 会话引用
     */
    virtual void on_session_created(Session& session) = 0;
    
    /**
     * 当会话关闭时调用
     * 
     * @param session 会话引用
     * @param reason 关闭原因
     */
    virtual void on_session_closed(Session& session, 
                                  const std::string& reason) = 0;
};

// 协议工厂
class ProtocolFactory {
public:
    using ProtocolCreator = std::function<std::unique_ptr<ProtocolHandler>()>;
    
    static ProtocolFactory& instance() {
        static ProtocolFactory factory;
        return factory;
    }
    
    void register_protocol(const std::string& name, ProtocolCreator creator) {
        protocols_[name] = std::move(creator);
    }
    
    std::unique_ptr<ProtocolHandler> create(const std::string& name) {
        auto it = protocols_.find(name);
        if (it != protocols_.end()) {
            return it->second();
        }
        return nullptr;
    }
    
    std::vector<std::string> available_protocols() const {
        std::vector<std::string> names;
        for (const auto& p : protocols_) {
            names.push_back(p.first);
        }
        return names;
    }

private:
    std::unordered_map<std::string, ProtocolCreator> protocols_;
};

// 会话类
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(asio::io_context& io_ctx, 
            tcp::socket socket, 
            std::shared_ptr<DataHandler> data_handler,
            const std::string& id,
            const std::string& protocol_name = "line")
        : socket_(std::move(socket)), 
          io_ctx_(io_ctx),
          data_handler_(data_handler),
          session_id_(id),
          protocol_(ProtocolFactory::instance().create(protocol_name)) {
        
        if (!protocol_) {
            throw std::runtime_error("Unsupported protocol: " + protocol_name);
        }
    }
    
    ~Session() {
        if (data_handler_) {
            data_handler_->on_session_closed(*this, "Destructor called");
        }
    }

    void start() {
        // 通知处理器新会话创建
        if (data_handler_) {
            data_handler_->on_session_created(*this);
        }
        
        // 确保在正确的io_context线程中执行
        asio::post(io_ctx_, [self = shared_from_this()] {
            self->do_read();
        });
    }

    // 安全写入应用数据
    void safe_write(const std::string& data) {
        // 确保在正确的io_context线程中执行
        asio::post(io_ctx_, [self = shared_from_this(), data] {
            // 通过协议处理器准备发送数据
            std::vector<char> buffer;
            self->protocol_->prepare_send(data, buffer);
            
            // 将协议封装后的数据加入队列
            bool write_in_progress = !self->write_queue_.empty();
            self->write_queue_.emplace_back(std::move(buffer));
            
            if (!write_in_progress) {
                self->start_writing();
            }
        });
    }

    const std::string& id() const { return session_id_; }
    tcp::endpoint remote_endpoint() const { return socket_.remote_endpoint(); }
    const std::string protocol_name() const { return protocol_->name(); }
    asio::io_context& get_io_context() { return io_ctx_; }
    std::shared_ptr<DataHandler> get_data_handler() { return data_handler_; }
    
    // 设置会话元数据
    void set_metadata(const std::string& key, const std::string& value) {
        metadata_[key] = value;
    }
    
    // 获取会话元数据
    std::string get_metadata(const std::string& key, 
                            const std::string& default_value = "") const {
        auto it = metadata_.find(key);
        return it != metadata_.end() ? it->second : default_value;
    }
    
    // 设置协议处理器
    void set_protocol(std::unique_ptr<ProtocolHandler> new_protocol) {
        protocol_ = std::move(new_protocol);
    }

private:
    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(asio::buffer(read_buffer_ + read_pos_, 
                                            sizeof(read_buffer_) - read_pos_),
            [this, self](asio::error_code ec, std::size_t length) {
                if (!ec) {
                    read_pos_ += length;
                    
                    // 处理接收到的数据
                    size_t processed = protocol_->process_data(
                        read_buffer_, 
                        read_pos_, 
                        *self
                    );
                    
                    // 移除已处理的数据
                    if (processed > 0) {
                        if (processed < read_pos_) {
                            std::memmove(read_buffer_, 
                                        read_buffer_ + processed, 
                                        read_pos_ - processed);
                        }
                        read_pos_ -= processed;
                    }
                    
                    // 继续读取
                    do_read();
                } else if (ec == asio::error::eof) {
                    // 客户端正常断开
                    if (data_handler_) {
                        data_handler_->on_session_closed(*this, "Client disconnected");
                    }
                } else {
                    std::string reason = "Read error: " + ec.message();
                    if (data_handler_) {
                        data_handler_->on_session_closed(*this, reason);
                    }
                }
            });
    }

    void start_writing() {
        auto self(shared_from_this());
        const auto& buffer = write_queue_.front();
        
        asio::async_write(socket_, asio::buffer(buffer.data(), buffer.size()),
            [this, self](asio::error_code ec, std::size_t /*bytes_transferred*/) {
                if (!ec) {
                    write_queue_.pop_front();
                    
                    if (!write_queue_.empty()) {
                        start_writing();
                    }
                } else {
                    std::string reason = "Write error: " + ec.message();
                    if (data_handler_) {
                        data_handler_->on_session_closed(*this, reason);
                    }
                    write_queue_.clear();
                    socket_.close();
                }
            });
    }

    tcp::socket socket_;
    asio::io_context& io_ctx_;
    std::shared_ptr<DataHandler> data_handler_;
    std::unique_ptr<ProtocolHandler> protocol_;
    std::deque<std::vector<char>> write_queue_;
    
    // 读缓冲区
    static constexpr size_t READ_BUFFER_SIZE = 4096;
    char read_buffer_[READ_BUFFER_SIZE];
    size_t read_pos_ = 0;
    
    std::string session_id_;
    std::unordered_map<std::string, std::string> metadata_;
};

// 会话ID生成器
class SessionIdGenerator {
public:
    static std::string generate() {
        static std::atomic<size_t> counter = 0;
        size_t id = counter.fetch_add(1, std::memory_order_relaxed);
        
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        
        std::ostringstream oss;
        oss << "SESS-" 
            << std::put_time(&tm, "%Y%m%d%H%M%S") 
            << "-" 
            << std::setfill('0') << std::setw(6) << id;
        return oss.str();
    }
};

// ===================== 协议处理器实现 =====================

// 行协议处理器（以换行符为分隔）
class LineProtocolHandler : public ProtocolHandler {
public:
    size_t process_data(const char* data, 
                       size_t length, 
                       Session& session) override {
        size_t processed = 0;
        size_t start = 0;
        
        for (size_t i = 0; i < length; ++i) {
            if (data[i] == '\n') {
                // 找到完整的一行
                size_t line_length = i - start;
                if (i > 0 && data[i-1] == '\r') {
                    line_length--; // 处理CRLF
                }
                
                std::string line(data + start, line_length);
                process_line(line, session);
                
                processed = i + 1;
                start = i + 1;
            }
        }
        
        return processed;
    }
    
    void prepare_send(const std::string& data, 
                     std::vector<char>& output) override {
        output.reserve(data.size() + 2);
        output.assign(data.begin(), data.end());
        output.push_back('\r');
        output.push_back('\n');
    }
    
    std::string name() const override { return "line"; }

private:
    void process_line(const std::string& line, Session& session) {
        if (auto handler = session.get_data_handler()) {
            std::string response = handler->process_app_data(line, session);
            session.safe_write(response);
        }
    }
};

// 固定长度协议处理器
class FixedLengthProtocolHandler : public ProtocolHandler {
public:
    FixedLengthProtocolHandler(size_t frame_size = 256) 
        : frame_size_(frame_size) {}
    
    size_t process_data(const char* data, 
                       size_t length, 
                       Session& session) override {
        if (length < frame_size_) {
            return 0; // 等待更多数据
        }
        
        std::string frame(data, frame_size_);
        process_frame(frame, session);
        return frame_size_;
    }
    
    void prepare_send(const std::string& data, 
                     std::vector<char>& output) override {
        output.reserve(frame_size_);
        
        if (data.size() >= frame_size_) {
            output.assign(data.begin(), data.begin() + frame_size_);
        } else {
            output.assign(data.begin(), data.end());
            output.resize(frame_size_, '\0'); // 填充空字符
        }
    }
    
    std::string name() const override { 
        return "fixed-" + std::to_string(frame_size_); 
    }

private:
    void process_frame(const std::string& frame, Session& session) {
        if (auto handler = session.get_data_handler()) {
            std::string response = handler->process_app_data(frame, session);
            session.safe_write(response);
        }
    }
    
    size_t frame_size_;
};

// 带长度的二进制协议处理器
class BinaryProtocolHandler : public ProtocolHandler {
public:
    size_t process_data(const char* data, 
                       size_t length, 
                       Session& session) override {
        // 需要至少4字节获取长度
        if (length < 4) return 0;
        
        // 解析长度（大端序）
        uint32_t payload_length = 
            (static_cast<uint8_t>(data[0]) << 24) |
            (static_cast<uint8_t>(data[1]) << 16) |
            (static_cast<uint8_t>(data[2]) << 8)  |
            static_cast<uint8_t>(data[3]);
            
        // 检查是否有足够数据
        if (length < 4 + payload_length) {
            return 0;
        }
        
        // 处理有效载荷
        std::string payload(data + 4, payload_length);
        process_payload(payload, session);
        
        return 4 + payload_length;
    }
    
    void prepare_send(const std::string& data, 
                     std::vector<char>& output) override {
        uint32_t length = data.size();
        output.resize(4 + length);
        
        // 写入长度（大端序）
        output[0] = static_cast<char>((length >> 24) & 0xFF);
        output[1] = static_cast<char>((length >> 16) & 0xFF);
        output[2] = static_cast<char>((length >> 8) & 0xFF);
        output[3] = static_cast<char>(length & 0xFF);
        
        // 写入数据
        std::copy(data.begin(), data.end(), output.begin() + 4);
    }
    
    std::string name() const override { return "binary"; }

private:
    void process_payload(const std::string& payload, Session& session) {
        if (auto handler = session.get_data_handler()) {
            std::string response = handler->process_app_data(payload, session);
            session.safe_write(response);
        }
    }
};

// HTTP协议处理器（简化版）
// class HttpProtocolHandler : public ProtocolHandler {
// public:
//     size_t process_data(const char* data, 
//                        size_t length, 
//                        Session& session) override {
//         // 添加到缓冲区
//         buffer_.append(data, length);
        
//         // 检查是否收到完整HTTP请求
//         size_t end_pos = buffer_.find("\r\n\r\n");
//         if (end_pos == std::string::npos) {
//             // 检查是否有Content-Length
//             size_t content_length = 0;
//             size_t cl_pos = buffer_.find("Content-Length: ");
//             if (cl_pos != std::string::npos) {
//                 size_t start = cl_pos + 16;
//                 size_t end = buffer_.find("\r\n", start);
//                 if (end != std::string::npos) {
//                     content_length = std::stoul(buffer_.substr(start, end - start));
//                 }
//             }
            
//             if (content_length > 0) {
//                 size_t body_start = buffer_.find("\r\n\r\n");
//                 if (body_start != std::string::npos) {
//                     body_start += 4;
//                     if (buffer_.size() - body_start >= content_length) {
//                         return process_http_request(buffer_, session);
//                     }
//                 }
//             }
//             return 0; // 需要更多数据
//         }
        
//         return process_http_request(buffer_, session);
//     }
    
//     void prepare_send(const std::string& data, 
//                      std::vector<char>& output) override {
//         // 构建HTTP响应
//         std::ostringstream oss;
//         oss << "HTTP/1.1 200 OK\r\n"
//             << "Content-Type: text/plain\r\n"
//             << "Content-Length: " << data.size() << "\r\n"
//             << "Connection: keep-alive\r\n"
//             << "\r\n"
//             << data;
        
//         const std::string& response = oss.str();
//         output.assign(response.begin(), response.end());
//     }
    
//     std::string name() const override { return "http"; }

// private:
//     size_t process_http_request(const std::string& request, Session& session) {
//         if (auto handler = session.get_data_handler()) {
//             // 提取HTTP请求路径（简化处理）
//             size_t start = request.find(' ') + 1;
//             size_t end = request.find(' ', start);
//             if (end == std::string::npos) {
//                 return request.size(); // 无效请求，跳过
//             }
            
//             std::string path = request.substr(start, end - start);
            
//             // 提取请求体（如果有）
//             std::string body;
//             size_t body_start = request.find("\r\n\r\n");
//             if (body_start != std::string::npos) {
//                 body_start += 4;
//                 body = request.substr(body_start);
//             }
            
//             // 调用业务处理器
//             std::string response = handler->process_app_data(
//                 "PATH=" + path + " BODY=" + body, 
//                 session
//             );
            
//             // 准备发送HTTP响应
//             std::vector<char> output;
//             prepare_send(response, output);
            
//             // 直接发送响应
//             asio::post(session.get_io_context(), [&session, output = std::move(output)] {
//                 bool write_in_progress = !session.write_queue_.empty();
//                 session.write_queue_.emplace_back(std::move(output));
                
//                 if (!write_in_progress) {
//                     session.start_writing();
//                 }
//             });
//         }
        
//         return request.size();
//     }
    
//     std::string buffer_;
// };

// 注册协议处理器
void register_protocols() {
    ProtocolFactory::instance().register_protocol("line", [] {
        return std::make_unique<LineProtocolHandler>();
    });
    
    ProtocolFactory::instance().register_protocol("fixed-128", [] {
        return std::make_unique<FixedLengthProtocolHandler>(128);
    });
    
    ProtocolFactory::instance().register_protocol("fixed-256", [] {
        return std::make_unique<FixedLengthProtocolHandler>(256);
    });
    
    ProtocolFactory::instance().register_protocol("binary", [] {
        return std::make_unique<BinaryProtocolHandler>();
    });
    
    // ProtocolFactory::instance().register_protocol("http", [] {
    //     return std::make_unique<HttpProtocolHandler>();
    // });
}

// ===================== 数据处理实现 =====================

// 基础回显处理器
class EchoHandler : public DataHandler {
public:
    std::string process_app_data(const std::string& data, 
                                Session& session) override {
        return "Echo: " + data;
    }
    
    void on_session_created(Session& session) override {
        std::cout << "New session [" << session.id() 
                  << "] using protocol: " << session.protocol_name()
                  << " from " << session.remote_endpoint() << std::endl;
    }
    
    void on_session_closed(Session& session, 
                          const std::string& reason) override {
        std::cout << "Session [" << session.id() 
                  << "] closed: " << reason << std::endl;
    }
};

// 协议检测处理器
class ProtocolDetector : public DataHandler {
public:
    std::string process_app_data(const std::string& data, 
                                Session& session) override {
        // 根据初始数据检测协议
        if (data.find("GET /") == 0 || data.find("POST /") == 0) {
            session.set_metadata("protocol", "http");
            auto http_protocol = ProtocolFactory::instance().create("http");
            if (http_protocol) {
                session.set_protocol(std::move(http_protocol));
            }
            return "Switched to HTTP protocol";
        }
        
        // 默认为行协议
        session.set_metadata("protocol", "line");
        auto line_protocol = ProtocolFactory::instance().create("line");
        if (line_protocol) {
            session.set_protocol(std::move(line_protocol));
        }
        return "Switched to Line protocol";
    }
    
    void on_session_created(Session& session) override {
        std::cout << "Protocol detection started for session [" 
                  << session.id() << "]" << std::endl;
    }
    
    void on_session_closed(Session& session, 
                          const std::string& reason) override {
        std::cout << "Session [" << session.id() 
                  << "] closed: " << reason << std::endl;
    }
};

// ===================== 服务器主类 =====================

class Server {
public:
    Server(short port, 
           std::shared_ptr<DataHandler> handler, 
           size_t thread_pool_size = 0)
        : acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)),
          handler_(handler) {
        
        // 创建工作线程池
        size_t worker_count = thread_pool_size > 0 ? thread_pool_size : 
            std::max(1u, std::thread::hardware_concurrency());
        
        for (size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back([this] {
                io_context_.run();
            });
        }
        
        do_accept();
    }

    void run() {
        std::cout << "Server started with " << workers_.size() 
                  << " worker threads" << std::endl;
    }

    void stop() {
        // 停止acceptor
        acceptor_.close();
        
        // 停止io_context
        io_context_.stop();
        
        // 等待所有线程结束
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        
        std::cout << "Server stopped" << std::endl;
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](asio::error_code ec, tcp::socket socket) {
                if (!ec) {
                    // 生成唯一会话ID
                    std::string session_id = SessionIdGenerator::generate();
                    
                    // 创建会话
                    auto session = std::make_shared<Session>(
                        io_context_, 
                        std::move(socket), 
                        handler_,
                        session_id,
                        "detect" // 初始使用协议检测
                    );
                    
                    session->start();
                    
                    // 存储会话引用
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    sessions_[session_id] = session;
                }
                
                if (acceptor_.is_open()) {
                    do_accept();
                }
            });
    }

    asio::io_context io_context_;
    tcp::acceptor acceptor_;
    std::shared_ptr<DataHandler> handler_;
    std::vector<std::thread> workers_;
    
    // 会话管理
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
    std::mutex sessions_mutex_;
};

int main() {
    try {
        // 注册所有支持的协议
        register_protocols();
        
        const short port = 12345;
        const size_t thread_count = 4; // 工作线程数
        
        // 创建处理器
        auto handler = std::make_shared<EchoHandler>();
        // auto handler = std::make_shared<ProtocolDetector>();
        
        // 创建服务器
        Server server(port, handler, thread_count);
        server.run();
        
        // 主线程等待
        std::cout << "Press Enter to stop the server..." << std::endl;
        std::cin.ignore();
        
        server.stop();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}