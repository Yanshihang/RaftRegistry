//
// File created on: 2024/03/22
// Author: Zizhou

#ifndef RR_RPC_SERIALIZER_H
#define RR_RPC_SERIALIZER_H

#include <memory>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include <algorithm>
#include <cstdint>
#include <string>
#include <sstream>
#include <utility>
#include "RaftRegistry/common/byte_array.h"
#include "RaftRegistry/common/reflection.h"
#include "RaftRegistry/rpc/protocol.h"

namespace RR::rpc {

/**
 * @brief 基于反射实现的无侵入式序列化和反序列化
 * @details 序列化有以下规则：
 * 1.默认情况下序列化，8，16位类型以及浮点数不压缩，32，64位有符号/无符号数采用 zigzag 和 varints 编码压缩
 * 2.针对 std::string 会将长度信息压缩序列化作为元数据，然后将原数据直接写入。char数组会先转换成 std::string 后按此规则序列化
 * 3.调用 writeFint 将不会压缩数字，调用 writeRowData 不会加入长度信息
 *
 * 支持标准库容器：
 * 顺序容器：string, list, vector
 * 关联容器：set, multiset, map, multimap
 * 无序容器：unordered_set, unordered_multiset, unordered_map, unordered_multimap
 * 异构容器：tuple
 *
 * 用户自定义的类只有为聚合类才能自动序列化，如果非聚合类可以实现以下两个友元函数来支持序列化
 * friend Serializer& operator<<(Serializer& s, const UserDefind& u);
 * friend Serializer& operator>>(Serializer& s, UserDefind& u);
 */
class Serializer {
public:
    using ptr = std::shared_ptr<Serializer>;

    // 构造函数，初始化字节数组
    Serializer() {
        m_byteArray = std::make_shared<RR::ByteArray>();
    }

    // 通过已有的字节数组构造序列化器
    Serializer(RR::ByteArray::ptr byteArray) : m_byteArray(byteArray) {}

    // 通过字符串初始化序列化器，并将字符串作为原始数据写入
    Serializer(const std::string& str) {
        m_byteArray = std::make_shared<RR::ByteArray>();
        writeRowData(&str[0], str.size());
        reset();
    }

    // 通过字符数组初始化序列化器，并将字符数组作为原始数据写入
    Serializer(const char* str, int len) {
        m_byteArray = std::make_shared<RR::ByteArray>();
        writeRowData(str, len);
        reset();
    }

    // 获取序列化器中数据的大小
    int size() const {
        return m_byteArray->getSize();
    }

    // 重置读写位置，从头开始读取
    void reset() {
        m_byteArray->setPosition(0);
    }

    // 设置偏移量，用于跳过某些字节
    void offset(int off) {
        int old = m_byteArray->getPosition();
        m_byteArray->setPosition(old + off);
    }

    // 将序列化器中的数据转换为字符串形式
    std::string toString() {
        return m_byteArray->toString();
    }

    // 获取内部的字节数组对象
    ByteArray::ptr getByteArray() const {
        return m_byteArray;
    }

    /**
     * 写入原始数据
     * @param in 原始数据的指针
     * @param len 数据长度
     */
    void writeRowData(const char* in, int len) {
        m_byteArray->write(in, len);
    }

    /**
     * 写入未压缩的数字
     * @param value 要写入的值
     */
    template <typename T>
    void writeFint(T value) {
        m_byteArray->WriteFInt(value);
    }

    // 清除字节数组中的数据
    void clear() {
        m_byteArray->clear();
    }

    /**
     * @brief 读取数据
     * 
     * @tparam T 传入数据的类型
     * @param t 存放读取到的变量
     */
    template <typename Type>
    void read(Type& t) {
        using T = std::remove_cvref_t<Type>;
        static_assert(!std::is_pointer_v<T>); // 传入的数据类型不能是指针

        // 如果T是bool、char或unsigned char类型，则直接读取int8_t类型的数据，即调用readFInt8
        if constexpr (std:is_same_v<T, bool> || std::is_same_v<T, char> || std::is_same_v<T, unsigned char>) {
            t = m_byteArray->readFInt8();
        } else if constexpr (std::is_same_v<T, float>) { // 如果T是float类型，则读取float类型的数据
            t = m_byteArray->readFloat();
        } else if constexpr (std::is_same_v<T, double>) {
            t = m_byteArray->readDouble();
        } else if constexpr (std::is_same_v<T, int8_t>) {
            t = m_byteArray->readFInt8();
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            t = m_byteArray->readFUint8();
        } else if constexpr (std::is_same_v<T, int16_t>) {
            t = m_byteArray->readFInt16();
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            t = m_byteArray->readFUint16();
        } else if constexpr (std::is_same_v<T, int32_t>) {
            t = m_byteArray->readVInt32();
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            t = m_byteArray->readVUint32();
        } else if constexpr (std::is_same_v<T, int64_t>) {
            t = m_byteArray->readVInt64();
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            t = m_byteArray->readVUint64();
        } else if constexpr (std::is_same_v<T, std::string>) {
            t = m_byteArray->readStringVint();
        } else if constexpr (std::is_enum_v<T>) {
            t = static_cast<T>(m_byteArray-?readVInt32());
        } else if constexpr (std::is_class_v<T>) { // 如果传入的数据的类型是类，则判断是否是聚合类，如果是聚合类则递归读取
            static_assert(std::is_aggregate_v<T>);
            VisitMembers(t, [&](auto &&...items) {
                static_cast<void>((*this) >> ... >> items);
            });
        }
    }

    /**
     * @brief 写入数据
     * 
     * @tparam Type 要写入数据的类型
     * @param t 要写入的数据
     */
    template <typename Type>
    void write(const Type& t) {
        using T = std::remove_cvref_t<Type>;
        static_cast(!<std::is_pointer_v<T>);
        if constexpr (std:is_same_v<T, bool> || std::is_same_v<T, char> || std::is_same_v<T, unsigned char>) {
            m_byteArray->writeFInt8(t);
        } else if constexpr (std::is_same_v<T, float>) {
            m_byteArray->writeFloat(t);
        } else if constexpr (std::is_same_v<T, double>) {
            m_byteArray->writeDouble(t);
        } else if constexpr (std::is_same_v<T, int8_t>) {
            m_byteArray->writeFInt8(t);
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            m_byteArray->writeFUint8(t);
        } else if constexpr (std::is_same_v<T, int16_t>) {
            m_byteArray->writeFInt16(t);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            m_byteArray->writeFUint16(t);
        } else if constexpr (std::is_same_v<T, int32_t>) {
            m_byteArray->writeVInt32(t);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            m_byteArray->writeVUint32(t);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            m_byteArray->writeVInt64(t);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            m_byteArray->writeVUint64(t);
        } else if constexpr (std::is_same_v<T, std::string>) {
            m_byteArray->writeStringVint(t);
        } else if constexpr (std::is_same_v<T, char*>) {
            m_byteArray->writeStringVint(std::string(t));
        } else if constexpr (std::is_enum_v<T>) {
            m_byteArray->writeVInt32(static_cast<int32_t>(t));
        } else if constexpr (std::is_class_v<T>) {
            static_assert(std::is_aggregate_v<T>);
            VisitMembers(t, [&](auto &&... items) {
                static_cast<void>((*this) << ... << items);
            });
        }
    }

    // 重载流运算符 >> 用于反序列化
    template <typename T>
    Serializer& operator>> (T& i) {
        read(i); // 调用read方法反序列化数据到i
        return *this;
    }

    // 重载流运算符 << 用于序列化
    template <typename T>
    Serializer& operator << (const T& i) {
        write(i); // 调用write方法序列化数据到i
        return *this;
    }

    // 以下一系列方法用于序列化和反序列化各种容器类型，如std::list, std::vector, std::set等
    // 实现逻辑大致相同：首先序列化容器的大小，然后逐个序列化容器中的元素
    // 反序列化时先读取容器的大小，然后按大小读取元素填充到容器中

    /**
     * @brief 实际的反序列化函数，利用折叠表达式展开参数包
     */
    template <typename... Args>
    Serializer& operator>>(std::tuple<Args...>& t) {
        // 反序列化std::tuple，使用折叠表达式展开参数包，并逐个反序列化
        const auto& deserializer = [this]<typename Tuple, std::size_t... Index>(Tuple& t, std::index_sequence<Index...>) {
            static_cast<void>((*this) >> ... >> std::get<Index>(t));
        }
        deserializer(t, std::index_sequence_for<Args...>{});
        return *this;
    }

    /**
     * @brief 实际的序列化函数，利用折叠表达式展开参数包
     */
    template <typename... Args>
    Serializer& operator<< (const std::tuple<Args...>& t) {
        std::apply([this](const auto&... args) {
            static_cast<void>((*this) << ... << args);
        }, t);
        return *this;
    }

    // 对于容器的序列化和反序列化，处理逻辑基本相似，首先处理容器的大小，然后逐个处理容器内的元素
    
    // 序列化std::vector
    template <typename T>
    Serializer& operator>>(std::vector<T>& vec) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            T t;
            (*this) >> t;
            vec.template emplace_back(t);
        }
        return *this;
    }

    template <typename T>
    Serializer& operator<< (const std::vector<T>& vec) {
        write(vec.size());
        for (auto& t : v) {
            (*this) << t
        }
        return *this;
    }

    template <typename T>
    Serializer& operator>> (std::list<T>& list) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            T t;
            (*this) >> t;
            list.template emplace_back(t);
        }
        return *this;
    }

    template <typename T>
    Serializer& operator<< (const std::list<T>& list) {
        write(list.size());
        for (auto& t : list) {
            (*this) << t;
        }
        return *this;
    }

    template <typename T>
    Serializer& operator>> (std::set<T>& set) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            T t;
            (*this) >> t;
            set.template emplace(t);
        }
        return *this;
    }

    template <typename T>
    Serializer& operator<< (const std::set<T>& set) {
        write(set.size());
        for (auto& t : set) {
            (*this) << t;
        }
        return *this;
    }

    template <typename T>
    Serializer& operator>> (std::multiset<T>& mset) {
        size_t size;
        read(size);
        for (size_t i = 0;i< size;++i) {
            T t;
            (*this) >> t;
            mset.template emplace(t);
        }
        return *this;
    }

    template <typename T>
    Serializer& operator<< (const std::multiset<T>& mset) {
        write(mset.size());
        for (auto& t : mset) {
            (*this) << t;
        }
        return *this;
    }

    template <typename T>
    Serializer& operator>> (std::unordered_set<T>& uset) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            T t;
            (*this) >> t;
            uset.template emplace(t);
        }
        return *this;
    }

    template <typename T>
    Serializer& operator<< (const std::unordered_set<T>& uset) {
        write(uset.size());
        for (auto& t : uset) {
            (*this) << t;
        }
        return *this;
    }

    template <typename T>
    Serializer& operator>> (std::unordered_multiset<T>& umset) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            T t;
            (*this) >> t;
            umset.template emplace(t);
        }
        return *this;
    }

    template <typename T>
    Serializer& operator<< (const std::unordered_multiset<T>& umset) {
        write(umset.size());
        for (auto& t : umset) {
            (*this) << t;
        }
        return *this;
    }

    template <typename K, typename V>
    Serializer& operator>> (std::pair<K, V>& pair) {
        (*this) >> pair.first >> pair.second;
        return *this;
    }

    template <typename K, typename V>
    Serializer& operator<< (const std::pair<K, V>& pair) {
        (*this) << pair.first << pair.second;
        return *this;
    }

    template <typename K, typename V>
    Serializer& operator>> (std::map<K, V>& map) {
        size_t size;
        read(size);
        for (size_t i = 0;i < size;++i) {
            std::pair<K, V> p;
            (*this) >> p;
            map.template emplace(p);
        }
        return *this;
    }

    template <typename K, typename V>
    Serializer& operator<< (const std::map<K, V>& map) {
        write(map.size());
        for (auto& p : map) {
            (*this) << p;
        }
        return *this;
    }

    template <typename K, typename V>
    Serializer& operator>> (std::unordered_map<K, V>& umap) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            std::pair<K, V> p;
            (*this) >> p;
            umap.template emplace(p);
        }
        return *this;
    }

    template <typename K, typename V>
    Serializer& operator<< (const std::unordered_map<K, V>& umap) {
        write(umap.size());
        for (auto& p : umap) {
            (*this) << p;
        }
        return *this;
    }

    template <typename K, typename V>
    Serializer& operator>> (std::multimap<K, V>& mmap) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            std::pair<K, V> p;
            (*this) >> p;
            mmap.template emplace(p);
        }
        return *this;
    }

    template <typename K, typename V>
    Serializer& operator<< (const std::multimap<K, V>& mmap) {
        write(mmap.size());
        for (auto& p : mmap) {
            (*this) << p;
        }
        return *this;
    }

    template <typename K, typename V>
    Serializer& operator>> (std::unordered_multimap<K, V>& ummap) {
        size_t size;
        read(size);
        for (size_t i = 0; i < size; ++i) {
            std::pair<K, V> p;
            (*this) >> p;
            ummap.template emplace(p);
        }
        return *this;
    }

    template <typename K, typename V>
    Serializer& operator<< (const std::unordered_multimap<K, V>& ummap) {
        write(ummap.size());
        for (auto& p : ummap) {
            (*this) << p;
        }
        return *this;
    }


private:
    RR::ByteArray::ptr m_byteArray;
    Protocol::ptr m_protocol;
};

}

#endif //RR_RPC_SERIALIZER_H