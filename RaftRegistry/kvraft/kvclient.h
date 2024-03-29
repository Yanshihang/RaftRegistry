//
// File created on: 2024/03/28
// Author: Zizhou

#ifndef RR_KVRAFT_KVCLIENT_H
#define RR_KVRAFT_KVCLIENT_H

#include <unordered_map>
#include <map>
#include <atomic>
#include <memory>
#include <vector>
#include <libgo/libgo.h>
#include <optional>
#include "RaftRegistry/common/util.h"
#include "RaftRegistry/rpc/rpc_client.h"
#include "RaftRegistry/net/address.h"
#include "common.h"

namespace RR::kvraft {
using namespace RR::rpc;

class KVClient : public RpcClient {
public:
    using ptr = std::shared_ptr<KVClient>;
    using MutexType = co::co_mutex;

    KVClient(std::map<int64_t, std::string>& servers);
    ~KVClient();

    // 声明键值存储的基本操作接口，包括获取、放置、追加、删除和清除

    Error Get(const std::string& key, const std::string& value);
    Error Put(const std::string& key, const std::string& value);
    Error Append(const std::string& key, const std::string& value);
    Error Delete(const std::string& key);
    Error Clear();

    /**
     * @brief 订阅频道，会阻塞
     * @tparam Args std::string ...
     * @tparam Listener 用于监听订阅事件
     * @param channels 订阅的频道列表
     * @return 当成功取消所有的订阅后返回，否则一直阻塞
     */
    template <typename... Args>
    void subscribe(PubsubListener::ptr listener, const Args&... channels) {
        static_assert(sizeof...(channels));
        subscribe(std::move(listener), {channels...});
    }

    void subscribe(PubsubListener::ptr listener, const std::vector<std::string>& channels);

    /**
     * @brief 取消订阅channel中指定的频道
     * 
     * @param channel 
     */
    void unsubscribe(const std::string& channel);

    /**
     * 模式订阅，阻塞
     * 匹配规则：
     *  '?': 匹配 0 或 1 个字符
     *  '*': 匹配 0 或更多个字符
     *  '+': 匹配 1 或更多个字符
     *  '@': 如果任何模式恰好出现一次
     *  '!': 如果任何模式都不匹配
     * @tparam Args std::string ...
     * @param listener 用于监听订阅事件
     * @param patterns 订阅的模式列表
     * @return 当成功取消所有的订阅后返回，否则一直阻塞
     */
    template<typename... Args>
    void patternSubscribe(PubsubListener::ptr listener, const Args&... patterns) {
        static_assert(sizeof...(patterns));
        patternSubscribe(std::move(listener), {patterns...});
    }
    void patternSubscribe(PubsubListener::ptr listener, const std::vector<std::string>& patterns);

    void patternUnsubscribe(std::string& pattern);

private:
    CommandResponse Command(CommandRequest& request);
    bool connect();
    int64_t nextLeaderId();
    // 获取一个随机数
    static int64_t GetRandom();

private:
// 服务器列表、客户端 ID、领导者 ID、命令 ID、停止标志、订阅列表、互斥锁和心跳计时器

// 服务器列表, key 为服务器 ID，value 为服务器地址
std::unordered_map<int64_t, Address::ptr> m_servers;
int64_t m_clientId;
int64_t m_leaderId;
int64_t m_commandId;
std::atomic_bool m_stop;
co::co_chan<bool> m_stopChan;
std::vector<std::string> m_subs;
MutexType m_pubsubMutex;
CycleTimerTocken m_heart;
}

}

#endif // RR_KVRAFT_KVCLIENT_H