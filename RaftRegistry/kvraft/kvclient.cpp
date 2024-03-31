//
// File created on: 2024/03/28
// Author: Zizhou

#include "kvclient.h"
#include "RaftRegistry/command/config.h"
#include <random>
#include <string>
#include <vector>

namespace RR::kvraft {
static auto Logger = GetLoggerInstance();

// 定义 rpc 连接超时时间的配置变量，默认为 3000 毫秒
static ConfigVar<uint64_t>::ptr g_rpc_timeout = Config::LookUp<uint64_t>("kvraft.rpc.timeout", 3000, "kvraft rpc timeout(ms)");
// 定义连接重试的延时的配置变量，默认为 2000 毫秒
static ConfigVar<uint32_t>::ptr g_connect_delay = Config::LookUp<uint32_t>("kvraft.rpc.reconnect_delay", 2000, "kvraft rpc reconnect delay(ms)");

static uint64_t s_rpc_timeout;
static uint32_t s_connect_delay;

namespace {
    struct KVClientIniter {
        KVClientIniter() {
            s_rpc_timeout = g_rpc_timeout->getValue();
            g_rpc_timeout->addListener([] (const uint64_t& old_value, const uint64_t& new_value) {
                SPDLOG_LOGGER_INFO(Logger, "kvraft rpc timeout changed from {} to {}", old_value, new_value);
                s_rpc_timeout = new_value;
            });
            s_connect_delay = g_connect_delay->getValue();
            g_connect_delay->addListener([] (const uint32_t& old_value, const uint32_t& new_value) {
                SPDLOG_LOGGER_INFO(Logger, "kvraft rpc reconnect delay changed from {} to {}", old_value, new_value);
                s_connect_delay = new_value;
            });
        }
    };

    // 实例化，以触发构造函数中的逻辑
    [[maybe_unused]] static KVClientIniter s_initer;
}

KVClient::KVClient(std::map<int64_t, std::string>& servers) {
    for (auto peer : servers) {
        // 查找每个服务器的地址，并将结果存储在address中
        Address::ptr address = Address::LookUpAny(peer.second);
        if (!address) {
            SPDLOG_LOGGER_ERROR(Logger, "lookup server[{}] address fail, address: {}", peer.first, peer.second);
            continue;
        }
        m_servers[peer.first] = address;
    }
    // 设置rpc超时时间
    setTimeout(s_rpc_timeout);
    setHeartbeat(false);
    if (servers.empty()) {
        SPDLOG_LOGGER_CRITICAL(Logger, "servers empty");
    } else {
        // 如果servers不为空，将m_servers中的第一个服务器设置为领导者
        m_leaderId = m_servers.begin()->first;
    }
    m_stop = false;
}

KVClient::~KVClient() {
    // 停止心跳定时器
    m_heart.stop();
    m_stop = true;
    sleep(5);
}

// 声明键值存储的基本操作接口，包括获取、放置、追加、删除和清除

Error KVClient::Get(const std::string& key, const std::string& value) {
    CommandRequest request{.op = GET, .key = key};
    CommandResponse response = Command(request);
    value = response.value;
    return response.err;
}
Error KVClient::Put(const std::string& key, const std::string& value) {
    CommandRequest request{.op = PUT, .key = key, .value = value};
    return Command(request).err;
}
Error KVClient::Append(const std::string& key, const std::string& value) {
    CommandRequest request{.op = APPEND, .key = key, .value = value};
    return Command(request).err;
}
Error KVClient::Delete(const std::string& key) {
    CommandRequest request{.op = DELETE, .key = key};
    return Command(request).err;
}
Error KVClient::Clear() {
    CommandRequest request{.op = CLEAR};
    return Command(request).err;
}

CommandResponse KVClient::Command(CommandRequest& request) {
    request.clientId = m_clientId;
    request.commandId = m_commandId;
    // 开始一个循环，只要m_stop为false就继续。
    while(!m_stop) {
        CommandResponse response;
        if(!connect()) { // 如果与当前的leader连接失败，则调用nextLeaderId()函数获取下一个leader的id
            m_leaderId = nextLeaderId();
            co_sleep(s_connect_delay);
            continue;
        }

        // 使用call调用远程函数，call是rpc_client.h中的一个模板函数
        // 将这里的的CommandRequest作为数据包传给call函数，然后由call函数真正的在网络中发起rpc调用
        Result<CommandResponse> result = call<CommandResponse>(COMMAND, request);
        if (result.getCode() == RpcState::RPC_SUCCESS) {
            response = result.getVal();
        }
        if (result.getCode() == RpcState::RPC_CLOSED) {
            // 关闭 RPC 客户端，断开与服务器的连接，清理资源
            RpcClient::close();
        }
        // 如果RPC调用失败或者返回的错误码为WRONG_LEADER，则将m_leaderId设置为response中的leaderId
        if (result.getCode() != RpcState::RPC_SUCCESS || response.err == WRONG_LEADER) {
            if (response.leaderId >= 0) { // 如果response获取到了leaderId，则将m_leaderId设置为response中的leaderId
                m_leaderId = response.leaderId;
            } else { // 如果response没有获取到leaderId，则调用nextLeaderId()函数获取下一个leader的id
                m_leaderId = nextLeaderId();
            }
            // 如果调用失败，证明当前rpc连接有问题，所以关闭。
            // 循环的下一轮会调用connect重新连接，获取新的leader（使用上面设置的m_leaderId）
            RpcClient::close();
            continue;
        }
        ++m_commandId;
        return response;
    }
    return {.err = Error::CLOSED};
}

bool KVClient::connect() {
    if (!isClosed()) {
        return true;
    }

    // 获取当前leader的地址
    auto address = m_servers[m_leaderId];
    // 连接到leader
    RpcClient::connect(address);
    if(!isClosed()) {
        // 如果心跳已经被取消，则重新启动心跳
        if (m_heart.isCancel()) {
            // 启动一个周期为3000毫秒的定时器，每次触发时调用一个lambda函数
            // lambda函数调用KVClient的Get方法，发送一个空的GET请求，用于保持心跳
            m_heart = CycleTimer(3000, [this] {
                [[maybe unused]] std::string dummy;
                Get("", dummy);
            });
        }
        // 连接成功，返回true
        return true;
    }
    return false;
}

int64_t KVClient::nextLeaderId() {
    auto iter = m_servers.find(m_leaderId);
    // 如果迭代器等于m_servers的end迭代器，说明m_leaderId在m_servers中不存在
    if (iter == m_servers.end()) {
        SPDLOG_LOGGER_CRITICAL(Logger, "leader id {} not exist", m_leaderId);
        return 0;
    }
    // 根据函数名可知，这里是获取下一个leader的id
    ++iter;
    if (iter == m_servers.end()) {
        return m_servers.begin()->first;
    }

    return iter->first;
}

int64_t KVClient::GetRandom() {
    static std::default_random_engine engine(RR::GetCuurentTimeMs());
    static std::uniform_int_distribution<int64_t> dist(0, INT64_MAX);
    return dist(engine);
}

void KVClient::subscribe(PubsubListener::ptr listener, const std::vector<std::string>& channels) {
    {
        std::unique_lock<MutexType> lock(m_mutex);
        // 将channels赋值给m_subs
        m_subs = channels;
    }
    while (true) {
        // 连接上leader，Get方法用于建立连接
        [[maybe_unused]] std::string dummy;
        // Get的调用可能是为了确保与leader的连接是活动的
        Get("", dummy);
        // 定义一个字符串向量vec
        std::vector<std::string> vec;
        {
            std::unique_lock<MutexType> lock(m_mutex);
            // 将m_subs赋值给vec
            vec = m_subs;
        }
        // 调用RpcClient的subscribe方法，如果成功则跳出循环
        if (RpcClient::subscribe(listener, vec)) {
            break;
        }
    }
}

void KVClient::unsubscribe(std::string& channel) {
    // 先在本地的订阅频道列表中删除channel（逻辑删除）
    {
        std::unique_lock<MutexType> lock(m_mutex);
        m_subs.erase(std::remove(m_subs.begin(), m_subs.end(), channel), m_subs.end())
    }
    // 然后进行物理删除
    RpcClient::unsubscribe(channel);
}

void KVClient::patternSubscribe(PubsubListener::ptr listener, const std::vector<std::string>& patterns) {
    {
        std::unique_lock<MutexType> lock(m_mutex);
        m_subs = patterns;
    }
    while(true) {
        [[maybe_unused]] std::string dummy;
        Get("", dummy);
        std::vector<std::string> vec;
        {
            std::unique_lock<MutexType> lock(m_mutex);
            vec = m_subs;
        }
        if (RpcClient::patternSubscribe(listener, vec)) {
            break;
        }
    }
}

void KVClient::patternUnsubscribe(std::string& pattern) {
    {
        std::unique_lock<MutexType> lock(m_mutex);
        msubs.erase(std::remove(m_subs.begin(), m_subs.end(), pattern), m_subs.end());
    }
    RpcClient::patternUnsubscribe(pattern);
}

} // namespace RR::kvraft