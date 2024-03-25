//
// File created on: 2024/03/23
// Author: Zizhou

#ifndef RAFTREGISTRY_RPC_SESSION_CPP
#define RAFTREGISTRY_RPC_SESSION_CPP

#include "RaftRegistry/rpc/protocol.h"
#include "RaftRegistry/net/socket_stream.h"
#include <libgo/libgo.h>

namespace RR::rpc {

class RpcSession : public SocketStream {
public:
    using ptr = std::shared_ptr<RpcSession>;
    using MutexType = co::co_mutex;

    /**
     * @brief 构造函数
     * 
     * @param socket 
     * @param owner 是否拥有socket
     */
    RpcSession(Socket::ptr socket, bool owner = true);

    /**
     * @brief 接受协议
     * 
     * @return Protocol::ptr 如果返回空则表示接收失败
     */
    Protocol::ptr recvProtocol();

    /**
     * @brief 发送协议
     * 
     * @param protocol 要发送的协议
     * @return ssize_t 发送的大小
     */
    ssize_t sendProtocol(Protocol::ptr protocol);


private:
    MutexType m_mutex;
};

} // namespace RR::rpc

#endif // RAFTREGISTRY_RPC_SESSION_CPP