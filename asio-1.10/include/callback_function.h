#pragma once

#include <memory>
#include <functional>

namespace asio_net {

class TcpConnection;
class ConnectionBuffer;
typedef std::shared_ptr<TcpConnection> TcpConnPtr;
typedef std::weak_ptr<TcpConnection> TcpConnWeakPtr;
typedef std::function<void(const TcpConnPtr& conn_ptr)> ConnectionCallback;
typedef std::function<void(const TcpConnPtr& conn_ptr)> CloseCallback;
typedef std::function<void(const TcpConnPtr& conn_ptr, ConnectionBuffer* bufptr)> ReceiveCallback;

}  // namespace asio_net
