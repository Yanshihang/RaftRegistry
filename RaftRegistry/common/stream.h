//
// File created on: 2024/03/11
// Author: Zizhou

#ifndef RR_SREAM_H
#define RR_SREAM_H

#include <memory>

#include "byte_array.h"

namespace RR {
/**
 * @brief Stream 类是一个抽象基类，用于定义一个通用的流接口
 * 
 * @details Stream包含了读取和写入数据的基本方法，
 *          同时也提供了关闭流的方法。
 *          通过这个类的接口，可以对数据进行读取和写入操作，
 *          这对于文件操作、网络通信等场景非常有用。
 */
class Stream {
public:
    using ptr = std::shared_ptr<Stream>;
    virtual ~Stream() {}

    /**
     * @brief 从流中读取数据到buffer中
     * 
     * @param buffer 用于保存从流中读取的数据
     * @param len 想要读取的数据的长度
     * @return ssize_t ssize_t 是一个有符号整数类型，通常用于表示读取或写入的字节数。它的大小在不同的平台上可能会有所不同，但通常是与 size_t 相同的大小。
     */
    virtual ssize_t read(void* buffer, size_t len) = 0;

    /**
     * @brief 从流中读取数据到ByteArray中
     */
    virtual ssize_t read(ByteArray::ptr buffer, size_t len) = 0;

    /**
     * @brief 将buffer中的数据写入到流中
     * 
     * @param buffer 保存着用于写入流中的数据
     * @param len 要写入的长度
     * @return ssize_t 
     */
    virtual ssize_t write(const void* buffer, size_t len) = 0;

    /**
     * @brief 将ByteArray中的数据写入到流中
     */
    virtual ssize_t write(ByteArray::ptr buffer, size_t len) = 0;

    /**
     * @brief 关闭流
     */
    virtual void close() = 0;

    /**
     * @brief 从流中读取固定长度的数据到buffer中
     */
    ssize_t readFixSize(void* buffer, size_t len);
    ssize_t readFixSize(ByteArray::ptr buffer, size_t len);
    ssize_t writeFixSize(const void* buffer, size_t len);
    ssize_t writeFixSize(ByteArray::ptr buffer, size_t len);


private:

};
}

#endif