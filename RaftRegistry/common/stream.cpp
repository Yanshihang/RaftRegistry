//
// File created on: 2024/03/11
// Author: Zizhou

#include "stream.h"

namespace RR {
    ssize_t Stream::readFixSize(void* buffer, size_t len) {
        auto offset = 0; // 偏移量, 用于记录已经读取的数据的长度
        auto left = len; // 剩余需要读取的数据长度

        while(left) {

            // read函数是纯虚函数，所以read存在运行时多态
            // 根据socket_stream.cpp文件中派生类对read函数的实现，可以得知
            // 返回-1，证明没有连接
            // 返回0，证明socket关闭
            auto readSize = read(static_cast<char*>(buffer)+offset, left);
            if (readSize <= 0) {
                return readSize;
            }
            offset +=readSize;
            left -= readSize;
        }
        return len;
    }

    ssize_t Stream::readFixSize(ByteArray::ptr buffer, size_t len) {
        auto left = len;
        while(left) {
            // 如果缓冲区是ByteArray类型，调用read函数时不用使用offset
            // 因为ByteArray的m_position会记住此时的位置
            auto readSize = read(buffer, left);
            if (readSize <= 0) {
                return readSize;
            }
            left -= readSize;
        }
        return len;
    }

    ssize_t Stream::writeFixSize(const void* buffer, size_t len) {
        auto offset = 0;
        auto left = len;

        while(left) {
            auto writeSize = write(static_cast<const char*>(buffer),left);
            if (writeSize <= 0) {
                return writeSize;
            }
            offset += writeSize;
            left -= writeSize;
        }
        return len;
    }

    ssize_t Stream::writeFixSize(ByteArray::ptr buffer, size_t len) {
        auto left = len;
        while(left) {
            auto writeSize = write(buffer, left);
            if (writeSize <= 0) {
                return writeSize;
            }
            left -= writeSize;
        }
        return len;
    }
}