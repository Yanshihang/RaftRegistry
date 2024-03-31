//
// File created on: 2024/03/26
// Author: Zizhou

#ifndef RR_RAFT_RAFT_NODE_H
#define RR_RAFT_RAFT_NODE_H

#include <string>
#include <map>
#include <cstdint>
#include <vector>
#include "RaftRegistry/rpc/rpc_server.h"
#include "entry.h"
#include "snapshot.h"
#include "RaftRegistry/rpc/serializer.h"
#include "raft_peer.h"
#include "raft_log.h"

namespace RR::raft {
using namespace RR::rpc;

/**
 * @brief Raft 的状态
 */
enum RaftState {
    Follower,
    Candidate,
    Leader
};

// 用来表示应用到状态机的消息
struct ApplyMsg {
    enum MsgType {
        ENTRY,
        SNAPSHOT
    };

    ApplyMsg() = default;
    explicit ApplyMsg(const Entry& entry) : type(ENTRY), data(entry.data), index(entry.index), term(entry.term) {}
    explicit ApplyMsg(const Snapshot& snap) : type(SNAPSHOT), data(snap.data), index(snap.metadata.index), term(snap.metadata.term) {}

    /**
     * @brief 用于将 ApplyMsg 的 data转换为指定类型
     * 
     * @tparam T 类型参数，用于返回类型，标明要转换为什么类型
     * @return T 
     */
    template <typename T>
    T as() {
        rpc::Serializer serializer(data);
        T t;
        serializer >> t;
        return t;
    }

    std::string toString() const {
        std::string typeStr;
        std::string result;
        switch (type) {
            case ENTRY:
                typeStr = "ENTRY";
                break;
            case SNAPSHOT:
                typeStr = "SNAPSHOT";
                break;
            default:
                typeStr = "UNEXPECTED";
                break;
        }
        typeStr = fmt.fromat("type: {}, index: {}, term: {}, data size: {}", typeStr, index, term, data.size());
        return typeStr;
    }

    MsgType type = ENTRY;
    std::string data{};
    int64_t index{};
    int64_t term{};
}

/**
 * @brief Raft 节点，处理 rpc 请求，并改变状态，通过 RaftPeer 调用远端 Raft 节点
 */
class RaftNode : public rpc::RpcServer {
public:
    using ptr = std::shared_ptr<RaftNode>;
    using Mutextype = co::co_mutex;

    /**
     * @brief 创建一个 Raft 节点
     * @param servers 其他 raft 节点的地址，key 为节点 id，value 为主机名或IP地址以及端口号
     * @param id 当前 raft 节点在集群内的唯一标识
     * @param persister raft 保存其持久状态的地方，并且从保存的状态初始化当前节点
     * @param applyChan 是发送达成共识的日志的 channel
     */
    RaftNode(std::map<int64_t, std::string>& servers, int64_t id, Persister::ptr persister, co::co_chan<ApplyMsg> applyChan);

    ~RaftNode();

    /**
     * @brief 启动raft节点
     * 
     * @details 如果硬状态存在，就恢复崩溃前的状态，否则，节点成为跟随者，任期为0。
     *          然后，它会重新安排选举，启动一个新的协程执行日志应用函数，并启动 RPC 服务器。
     */
    void start() override;
    
    /**
     * @brief 关闭raft节点
     * 
     */
    void stop() override;

    /**
     * @brief 增加raft节点
     * 
     * @param id 增加的节点id
     * @param address 增加的节点地址
     */
    void addPeer(int64_t id, Address::ptr address);

    /**
     * @brief 返回当前节点是否是leader
     * 
     * @return true 当前节点是leader
     * @return false 当前节点不是leader
     */
    bool isLeader();

    /**
     * @brief 获取当前节点状态
     * 
     * @return currentTerm 任期，isLeader 是否为 leader
     */
    std::pair<int64_t, bool> getState();

    /**
     * @brief 获取当前节点的leader id
     * 
     * @return int64_t 
     */
    int64_t getLeaderId() const { return m_leaderId;}

    /**
     * @brief 发起一条消息
     * @return 如果该节点不是 Leader 返回 std::nullopt
     */
    std::optional<Entry> propose(const std::string& data);

    template <typename T>
    std::optional<Entry> propose(const T& data) {
        std::unique_lock<Mutextype> lock(m_mutex);
        rpc::Serializer s;
        s << data;
        s.reset();
        return Propose(s.toString());
    }

    /**
     * @brief 处理远端 raft 节点的投票请求
     */
    RequestVoteReply handleRequestVote(RequestVoteArgs request);

    /**
     * @brief 处理远端 raft 节点的日志追加请求
     */
    AppendEntriesReply handleAppendEntries(AppendEntriesArgs request);

    /**
     * @brief 处理远端 raft 节点的快照安装请求
     */
    InstallSnapshotReply handleInstallSnapshot(InstallSnapshotArgs request);

    /**
     * @brief 获取节点id
     * 
     * @return int64_t 
     */
    int64_t getNodeId() const { return m_id; }

    /**
     * @brief 持久化，加锁
     * 
     * @param index 将该index之前的日志都删除
     * @param snap 快照数据
     */
    void persistStateAndSnapshot(int64_t index, const std::string& snap);

    /**
     * @brief 持久化，加锁
     * 
     * @param snap 快照数据
     */
    void persistSnapshot(Snapshot::ptr snap);

    /**
     * @brief 以字符串格式输出raft节点状态
     * 
     * @return std::string 
     */
    std::string toString();

    /**
     * @brief 获取心跳超时时间
     * 
     * @return uint64_t 
     */
    static uint64_t GetStableHeartbeatTimeout();

    /**
     * @brief 获取随机选举时间
     * 
     * @return uint64_t 
     */
    static uint64_t GetRandomizedElectionTimeout();

private:
    /**
     * @brief 由candidate或leader转为follower
     * 
     * @param term 任期
     * @param leaderId 任期领导人id，如果还未选举出来则为-1
     */
    void becomeFollower(int64_t term, int64_t leaderId=  -1);

    /**
     * @brief 转为candidate
     */
    void becomeCandidate();

    /**
     * @brief 转为leader
     */
    void becomeLeader();

    /**
     * @brief 重置选举定时器
     */
    void rescheduleElection();

    /**
     * @brief 重置心跳定时器
     */
    void resetHeartbeatTimer();

    /**
     * @brief 对一个节点发起复制请求;用于领导者节点向其他节点复制日志条目
     * 
     * @param peerId 目标节点的id
     */
    void replicateOneRound(int64_t peerId);

    /**
     * @brief 用来往applyCh中push提交的日志,将已提交的日志条目应用到状态机
     */
    void applier();

    /**
     * @brief 开始选举，发起异步投票
     */
    void startElection();

    /**
     * @brief 广播心跳
     */
    void broadcastHeartbeat();

    /**
     * @brief 持久化，内部调用，不加锁
     * 
     * @param snap 
     */
    void persist(Snapshot::ptr snap = nullptr);

    /**
     * @brief 发起一条消息，不加锁
     * 
     * @param data 
     * @return std::optional<Entry> 
     * 
     * @details 这个方法的目的是在当前节点是领导者时，提出一个新的日志条目。
     */
    std::optional<Entry> Propose(const std::string& data);

private:
    Mutextype m_mutex;
    // 节点状态，初始为 Follower
    RaftState m_state = Follower;
    // 节点的唯一id
    int64_t m_id;
    // 任期内的leader id，用于follower返回给客户端，让客户端重定向请求到leader，-1表示无leader
    int64_t m_leaderId = -1;
    // 节点已知最新的任期（节点首次启动时，初始化为0，单调递增）
    int64_t m_currentTerm = 0;
    // 当前任期内收到选票的candidateId，如果没有则为-1
    int64_t m_votedFor = -1;
    // 日志条目，每个条目包含了用于状态机的命令，以及leader接收到该条目时的任期（初始索引为1）
    RaftLog m_logs;
    // 保存其他节点，key为节点id，value为节点信息
    std::map<int64_t, RaftPeer::ptr> m_peers;
    // 对于每一台服务器，发送到该服务器的下一个日志条目的索引（初始值为领导人最后的日志条目的索引+1）
    std::map<int64_t, int64_t> m_nextIndex;
    // 对于每一台服务器，已知的已经复制到该服务器的最高日志条目的索引（初始值为0，单调递增）
    std::map<int64_t, int64_t> m_matchIndex;
    // 选举定时器，超时后节点将转换为candidate，然后发起投票
    CycleTimerTocken m_electionTimer;
    // 心跳定时器，leader定期发送心跳给follower
    CycleTimerTocken m_heartbeatTimer;
    // 持久化
    Persister::ptr m_persister;
    // 用于以下两个场景：
    // 1.当一个新的日志条目被复制到大多数的节点上时，一个线程可能会等待 m_applyCond，然后将这个日志条目应用到状态机上。
    // 2.当一个新的日志条目被复制到大多数的节点上时，另一个线程可能会通知正在等待 m_applyCond 的线程。
    co::co_condition_variable m_applyCond;
    // 用来已通过raft达成共识的已提交的提议给其他组件的通道
    co::co_chan<ApplyMsg> m_applyChan;

}

}

#endif // RR_RAFT_RAFT_NODE_H