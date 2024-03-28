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
    // 一轮的选举，如果节点在becomeFollower后马上重置选举定时器，会导致节点无法发起选举。
    // 该集群无法选出新的 Leader。
    // （这里的意思是：如果candidate不断发起新的选举，那么follower会不断重置选举定时器，由于candidate收不到follower的消息，所以不会收到选举成功的消息，不会成为leader，而由于follower投票前就重置选举定时器，导致follower无法成为candidate，这样的话所有节点都卡住了，无法选出leader）
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
AppendEntriesReply RaftNode::handleAppendEntries(AppendEntriesArgs request) {
    std::unique_lock<Mutextype> lock(m_mutex);
    AppendEntriesReply reply{};
    co_defer_scope {
        if (reply.success && m_logs.committed() < request.leaderCommit) {
           // 更新提交索引，为什么取了个最小？committed是leader发来的，是全局状态，但是当前节点
           // 可能落后于全局状态，所以取了最小值。last_index 是这个节点最新的索引， 不是最大的可靠索引，
           // 如果此时节点异常了，会不会出现commit index以前的日志已经被apply，但是有些日志还没有被持久化？
           // 这里需要解释一下，raft更新了commit index，raft会把commit index以前的日志交给使用者apply同时
           // 会把不可靠日志也交给使用者持久化，所以这要求必须先持久化日志再apply日志，否则就会出现刚刚提到的问题。
            m_logs.commitTo(std::min(request.leaderCommit, ,m_logs.lastIndex()));
            m_applyCond.notify_one();
        }
        persist();
        SPDLOG_LOGGER_TRACE(Logger, "Node[{}] before processing AppendEntriesArgs {} and reply AppendEntriesResponse {}, state is {}", m_id, request.toString(), reply.toString(), toString());
    };
    // 拒绝任期小于自己的 leader 的日志复制请求
    if (request.term < m_currentTerm) {
        reply.term = m_currentTerm;
        reply.leaderId = m_leaderId;
        reply.success = false;
        return reply;
    }

    // 对方任期大于自己或者自己为同一任期内败选的候选人则转变为 follower
    // 已经是该任期内的follower就不用变
    if (request.term > m_currentTerm || (request.term == m_currentTerm && m_state == RaftState::Candidate)) {
        becomeFollower(request.term, request.leaderId);
    }

    if (m_leaderId < 0) {
        m_leaderId = request.leaderId;
    }

    // 自己为同一任期内的follower，更新选举定时器就行
    rescheduleElection();
    
    // 拒绝错误的日志追加请求
    // 如果对方的prevLogIndex小于快照的最后一个索引，说明对方的日志已经过时了
    if (request.prevLogIndex < m_logs.lastSnapshotIndex()) {
        reply.term = 0; // 表示相应无效
        reply.leaderId = -1;
        reply.success = false;
        SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] receives unexpected AppendEntriesArgs {} from Node[{}] because prevLogIndex {} < firstLogIndex {}", m_id, request.toString(), request.leaderId, request.prevLogIndex, m_logs.lastSnapshotIndex());
        return reply;
    }

    // 获取append后的最后一个日志的索引
    int64_t lastIndex = m_logs.maybeAppend(request.prevLogIndex, request.prevLogTerm, request.leaderCommit, request.entries);
    
    if (lastIndex < 0) {
        // 尝试查找冲突的日志
        int64_t conflict = m_logs.findConflict(request.prevLogIndex, request.prevLogTerm);
        reply.term = m_currentTerm;
        reply.leaderId = m_leaderId;
        reply.success = false;
        reply.nextIndex = conflict;
        return reply;
    }

    reply.term = m_currentTerm;
    reply.leaderId = m_leaderId;
    reply.success = true;
    reply.nextIndex = lastIndex + 1;
    return reply;
}

/**
 * @brief 处理远端 raft 节点的快照安装请求
 */
InstallSnapshotReply RaftNode::handleInstallSnapshot(InstallSnapshotArgs request) {
    std::unique_lock<Mutextype> lock(m_mutex);
    InstallSnapshotReply reply{};
    // 在当前协程结束时，执行以下的代码块，打印一些调试信息
    co_defer_scope {
        SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] before processing InstallSnapshotArgs {} and reply InstallSnapshotReply {}, state is {}", m_id, request.toString(), reply.toString(), toString());
    };

    // 设置回复的任期和领导者ID为当前节点的任期和领导者ID
    reply.term = m_currentTerm;
    reply.leaderId = m_leaderId;

    // 如果请求的任期小于当前节点的任期，直接返回回复
    if (request.term < m_currentTerm) {
        return reply;
    }

    // 如果请求的任期小于当前节点的任期，直接返回回复
    if (request.term > m_currentTerm) {
        becomeFollower(request.term, request.leaderId);
    }

    rescheduleElection();
    // 更新回复的领导者ID为当前节点的领导者ID
    reply.leaderId = m_leaderId;

    const int64_t snapIndex = request.snapshot.metadata.index;
    const int64_t snapTerm = request.snapshot.metadata.term;

    // 如果快照的索引小于或等于已提交的日志的索引，忽略这个快照
    if (snapIndex <= m_logs.committed()) {
        SPDLOG_LOGGER_DEBUG(Logger,"Node[{}] ignored snapshot [index: {}, term: {}]", m_id, snapIndex, snapTerm);
        return reply;
    }

    // 如果快照的索引和任期与日志中的匹配，快速前进到快照的索引
    // 这里的逻辑是：检查节点的日志中是否有与快照相匹配的条目。如果有，那么这个节点的日志是最新的，只是它还没有提交（commit）到快照的索引位置。
    if (m_logs.matchLog(snapIndex, snapTerm)) {
        SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] fast-forwarded to snapshot [index: {}, term: {}]", m_id, snapIndex, snapTerm);
        m_logs.commitTo(snapIndex);
        m_applyChan.notify_one();
        return reply;
    }
    
    // 开始恢复快照

    SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] starts to install snapshot [index: {}, term: {}]", m_id, snapIndex, snapTerm);
    // 如果快照的索引大于日志的最后一个索引，清除所有的日志条目
    // 如果快照中保存的最后一条日志的索引大于当前节点的日志的最后一个索引，这意味着当前节点的日志严重落后，甚至可能丢失了一些日志条目。
    // 在这种情况下，最简单和最有效的方式是清除当前节点的所有日志条目，然后使用快照来更新节点的状态。
    if (request.snapshot.metadata.index > m_logs.lastIndex()) {
        m_logs.clearEntries(request.snapshot.metadata.index, request.snapshot.metadata.term);
    } else { // 否则，压缩日志到快照的索引
        m_logs.compact(request.snapshot.metadata.index);
    }
    // （上面这段判断逻辑的行为是：如果节点中的日志落后于快照，那么就清空日志，然后使用快照中的数据来更新节点的状态；如果节点中的日志不落后（持平或领先）快照，那么就删除快照之前的日志，然后使用快照中的数据来更新节点的状态。）
    // 创建一个新的协程，将快照发送到应用通道
    go [snap = request.snapshot, this] {
        m_applychan << ApplyMsg{snap};
    };
    
    persist(std::make_shared<Snapshot>(std::move(request.snapshot)));
    return reply;
}

void RaftNode::addPeer(int64_t id, Address::ptr address) {
    // 创建一个新的 RaftPeer 对象，使用给定的 id 和 address
    RaftPeer::ptr peer = std::make_shared<RaftPeer>(id, address);
    m_peers[id] = peer;
    m_nextIndex[id] = 0;
    m_matchIndex[id] = 0;
    SPDLOG_LOGGER_DEBUG(Logger, "Node [{}] add peer [{}], address is {}", m_id, id, address->toString());
}

bool RaftNode::isLeader() {
    std::unique_lock<Mutextype> lock(m_mutex);
    return m_state == RaftState::Leader;
}

std::pair<int64_t, bool> RaftNode::getState() {
    std::unique_lock<Mutextype> lock(m_mutex);
    return {m_currentTerm, m_state == RaftState::Leader};
}

std::string RaftNode::toString() {
    // 用于将节点状态映射为字符串
    std::map<RaftState, std::string> mp{{Follower, "Follower"}, {Candidate, "Candidate"}, {Leader, "Leader"}};
    std::string str = fmt.format("Id: {}, State: {}, LeaderId: {}, CurrentTerm: {}, VotedFor: {}, CommitIndex: {}, LastApplied: {}", m_id, mp[m_state], m_leaderId, m_currentTerm, m_votedFor, m_logs.committed(), m_logs.applied());
    return "{" + str + "}";
}

void RaftNode::becomeFollower(int64_t term, int64_t leaderId) {
    // 如果当前节点是领导者，则先停止心跳定时器
    if (m_heartbeatTimer && m_state == Leader) {
        m_heartbeatTimer.stop();
    }

    m_state = Follower;
    m_currentTerm = term;
    m_votedFor = -1;
    m_leaderId = leaderId;
    // 持久化当前节点的状态
    persist();
    SPDLOG_LOGGER_DEBUG(Logger, "Node [{}] become follower at term {}, state is {}", m_id, m_currentTerm, toString());
}
void RaftNode::becomeCandidate() {
    m_state = Candidate;
    ++m_currentTerm;
    m_votedFor = m_id;
    m_leaderId = -1;
    persist();
    SPDLOG_LOGGER_DEBUG(Logger, "Node [{}] become candidate at term {}, state is{}", m_id, m_currentTerm, toString());
}

void RaftNode::becomeLeader() {
    // 成为leader停止选举定时器
    if (m_electionTimer) {
        m_electionTimer.stop();
    }
    m_state = Leader;
    m_leaderId = m_id;
    // 成为领导者后，领导者并不知道其它节点的日志情况，因此与其它节点需要同步那么日志，领导者并不知道集群其他节点状态，
    // 因此他选择了不断尝试。nextIndex、matchIndex 分别用来保存其他节点的下一个待同步日志index、已匹配的日志index。
    // nextIndex初始化值为lastIndex+1，即领导者最后一个日志序号+1，因此其实这个日志序号是不存在的，显然领导者也不
    // 指望一次能够同步成功，而是拿出一个值来试探。
    // matchIndex初始化值为0，这个很好理解，因为他还未与任何节点同步成功过，所以直接为0
    for (auto& peer : m_peers) {
        m_nextIndex[peer.first] = m_logs.lastIndex() + 1;
        m_matchIndex[peer.first] = 0;
    }

    persist();
    SPDLOG_LOGGER_DEBUG(Logger, "Node [{}] become leader at term {}, state is {}", m_id, m_currentTerm, toString());
}

void RaftNode::rescheduleElection() {
    // 停止当前的选举定时器
    m_electionTimer.stop();
    // 重新设置选举定时器，定时器的超时时间是一个随机的选举超时时间
    m_electionTimer = CycleTimer(GetRandomizedElectionTimeout(), [this] {
        std::unique_lock<Mutextype> lock(m_mutex);
        // 如果当前节点的状态不是领导者，那么将其状态变为候选者，并开始新的选举
        // 如果当前节点是领导者，则无需进行选举
        if (m_state != RaftState::Leader) {
            // 如果当前节点的状态不是领导者，那么将其状态变为候选者，并开始新的选举
            becomeCandidate();
            // 开始新的选举，这是一个异步操作，不会阻塞选举定时器
            startElection();
        }
    });
}

void RaftNode::resetHeartbeatTimer() {
    m_heartbeatTimer.stop();
    m_heartbeatTimer = CycleTimer(GetRandomizedElectionTimeout(), [this] {
        std::unique_lock<Mutextype> lock(m_mutex);
        if (m_state == RaftState::Leader) {
            broadcastHeartbeat();
        }
    });
}

static uint64_t RaftNode::GetStableHeartbeatTimeout() {
    return s_timer_heartbeat;
}

static uint64_t RaftNode::GetRandomizedElectionTimeout() {
    static std::default_random_engine engine(RR::GetCuurentTimeMs());
    static std::uniform_int_distribution<int64_t> dist(s_timer_election_base, s_timer_election_top);
    return dist(engine);
}

void RaftNode::persist(Snapshot::ptr snap) {
    HardState hs{};
    hs.vote = m_votedFor;
    hs.term = m_currentTerm;
    hs.commit = m_logs.committed();
    m_persister->persist(hs, m_logs.allEntries(), snap);
}

void RaftNode::persistStateAndSnapshot(int64_t index, const std::string& snap) {
    std::unique_lock<Mutextype> lock(m_mutex);

    // 创建一个快照，快照的创建基于给定的索引和快照数据
    auto snapshot = m_logs.createSnapshot(index, snap);

    if (snapshot) {
        m_logs.compact(snapshot->metadata.index);
        SPDLOG_LOGGER_DEBUG(Logger, "starts to restore snapshot [index: {}, term:{}]", snapshot->metadata.index, snapshot->metadata.term);
        persist(snapshot);
    }
}

void RaftNode::persistSnapshot(Snapshot::ptr snap) {
    std::unique_lock<Mutextype> lock(m_mutex);
    if (snap) {
        m_logs.compact(snap->metadata.index);
        SPDLOG_LOGGER_DEBUG(Logger, "starts to restore snapshot [index: {}, term:{}]", snap->metadata.index, snap->metadata.term);
        persist(snap);
    }
}

std::optional<Entry> RaftNode::propose(const std::string& data) {
    std::unique_lock<Mutextype> lock(m_mutex);
    return Propose(data);
}

std::optional<Entry> RaftNode::Propose(const std::string& data) {
    if (m_state != Leader) {
        SPDLOG_LOGGER_DEBUG(Logger, "Node [{}] no leader at term {}, dropping proposal", m_id, m_currentTerm);
        return std::nullopt;
    }

    Entry entry;
    entry.term = m_currentTerm;
    // 设置日志条目的索引为日志的最后一个索引加一
    entry.index = m_logs.lastIndex() +1;
    entry.data = data;
    
    // 将新的日志条目添加到日志中
    m_logs.append(entry);
    // 广播心跳消息，通知其他节点有新的日志条目
    broadcastHeartbeat();
    // 打印一条调试信息，包含新日志条目的索引和任期
    SPDLOG_LOGGER_DEBUG(Logger, "Node [{}] receives a new log entry [index: {}, term: {}]", m_id, entry.index, entry.term);
    return entry;
}

}

