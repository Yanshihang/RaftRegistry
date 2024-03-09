//
// File created on: 2024/03/06
// Author: Zizhou

#ifndef RR_BYTE_ARRAY_H
#define RR_BYTE_ARRAY_H

#include <memory>

#include "util.h"


namespace RR {
class ByteArray {
public:
    using ptr = std::shared_ptr<ByteArray>;

    ByteArray(size_t size = 4096);

    /**
     * @brief 销毁ByteArray对象，主要是销毁Node链表
     */
    ~ByteArray();

    /**
     * @brief 存储节点,一个node存储一个字符串
     * 
     * @details Node结构体定义了一个内存块的相关信息，包括：
     *          - 内存块的地址指针
     *          - 下一个内存块的地址
     *          - 内存块的大小
     *          ByteArray类通过使用Node结构体来管理内存块的分配和释放。
     *          通过Node结构体的链表形式，ByteArray类能够有效地管理和操作字节数组的存储。
     */
    class Node {
    public:
        Node();
        Node(size_t size);
        ~Node();

        char* ptr; // char数组的指针，char数组用于存储数据
        Node* next; // 下一个块的指针
        size_t size; //块的大小
    };

    bool isLittleEndian() const;

    void setLittleEndian();

    void setBigEndian();

    // 写入数据

    /**
     * @brief 将buf中的size长度的数据写入到node链表中
     * 
     * @param buf 内存缓冲指针
     * @param size 数据大小
     */
    void write(const void* buf, size_t size);

    /**
     * @brief 首先判断被写入的值是否为host字节序，如果不是则转换字节序；然后调用write()函数写入
     * 
     * @tparam T 传入参数的类型
     * @param value 被写入的值
     */
    template<typename T>
    void writeFInt(T value) {
        // 判断要写入的值是否为host字节序
        if (!m_endian == std::endian::native) {
            value = ByteSwap(value)
        }
        write(&value, sizeof(value))
    }

    void writeFInt8(int8_t value);
    void writeFUint8(uint8_t value);

    // 写入固定位数的整数
    /**
     * @brief 写入固定长度int16_t类型的数据
     * 
     * @param value 要写入的数据
     */
    void writeFInt16(int16_t value);
    void writeFUint16(uint16_t value);
    void writeFInt32(int32_t value);
    void writeFUint32(uint32_t value);
    void writeFInt64(int64_t value);
    void writeFUint64(uint64_t value);

    // 写入可变长度的整数（只适用于32位64位）
    /**
     * @brief 写入可变长度int32_t类型的数据
     * 
     * @param value 要写入的数据
     * @details 按照Varint的编码规则，将value写入到node链表中
     */
    void writeVInt32(int32_t value);
    void writeVUint32(uint32_t value);
    void writeVInt64(int64_t value);
    void writeVUint64(uint64_t value);

    /**
     * @brief 写入float类型的数据
     * 
     * @param value 
     */
    void writeFloat(float value);
    
    /**
     * @brief 写入double类型的数据
     * 
     * @param value 
     */
    void writeDouble(double value);

    /**
     * @brief 写入字符串
     * 
     * @param value 
     */
    void writeStringF16(const std::string& value);
    void writeStringF32(const std::string& value);
    void writeStringF64(const std::string& value);
    void writeStringVint(const std::string& value);
    void writeStringWithoutLength(const std::string& value);

    // 读取数据

    /**
     * @brief 从node链表中读取size长度的数据到buf中
     * 
     * @param buf 内存缓冲指针
     * @param size 要读取的数据大小
     */
    void read(void* buf, size_t size);

    /**
     * @brief 读取size长度的数据到buf中
     * 
     * @param buf 内存缓冲指针
     * @param size 要读取的数据大小
     * @param position 读取操作开始的位置
     */
    void read(void* buf, size_t size, size_t position) const;
    
    int8_t readFInt8();
    uint8_t readFUint8();
    int16_t readFInt16();
    uint16_t readFUint16();
    int32_t readFInt32();
    uint32_t readFUint32();
    int64_t readFInt64();
    uint64_t readFUint64();
    int32_t readVInt32();
    uint32_t readVUint32();
    int64_t readVInt64();
    uint64_t readVUint64();

    float readFloat();
    double readDouble();

    std::string readStringF16();
    std::string readStringF32();
    std::string readStringF64();
    std::string readStringVint();
    // std::string readStringWithoutLength();

    bool writeToFile(const std::string& name) const;
    bool readFromFile(const std::string& name);

    size_t getReadableSize() const;

    size_t getPosition() const;
    void setPosition(size_t position);

    size_t getNodeSize() const;

    /**
     * @brief 将ByteArray中的数据转换为字符串
     */
    std::string toString() const;

    /**
     * @brief 将ByteArray中的数据转换为16进制字符串
     */
    std::string toHexString() const;

    /**
     * @brief 获取可读取的数据，保存到一个或多个iovec缓冲区的集合
     * 
     * @param buffers 用于保存缓冲区的集合
     * @param len 想要读取的数据长度
     * @return uint64_t 返回实际读取的数据长度
     * 
     * @details 获取一个或多个缓冲区的集合，
     *          这些缓冲区共同表示ByteArray中当前位置之后的数据。
     *          这些缓冲区映射到ByteArray内部的不同内存块，可用于执行非连续内存的读取操作。
     */
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const;
    
    /**
     * @brief 获取可读取的数据，保存到一个或多个iovec缓冲区的集合，从position位置开始
     * 
     * @param buffers 用于保存缓冲区的集合
     * @param len 想要读取的数据长度
     * @param position 读取位置
     * @return uint64_t 返回实际读取的数据长度
     */
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const;


    /**
     * @brief 获取可写入的缓存，保存到一个或多个iovec缓冲区的集合
     * 
     * @param buffers 保存可写入的内存的iovec集合
     * @param len 写入长度
     * @return uint64_t 返回实际可写入数据的长度
     */
    uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);

    // 清空ByteArray中的数据
    void clear();

private:
    /**
     * @brief 获取可用容量
     */
    size_t getAvailableCapacity() const;

    /**
     * @brief 增加容量
     * 
     * @param size 增加的字节数
     * 
     * @details 判断是否需要增加容量的逻辑也在这个函数中
     *          会先判断剩余容量是否大于size，
     *          如果大于则不需要分配新的内存块
     */
    void addCapacity(size_t size);


    size_t m_nodeSize; // 一个内存块的大小
    size_t m_position; // 当前位置，以字节为单位
    size_t m_capacity; // 链表总容量

    // 每当且仅当m_position的值大于m_size时，就会将m_size的值更新为m_position；而m_position变小时，不会更新m_size的值为m_position
    // 但是m_size的值不会减小，这样就可以保证m_size的值一直是ByteArray中的数据的大小
    size_t m_size; // 当前已保存的字节数
    
    std::endian m_endian; // 字节序

    Node* m_head; // 指向整个node链表中的第一个node
    Node* m_current; // 指向当前node，即第一个没有满的node

};

}
#endif