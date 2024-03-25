//
// File created on: 2024/03/21
// Author: Zizhou

#ifndef RR_RPC_PROTOCOL_H
#define RR_RPC_PROTOCOL_H

#include <cstdint>
#include <memory>
// #include <format>
#include "RaftRegistry/common/byte_array.h"

namespace RR::rpc {

/*
 * 私有通信协议
 * +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
 * |  BYTE  |        |        |        |        |        |        |        |        |        |        |             ........                                                           |
 * +--------------------------------------------+--------+--------------------------+--------+-----------------+--------+--------+--------+--------+--------+--------+-----------------+
 * |  magic | version|  type  |          sequence id              |          content length           |             content byte[]                                                     |
 * +--------+-----------------------------------------------------------------------------------------------------------------------------+--------------------------------------------+
 * 第一个字节是魔法数。
 * 第二个字节代表协议版本号，以便对协议进行扩展，使用不同的协议解析器。
 * 第三个字节是请求类型，如心跳包，rpc请求。
 * 第四个字节开始是一个32位序列号。
 * 第八个字节开始的四字节表示消息长度，即后面要接收的内容长度。
 * 第十二个字节开始是消息内容。
 */

class Protocol {
public:
    using ptr = std::shared_ptr<Protocol>;

    // 协议的基本属性常量
    static constexpr uint8_t MAGIC = 0XCC;
    static constexpr uint8_t DEFAULT_VERSION = 0X01;
    static constexpr uint8_t BASE_LENGTH = 11;

    // 消息类型枚举，定义了不同类型的协议消息
    enum class MsgType : uint8_t {
        HEARTBEAT_PACKET, // 心跳包

        RPC_PROVIDER, // 服务提供者声明
        RPC_CONSUMER, // 服务消费者声明

        RPC_REQUEST, // rpc请求
        RPC_RESPONSE, // rpc响应

        RPC_METHOD_REQUEST, // rpc方法请求
        RPC_METHOD_RESPONSE, // rpc方法响应

        RPC_SERVICE_REGISTER, // 服务注册
        RPC_SERVICE_REGISTER_RESPONSE, // 服务注册响应

        RPC_SERVICE_DISCOVERY, // 服务发现
        RPC_SERVICE_DISCOVERY_RESPONSE, // 服务发现响应

        RPC_PUBSUB_REQUEST, // 发布订阅请求
        RPC_PUBSUB_RESPONSE, // 发布订阅响应
    };

    // 静态工厂方法：用于创建并初始化一个Protocol对象的智能指针
    static ptr Create(MsgType type, const std::string& content, uint32_t id = 0) {
        ptr proto = std::make_shared<Protocol>();
        proto->setType(type);
        proto->setContent(content);
        proto->setSequenceId(id);
        return proto;
    }

    // 创建一个心跳包
    static ptr HeartBeat() {
        static ptr Heartbeat = Create(MsgType::HEARTBEAT_PACKET, "");
        return Heartbeat;
    }

    // getter

    uint8_t getMagic() const { return m_magic;}
    uint8_t getVersion() const { return m_version;}
    MsgType getType() const { return static_cast<MsgType>(m_type);}
    uint32_t getSequenceId() const { return m_sequenceId;}
    uint32_t getContentLength() const { return m_contentLength;}
    const std::string& getContent() const { return m_content;}

    // setter

    void setMagic(uint8_t magic) { m_magic = magic;}
    void setVersion(uint8_t version) { m_version = version;}
    void setType(MsgType type) { m_type = static_cast<uint8_t>(type);}
    void setSequenceId(uint32_t id) { m_sequenceId = id;}
    void setContentLength(uint32_t length) { m_contentLength = length;}
    void setContent(const std::string& content) {m_content = content;}

    // 编码协议消息的元数据的方法，不包括消息内容本身。这通常用于在发送消息内容之前预先发送元数据。
    ByteArray::ptr encodeMeta() {
        ByteArray::ptr bt = std::make_shared<ByteArray>();
        bt->writeFUint8(m_magic);
        bt->writeFUint8(m_version);
        bt->writeFUint8(m_type);
        bt->writeFUint32(m_sequenceId);
        bt->writeFUint32(m_contentLength);
        bt->setPosition(0); // 将字节数组的位置重置为起始位置，便于读取
        return bt; // 返回包含编码后的元数据的字节数组
    }

    // 完整地编码一个协议消息，包括元数据和实际的消息内容。
    ByteArray::ptr encode() {
        ByteArray::ptr bt = std::make_shared<ByteArray>();
        bt->writeFUint8(m_magic);
        bt->writeFUint8(m_version);
        bt->writeFUint8(m_type);
        bt->writeFUint32(m_sequenceId);
        bt->writeStringF32(m_content); // 写入内容，writeStringF32函数会首先写入内容长度，然后是内容本身
        bt->setPosition(0);
        return bt;
    }

    // 解码协议消息的元数据的方法
    void decodeMeta(ByteArray::ptr bt) {
        m_magic = bt->readFUint8();
        m_version = bt->readFUint8();
        m_type = bt->readFUint8();
        m_sequenceId = bt->readFUint32();
        m_contentLength = bt->readFUint32();
    }

    // 完整地解码一个接收到的字节数组，包括元数据和消息内容。
    void decode(ByteArray::ptr bt) {
        m_magic = bt->readFUint8();
        m_version = bt->readFUint8();
        m_type = bt->readFUint8();
        m_sequenceId = bt->readFUint32();
        m_content = bt->readStringF32();
        m_contentLength = m_content.size();
    }

    // toString方法用于生成协议消息的字符串表示，JSON格式的字符串模板
    std::string toString() {
        auto str = fmt::format(R"("magic": {}, "version" : {}, "type" : {}, "sequenceId" : {}, "contentLength" : [], "content" : {})", m_magic, m_version, m_type, m_sequenceId, m_contentLength, m_content);
        return str;
    }

private:
    uint8_t m_magic = MAGIC;
    uint8_t m_version = DEFAULT_VERSION;
    uint8_t m_type = 0;
    uint32_t m_sequenceId = 0;
    uint32_t m_contentLength = 0;
    std::string m_content;

};

} // namespace RR::rpc


#endif // RR_PROTOCOL_H