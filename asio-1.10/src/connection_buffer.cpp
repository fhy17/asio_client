#include "connection_buffer.h"
#include <cstring>

using namespace asio_net;

ConnectionBuffer::ConnectionBuffer() : buffer_(buf_size), read_index_(0), write_index_(0) {}

ConnectionBuffer::ConnectionBuffer(uint32_t size) : buffer_(size), read_index_(0), write_index_(0) {}

ConnectionBuffer::~ConnectionBuffer() {}

void ConnectionBuffer::append(const std::string& data) { append(data.c_str(), data.size()); }

void ConnectionBuffer::append(const char* dataptr, size_t datalen) {
    if (writableCount() > datalen) {
        memcpy(writePtr(), dataptr, datalen);
        write_index_ += static_cast<uint32_t>(datalen);
    } else if (read_index_ > buffer_.size() - write_index_ + datalen) {
        memcpy(start(), readPtr(), write_index_ - read_index_);
        write_index_ = write_index_ - read_index_;
        read_index_ = 0;
        memcpy(writePtr(), dataptr, datalen);
        write_index_ += static_cast<uint32_t>(datalen);
    } else {
        int32_t count = static_cast<uint32_t>(datalen / buf_size) + 1;
        buffer_.resize(buffer_.size() + buf_size * count);  //	may be unsafe when the amount of data is very huge
        memcpy(writePtr(), dataptr, datalen);
        write_index_ += static_cast<uint32_t>(datalen);
    }
}

void ConnectionBuffer::retriveReadIndex(uint32_t count) {
    read_index_ += count;
    if (read_index_ >= write_index_) {
        read_index_ = 0;
        write_index_ = 0;
    }
}

void ConnectionBuffer::retriveWriteIndex(uint32_t count) {
    write_index_ += count;
    if (write_index_ >= buffer_.size()) {
        write_index_ = static_cast<uint32_t>(buffer_.size());
        buffer_.resize(buffer_.size() * 2);  //	may be unsafe when the amount of data is very huge
    }
}

void ConnectionBuffer::retriveBothIndex() {
    read_index_ = 0;
    write_index_ = 0;
}

void ConnectionBuffer::adjustInternal() {
    if (read_index_ >= buffer_.size() - write_index_) {
        memcpy(start(), readPtr(), write_index_ - read_index_);
        write_index_ = write_index_ - read_index_;
        read_index_ = 0;
    }
}
