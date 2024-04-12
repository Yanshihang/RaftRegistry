#include "RaftRegistry/rpc/rpc_client.h"
#include "RaftRegistry/common/config.h"

namespace RR::rpc {

static auto Logger = GetLoggerInstance();

static ConfigVar<size_t>::ptr g_channel_capacity = Config::LookUp<size_t>("rpc.client.channel_capacity", 1024, "rpc client channel capacity");

static uint64_t s_channel_capacity = 1;

struct RpcClientIniter {
    RpcClientIniter() {
        s_channel_capacity = g_channel_capacity->getValue();
        g_channel_capacity->addListener([](const size_t& old_val, const size_t& new_val) {
            SPDLOG_LOGGER_INFO(Logger, "rpc client channel capacity changed from {} to {}", old_val, new_val);
            s_channel_capacity = new_val;
        });
    }
};

static RpcClientIniter s_initer; // 初始化RpcClientIniter对象

RpcClient::RpcClient() : m_chan(s_channel_capacity) {}

RpcClient::~RpcClient() {
    close();
}

void RpcClient::close() {
    std::unique_lock<MutexType> lock(m_mutex);

    if (m_isClose) { // 如果已经关闭，则直接返回
        return;
    }

    m_isHeartClose = true;
    m_isClose = true;

    // 清理所有等待响应的请求
    for (auto& i : m_responseHandle) {
        i.second << nullptr; // 发送空指针到 Channel 中，表示请求已被取消或关闭
    }

    m_responseHandle.clear(); // 清空响应处理器
    m_heartTimer.stop(); // 停止心跳定时器

    if (m_session && m_session->isConnected()) {
        m_session->close(); // 关闭会话
    }

    // 清理订阅相关资源
    {
        std::unique_lock<MutexType> pubsubLock(m_pubsubMutex);
        for (auto& i : m_subs) {
            i.second << false; // 向订阅的 Channel 发送 false，表示订阅已被取消
        }
        m_subs.clear();
    }
    m_chan.close(); // 关闭channel
    lock.unlock();
    m_recvCloseChan >> nullptr; // 从关闭 Channel 中读取值，确保 recv 协程已结束
}

// 建立与 RPC 服务器的连接
bool RpcClient::connect(Address::ptr address) {
    close(); // 关闭现有连接
    std::unique_lock<MutexType> lock(m_mutex);
    Socket::ptr sock = Socket::CreateTCP(address);

    if (!sock) {
        return false;
    }
    if (!sock->connect(address, m_timeout)) { // 连接到服务器，如果失败，清理会话并返回 false
        m_session = nullptr;
        return false;
    }
    // 设置心跳和关闭标志
    m_isHeartClose = false;
    m_isClose = false;
    m_recvCloseChan = co::co_chan<bool>{}; // 初始化关闭channel
    m_session = std::make_shared<RpcSession>(sock); // 创建会话
    m_chan = co::co_chan<Protocol::ptr>(s_channel_capacity); // 初始化消息channel

    // 启动send和recv协程
    go [this] { handleSend(); };
    go [this] { handleRecv(); };

    // 如果启用自动心跳，设置心跳定时器
    if (m_autoHeartbeat) {
        // 设置心跳包发送定时器，每30秒执行一次
        m_heartTimer = CycleTimer(30'000, [this] {
            SPDLOG_LOGGER_DEBUG(Logger, "heart beat");
            if (m_isHeartClose) { // 如果之前的心跳包没有收到响应，则认为服务器已关闭
                SPDLOG_LOGGER_DEBUG(Logger, "server closed");
                if (!m_isClose) { // 如果客户端尚未关闭，则主动关闭客户端
                    close();
                }
                return; // 结束定时器任务
            }

            // 构造一个心跳包协议消息
            Protocol::ptr proto = Protocol::Create(Protocol::MsgType::HEARTBEAT_PACKET, "");
            // 将心跳包协议消息发送到发送 Channel 中
            m_chan << proto;
            // 设置心跳响应检测标志为 true，表示等待心跳响应
            m_isHeartClose = true;
        });
    }
}


void RpcClient::setTimeout(uint64_t timeoutMs) {
    m_timeout = timeoutMs;
}

bool RpcClient::isSubscribe() {
    std::unique_lock<MutexType> lock(m_pubsubMutex);
    return !m_subs.empty();
}

void RpcClient::unsubscribe(const std::string& channel) {
    // 判断是否正在订阅通道
    assert(isSubscribe());
     // 构造取消订阅的请求
    PubsubRequest request{.type = PubsubMsgType::Unsubscribe, .channel = channel};
    Serializer s;
    s << request;
    s.reset();
    // 发送协议请求并接收响应
    auto [response, timeout] = sendProtocol(Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString()));
    if (!response || timeout) {
        SPDLOG_LOGGER_DEBUG(Logger, "unsubscribe {} failed", channel);
        return;
    }
    {
        std::unique_lock<MutexType> lock(m_pubsubMutex);
        // 查找给定频道
        auto iter = m_subs.find(channel);
        if (iter = m_subs.end()) { // 如果没有找到
            SPDLOG_LOGGER_DEBUG(Logger, "unsubscribe channel {} not found", channel);
            return;
        }
        // 向协程通道发送信号，指示取消订阅
        iter->second << true;
        m_subs.erase(iter); // 从订阅列表中移除
    }
}

bool RpcClient::subscribe(PubsubListener::ptr listener, const std::vector<std::string>& channels) {
    // 用于一次性订阅所有频道
    // 1.先将所有频道添加到订阅列表m_subs中
    // 2.然后遍历m_subs，为每个订阅的频道发送订阅请求
    
    assert(isSubscribe()); // 确认当前是否正在订阅通道
    co::co_chan<bool> chan;
    bool success = true;
    size_t size{0};
    m_listener = std::move(listener);
    {
        std::unique_lock<MutexType> lock(m_pubsubMutex);
        for (auto& channel : channels) {
            if (m_subs.contains(channels)) {
                SPDLOG_LOGGER_WARN(Logger, "ignore duplicated subscribe: {}", channel);
            } else {
                // 订阅频道，同时创建一个用于取消订阅的 Channel，容量为1
                m_subs.emplace(channel, co::co_chan<bool>{1});
            }
        }
        size = m_subs.size();
        // 创建用于等待所有订阅请求操作结果的协程通道
        chan = co::co_chan<bool>{size};

        // 遍历订阅列表，为每个订阅的频道发送订阅请求
        for (auto& sub : m_subs) {
            go [chan, sub, this] {
                PubsubRequest request{.type = PubsubMsgType::Subscribe, .channel = sub.first};
                Serializer s;
                s << request;
                s.reset();
                auto [response, timeout] = sendProtocol(Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString()));
                if (!response || timeout) {
                    chan << false;
                    return;
                }
                go [channel = sub.first, this] {
                    m_listener->onSubscribe(channel);
                };

                // 等待取消订阅的信号
                // 也就是说，如果没有取消订阅的信号，那么就会一直等待；这样就会一直保持订阅状态
                bool res;
                sub.second >> res;
                if (res) {
                    m_listener->onUnsubscribe(sub.first);
                }
                chan << res;
            }
        }
    }

    // 根据订阅列表大小，循环接收所有订阅操作的结果
    // 也就是
    for (size_t i = 0;i <size;++i) {
        bool res;
        wait >> res;
        if (!res) {
            success = false;
        }
    }

    {
        std::unique_lock<MutexType> lock(m_pubsubMutex);
        m_subs.clear();
    }

    m_listener = nullptr;
    return success;
}   


bool RpcClient::patternSubscribe(PubsubListener::ptr listener, const std::vector<std::string>& patterns) {
    assert(!isSubscribe());
    
    // 创建一个通道，用于等待所有订阅操作的完成
    co::co_chan<bool> wait;

    bool success = true;
    size_t size{0};

    m_listener = std::move(listener);
    {
        std::unique_lock<MutexType> lock(m_pubsubMutex);
        for (auto& pattern : patterns) {
            if (m_subs.contains(pattern)) {
                SPDLOG_LOGGER_WARN(Logger, "ignore duplicated pattern subscribe: {}", pattern);
                continue;
            } else {
                m_subs.emplace(pattern, co::co_chan<bool>{1});
            }
        }


        size = m_subs.size();
        wait = co::co_chan<bool>{size};

        for (auto& sub : m_subs) {
            go [wait, sub, this] {
                PubsubRequest request{.type = PubsubMsgType::PatternSubscribe, .pattern = sub.first};
                Serializer s;
                s << request;
                s.reset();
                auto [response, timeout] = sendProtocol(Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString()));
                if (!response || timeout) {
                    wait << false;
                    return;
                }
                go [pattern = sub.first, this] {
                    m_listener->onPatternMessage()
                };

                // 等待取消订阅的信号
                // 也就是说，如果没有取消订阅的信号，那么就会一直等待；这样就会一直保持订阅状态
                bool res;
                sub.second >> res;

                if (res) {
                    m_listener->onUnsubscribe(pattern);
                }

                wait << res;
            };
        }
    }
    // 等待所有订阅操作的完成
    for (size_t i = 0; i< size; ++i) {
        bool res;
        wait >> res;
        if (!res) {
            success = false;
        }
    }
    {
        std::unique_lock<MutexType> lock(m_pubsubMutex);
        m_subs.clear();
    
    }
}

void RpcClient::patternUnsubscribe(const std::string& pattern) {
    // 确保客户端已经订阅了频道
    assert(isSubscribe());

    // 创建一个取消订阅的请求
    PubsubRequest request{.type = PubsubMsgType::PatternUnsubscribe, .pattern = pattern};

    Serializer s;
    s << request;
    s.reset();

    // 发送取消订阅的请求，并获取响应和超时状态
    auto [response, timeout] = sendProtocol(Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString()));
    
    if (!response || timeout) {
        SPDLOG_LOGGER_DEBUG(Logger, "patternunsubscribe channel: {} fail", pattern);
        return;
    }

    // 删除订阅列表中的频道
    {
        std::unique_lock<MutexType> lock(m_pubsubMutex);
        auto iter = m_subs.find(pattern);

        if (iter == m_subs.end()) {
            SPDLOG_LOGGER_DEBUG(Logger, "patternsubscribe channel: {} fail, no exist", pattern);
            return;
        }
        iter->second << true;
        m_subs.erase(iter);
    }
}

bool RpcClient::publish(const std::string& channel, const std::string& message) {
    // 确保客户端没有订阅任何频道
    assert(!isSubscribe());

    // 如果客户端已经关闭，返回 false
    if (isClosed()) {
        return false;
    }

    // 创建一个发布消息的请求，包含消息类型（发布），频道名和消息内容
    PubsubRequest request{.type = PubsubMsgType::Publish, .channel = channel, .message = message};
    Serializer s;
    s << request;
    s.reset();

    auto [response, timeout] = sendProtocol(Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString()));
    if (!response || timeout) {
        return false;
    }

    // 如果收到了响应并且没有超时，返回 true
    return true;
}

void RpcClient::handleSend() {
    using namespace std::chrono_literals; // 用于支持字面量表示的时间，如 3s 表示 3 秒
    SPDLOG_LOGGER_TRACE(Logger, "start handleSend");

    Protocol::ptr request; // 用于暂存从 Channel 中接收到的请求
    co::co_chan<Protocol::ptr> chan = m_chan; // 仅仅是复制了对通道的引用
    // 通过 Channel 收集调用请求，如果没有消息时 Channel 内部会挂起该协程等待消息到达
    // 循环从 Channel 中接收请求并发送给服务器，直到 Channel 被关闭
    while(true) {
        bool isSuccess = chan.TimedPop(request, 3s); // 尝试从 Channel 中弹出请求，最多等待 3 秒

        if (chan.closed()) {
            break;
        }

        if (!isSuccess || !request) {
            continue;
        }

        // 发送请求到服务器，如果发送失败，则退出循环
        if (m_session->sendProtocol(request) <= 0) {
            break;
        }
    }
    SPDLOG_LOGGER_TRACE(Logger, " stop handleSend");
}

void RpcClient::handleRecv() { // RPC 接收协程逻辑
    // 循环接收服务器响应，直到会话关闭或接收失败
    while(true) {
        // 从会话中接收响应协议
        Protocol::ptr proto = m_session->recvProtocol();
        
        // 如果接收失败，则认为会话已关闭，记录调试信息，并尝试主动关闭客户端
        if (!proto) {
            SPDLOG_LOGGER_TRACE(Logger, "rpc {} closed", m_session->getSocket()->toString());
            if (!m_isClose) {
                go [this] {
                    close(); // 异步关闭客户端
                }
            }
            m_recvCloseChan << true; // 向关闭 Channel 中发送 true，表示接收协程已结束
            break;
        }
        // 重置心跳响应检测标志，表示心跳响应已收到
        m_isHeartClose = false;
        Protocol::MsgType type = proto->getType()
        // 根据响应协议的类型，进行相应的处理
        switch (type) {
            case Protocol::MsgType::HEARTBEAT_PACKET:
                m_isHeartClose = false;
                break;
            case Protocol::MsgType::RPC_METHOD_RESPONSE:
                recvProtocol(proto);
                break;
            case Protocol::MsgType::RPC_PUBSUB_REQUEST:
                handlePublish(proto);
                break;
            case Protocol::MsgType::RPC_PUBSUB_RESPONSE:
                recvProtocol(proto);
                break;
            default:
                SPDLOG_LOGGER_DEBUG(Logger, "protocol: {}", proto->toString());
                break;
        }
    }
}

void RpcClient::handlePublish(Protocol::ptr proto) {
    // 解析发布/订阅请求结构体
    PubsubRequest res;
    Serializer s(proto->getContent());
    s >> res;
    // 根据消息类型，分发到对应的监听器回调
    switch (res.type) {
        case PubsubMsgType::Message: // 如果是普通消息，调用监听器的 onMessage 方法
            go [this, res = std::move(res)] {
                m_listener->onMessage(res.channel, res.message);
            };
            break;
        case PubsubMsgType::PatternMessage:
            go [res = std::move(res), this] {
                m_listener->onPatternMessage(res.pattern, res.channel, res.message);
                
            };
            break;
        default:
            SPDLOG_LOGGER_DEBUG(Logger, "unexpect pubsubmsgtype: {}", static_cast<int>(res.type));
    }
}

void RpcClient::recvProtocol(Protocol::ptr response) {
    // 从协议中获取序列号
    auto seqId = response->getSequenceId();

    std::unique_lock<MutexType> lock(m_mutex);

    // 查找等待该序列号响应的 Channel
    auto iter = m_responseHandle.find(seqId);
    if (iter == m_responseHandle.end()) {
        SPDLOG_LOGGER_DEBUG(Logger, "seqId: {} not found", seqId);
        return;
    }
    // 获取对应的 Channel
    auto chan= iter->second;
    // 向 Channel 发送响应消息，唤醒等待该响应的调用者
    chan << response;
}

std::pair<Protocol::ptr, bool> RpcClient::sendProtocol(Protocol::ptr request) {
    assert(request); // 确保请求协议非空
    // 开启一个 Channel 接收调用结果
    co_chan<Protocol::ptr> recvChan;
    // 本次调用的序列号
    uint32_t seqId = 0;
    std::map<uint32_t, co::co_chan<Protocol::ptr>>::iterator iter;
    {
        std::unique_lock<MutexType> lock(m_mutex);
        ++m_sequenceId;
        seqId = m_sequenceId;
        // 将请求序列号与接收 Channel 关联
        iter = m_responseHandle.emplace(m_sequenceId, recvChan).first;
    }

    // 创建请求协议，附带上请求 id
    request->setSequenceId(seqId);
    // 向 send 协程的 Channel 发送消息
    m_chan << request;

    bool timeout = false;
    Protocol::ptr response;
    // 根据是否设置了超时时间，采用不同方式等待响应
    if (m_timeout == static_cast<uint64_t>(-1)) {
        // 如果没有设置超时时间，使用阻塞方式等待响应
        recvChan >> response;
    } else {
        // 如果设置了超时时间，使用带超时的方式等待响应
        // 如果超时时间内取到了响应，则 timeout 为 false，否则为 true
        timeout = !recvChan.TimedPop(response, std::chrono::milliseconds(m_timeout));
    }
    // 当 RPC 客户端没有关闭时，从映射中移除对应的序列号和 Channel
    if (!m_isClose) {
        // 因为无论是否取到，都不在需要该 Channel，所以直接移除
        m_responseHandle.erase(iter);
    }

    // 返回响应协议指针和是否超时的信息
    return {response, timeout};
}


} // namespace RR::rpc