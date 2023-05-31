#pragma once

#include "noncopy.h"
#include <vector>
#include <cstdint>
#include <string>

namespace asio_net {

class ConnectionBuffer : NonCopy {
public:
    static const uint32_t buf_size = 4 * 1024;
    ConnectionBuffer();
    explicit ConnectionBuffer(uint32_t size);
    ~ConnectionBuffer();

    char* writePtr() { return start() + write_index_; }

    uint32_t writableCount() const { return static_cast<uint32_t>(buffer_.size()) - write_index_; }

    const char* readPtr() { return start() + read_index_; }

    uint32_t readableCount() const { return write_index_ - read_index_; }

    void append(const std::string& data);
    void append(const char* dataptr, size_t datalen);
    void retriveReadIndex(uint32_t count);
    void retriveWriteIndex(uint32_t count);
    void retriveBothIndex();
    void adjustInternal();

private:
    char* start() {
        return &*buffer_.begin();  //  begin() better than buffer_[0]
    }

    std::vector<char> buffer_;
    uint32_t read_index_;  //  better than iterator for copy, relative better than absolute
    uint32_t write_index_;
};

}  // namespace asio_net
