//
// File created on: 2024/03/18
// Author: Zizhou

#include "RaftRegistry/net/socket_stream.h"

namespace RR {

SocketStream::SocketStream(Socket::ptr socket, bool owner) : m_socket(socket), m_owner(owner) {}

SocketStream::~SocketStream() {
    // 如果持有socket，且拥有所有权，才可以进行close()
    if (m_socket && m_owner) {
        close();
    }
}

bool SocketStream::isConnected() const {
    return m_socket && m_socket->isConnected();
}

ssize_t SocketStream::read(void* buffer, size_t length) {
    if (isConnected()) {
        return m_socket->recv(buffer, length);
    }
    return -1;
}

ssize_t SocketStream::read(ByteArray::ptr buffer, size_t len) {
    if (isConnected()) {
        std::vector<iovec> iovs; // 创建iovs向量，用于接收数据
        buffer->getWriteBuffers(iovs, len); // 获取iovs向量，每个iovec都指向了ByteArray的一个Node
        ssize_t reslLen = m_socket->recv(&iovs[0], len); // 调用Socket的recv方法，接收数据
        if (reslLen > 0) {
            buffer->setPosition(buffer->getPosition() + reslLen); // 更新ByteArray的写位置
        }
        return reslLen;
    }
    return -1;
}

ssize_t SocketStream::write(const void* buffer, size_t len) {
    if (isConnected()) {
        return m_socket->send(buffer, len);
    }
    return -1;
}

ssize_t SocketStream::write(ByteArray::ptr buffer, size_t len) {
    if (isConnected()) {
        std::vector<iovec> iovs; // 创建iovs向量，用于发送数据
        buffer->getReadBuffers(iovs, len); // 获取iovs向量，每个iovec都指向了ByteArray的一个Node
        ssize_t realLen = m_socket->recv(&iovs[0], len); // 调用Socket的send方法，发送数据
        if (realLen > 0) {
            buffer->setPosition(buffer->getPosition() + realLen); // 更新ByteArray的读位置
        }
        return realLen;
    }

    return -1;
}

void SocketStream::close() {
    // 如果持有socket，则调用socket自身的close函数
    if (m_socket) {
        m_socket->close();
    }
}

}