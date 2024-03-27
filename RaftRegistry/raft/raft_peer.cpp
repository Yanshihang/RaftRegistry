//
// File created on: 2024/03/26
// Author: Zizhou

#include "raft_peer.h"
#include "RaftRegistry/common/config.h"
#include "RaftRegistry/rpc/rpc.h"
#include <optional>


namespace RR::raft {
static auto Logger = GetLoggerInstance();

// RPC连接超时时间的配置项
static ConfigVar<uint64_t>::ptr g_rpc_timeout = Config::LookUp<uint64_t>("raft.rpc.timeout", 3000, "raft rpc timeout(ms)");

// RPC连接重试次数的配置项
static ConfigVar<uint32_t>::ptr g_connect_retry = config::LookUp<uint32_t>("raft.rpc.connect_retry", 3, "raft rpc connect retry times");

static uint64_t s_rpc_timeout;
static uint32_t s_connect_retry;

namespace {
struct RaftPeerIniter {
    RaftPeerIniter () {
        s_rpc_timeout = g_rpc_timeout->getValue();
        g_rpc_timeout->addListener([](const uint64_t& old_value, const uint64_t& new_value) {
            SPDLOG_LOGGER_INFO(Logger, "raft rpc timeout changed from {} to {}", old_value, new_value);
            s_rpc_timeout = new_value;
        });

        s_connect_retry = g_connect_retry->getValue();
        g_connect_retry->addListener([](const uint32_t& old_value, const uint32_t& new_value) {
            SPDLOG_LOGGER_INFO(Logger, "raft rpc connect retry times changed from {} to {}", old_value, new_value);
            s_connect_retry = new_value;
        });
    }
};

[[maybe_unused]] static RaftPeerIniter s_initer; // 创建一个RaftPeerIniter的实例，用于初始化配置

}

RaftPeer::RaftPeer(int64_t id, Address::ptr address) : m_id(id), m_address(std::move(address)) {
    m_client = std::make_shared<rpc::RpcClient>(); // 创建RPC客户端
    m_client->setHeartbeat(false);  // 关闭心跳
    m_client->setTimeout(s_rpc_timeout);
}

bool RaftPeer::connect() {
    if (!m_client->isClosed()) { // 如果客户端没有关闭，则返回true
        return true;
    }

    for (int i =1;i <= static_cast<int>(s_connect_retry); ++i) { // 根据设置的重试次数尝试连接
        m_client->connect(m_address); // 尝试连接
        if (!m_client->isClosed()) { // 如果连接成功
            return true; // 返回true
        }
        co_sleep(10*i); // 等待一段时间后重试，时间随重试次数增加
    }
    return false;
}

std::optional<RequestVoteReply> RaftPeer::requestVote (const RequestVoteArgs& args) {
    if (!connect()) { // 如果未连接
        return std::nullopt;
    }

    rpc::Result<RequestVoteArgs> result = m_client->call<RequestVoteReply>(REQUEST_VOTE, args); // 调用RPC请求
    if (result.getCode() == rpc::RpcState::RPC_SUCCESS) { // 调用成功
        return result.getVal();
    }
    if (result.getCode() == rpc::RpcState::RPC_CLOSED) { // 调用失败
        m_client->close();
    }

    // 其它调用结果
    SPDLOG_LOGGER_DEBUG(Logger, "rpc call node[{}] method [{}] failed, code is {}, msg is {}, requestvoteargs is {}", m_id, REQUEST_VOTE, result.getCode(), result.getMsg(), args.toString());
    return std::nullopt;
}

std::optional<AppendEntriesReply> RaftPeer::appendEntries(const AppendEntriesArgs& args) {
    if (!connect()) { // 确保与远程节点连接
        return std::nullopt;
    }

    rpc::Result<AppendEntriesReply> result = m_client->call<RequestVoteReply>(APPEND_ENTRIES, args);
    if (result.getCode() == rpc::RpcState::RPC_SUCCESS) {
        return result.getVal();
    }
    if (result.getCode() == rpc::RpcState::RPC_CLOSED) {
        m_client->close();
    }

    SPDLOG_LOGGER_DEBUG(Logger, "rpc call node[{}] method [{}] failed, code is {}, msg is {}, appendentriesargs is {}", m_id, APPEND_ENTRIES, result.getCode(), result.getMsg(), args.toString());
    return std::nullopt;
}

std::optional<InstallSnapshotReply> RaftPeer::installSnapshot(const InstallSnapshotArgs& args) {
    if (!connect()) {
        return std::nullopt;
    }

    rpc::Result<InstallSnapshotReply> result = m_client->call<InstallSnapshotReply>(INSTALL_SNAPSHOT, args);
    if (result.getCode() == rpc::RpcState::RPC_SUCCESS) {
        return result.getVal();
    }
    if (result.getCode() == rpc::RpcState::RPC_CLOSED) {
        m_client->close();
    }

    SPDLOG_LOGGER_DEBUG(Logger, "rpc call node[{}] method [{}] failed, code is {}, msg is {}, installsnapshotargs is {}", m_id, INSTALL_SNAPSHOT, result.getCode(), result.getMsg(), args.toString());
    return std::nullopt;
}

} // namespace RR::raft