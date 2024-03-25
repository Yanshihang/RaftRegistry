//
// File created on: 2024/03/23
// Author: Zizhou

#include "RaftRegistry/rpc/rpc_session.h"

namespace RR::rpc {

RpcSession::RpcSession(Socket::ptr socket, bool owner) : SocketStream (socket, owner) {}

// 实现逻辑：
// 1. 创建Protocol对象和ByteArray对象来存储接收到的数据
// 2. 使用readFixSize函数尝试从socket中读取协议基础长度的数据
// 3. 如果读取失败（没有足够的数据），返回空指针
// 4. 使用Protocol对象的decodeMeta方法解析基础数据
// 5. 校验魔数，如果不匹配，返回空指针
// 6. 根据解析出的内容长度，再次从socket中读取指定长度的数据作为协议内容
// 7. 将读取的内容设置到Protocol对象中，并返回
Protocol::ptr RpcSession::recvProtocol() {
    Protocol::ptr proto = std::make_shared<Protocol>(); // 创建protocol对象
    ByteArray::ptr byteArray = std::make_shared<ByteArray>(); // 创建ByteArray对象
    
    // 读取基础长度的数据（头部）
    if (readFixSize(byteArray, proto->BASE_LENGTH) <= 0) {
        return nullptr;
    }

    byteArray->setPosition(0); // 重置读取位置，使得下面decode时能够从头开始读取
    proto->decodeMeta(byteArray); // 解析基础信息（头部信息）

    if (proto->getMagic() != Protocol::MAGIC) { /// 校验魔数
        return nullptr;
    }

    if (!proto->getContentLength()) { // 如果消息体长度为0，返回空指针
        return nullptr;
    }

    // 如果消息体长度不为0，继续读取消息体
    std::string buff;
    buff.resize(proto->getContentLength());

    if (readFixSize(&buff[0], proto->getContentLength())) {
        return nullptr;
    }

    proto->setContent(buff);
    return proto;
}

// 实现逻辑：
// 1. 对传入的Protocol对象进行编码，得到编码后的ByteArray对象
// 2. 使用互斥锁保证线程安全
// 3. 调用基类的writeFixSize方法，将编码后的ByteArray对象写入到socket中
// 4. 返回写入的字节数
ssize_t RpcSession::sendProtocol(Protocol::ptr protocol) {
    ByteArray::ptr byteArray = protocol->encode(); // 讲protocol对象进行编码
    std::unique_lock<MutexType> lock(m_mutex);
    return writeFixSize(byteArray, byteArray->getSize());
}

} // namespace RR::rpc