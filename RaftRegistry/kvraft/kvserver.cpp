//
// File created on: 2024/03/28
// Author: Zizhou

#include "kvserver.h"
#include <random>
#include <chrono>
#include "RaftRegistry/common/config.h"

namespace RR::kvraft {
using namespace RR;

static auto Logger = GetLoggerInstance();

// 定义静态函数GetRandom，用于生成随机数
static int64_t GetRandom() {
    static std::default_random_engine engine(GetCuurentTimeMs());
    static std::uniform_int_distribution<int64_t> dist(0, INT64_MAX);
    return dist(engine);
}

KVServer::KVServer(std::map<int64_t, std::string>& servers, int64_t id, Persister::ptr persister, int64_t maxRaftState) : m_id(id), m_persister(persister), m_maxRaftState(maxRaftState) {
    Address::ptr addr = Address::LookUpAny(servers[id]);
    m_raft = std::make_unique<RaftNode>(servers, id, persister, m_applyCh);
    // 尝试绑定到地址，如果失败则重试
    while(!m_raft->bind(addr)) {
        SPDLOG_LOGGER_WARN(Logger, "kvserver[{}] bind {} fail", id, addr->toString());
        sleep(3);
    }

    // 注册处理命令的函数，当有命令需要处理时，调用handleCommand函数
    m_raft->registerMethod(COMMAND, [this](CommandRequest  request) {
        return handleCommand(std::move(request));
    });
}

KVServer::~KVServer() {
    stop();
}

void KVServer::start() {
    readSnapshot(m_persister->loadSnapshot()); // 从持久化器加载快照并恢复状态
    go [this] { // 启动一个协程运行applier函数，用于应用Raft日志
        applier();
    };
    m_raft->start(); // 启动Raft节点
}
void KVServer::stop() {
    std::unique_lock<MutexType> lock(m_mutex);
    m_raft->stop();
}

CommandResponse KVServer::handleCommand(CommandRequest request) {
    CommandResponse response; // 创建一个响应对象，用于返回操作结果
    co_defer_scope {
        SPDLOG_LOGGER_DEBUG(Logger, "Node [{}] processes commandrequest {} with commandresponse {}", m_id, request.toString(), response.toString());
    };

    std::unique_lock<MutexType> lock(m_mutex);
    // 如果请求不是GET类型，并且是重复请求，则直接返回之前的响应结果
    if (request.op != GET && isDuplicateRequest(request.clientId, request.commandId)) {
        response = m_lastOperation[request.clientId].second;
        return response;
    }
    lock.unlock(); // 解锁，因为接下来的操作可能会阻塞，不应持有锁

    // 向Raft集群提交命令，等待命令被处理
    auto entry = m_raft->propose(request);
    if (!entry) { // 如果命令未被处理（例如当前节点不是领导者），则返回错误信息
        response.err = WRONG_LEADER;
        response.leaderId = m_raft->getLeaderId();
        return response;
    }

    lock.lock();
    auto chan = m_notifyChans[entry->index]; // 获取与命令对应的通知通道
    lock.unlock();
    // 等待命令的处理结果，如果超时，则返回超时错误
    if (!chan.TimedPop(response, std::chrono::milliseconds(RR::Config::LookUp<uint64_t>("raft.rpc.timeout")->getValue()))) {
        response.err = TIMEOUT;
    }
    lock.lock();

    m_notifyChans.erase(entry->index); // 删除通知通道
    // 如果命令处理成功，根据命令类型执行相应的后续操作
    if (response.err == Error::OK) {
        switch (request.op) {
            case PUT:
                go [key = request.key, this] {
                    // 发布键值设置事件
                    m_raft->publish(TOPIC_KEYEVENT_PUT, key);
                    m_raft->publish(TOPIC_KEYSPACE + key, KEYEVENTS_PUT);
                };
                break;
            case APPEND:
                go [key = request.key, this] {
                    // 发布键值追加事件
                    m_raft->publish(TOPIC_KEYEVENT_APPEND, key);
                    m_raft->publish(TOPIC_KEYSPACE + key, KEYEVENTS_APPEND);
                };
                break;
            case DELETE:
                go [key = request.key, this] {
                    // 发布键值删除事件
                    m_raft->publish(TOPIC_KEYEVENT_DELETE, key);
                    m_raft->publish(TOPIC_KEYSPACE + key, KEYEVENTS_DELETE);
                };
                break;
            default:
                void(0); // 对于其他类型的命令，不做处理
        }
    }
    return response;
}

CommandResponse KVServer::Get(const std::string& key) {
    // 构建GET类型的命令请求，并生成随机的命令ID
    CommandRequest request{.op = GET, .key = key, .commandId = GetRandom()};
    return handleCommand(request);
}

CommandResponse KVServer::Put(const std::string& key, const std::string& value) {
    CommandRequest request{.op = PUT, .key = key, .value = value, .commandId = GetRandom()};
    return handleCommand(request);
}

CommandResponse KVServer::Append(const std::string& key, const std::string& value) {
    CommandRequest request{.op = APPEND, .key = key, .value = value, .commandId = GetRandom()};
    return handleCommand(request);
}

CommandResponse KVServer::Delete(const std::string& key) {
    CommandRequest request{.op = DELETE, .key = key, .commandId = GetRandom()};
    return handleCommand(request);
}

CommandResponse KVServer::Clear() {
    CommandRequest request{.op = CLEAR, .commandId = GetRandom()};
    return handleCommand(request);
}


void KVServer::applier() {
    // 创建一个ApplyMsg对象，用于接收日志消息
    ApplyMsg msg{};
    while(m_applyCh.pop(msg)) { // 循环从通道中取出日志消息并处理
        std::unique_lock<MutexType> lock(m_mutex);
        SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] tries to apply message {}", m_id, msg.toString());
        // 根据消息类型处理消息
        if (msg.type == ApplyMsg::SNAPSHOT) { // 如果是快照消息
            auto snap = std::make_shared<Snapshot>(); // 创建一个快照对象
            snap->metadata.index = msg.index;
            snap->metadata.term = msg.term;
            snap->data = std::move(msg.data);
            m_raft->persistSnapshot(snap); // 持久化快照
            readSnapshot(snap); // 从快照中恢复状态
            m_lastApplied = msg.index; // 更新已应用的最后一个日志索引
            continue;
        } else if (msg.type = ApplyMsg::ENTRY) { // 如果是日志条目消息
            // 如果日志条目的数据为空，可能是领导者选举成功后提交的空日志，直接跳过
            if (msg.data.empty()) {
                continue;
            }

            int64_t msgIndex = msg.index;
            // 如果日志条目的索引小于或等于已应用的最后一条日志的索引，则丢弃该条目
            if (msgIndex <= m_lastApplied) {
                SPDLOG_LOGGER_DEBUG(Logger, "Node[{}] discards outdated message {} because a newer snapshot which lastApplied is {} has been restored", m_id, msg.toString(), m_lastApplied);
                continue;
            }
            // 更新最后应用的日志索引
            m_lastApplied = msgIndex;
            // 将日志条目的数据转换为命令请求
            auto request = msg.as<CommandRequest>();
            // 创建一个响应对象
            CommandResponse response;
            // 如果命令不是GET类型，并且是重复请求，则不应用该命令到状态机
            if (request.op != GET && isDuplicateRequest(request.clientId, request.commandId)) {
                SPDLOG_LOGGER_DEBUG(Logger,  "Node[{}] doesn't apply duplicated message {} to stateMachine because maxAppliedCommandId is {} for client {}", m_id, request.toString(), m_lastOperation[request.clientId].second.toString(), request.clientId);
                response = m_lastOperation[request.clientId].second;
            } else {
                // 将日志条目应用到状态机，并获取响应结果
                response = applyLogToStateMachine(request);
                // 如果命令不是GET类型，则记录该客户端的最后一次操作
                if (request.op != GET) {
                    m_lastOperation[request.clientId] = {request.commandId, response};
                }
            }
            // 获取当前节点的状态（任期和是否为领导者）
            auto [term, isLeader] = m_raft->getState();
            // 如果当前节点是领导者，并且日志条目的任期与当前任期一致，则通过通知通道发送响应结果
            if (isLeader && msg.term == term) {
                m_notifyChans[msgIndex] << response;
            }
            // 如果需要创建快照，则保存快照
            if (needSnapshot()) {
                saveSnapshot(msg.index);
            }
        } else {
            SPDLOG_LOGGER_CRITICAL(Logger, "unexpected applymsg type: {}, index: {}, term: {}, data: {}", static_cast<int>(msg.type), msg.index, msg.term, msg.data);
            exit(EXIT_FAILURE);
        }
    }
}
void KVServer::saveSnapshot(int64_t index) {
    Serializer s;
    s << m_data << m_lastOperation; // 将键值对映射和最后操作映射序列化
    s.reset();
    m_raft->persistStateAndSnapshot(index, s.toString()); // 通过Raft节点持久化状态和快照
}

void KVServer::readSnapshot(Snapshot::ptr snapshot) {
    if (!snapshot) { // 如果快照为空，则直接返回
        return;
    }
    Serializer s(snapshot->data); // 从快照中读取数据
    try {
        m_data.clear(); // 清空当前的键值对映射
        m_lastOperation.clear(); // 清空当前的最后操作映射
        s >> m_data >> m_lastOperation; // 从序列化器中反序列化键值对映射和最后操作映射
    } catch(...) {
        SPDLOG_LOGGER_CRITICAL(Logger, "KVServer[{}] read snapshot failed", m_id); // 如果反序列化失败，则记录严重错误
    }
}

bool KVServer::isDuplicateRequest(int64_t client, int64_t command) {
    auto iter = m_lastOperation.find(client);// 在最后操作映射中查找该客户端的记录
    if (iter == m_lastOperation.end()) { // 如果没有找到，则不是重复请求
        return false;
    }
    return iter->second.first == command; // 如果找到的记录中的命令ID与当前命令ID相同，则是重复请求
}

bool KVServer::needSnapshot() {
    if (m_maxRaftState == -1) { // 如果没有设置快照阈值，则不需要创建快照
        return false;
    }
    return m_persister->getRaftStateSize() >= m_maxRaftState; // 如果Raft状态的大小超过了阈值，则需要创建快照
}

CommandResponse KVServer::applyLogToStateMachine(const CommandRequest& request) { // 将日志应用到状态
    CommandResponse response; // 创建一个响应对象
    KVMap::iterator iter; // 定义一个迭代器，用于在键值对映射中查找键

    // 根据命令的操作类型执行相应的操作
    switch (request.operation) {
        case GET: // 如果是获取操作
            iter = m_data.find(request.key); // 在键值对映射中查找键
            if (iter == m_data.end()) { // 如果没有找到键，则返回错误信息
                response.err = NO_KEY;
            } else {
                response.value = iter->second; // 如果找到键，则返回对应的值
            }
            break;
        case PUT: // 如果是设置操作
            m_data[request.key] = request.value;
            break;
        case APPEND: // 如果是追加操作
            m_data[request.key] += request.value;
            break;
        case DELETE: // 如果是删除操作
            iter = m_data.find(request.key); // 在键值对映射中查找键
            if (iter == m_data.end()) {
                response.err = NO_KEY;
            } else {
                m_data.erase(iter); // 如果找到键，则删除键值对
            }
            break;
        case  CLEAR:
            m_data.clear(); // 如果是清除操作，则清空键值对映射
            break;
        default:
            SPDLOG_LOGGER_CRITICAL(Logger, "unexpected operation {}", static_cast<int>(request.op));
            exit(EXIT_FAILURE);
    }
    return response;
}

}