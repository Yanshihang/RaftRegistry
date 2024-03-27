//
// File created on: 2024/03/25
// Author: Zizhou

#ifndef RR_RAFT_PERSISTER_H
#define RR_RAFT_PERSISTER_H

#include <cstdint>
#include <optional>
#include <memory>
#include <filesystem>
#include <vector>
#include <libgo/libgo.h>
#include "RaftRegistry/raft/snapshot.h"
#include "RaftRegistry/rpc/serializer.h"
#include "RaftRegistry/raft/entry.h"

namespace RR::raft {
// raft节点状态的持久化数据
struct HardState {
    // 当前任期号
    int64_t term;
    // 任期内给谁投过票
    int64_t vote;
    // 已经commit的最大index
    int64_t commit;

    friend rpc::Serializer& operator>>(rpc::Serializer& serializer, HardState& state) {
        serializer >> state.term >> state.vote >> state.commit;
        return serializer;
    }
    
    friend rpc::Serializer& operator<< (rpc::Serializer& serializer, HardState state) {
        serializer << state.term << state.vote << state.commit;
        return serializer;
    }
};

/**
 * @brief 持久化存储
 */
class Persister {
public:
    using ptr = std::shared_ptr<Persister>;
    using MutexType = co::co_mutex;

    explicit Persister(const std::filesystem::path& persist_path = ".");
    
    ~Persister() = default;

    /**
     * @brief 获取持久化的 raft 状态
     */
    std::optional<HardState> loadHardState();

    /**
     * @brief 获取持久化的 log
     */
    std::optional<std::vector<Entry>> loadEntries();

    /**
     * @brief 获取快照
     */
    Snapshot::ptr loadSnapshot();

    /**
     * @brief 获取 raft state 的长度
     */
    int64_t getRaftStateSize();

    /**
     * @brief 持久化当前raft节点的数据
     * 
     * @param hs 节点状态
     * @param entries 日志条目集合
     * @param snapshot 全量序列化数据
     * @return true 
     * @return false 
     */
    bool persist(const HardState& hs, const std::vector<Entry>& entries, const Snapshot::ptr snapshot = nullptr);

    /**
     * @brief 获取快照路径
     */
    std::string getFullPathName() {
        // canonical() 返回规范化的路径（绝对路径），如果m_path是相对路径或符号链接，则返回绝对路径
        return canonical(m_path);
    }
private:
    MutexType m_mutex;
    const std::filesystem::path m_path;
    Snapshotter m_snapshotter;
    const std::string m_name = "raft_state";
}
}

#endif // RR_RAFT_PERSISTER_H