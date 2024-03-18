//
// File created on: 2024/03/18
// Author: Zizhou

#ifndef RR_SOCKET_STREAM_H
#define RR_SOCKET_STREAM_H

#include "RaftRegistry/net/socket.h"
#include <memory>
#include <vector>
#include "RaftRegistry/common/stream.h"


namespace RR {
class SocketStream : public Stream {
public:
    using ptr = std::shared_ptr<SocketStream>;

    SocketStream(Socket::ptr socket, bool owner = true);
    ~SocketStream();

    // 检查SocketStream的连接状态
    bool isConnected() const;

    // 得到内部的Socket指针
    Socket::ptr get() const;

    // 重写Stream类的read方法，用于读取数据到指定的buffer中，长度为length
    // 实现逻辑是调用内部Socket对象的recv方法接收数据
    ssize_t read(void* buffer, size_t len) override;

    // 重写Stream类的read方法，用于读取数据到ByteArray对象中，长度为length
    // 实现逻辑是准备iovec结构体数组，然后调用Socket的recv方法，并更新ByteArray对象的写位置
    ssize_t read(ByteArray::ptr buffer, size_t len);

    ssize_t write(void* buffer, size_t len);
    ssize_t write(ByteArray::ptr buffer, size_t len);

    void close();

private:
    // 内部持有的Socket对象
    Socket::ptr m_socket;
    // 标志位，表示是否拥有这个对象
    bool m_owner;
};

}

#endif // RR_SOCKET_STREAM_H