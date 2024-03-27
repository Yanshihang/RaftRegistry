//
// File created on: 2024/03/26
// Author: Zizhou

#include "raft_node.h"
#include <random>
#include <utility>
#include "RaftRegistry/common/config.h"

namespace RR::raft {
static auto Logger = GetLoggerInstance();

static ConfigVar<uint64_t>::ptr g_timer_election_base = Config::LookUp<size_t>("raft.timer.election.base", 1500, "raft election timeout(ms) base");
static ConfigVar<uint64_t>::ptr g_timer_election_top = Config::LookUp<size_t>("raft.timer.election.top", 3000, "raft election timeout(ms) top");
static ConfigVar<uint64_t>::ptr g_timer_heartbeat = Config::LookUp<size_t>("raft.timer.heartbeat", 500, "raft heartbeat timeout(ms)");
    
// 选举超时时间，从base-top的区间中随机选择
static uint64_t s_timer_election_base;
static uint64_t s_timer_election_top;
// 心跳超时时间，心跳超时时间必须小于选举超时时间
static uint64_t s_timer_heartbeat;

struct RaftNodeIniter {
    RaftNodeIniter() {
        s_timer_election_base = g_timer_election_base->getValue();
        g_timer_election_base->addListener([] (const uint64_t& old_value, const uint64_t& new_value) {
            SPDLOG_LOGGER_INFO(Logger, "raft election timeout base changed from {} to {}", old_value, new_value);
            s_timer_election_base = g_timer_election_base;
        })

        s_timer_election_top = g_timer_election_top->getValue();
        g_timer_election_top->addListener([] (const uint64_t& old_value, const uint64_t& new_value) {
            SPDLOG_LOGGER_INFO(Logger, "raft election timeout top changed from {} to {}", old_value, new_value);
            s_timer_election_top = g_timer_election_top;
        })

        s_timer_heartbeat = g_timer_heartbeat->getValue();
        g_timer_heartbeat->addListener([] (const uint64_t& old_value, const uint64_t& new_value) {
            SPDLOG_LOGGER_INFO(Logger, "raft heartbeat timeout changed from {} to {}", old_value, new_value);
            s_timer_heartbeat = g_timer_heartbeat;
        })
    }
};

// 初始化配置
[[maybe unused]] static RaftNodeIniter s_initer();

RaftNode::RaftNode(std::map<int64_t, std::string>& servers, int64_t id, Persister::ptr persister, co::co_chan<ApplyMsg> applyChan) : m_id(id), m_persister(persister), m_applyChan(applyChan),m_logs(persister, 1000) {
    // 设置服务器名称
    rpc::RpcServer::setName("Raft-Node[" + std::to_string(id) + "]");

    // 注册服务（注册方法）RequestVote
    registerMethod(REQUEST_VOTE, [this](RequestVoteArgs args) {
        return handleRequestVote(std::move(args));
    });

    // 注册服务（注册方法）AppendEntries
    registerMethod(APPEND_ENTRIES, [this](AppendEntriesArgs args) {
        return handleAppendEntries(std::move(args));
    });

    // 注册服务（注册方法）InstallSnapshot
    registerMethod(INSTALL_SNAPSHOT, [this](InstallSnapshotArgs args) {
        return handleInstallSnapshot(std::move(args));
    });

    for (auto peer : servers) {
        if (peer.first == id) { // 跳过自己
            continue;
        }
        Address::ptr address = Address::LookUpAny(peer.second); // 这里进行的是DNS解析，即主机名到IP地址的转换
        // 添加节点
        addPeer(peer.first, address);
    }
}

RaftNode::~RaftNode() {
    stop();
}

void RaftNode::start() {
    // 启动时，先从持久化存储中加载硬状态
    auto hs = m_persister->loadHardState();
    if (hs) {
        // 如果有持久化数据，加载数据
        m_currentTerm = hs->term;
        m_votedFor = hs->vote;
        SPDLOG_LOGGER_INFO(Logger, "initialze from state persisted before a crash, term {}, vote {}, commit {} ", m_currentTerm, m_votedFor, hs->commit);
        
    } else {
        // 如果硬状态不存在，成为跟随者，任期为0
        becomeFollower(0);
    }

    // 重新安排选举，通过安排选举去欸的那个谁应该成为领导者
    rescheduleElection();
    // 启动一个新的协程，执行日志应用函数
    go[this] {
        applier();
    }

    // 启动 RPC 服务器
    rpc::RpcServer::start();
}

void RaftNode::stop() {
    std::unique_lock<Mutextype> loack(m_mutex);
    if (isStop()) { // 如果已经stop，直接返回
        return ;
    }

    // 关闭应用通道，这可能会导致等待在这个通道上的线程被唤醒
    m_applyChan.close();
    // 停止心跳定时器，这将阻止节点发送心跳消息
    m_heartbeatTimer.stop();
    // 停止选举定时器，这将阻止节点启动新的选举
    m_electionTimer.stop();
}

void RaftNode::startElection() {
    // 创建一个投票请求
    RequestVoteArgs request{};
    // 设置请求的参数
    request.term = m_currentTerm;
    request.candidateId = m_id;
    request.lastLogIndex = m_logs.lastIndex();
    request.lastLogTerm = m_logs.lastTerm();

    SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] starts election with RequestVoteArgs {}",m_id , request.toString());
    
    // 投票给自己
    m_votedFor = m_id;
    // 持久化当前状态
    persist();

    // 创建一个共享指针，用于统计获得的投票数，初始值为1（自己的一票）
    std::shared_ptr<int64_t> grantedVotes = std::make_shared<int64_t>(1);
    
    // 遍历所有的节点
    for (auto& peer : m_peers) {
        // 使用协程发起异步投票，不阻塞选举定时器，才能在选举超时后发起新的选举
        go [grantedVotes, request, peer, this] {
            // 向peer发送投票请求，并获取回复
            auto reply = peer.second->requestVote(request);
            if (!reply) {
                // 如果没有回复，直接返回
                return;
            }

            std::unique_lock<Mutextype> lock(m_mutex);
            SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] receives RequestVoteReply {} from Node[{}] after sending RequestVoteArgs {} in term {}",m_id, reply->toString(), peer.first, request.toString(), m_currentTerm);
            // 检查自己的状态是否改变
            if (m_currentTerm == request.term && m_state == RaftState::Candidate) { // 如果未改变
                if (reply->voteGranted) { // 如果对等节点同意投票
                    ++(*grantedVotes); // 投票数加1
                    // 如果获得的投票数超过了所有节点数的一半，成为领导者
                    if (*grantedVotes > static_cast<int64_t>(m_peers.size() + 1) / 2) {
                        SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] receives majority votes in term {}", m_id, m_currentTerm);
                        becomeLeader();
                    }
                } else if (reply->term > m_currentTerm) {
                    // 如果对等节点的任期大于当前节点的任期，成为跟随者，并重新安排选举
                    SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] finds a new leader Node[{}] with term {} and steps down in term {}", m_id, peer.first, reply->term, m_currentTerm);
                    becomeFollower(reply->term, reply->leaderId);
                    rescheduleElection();
                }
            }
        };
    }
}


void RaftNode::applier() {
    // 循环，直到节点停止
    while (!isStop()) {
        // 创建一个独占锁，保证在同一时间只有一个线程可以执行以下的代码
        std::unique_lock<MutexType> lock(m_mutex);
        // 如果没有需要 apply 的日志则等待
        while (!m_logs.hasNextEntries()) {
            m_applyChan.wait(lock);
        }

        // 获取已提交的最后一条日志的索引
        auto lastCommit = m_logs.committed();
        // 获取下一批需要 apply 的日志条目
        auto entries = m_logs.nextEntries();
        // 创建一个向量，用于存储需要 apply 的消息
        std::vector<ApplyMsg> msg;
        // 遍历日志条目，将它们转换为消息，并添加到向量中
        for (auto& entry : entries) {
            msg.emplace_back(entry);
        }

        lock.unlock();

        // 遍历消息，将它们发送到 apply 频道
        for (auto& m : msg) {
            m_applyChan << m;
        }

        lock.lock();
        SPDLOG_LOGGER_DEBUG(Logger, "Node [{}] applies entries {} - {} in term {}", m_id, m_logs.applied(), lastCommit, m_currentTerm);
        // 更新已 apply 的日志的索引，防止与 installSnapshot 并发导致 lastApplied 回退
        m_logs.appliedTo(std::max(m_logs.applied(), lastCommit));
    }
}

void RaftNode::broadcastHeartbeat() {
    for (auto& peer : m_peers) {
        go [id = peer.first, this] {
            replicateOneRound(id);
        }
    }
}

void RaftNode::replicateOneRound(int64_t peerId) {
    // 发送 rpc的时候一定不能持锁，否则很可能产生死锁。
    std::unique_lock<Mutextype> lock(m_mutex);

    // 如果当前节点不是领导者，直接返回
    if (m_state != RaftState::Leader) {
        return;
    }

    // 获取要发送给peer节点的日志条目的索引
    int64_t prevIndex = m_nextIndex[peerId] - 1;
    // 如果对方节点的日志落后太多，直接发送快照进行同步
    if (prevIndex < m_logs.lastSnapshotIndex()) { // 对方日志落后于leader最新的快照就是落后太多
        // 创建 InstallSnapshot 请求
        InstallSnapshotArgs request{};
        // 加载快照
        Snapshot::ptr snapshot = m_persister->loadSnapshot();
        // 如果快照为空，打印错误信息并返回
        if (!snapshot || snapshot->empty()) {
            SPDLOG_LOGGER_ERROR(Logger, "need non-empty snapshot");
            return;
        }

        SPDLOG_LOGGER_TRACE(Logger, "Node[{}] [firstIndex: {}, commit: {}] send snapshot[index: {}, term: {}] to Node[{}]",
                            m_id, m_logs.firstIndex(), m_logs.committed(), snapshot->metadata.index, snapshot->metadata.term, peer);
        
        // 设置请求参数
        request.snapshot = std::move(*snapshot);
        request.term = m_currentTerm;
        request.leaderId = m_id;

        // 解锁，发送 RPC 请求
        lock.unlock();

        // 发送 InstallSnapshot 请求，并获取响应
        auto reply = m_peers[peerId]->installSnapshot(request);
        if (!reply) {
            return;
        }
        lock.lock();

        SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] receives InstallSnapshotReply {} from Node[{}] after sending InstallSnapshotArgs {} in term {}", m_id, reply->toString(), peerId, request.toString(), m_currentTerm);
        
        // 检测自己的状态是否改变
        if (m_currentTerm != request.term || m_state != RaftState::Leader) {
            // 如果当前节点的任期或状态已经改变，直接返回
            return;
        }
        
        // 如果收到的任期大于当前节点的任期，将当前节点转变为跟随者
        if (reply->term > m_currentTerm) {
            becomeFollower(reply->term, reply->leaderId);
            return;
        }

        // 更新 matchIndex 和 nextIndex
        if (request.snapshot.metadata.index >m_matchIndex[peerId]) { // 对于<=的情况，后面leader发送日志让follower复制的时候会更新
            m_matchIndex[peerId] = request.snapshot.metadata.index;
        }
        if (request.snapshot.metadata.index > m_nextIndex[peerId]) {
            m_nextIndex[peerId] = request.snapshot.metadata.index + 1;
        }
    } else {
        // 如果对方节点的日志没有落后太多，发送 AppendEntries 请求进行日志复制
        auto entries = m_logs.entries(m_nextIndex[peer]);
        // 创建 AppendEntries 请求
        AppendEntriesArgs request{};
        request.term = m_currentTerm;
        request.leaderId = m_id;
        request.prevLogIndex = prevIndex;
        request.prevLogTerm = m_logs.term(prevIndex);
        request.leaderCommit = m_logs.committed();
        request.entries = entries;

        lock.unlock();

        // 发送 AppendEntries 请求，并获取响应
        auto reply = m_peers[peerId]->appendEntries(request);
        if (!reply) {
            return;
        }

        lock.lock();

        SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] receives AppendEntriesReply {} from Node[{}] after sending AppendEntriesArgs {} in term {}", m_id, reply->toString(), peerId, request.toString(), m_currentTerm);
        
        // 如果当前节点不再是领导者，直接返回
        if (m_state != RaftState::Leader) {
            return;
        }

        if (reply->term > m_currentTerm) {
            becomeFollower(reply->term, reply->leaderId);
            return;
        }

        // 如果收到的任期小于当前节点的任期，忽略这个过期的响应
        if (reply->ter < m_currentTerm) {
            return ;
        }
        
        // 如果日志追加失败，根据 nextIndex 更新 m_nextIndex 和 m_matchIndex
        if(!reply->success) {
            if (reply->nextIndex) {
                m_nextIndex[peerId] = reply->nextIndex;
                m_matchIndex[peerId] = std::min(m_matchIndex[peerId], reply->nextIndex - 1);
            }
            return;
        }

        // 如果日志追加成功，更新 m_nextIndex 和 m_matchIndex
        if (reply-> nextIndex > m_nextIndex[peerId]) {
            m_nextIndex[peerId] = reply->nextIndex;
            m_matchIndex[peerId] = reply->nextIndex - 1;
        }

        // 最后一条已经复制到 peer 节点的日志条目的索引
        int64_t lastIndex = m_matchIndex[peerId];

        // 计算副本数目大于节点数量的一半才提交一个当前任期内的日志
        int64_t vote = 1;
        for (auto match : m_matchIndex) {
            // 如果有节点的最新复制的日志条目索引大于等于 lastIndex，证明这个节点也复制了lastIndex，所以复制lastIndex的副本数加1
            if (match.second >= lastIndex) {
                ++vote;
            }
            // 如果满足条件，提交日志
            if (vote > static_cast<int64_t>(m_peers.size() + 1) / 2) {
                // 只有领导人当前任期里的日志条目可以被提交
                if (m_logs.maybeCommit(lastIndex, m_currentTerm)) {
                    m_applyCond.notify_one();
                }
            }
        }
    }
}

RequestVoteReply RaftNode::handleRequestVote(RequestVoteArgs request) {
    std::unique_lock<Mutextype> lock(m_mutex);

    RequestVoteReply reply();
    
    co_defer_scope {
        persist();
        // 投票后节点的状态
        SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] before processing RequestVoteArgs {} and reply RequestVoteReply {}, state is {}", m_id, request.toString() reply().toString(), toString());
        
    };

    // 拒绝给任期小于自己的候选人投票
    if (request.term < m_currentTerm || (request.term == m_currentTerm && m_votedFor != -1 && m_votedFor != request.candidateId)) {
        reply().term = m_currentTerm;
        reply().leaderId = m_leaderId;
        reply().voteGranted = false;
        return reply;
    }

    // 任期比自己大，变成追随者
    if (request.term > m_currentTerm) {
        becomeFollower(request.term);
    }

    // 拒绝掉那些日志没有自己新的投票请求
    if (!m_logs.isUpToDate(request.lastLogIndex, request.lastLogTerm)) {
        reply.term = m_currentTerm;
        reply.leaderId = m_leaderId;
        reply.voteGranted = false;
        return reply;
    }

    // 投票给候选人
    m_votedFor = request.candidateId;
    
    // 重点，成功投票后才重置选举定时器，这样有助于网络不稳定条件下选主的 liveness 问题
    // 例如：极端情况下，Candidate只能发送消息而收不到Follower的消息，Candidate会不断发起新
    // 一轮的选举，如果节点在becomeFollower后马上重置选举定时器，会导致节点无法发起选举，该集群
    // 无法选出新的 Leader。
    // 只是重置定时器的时机不一样，就导致了集群无法容忍仅仅 1 个节点故障 
    rescheduleElection();

    reply().term = m_currentTerm;
    reply().leaderId = m_leaderId;
    reply.voteGranted = true;

    return reply;
}

/**
 * @brief 处理远端 raft 节点的日志追加请求
 */
AppendEntriesReply RaftNode::handleAppendEntries(AppendEntriesArgs request);

/**
 * @brief 处理远端 raft 节点的快照安装请求
 */
InstallSnapshotReply RaftNode::handleInstallSnapshot(InstallSnapshotArgs request);

/**
 * @brief 增加raft节点
 * 
 * @param id 增加的节点id
 * @param address 增加的节点地址
 */
void RaftNode::addPeer(int64_t id, Address::ptr address) {

}

/**
 * @brief 返回当前节点是否是leader
 * 
 * @return true 当前节点是leader
 * @return false 当前节点不是leader
 */
bool RaftNode::isLeader();

/**
 * @brief 获取当前节点状态
 * 
 * @return currentTerm 任期，isLeader 是否为 leader
 */
std::pair<int64_t, bool> RaftNode::getState();

/**
 * @brief 获取当前节点的leader id
 * 
 * @return int64_t 
 */
int64_t RaftNode::getLeaderId() const { return m_leaderId;}

/**
 * @brief 发起一条消息
 * @return 如果该节点不是 Leader 返回 std::nullopt
 */
std::optional<Entry> RaftNode::propose(const std::string& data);


/**
 * @brief 获取节点id
 * 
 * @return int64_t 
 */
int64_t RaftNode::getNodeId() const { return m_id; }

/**
 * @brief 持久化，加锁
 * 
 * @param index 将该index之前的日志都删除
 * @param snap 快照数据
 */
void RaftNode::persistStateAndSnapshot(int64_t index, const std::string& snap);

/**
 * @brief 持久化，加锁
 * 
 * @param snap 快照数据
 */
void RaftNode::persistSnapshot(Snapshot::ptr snap);

    /**
 * @brief 以字符串格式输出raft节点状态
 * 
 * @return std::string 
 */
std::string RaftNode::toString();

/**
 * @brief 获取心跳超时时间
 * 
 * @return uint64_t 
 */
static uint64_t RaftNode::GetStableHeartbeatTimeout();

/**
 * @brief 获取随机选举时间
 * 
 * @return uint64_t 
 */
static uint64_t RaftNode::GetRandomizedElectionTimeout();

    /**
 * @brief 由candidate或leader转为follower
 * 
 * @param term 任期
 * @param leaderId 任期领导人id，如果还未选举出来则为-1
 */
void RaftNode::becomeFollower(int64_t term, int64_t leaderId=  -1);

/**
 * @brief 转为candidate
 */
void RaftNode::becomeCandidate();

/**
 * @brief 转为leader
 */
void RaftNode::becomeLeader();

/**
 * @brief 重置选举定时器
 */
void RaftNode::rescheduleElection();

/**
 * @brief 重置心跳定时器
 */
void RaftNode::resetHeartbeatTimer() {

}


    /**
 * @brief 持久化，内部调用，不加锁
 * 
 * @param snap 
 */
void RaftNode::persist(Snapshot::ptr snap = nullptr);

/**
 * @brief 发起一条消息，不加锁
 * 
 * @param data 
 * @return std::optional<Entry> 
 */
std::optional<Entry> RaftNode::Propose(const std::string& data);

}

