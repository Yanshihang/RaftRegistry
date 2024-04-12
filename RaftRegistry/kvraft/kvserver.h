//
// File created on: 2024/03/28
// Author: Zizhou

#ifndef RR_KVRAFT_KVSERVER_H
#define RR_KVRAFT_KVSERVER_H

#include <string>
#include <map>
#include <memory>
#include <libgo/libgo.h>
#include <cstdint>
#include "command.h"
#include "RaftRegistry/raft/raft_node.h"

namespace RR::kvraft {
using namespace RR::raft;

class KVServer {
public:
    using ptr = std::shared_ptr<KVServer>;
    using MutexType = co::co_mutex;
    using KVMap = std::map<std::string, std::string>;

    KVServer(std::map<int64_t, std::string>& servers, int64_t id, Persister::ptr persister, int64_t maxRaftState = 1000);
    ~KVServer();

    void start(); // 启动KV服务器，包括启动Raft节点和应用日志的协程
    void stop(); // 停止KV服务器和Raft节点

    // 处理客户端发送的命令请求
    CommandResponse handleCommand(CommandRequest request);

    // 提供的键值存储操作接口
    
    // 获取键对应的值
    CommandResponse Get(const std::string& key);
    // 设置键值对
    CommandResponse Put(const std::string& key, const std::string& value);
    // 在键对应的值后追加数据
    CommandResponse Append(const std::string& key, const std::string& value);
    // 删除键
    CommandResponse Delete(const std::string& key);
    CommandResponse Clear();

    // 获取当前的键值对数据，用于调试或其他目的
    [[nodiscard]] const KVMap& getData() const { return m_data;}

private:
    // 应用Raft日志到状态机的后台协程
    void applier();
    // 保存当前状态的快照
    void saveSnapshot(int64_t index);
    // 从快照中恢复状态
    void readSnapshot(Snapshot::ptr snapshot);

    // 检查是否是重复的请求
    bool isDuplicateRequest(int64_t client, int64_t command);
    // 判断是否需要创建快照
    bool needSnapshot();
    // 将日志应用到状态机
    CommandResponse applyLogToStateMachine(const CommandRequest& request);

private:
    MutexType m_mutex;
    int64_t m_id; // 服务器的ID
    co::co_chan<raft::ApplyMsg> m_applyCh; // 应用Raft日志的通道

    KVMap m_data;// 存储键值对的映射
    Persister::ptr m_persister; // 持久化器，用于保存Raft状态和快照
    std::unique_ptr<RaftNode> m_raft; // Raft节点实例

    std::map<int64_t, std::pair<int64_t, CommandRequest>> m_lastOperation; // 记录每个客户端的最后一次操作，用于去重
    std::map<int64_t, co::co_chan<CommandResponse>> m_notifyChans; // 用于通知命令处理结果的通道映射，key为日志索引

    int64_t m_lastApplied = 0; // 已应用的最后一个日志条目的索引
    int64_t m_maxRaftState = -1; // Raft状态达到此大小时，需要创建快照
}

}


#endif // RR_KVRAFT_KVSERVER_H