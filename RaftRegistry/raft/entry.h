//
// File created on: 2024/03/25
// Author: Zizhou

#ifndef RR_RAFT_ENTRY_H
#define RR_RAFT_ENTRY_H

#include "RaftRegistry/rpc/serializer.h"

namespace RR::raft {

// Entry结构体表示Raft共识算法中的单个日志条目。
struct Entry {
    int64_t index = 0; // 日志条目的索引，用于日志中的排序
    int64_t term = 0; // 创建日志条目时的任期号，用于Raft的领导人选举和一致性检查
    std::string data{}; // 要应用于状态机的命令或操作，以序列化数据形式存储

    std::string toString() const {
        return fmt::format("Term: {}, Index: {}, Data: {}", term, index, data);
    }

    // 使用提供的Serializer实例序列化日志条目，用于存储或网络传输
    friend rpc::Serializer& operator << (rpc::Serializer& s, const Entry& e) {
        s << e.index << e.term << e.data;
        return s;
    }

    // 使用提供的Serializer实例反序列化日志条目，用于从存储或网络读取
    friend rpc::Serializer& operator >> (rpc::Serializer& s, Entry& s) {
        s >> e.index >> e.term >> e.data;
        return s;
    }
}

} // namespace RR::raft


#endif // RR_RAFT_ENTRY_H