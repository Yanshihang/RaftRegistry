//
// File created on: 2024/03/23
// Author: Zizhou

#include "RaftRegistry/rpc/rpc_server.h"
#include "RaftRegistry/common/util.h"
#include "RaftRegistry/common/config.h"
#include "RaftRegistry/rpc/pubsub.h"
#include <chrono>
#include <fnmatch.h>

namespace RR::rpc {

static auto Logger = GetLoggerInstance();

// 心跳超时配置
static RR::ConfigVar<uint64_t>::ptr g_heartbeat_timeout = RR::Config::LookUp<uint64_t>("rpc.server.heartbeat_timeout", 40'000, "rpc server heartbeat timeout (ms)");

// 单个客户端的最大并发配置
static RR::ConfigVar<uint32_t>::ptr g_concurrent_number = RR::Config::LookUp<uint32_t>("rpc.server.concurrent_number", 500, "rpc server concurrent number");

static uint64_t s_heartbeat_timeout = 0; // 心跳超时静态变量
static uint32_t s_concurrent_number = 0; // 最大并发静态变量

struct RpcServerIniter {
    RpcServerIniter() {
        s_heartbeat_timeout = g_heartbeat_timeout->getValue(); // 获取心跳超时配置值
        // 添加心跳超时配置监听器
        g_heartbeat_timeout->addListener([](const uint64_t& old_val, const uint64_t& new_val) {
            SPDLOG_LOGGER_INFO(Logger, "rpc server heartbeat timeout changed from {} to {}", old_val, new_val);
            s_heartbeat_timeout = new_val; // 更新心跳超时配置值
        });
        s_concurrent_number = g_concurrent_number->getValue(); // 获取最大并发配置值;
        g_concurrent_number->addListener([](const uint32_t& old_val, const uint32_t& new_val) {
            SPDLOG_LOGGER_INFO(Logger, "rpc server concurrent number changed from {} to {}", old_val, new_val);
            s_concurrent_number = new_val; // 更新最大并发配置值
        });
    }
};

static RpcServerIniter s_initer; // 静态初始化RpcServerIniter对象

RpcServer::RpcServer() : TcpServer(), m_aliveTime(s_heartbeat_timeout) {}

RpcServer::~RpcServer() {
    stop(); // 停止服务器
}

bool RpcServer::bind(Address::ptr address) {
    m_port = std::dynamic_pointer_cast<Ipv4Address>(address)->getPort(); // 获取并设置绑定的端口号
    return TcpServer::bind(address); // 调用父类的bind方法
}

bool RpcServer::bindRegistry(Address::ptr address) { // 绑定注册中心地址方法
    Socket::ptr sock = Socket::CreateTCP(address); // 创建一个TCP套接字

    if (!sock) {
        return false;
    }
    // 连接注册中心地址
    if (!sock->connect(address)) {
        SPDLOG_LOGGER_WARN(Logger, "connect registry server {} failed", address->toString());
        m_registry = nullptr;
        return false;
    }

    m_registry = std::make_shared<RpcSession>(sock); // 创建一个RPC会话对象

    Serializer s; // 创建一个序列化器对象
    s << m_port; // 序列化端口号
    s.reset();

    // 向服务中心声明为provider，注册服务端口
    Protocol::ptr proto = Protocol::Create(Protocol::MsgType::RPC_PROVIDER, s.toString()); // 创建协议消息
    m_registry->sendProtocol(proto); // 发送协议消息
    return true;
}

void RpcServer::start() {
    if (!isStop()) {
        return;
    }

    if (m_registry) {
        for (auto & item : m_handlers) {
            registerService(item.first); // 向注册中心注册服务
        }
        m_registry->getSocket()->setRecvTimeout(30'000); // 设置注册中心socket的接收超时时间

        // 设置心跳包定时器，每30秒向注册中心发送一次心跳包
        m_heartbeatTimer = CycleTimer(30'000, [this] {
            SPDLOG_LOGGER_DEBUG(Logger, "heartbeat");
            // 创建心跳包协议消息
            Protocol::ptr proto = Protocol::Create(Protocol::MsgType::HEARTBEAT_PACKET,"");
            m_registry->sendProtocol(proto); // 发送心跳包
            // 接收心跳相应
            Protocol::ptr response = m_registry->recvProtocol();
            if (!response) {
                SPDLOG_LOGGER_WARN(Logger, "heartbeat response timeout");
                // 停止心跳定时器，服务器将独自提供服务
                m_heartbeatTimer.stop();
                return;
            }
        });
        
    }

    TcpServer::start();
}

void RpcServer::stop() {
    if (isStop()) {
        return;
    }
    m_heartbeatTimer.stop(); // 停止心跳定时器
    TcpServer::stop(); // 停止服务器
}

void RpcServer::setName(const std::string& name) {
    TcpServer::setName(name); // 调用父类的setName方法
}

void RpcServer::publish(const std::string& channel, const std::string& message) {
    std::unique_lock<MutexType> lock(m_pubsubMutex); // 创建一个互斥锁

    // 找出订阅该频道的客户端
    auto iterClientList = m_pubsubChannels.find(channel);
    if (iterClientList!= m_pubsubChannels.end()) {
        auto& clientsList = iterClientList->second;
        if (clientsList.empty()) { // 如果该频道没有订阅者，则删除该频道
            m_pubsubChannels.erase(iterClientList);
        } else {
            // 把要发送的消息封装成一个PubsubRequest对象
            PubsubRequest request{.type = PubsubMsgType::Message, .channel = channel, .message = message};
            // 将request序列化
            Serializer s;
            s << request;
            s.reset();

            // 创建包含消息的协议消息
            Protocol::ptr proto = Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString());
            // 遍历订阅该频道的客户端，向每个客户端发送消息
            auto iterClient = clientsList.begin();
            while(iterClient != clientsList.end()) {
                if (!(*iterClient)->isConnected()) {
                    iterClient = clientsList.erase(iterClient);
                    continue;
                }

                // 创建一个RPC会话对象，然后通过session对象发送协议消息
                RpcSession::ptr session = std::make_shared<RpcSession>((*iterClient), false);
                session->sendProtocol(proto);
                ++iterClient;
            }
        }
    }

    // 遍历模式
    PubsubRequest request{.type = PubsubMsgType::PatternMessage, .channel = channel, .message = message};
    auto iterClientPattern = m_patternChannels.begin();
    while(iterClientPattern != m_patternChannels.end()) {
        auto& pattern = iterClientPattern->first;
        auto& client = iterClientPattern->second;
        if (!client->isConnected()) {
            iterClientPattern = m_patternChannels.erase(iterClientPattern);
            continue;
        }
        if (!fnmatch(pattern.c_str(), channel.c_str(), 0)) { // 判断频道是否匹配模式
            request.pattern = pattern;
            Serializer s;
            s << request;
            s.reset();
            Protocol::ptr proto = Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString());
            RpcSession::ptr session = std::make_shared<RpcSession>(client, false);
            session->sendProtocol(proto);
        }
        ++iterClientPattern;
    }
}


void RpcServer::registerService(const std::string& name) {
    // 携带要注册的函数名的proto
    Protocol::ptr proto = Protocol::Create(Protocol::MsgType::RPC_SERVICE_REGISTER, name);
    // 将带有函数名的proto发送给注册中心
    m_registry->sendProtocol(proto);

    // 接收服务中心给的相应
    Protocol::ptr rp = m_registry->recvProtocol();
    if (!rp) {
        SPDLOG_LOGGER_WARN(Logger, "registerService: {} fail, registry socket: {}", name, m_registry->getSocket()->toString());
        return;
    }
    // 封装rpc调用的结果
    Result<std::string> result;
    Serializer s(rp->getContent());
    s >> result;

    if (result.getCode() !=  RPC_SUCCESS) {
        SPDLOG_LOGGER_WARN(Logger, result.toString());

    } else {
        SPDLOG_LOGGER_INFO(Logger, result.toString());
    }
}

Serializer RpcServer::call(const std::string& name, const std::string& arg) {
    Serializer serializer;
    if (m_handlers.find(name) == m_handlers.end()) {
        return serializer;
    }
    auto fun = m_handlers[name];
    fun(serializer,arg);
    serializer.reset();
    return serializer;
}


void RpcServer::handleClient(Socket::ptr client) {
    SPDLOG_LOGGER_DEBUG(Logger, "handleClient: {}", client->toString());
    // 创建一个RPC会话对象
    RpcSession::ptr session = std::make_shared<RpcSession>(client);

    // 定义客户端关闭时的操作
    auto on_close = [client] {
        SPDLOG_LOGGER_DEBUG(Logger, "client: {} closed", client->toString());
        client->close(); // 关闭客户端socket// 关闭客户端socket
    };
    // 开启心跳定时器，设置超时时间为m_aliveTime
    co_timer_id heartTimer = m_timer.ExpireAt(std::chrono::milliseconds(m_aliveTime), on_close); // 创建一个定时器

    // 创建协程通道，用于限制并发处理客户端请求数量
    co::co_chan<bool> wait_queue(s_concurrent_number);
    // 循环处理客户端请求
    while (true) {
        Protocol::ptr request = session->recvProtocol(); // 接收客户端请求
        if (!request) { // 如果接收失败
            client->close(); // 关闭客户端socket
            break; // 退出循环
        }

        // 将true发送到协程通道，如果通道已满，则协程会等待
        wait_queue << true;

        // 更新心跳定时器，防止连接因超时而关闭
        heartTimer.StopTimer(); // 停止当前的计时器
        heartTimer = m_timer.ExpireAt(std::chrono::milliseconds(m_aliveTime), on_close); // 创建新的定时器;
        
        go [request, client, session, wait_queue, this] {
            wait_queue >> nullptr; // 从协程通道中取出一个值
            Protocol::ptr response;
            Protocol::MsgType type = request->getType(); // 获取请求类型
            switch (type) {
                case Protocol::MsgType::RPC_METHOD_REQUEST: // 如果是方法调用请求
                    response = handleMethodCall(request); // 处理方法调用
                    break;
                case Protocol::MsgType::HEARTBEAT_PACKET: // 如果是心跳包
                    response = handleHeartbeatPacket(request); // 处理心跳包
                    break;
                case Protocol::MsgType::RPC_PUBSUB_REQUEST: // 如果是发布订阅请求
                    response = handlePubsubRequest(request, client); // 处理发布订阅请求
                    break;
                default:
                    SPDLOG_LOGGER_WARN(Logger, "unknown message type: {}", static_cast<uint8_t>(type));
                    break;
            }

            if (response && session->isConnected()) {
                session->sendProtocol(response); // 发送响应
            }
        };
    }
}

Protocol::ptr RpcServer::handleMethodCall(Protocol::ptr proto) {
    std::string func_name;
    Serializer request(proto->getContent()); // 如果是一个函数调用，则content中的内容应该是
    // 能只反序列化出函数名，而没有将后面的参数也反序列化，是因为函数名是以字符串的格式序列化的，
    // 在序列化时会将字符串的长度先写入bytearray，然后再写入字符串
    request >> func_name;
    Serializer result = call(func_name, request.toString()); // 反序列化后bytearray的position移动到了参数的开始位置，所以直接tostring可以得到后面的所有参数
    Protocol::ptr response = Protocol::Create(Protocol::MsgType::RPC_METHOD_RESPONSE, result.toString(), proto->getSequenceId());
    return response;
}

Protocol::ptr RpcServer::handleHeartbeatPacket(Protocol::ptr proto) {
    return Protocol::HeartBeat();
}

Protocol::ptr RpcServer::handlePubsubRequest(Protocol::ptr proto, Socket::ptr client) {
    PubsubRequest request;
    Serializer s(proto->getContent());
    s >> request;

    PubsubResponse response{.type = request.type};
    switch (request.type) {
        case PubsubMsgType::Publish:
            go [ request, this] {
                publish(request.channel, request.message);
            };
            break;
        case PubsubMsgType::Subscribe:
            subscribe(request.channel, client);
            response.channel = request.channel;
            break;
        case PubsubMsgType::Unsubscribe:
            unsubscribe(request.channel, client);
            response.channel = request.channel;
            break;
        case PubsubMsgType::PatternSubscribe:
            patternSubscribe(request.pattern, client);
            response.pattern = request.pattern;
            break;
        case PubsubMsgType::PatternUnsubscribe:
            patternUnsubscribe(request.pattern, client);
            response.pattern = request.pattern;
            break;
        default:
            SPDLOG_LOGGER_DEBUG(Logger, "unexpect PubsubMsgType: {}", static_cast<uint8_t>(request.type));
            return {};
            break;
    }
    s.reset();
    // 序列化response
    s << response;
    s.reset();
    return Protocol::Create(Protocol::MsgType::RPC_PUBSUB_RESPONSE, s.toString(), proto->getSequenceId());
}

void RpcServer::subscribe(const std::string& channel, Socket::ptr client) {
    std::unique_lock<MutexType> loc(m_pubsubMutex);
    auto& clients = m_pubsubChannels[channel];
    clients.emplace_back(std::move(client));
}
void RpcServer::unsubscribe(const std::string& channel, Socket::ptr client) {
    std::unique_lock<MutexType> loc(m_pubsubMutex);
    // it是指向含有channel 的元素的迭代器
    auto it = m_pubsubChannels.find(channel);
    if (it == m_pubsubChannels.end()) {
        return;
    }

    // clients是一个list，里面存放了订阅channel的客户端
    auto& clients  = it->second;
    // iter是一个指向每个客户端socket的迭代器
    auto iter = clients.begin();
    while (iter != clients.end()) {
        // 如果客户端socket已经断开连接或者是要取消订阅的客户端socket
        // 也就是在取消指定客户端订阅的同时，检查一遍是否有断开连接的客户端
        if (!(*iter)->isConnected() || (*iter) == client) {
            iter = clients.erase(iter);
            continue;
        }
        ++iter;
    }
    if (clients.empty()) {
        m_pubsubChannels.erase(it);
        return;
    }
}
void RpcServer::patternSubscribe(const std::string& pattern, Socket::ptr client) {
    std::unique_lock<MutexType> lock(m_pubsubMutex);
    m_patternChannels.emplace_back(std::make_pair(pattern, client));
}

void RpcServer::patternUnsubscribe(const std::string& pattern, Socket::ptr client) {
    std::unique_lock<MutexType> lock(m_pubsubMutex);
    auto iter = m_patternChannels.begin();
    while(iter != m_patternChannels.end()) {
        if (!iter->second->isConnected() || iter->first == pattern && iter->second == client) {
            m_patternChannels.erase(iter);
            continue;
        }
        ++iter;
    }
}


} // namespace RR::rpc