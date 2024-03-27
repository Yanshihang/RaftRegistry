//
// File created on: 2024/03/25
// Author: Zizhou

#ifndef RR_RAFT_SNAPSHOT_H
#define RR_RAFT_SNAPSHOT_H

#include <cstdint>
#include <filesystem>
#include "RaftRegistry/rpc/serializer.h"

namespace RR::raft {

/**
 * @brief 快照元数据
 */
struct SnapshotMeta {
    // 快照中包含的最后日志条目的索引值
    int64_t index;
    // 快照中包含的最后日志条目的任期号
    int64_t term;

    friend rpc::Serializer& operator<<(rpc::Serializer& serializer, const SnapshotMeta& snap) {
        serializer << snap.index << snap.term;
        return serializer;
    }

    friend rpc::Serializer& operator>>(rpc::Serializer& serializer, SnapshotMeta& snap) {
        serializer >> snap.index >> snap.term;
        return serializer;
    }
};

/**
 * @brief 快照
 */
struct Snapshot {
    using ptr = std::shared_ptr<Snapshot>;
    // 快照的元数据，包含了关键信息如最后日志条目的索引和任期号
    SnapshotMeta metadata;
    // 快照的数据部分，存储了使用者状态的序列化表示
    std::string data;
    
    bool empty() const {
        return metadata.index == 0;
    }

    friend rpc::Serializer& operator<<(rpc::Serializer& serializer, const Snapshot& snap) {
        serializer << snap.metadata << snap.data;
        return serializer;
    }

    friend rpc::Serializer& operator>>(rpc::Serializer& serializer, Snapshot& snap) {
        serializer >> snap.metadata >> snap.data;
        return serializer;
    }
};

/**
 * @brief 快照管理器类
 * 负责快照的存储和加载。它定义了如何将快照数据持久化到磁盘，并在需要时加载它们。
 */
struct Snapshotter {
    /**
     * @brief 构造函数
     * @param dir 快照的存储目录
     * @param snap_suffix 快照文件的后缀名，默认为".snap"
     */
    Snapshotter(const std::filesystem::path& dir, const std::string& suffix = ".snap") : m_dir(dir), m_snap_suffix(suffix) {}

    /**
    * @brief 存储并持久化一个快照
    * @param snapshot 要存储的快照，使用智能指针管理
    * @return bool 指示存储操作是否成功
    */
    bool saveSnap(const Snapshot::ptr& snapshot);

    /**
    * @brief 存储并持久化一个快照
    * @param snapshot 要存储的快照实例
    * @return bool 指示存储操作是否成功
    */
    bool saveSnap(const Snapshot& snapshot);

    /**
    * @brief 加载最新的一个快照
    * @return Snapshot::ptr 加载的快照的智能指针，如果没有快照则返回nullptr
    */
    Snapshot::ptr loadSnap();

private:
    /**
    * @brief 获取按逻辑顺序排列的快照文件名列表
    * @return std::vector<std::string> 快照文件名列表
    */
    std::vector<std::string> snapNames();

    /**
     * @brief 校验文件名后缀
    * @param names 待检查的文件名列表
    * @return std::vector<std::string> 经过筛选后包含合法快照文件名的列表
    */
    std::vector<std::string> checkSuffix(const std::vector<std::string>& names);

    /**
    * @brief 将给定的快照序列化并持久化到磁盘中
    * @param snapshot 要持久化的快照
    * @return bool 指示持久化操作是否成功
    */
    bool save(const Snapshot& snapshot);

    /**
    * @brief 从磁盘中读取指定文件名的快照并反序列化
    * @param snapname 快照文件的名称
    * @return std::unique_ptr<Snapshot> 指向加载的快照的智能指针，如果读取失败则为nullptr
    */
    std::unique_ptr<Snapshot> read(const std::string& snapname);
private:
    // 快照目录
    const std::filesystem::path m_dir;
    const std::string m_snap_suffix
}
}


#endif // RR_RAFT_SNAPSHOT_H