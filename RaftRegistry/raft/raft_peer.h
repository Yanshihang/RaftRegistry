//
// File created on: 2024/03/26
// Author: Zizhou

#ifndef RR_RAFT_RAFT_PEER_H
#define RR_RAFT_RAFT_PEER_H

#include <string>
#include <stdint.h>
#include "RaftRegistry/rpc/rpc_client.h"
#include "snapshot.h"

namespace RR::raft {

inline const std::string REQUEST_VOTE = "RaftNode::handleRequestVote";
inline const std::string APPEND_ENTRIES = "RaftNode::handleAppendEntries";
inline const std::string INSTALL_SNAPSHOT = "RaftNode::handleInstallSnapshot";

/**
 * @brief RequestVote rpc 调用的参数
 */
struct RequestVoteArgs {
    int64_t term;   // 候选人任期
    int64_t candidateId;    // 候选人ID
    int64_t lastLogIndex;   // 候选人最后一条日志的索引
    int64_t lastLogTerm;    // 候选人最后一条日志的任期
    std::string toString() const {
        return fmt::format("{Term: {}, candidateId: {}, lastLogIndex: {}, lastLogTerm: {}}", term, candidateId, lastLogIndex, lastLogTerm);
    }
};

/**
 * @brief RequestVote rpc 调用的返回值
 */
struct RequestVoteReply {
    int64_t term;   // 节点任期；当前任期号大于候选人的任期号时，候选人会转变为跟随者
    int64_t leaderId;   // 当前任期的leader ID
    bool voteGranted;   // true表示候选人赢得此节点的选票
    std::string toString() const {
        std::string str = fmt::format("Term: {}, leaderId: {}, voteGranted: {}", term, leaderId, voteGranted);
        return "{" + str + "}";
    }
};

/**
 * @brief AppendEntries rpc 调用的参数
 */
struct AppendEntriesArgs {
    int64_t term;   // leader的任期
    int64_t leaderId;   // leader的id
    int64_t prevLogIndex;   // 紧邻新日志之前的日志的索引
    int64_t prevLogTerm;    // 紧邻最新日志之前的日志的任期
    std::vector<Entry> entries;  // 要追加的日志条目(心跳时为空)
    int64_t leaderCommit;   // 领导人已知的已提交的最新日志条目的索引
    std::string toString() const {
        std::string str;
        str = fmt::format("term: {}, leaderId: {}, prevLogIndex: {}, prevLogTerm: {}, leaderCommit: {}, Entries: [", term, leaderId, prevLogIndex, prevLogTerm, leaderCommit);
        for (int i =0; i< entries.size(); ++i) {
            if (i) {
                str.push_back(',');
            }
            str += entries[i].toString();
        }
        return "{" + str + "]}";
    }
};

/**
 * @brief AppendEntries rpc 调用的返回值
 */
struct AppendEntriesReply {
    bool success = false;   // 如果跟随者所含有的条目和prevLogIndex和prevLogTerm匹配，那么返回true
    int64_t term;   // 当前任期，如果大于leader的任期，则leader会转变为跟随者
    int64_t leaderId;   // 当前任期的leader ID
    int64_t nextIndex;  // 下次希望接收的index
    std::string toString() const {
        std::string str = fmt.format("success: {}, term: {}, leaderId: {}, nextIndex: {}", success, term, leaderId, nextIndex);
        return "{" + str + "}";
    }
};

struct InstallSnapshotArgs {
    int64_t term;   // 领导人的任期号
    int64_t leaderId;   // 领导人id，以便跟随者重定向请求
    Snapshot snapshot;  // 快照
    std::string toString() const {
        std::string str = fmt::format("term: {}, leaderId: {}, snapshot: {}", term, leaderId, snapshot.toString());
        return "{" + str + "}";
    }
};

struct InstallSnapshotReply {
    int64_t term;   // 当前任期号，便于leader更新自己
    int64_t leaderId;   // 当前任期的leader ID
    std::string toString() const {
        std::string str = fmt::format("term: {}, leaderId: {}", term, leaderId);
        return "{" + str + "}";
    }
};

/**
 * @brief RaftNode 通过 RaftPeer 调用远端 Raft 节点，封装了 rpc 请求
 */
class RaftPeer {
public:
    using ptr = std::shared_ptr<RaftPeer>;

    RaftPeer(int64_t id, Address::ptr address);

    std::optional<RequestVoteReply> requestVote (const RequestVoteArgs& args);

    std::optional<AppendEntriesReply> appendEntries(const AppendEntriesArgs& args);

    std::optional<InstallSnapshotReply> installSnapshot(const InstallSnapshotArgs& args);

    Address::ptr getAddress() const { return m_address;}

private:
    bool connect();

private:
    int64_t m_id;   // raftNode的id
    rpc::RpcClient::ptr m_client;
    Address::ptr m_address;
};

}

#endif // RR_RAFT_RAFT_PEER_H